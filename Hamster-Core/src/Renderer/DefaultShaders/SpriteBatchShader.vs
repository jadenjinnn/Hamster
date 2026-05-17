#version 400 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aColour;

out vec2 vUV;
out vec3 vColour;

uniform mat4 projection;

void main() {
    vUV = aUV;
    vColour = aColour;
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
}
