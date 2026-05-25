#version 400 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColour;

out vec4 vColour;

uniform mat4 projection;

void main() {
    vColour = aColour;
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
}
