#version 140

uniform sampler2D texUnit;
uniform float offset;
uniform vec2 halfpixel;
uniform float saturation;

in vec2 uv;

out vec4 fragColor;

// Saturation as a mix with the luminance (premultiplied colour).
vec4 applySaturation(vec4 color)
{
    float luminance = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    return vec4(clamp(mix(vec3(luminance), color.rgb, saturation), 0.0, color.a), color.a);
}

void main(void)
{
    vec4 sum = texture(texUnit, uv + vec2(-halfpixel.x * 2.0, 0.0) * offset);
    sum += texture(texUnit, uv + vec2(-halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(0.0, halfpixel.y * 2.0) * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(halfpixel.x * 2.0, 0.0) * offset);
    sum += texture(texUnit, uv + vec2(halfpixel.x, -halfpixel.y) * offset) * 2.0;
    sum += texture(texUnit, uv + vec2(0.0, -halfpixel.y * 2.0) * offset);
    sum += texture(texUnit, uv + vec2(-halfpixel.x, -halfpixel.y) * offset) * 2.0;

    fragColor = applySaturation(sum / 12.0);
}
