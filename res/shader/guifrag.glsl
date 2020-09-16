#version 330

in vec2 uvout;

out vec4 Color;

uniform sampler2D gui_tex;
uniform mat4 viewmat;
uniform mat4 modelmat;

void main()
{
    vec4 matdiffuse = texture2D(gui_tex, uvout).rgba;

    Color = matdiffuse;
}
