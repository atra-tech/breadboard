#version 330

in vec2 uvout;
in vec4 colors;

out vec4 Color;

uniform sampler2D gui_tex;
uniform mat4 viewmat;
uniform mat4 modelmat;

void main()
{
    vec4 matdiffuse = texture2D(gui_tex, uvout).rgba;

    if (matdiffuse.a > 0.0) {
        matdiffuse.r = colors.r;
        matdiffuse.g = colors.g;
        matdiffuse.b = colors.b;
        matdiffuse.a = colors.a;
    }

    Color = matdiffuse;
}
