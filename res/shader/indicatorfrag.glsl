#version 330

out vec4 Color;

uniform mat4 viewmat;
uniform mat4 modelmat;
uniform vec4 color;

void main()
{
    Color = color;
}
