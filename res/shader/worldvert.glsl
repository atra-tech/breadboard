#version 330

layout (location = 0) in vec3 verts;
layout (location = 1) in vec2 uvs;
layout (location = 2) in vec3 norms;
layout (location = 3) in vec3 color;

uniform mat4 projviewmat;
uniform mat4 viewmat;
uniform mat4 modelmat;

out vec2 uvout;
out vec3 wirecolor;

out vec2 uvmin;
out vec2 uvmax;

const float img_size = 1024.0;
const float tex_size = 64.0;
const float pix_size = 1.0 / img_size;

const float offset = pix_size * 0.001;

const float tex_size_uv = tex_size / img_size;

void main()
{
    float tex_pos_u = floor(uvs[0] / tex_size_uv) * tex_size_uv;
    float tex_pos_v = floor(uvs[1] / tex_size_uv) * tex_size_uv;

    uvout = uvs;
    wirecolor = color;
    
    uvmin = vec2(tex_pos_u + offset, tex_pos_v + offset);
    uvmax = uvmin + vec2(tex_size_uv - 2 * offset, tex_size_uv - 2 * offset);

    gl_Position = projviewmat * modelmat * vec4(verts, 1.0);
}
