#version 330

in vec2 uvout;

out vec4 Color;

uniform sampler2D albedo_map;
uniform mat4 viewmat;
uniform mat4 modelmat;

void main()
{
    vec4 matdiffuse = texture2D(albedo_map, uvout).rgba;
    Color = matdiffuse;
}
