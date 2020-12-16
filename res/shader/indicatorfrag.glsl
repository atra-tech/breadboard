#version 330

out vec4 Color;

uniform mat4 viewmat;
uniform mat4 modelmat;
uniform vec4 color;

void main()
{
    //Color = color;
    Color.r = color.r;
    Color.g = color.g;
    Color.b = color.b;
    Color.a = color.a;
}
