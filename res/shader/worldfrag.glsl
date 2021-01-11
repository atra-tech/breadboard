#version 330

in vec2 uvout;
in vec3 wirecolor;

out vec4 Color;

uniform sampler2D albedo_map;
uniform mat4 viewmat;
uniform mat4 modelmat;

void main()
{
    vec4 matdiffuse = texture2D(albedo_map, uvout).rgba;

    if (matdiffuse.a < 0.1)
        discard;

    Color = vec4(matdiffuse.rgb * wirecolor, matdiffuse.a);
}
