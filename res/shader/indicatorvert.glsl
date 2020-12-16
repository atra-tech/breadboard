#version 330

layout (location = 0) in vec3 verts;
layout (location = 1) in vec2 uvs;
layout (location = 2) in vec3 norms;

uniform mat4 projviewmat;
uniform mat4 viewmat;
uniform mat4 modelmat;
uniform vec4 color;

out vec4 colors;

void main()
{
    gl_Position = projviewmat * modelmat * vec4(verts, 1.0);
}
