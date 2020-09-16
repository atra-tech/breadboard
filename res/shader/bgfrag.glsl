#version 330

in vec2 uvout;

out vec4 color;

uniform sampler2D albedo_map;

void main()
{
    color = texture2D(albedo_map, uvout);
}
