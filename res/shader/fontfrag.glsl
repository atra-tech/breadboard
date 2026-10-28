#version 330

in vec2 uvout;
in vec4 colors;

in vec2 uvmin;
in vec2 uvmax;

out vec4 Color;

uniform sampler2D gui_tex;
uniform mat4 viewmat;
uniform mat4 modelmat;

const float img_size = 256.0;
const float tex_size = 16.0;
const float pix_size = 1.0 / img_size;

vec4
texture2D_bilinear(sampler2D tex_sampler, vec2 tex_coord)
{
    float offset = pix_size * 0.5;

    float x0 = clamp(tex_coord[0] - offset, uvmin[0], uvmax[0]);
    float y0 = clamp(tex_coord[1] - offset, uvmin[1], uvmax[1]);

    float x1 = clamp(tex_coord[0] + offset, uvmin[0], uvmax[0]);
    float y1 = clamp(tex_coord[1] + offset, uvmin[1], uvmax[1]);
    
    vec4 x0y0 = texture2D(tex_sampler, vec2(x0, y0));
    vec4 x1y0 = texture2D(tex_sampler, vec2(x1, y0));
    vec4 x0y1 = texture2D(tex_sampler, vec2(x0, y1));
    vec4 x1y1 = texture2D(tex_sampler, vec2(x1, y1));

    float a = fract((tex_coord[0] - offset) * img_size);

    vec4 lerp_y0 = vec4(
            mix(x0y0.r, x1y0.r, a),
            mix(x0y0.g, x1y0.g, a),
            mix(x0y0.b, x1y0.b, a),
            mix(x0y0.a, x1y0.a, a));

    vec4 lerp_y1 = vec4(
            mix(x0y1.r, x1y1.r, a),
            mix(x0y1.g, x1y1.g, a),
            mix(x0y1.b, x1y1.b, a),
            mix(x0y1.a, x1y1.a, a));

    float b = fract((tex_coord[1] - offset) * img_size);

    return vec4(
            mix(lerp_y0.r, lerp_y1.r, b),
            mix(lerp_y0.g, lerp_y1.g, b),
            mix(lerp_y0.b, lerp_y1.b, b),
            mix(lerp_y0.a, lerp_y1.a, b));
}
void main()
{
    vec4 matdiffuse = texture2D_bilinear(gui_tex, uvout).rgba;

    matdiffuse *= colors;

    Color = matdiffuse;
}
