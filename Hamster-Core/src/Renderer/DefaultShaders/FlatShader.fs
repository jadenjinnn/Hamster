#version 400 core

in vec2 TexCoords;
uniform vec3 colour;
uniform float alpha;
uniform int borderMode;
uniform float borderWidthX;
uniform float borderWidthY;

out vec4 FragColour;

void main()
{
    if (borderMode != 0) {
        if (TexCoords.x > borderWidthX && TexCoords.x < 1.0 - borderWidthX &&
            TexCoords.y > borderWidthY && TexCoords.y < 1.0 - borderWidthY)
            discard;
    }
    FragColour = vec4(colour, alpha);
}
