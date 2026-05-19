#version 400 core

in vec2 TexCoords;
out vec4 FragColour;

uniform sampler2D image;
uniform vec3 spriteColour;
// Sub-sprite UV window into the bound texture. (x,y) = top-left in [0,1],
// (z,w) = width/height in [0,1]. Default (0,0,1,1) = whole texture.
uniform vec4 uvRect;

void main()
{
    vec2 uv = uvRect.xy + TexCoords * uvRect.zw;
    FragColour = vec4(spriteColour, 1.0f) * texture(image, uv);
}
