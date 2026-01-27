/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#version 450

// Specialization constants for shader traits
layout(constant_id = 0) const bool TRAIT_MAP_TEXTURE = false;
layout(constant_id = 1) const bool TRAIT_UNIFORM_COLOR = false;
layout(constant_id = 2) const bool TRAIT_MODULATE = false;
layout(constant_id = 3) const bool TRAIT_ADJUST_SATURATION = false;
layout(constant_id = 4) const bool TRAIT_TRANSFORM_COLORSPACE = false;
layout(constant_id = 5) const bool TRAIT_ROUNDED_CORNERS = false;
layout(constant_id = 6) const bool TRAIT_BORDER = false;

// Input from vertex shader
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec2 fragPosition;

// Output color
layout(location = 0) out vec4 outColor;

// Texture sampler (binding 0)
layout(set = 0, binding = 0) uniform sampler2D texSampler;

// Uniform buffer (binding 1)
layout(set = 0, binding = 1) uniform UniformBufferObject {
    vec4 uniformColor;
    float opacity;
    float brightness;
    float saturation;
    vec3 primaryBrightness;

    // Rounded corners
    vec4 geometryBox;     // x, y, width, height
    vec4 borderRadius;    // topLeft, topRight, bottomRight, bottomLeft

    // Border
    float borderThickness;
    vec4 borderColor;

    // Color management
    int sourceTransferFunction;
    vec2 sourceTransferParams;
    int destTransferFunction;
    vec2 destTransferParams;
    mat4 colorimetryTransform;
    float sourceReferenceLuminance;
    float maxTonemappingLuminance;
    float destReferenceLuminance;
    float maxDestLuminance;
    mat4 destToLMS;
    mat4 lmsToDest;
} ubo;

// Color management constants
const int sRGB_EOTF = 0;
const int linear_EOTF = 1;
const int PQ_EOTF = 2;
const int gamma22_EOTF = 3;

// === Color Management Functions ===

vec3 srgbToLinear(vec3 color) {
    bvec3 isLow = lessThanEqual(color, vec3(0.04045));
    vec3 loPart = color / 12.92;
    vec3 hiPart = pow((color + 0.055) / 1.055, vec3(12.0 / 5.0));
    return mix(hiPart, loPart, isLow);
}

vec3 linearToSrgb(vec3 color) {
    bvec3 isLow = lessThanEqual(color, vec3(0.0031308));
    vec3 loPart = color * 12.92;
    vec3 hiPart = pow(color, vec3(5.0 / 12.0)) * 1.055 - 0.055;
    return mix(hiPart, loPart, isLow);
}

vec3 linearToPq(vec3 linear) {
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    vec3 powed = pow(clamp(linear, vec3(0), vec3(1)), vec3(m1));
    vec3 num = vec3(c1) + c2 * powed;
    vec3 denum = vec3(1.0) + c3 * powed;
    return pow(num / denum, vec3(m2));
}

vec3 pqToLinear(vec3 pq) {
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1_inv = 1.0 / 0.1593017578125;
    const float m2_inv = 1.0 / 78.84375;
    vec3 powed = pow(clamp(pq, vec3(0.0), vec3(1.0)), vec3(m2_inv));
    vec3 num = max(powed - c1, vec3(0.0));
    vec3 den = c2 - c3 * powed;
    return pow(num / den, vec3(m1_inv));
}

float singleLinearToPq(float linear) {
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    float powed = pow(clamp(linear, 0.0, 1.0), m1);
    float num = c1 + c2 * powed;
    float denum = 1.0 + c3 * powed;
    return pow(num / denum, m2);
}

float singlePqToLinear(float pq) {
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1_inv = 1.0 / 0.1593017578125;
    const float m2_inv = 1.0 / 78.84375;
    float powed = pow(clamp(pq, 0.0, 1.0), m2_inv);
    float num = max(powed - c1, 0.0);
    float den = c2 - c3 * powed;
    return pow(num / den, m1_inv);
}

const mat3 toICtCp = mat3(
    0.5,  1.613769531250,   4.378173828125,
    0.5, -3.323486328125, -4.245605468750,
    0.0,  1.709716796875, -0.132568359375
);
const mat3 fromICtCp = mat3(
    1.0,               1.0,             1.0,
    0.00860903703793, -0.008609037037,  0.56031335710680,
    0.11102962500303, -0.111029625003, -0.32062717498732
);

vec3 doTonemapping(vec3 color) {
    if (ubo.maxTonemappingLuminance < ubo.maxDestLuminance * 1.01) {
        return clamp(color.rgb, vec3(0.0), vec3(ubo.maxDestLuminance));
    }

    vec3 lms = (ubo.destToLMS * vec4(color, 1.0)).rgb;
    vec3 lms_PQ = linearToPq(lms / 10000.0);
    vec3 ICtCp = toICtCp * lms_PQ;
    float luminance = singlePqToLinear(ICtCp.r) * 10000.0;

    float inputRange = ubo.maxTonemappingLuminance / ubo.destReferenceLuminance;
    float outputRange = ubo.maxDestLuminance / ubo.destReferenceLuminance;
    float minDecentRange = min(inputRange, 1.5);
    float referenceDimming = 1.0 / clamp(minDecentRange / outputRange, 1.0, minDecentRange);
    float outputReferenceLuminance = ubo.destReferenceLuminance * referenceDimming;

    float low = min(luminance * referenceDimming, outputReferenceLuminance);
    float relativeHighlight = clamp((luminance / ubo.destReferenceLuminance - 1.0) / (inputRange - 1.0), 0.0, 1.0);
    const float e = 2.718281828459045;
    float high = log(relativeHighlight * (e - 1.0) + 1.0) * (ubo.maxDestLuminance - outputReferenceLuminance);
    luminance = low + high;

    ICtCp.r = singleLinearToPq(luminance / 10000.0);
    return (ubo.lmsToDest * vec4(pqToLinear(fromICtCp * ICtCp), 1.0)).rgb * 10000.0;
}

vec4 encodingToNits(vec4 color, int transferFunction, float luminanceOffset, float luminanceScale) {
    if (transferFunction == sRGB_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = srgbToLinear(color.rgb) * luminanceScale + vec3(luminanceOffset);
        color.rgb *= color.a;
    } else if (transferFunction == linear_EOTF) {
        color.rgb = color.rgb * luminanceScale + vec3(luminanceOffset);
    } else if (transferFunction == PQ_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = pqToLinear(color.rgb) * luminanceScale + vec3(luminanceOffset);
        color.rgb *= color.a;
    } else if (transferFunction == gamma22_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(2.2)) * luminanceScale + vec3(luminanceOffset);
        color.rgb *= color.a;
    }
    return color;
}

vec4 nitsToEncoding(vec4 color, int transferFunction, float luminanceOffset, float luminanceScale) {
    if (transferFunction == sRGB_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = linearToSrgb((color.rgb - vec3(luminanceOffset)) / luminanceScale);
        color.rgb *= color.a;
    } else if (transferFunction == linear_EOTF) {
        color.rgb = (color.rgb - vec3(luminanceOffset)) / luminanceScale;
    } else if (transferFunction == PQ_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = linearToPq((color.rgb - vec3(luminanceOffset)) / luminanceScale);
        color.rgb *= color.a;
    } else if (transferFunction == gamma22_EOTF) {
        color.rgb /= max(color.a, 0.001);
        color.rgb = pow(max((color.rgb - vec3(luminanceOffset)) / luminanceScale, vec3(0.0)), vec3(1.0 / 2.2));
        color.rgb *= color.a;
    }
    return color;
}

// === Saturation Adjustment ===

vec4 adjustSaturation(vec4 color) {
    float Y = dot(color.rgb, ubo.primaryBrightness);
    return vec4(mix(vec3(Y), color.rgb, ubo.saturation), color.a);
}

// === SDF Rounded Corners ===

float sdfRoundedBox(vec2 position, vec2 center, vec2 extents, vec4 radius) {
    vec2 p = position - center;
    float r = p.x > 0.0
        ? (p.y < 0.0 ? radius.y : radius.w)
        : (p.y < 0.0 ? radius.x : radius.z);
    vec2 q = abs(p) - extents + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

float sdfSubtract(float f1, float f2) {
    return max(f1, -f2);
}

// === Main ===

void main() {
    vec4 color = vec4(1.0);

    // Get base color
    if (TRAIT_MAP_TEXTURE) {
        color = texture(texSampler, fragTexCoord);
    }

    if (TRAIT_UNIFORM_COLOR) {
        color = ubo.uniformColor;
    }

    // Apply modulation (opacity and brightness)
    if (TRAIT_MODULATE) {
        color.rgb *= ubo.brightness;
        color *= ubo.opacity;
    }

    // Apply saturation adjustment
    if (TRAIT_ADJUST_SATURATION) {
        color = adjustSaturation(color);
    }

    // Apply color space transformation
    if (TRAIT_TRANSFORM_COLORSPACE) {
        color = encodingToNits(color, ubo.sourceTransferFunction,
                               ubo.sourceTransferParams.x, ubo.sourceTransferParams.y);
        color.rgb = (ubo.colorimetryTransform * vec4(color.rgb, 1.0)).rgb;
        color.rgb = doTonemapping(color.rgb);
        color = nitsToEncoding(color, ubo.destTransferFunction,
                               ubo.destTransferParams.x, ubo.destTransferParams.y);
    }

    // Apply rounded corners
    if (TRAIT_ROUNDED_CORNERS) {
        vec2 center = ubo.geometryBox.xy + ubo.geometryBox.zw * 0.5;
        vec2 extents = ubo.geometryBox.zw * 0.5;
        float dist = sdfRoundedBox(fragPosition, center, extents, ubo.borderRadius);

        // Anti-aliased edge
        float alpha = 1.0 - smoothstep(-0.5, 0.5, dist);
        color.a *= alpha;
    }

    // Apply border
    if (TRAIT_BORDER) {
        vec2 center = ubo.geometryBox.xy + ubo.geometryBox.zw * 0.5;
        vec2 extents = ubo.geometryBox.zw * 0.5;
        float outerDist = sdfRoundedBox(fragPosition, center, extents, ubo.borderRadius);

        vec2 innerExtents = extents - vec2(ubo.borderThickness);
        vec4 innerRadius = max(ubo.borderRadius - vec4(ubo.borderThickness), vec4(0.0));
        float innerDist = sdfRoundedBox(fragPosition, center, innerExtents, innerRadius);

        float borderDist = sdfSubtract(outerDist, innerDist);
        float borderAlpha = 1.0 - smoothstep(-0.5, 0.5, borderDist);

        color = mix(color, ubo.borderColor, borderAlpha);
    }

    outColor = color;
}
