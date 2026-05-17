#version 400 core

in vec2 vUV;
in vec3 vColour;
out vec4 FragColour;

uniform sampler2D image;

void main() {
    FragColour = vec4(vColour, 1.0) * texture(image, vUV);
}
