#version 400 core

in vec2 vUV;
in vec4 vColour;
out vec4 FragColour;

uniform sampler2D image;

void main() {
    float a = texture(image, vUV).r;
    FragColour = vec4(vColour.rgb, vColour.a * a);
}
