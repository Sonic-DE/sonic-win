/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#version 450

// Vertex attributes
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

// Output to fragment shader
layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec2 fragPosition;

// Push constants for transformation matrices
layout(push_constant) uniform PushConstants {
    mat4 mvp;           // Model-View-Projection matrix
    mat4 textureMatrix; // Texture coordinate transformation
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 0.0, 1.0);
    fragTexCoord = (pc.textureMatrix * vec4(inTexCoord, 0.0, 1.0)).xy;
    fragPosition = inPosition;
}
