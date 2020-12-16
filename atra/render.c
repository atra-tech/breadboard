/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  A collection of rendering tools, buffers, shaders, etc.
 *
 *  */

#include <SDL2/SDL_image.h>
#include <string.h>

#include "render.h"
#include "util/htable.h"
#include "util/log.h"
#include "util/utf.h"

static GLuint shadercompile(const char *path, GLenum type); 
static GLuint shaderlink(GLuint vertshaderid, GLuint fragshaderid);
static void shadergetuniforms(Shader *shader);

static HTable *textures;
static HTable *fonts;

void
vbufinit(VBuffer *vbo)
{
    vbo->vao = 0;
    vbo->vbocount = 0;
    vbo->indxcount = 0;

    iveclistinit(&vbo->buffers, 1);
    veclistinit(&vbo->verts, 3);
    veclistinit(&vbo->uvs, 2);
    veclistinit(&vbo->norms, 3);
    iveclistinit(&vbo->indxs, 3);
}

void
vbufaddvertex(VBuffer *vbo, vec3 vertex, vec2 uv, vec3 normal)
{
    veclistpush(&vbo->verts, vertex);
    veclistpush(&vbo->uvs, uv);
    veclistpush(&vbo->norms, normal);
}

void
vbufaddindex(VBuffer *vbo, ivec3 index)
{
    iveclistpush(&vbo->indxs, index);
}

void
vbufbindvbo(VBuffer *vbo, VecList *data)
{
    uint buf;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, data->size * data->elemsize * sizeof(float),
                 data->data, GL_STATIC_DRAW);
    glVertexAttribPointer(vbo->vbocount, data->elemsize,
            GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(vbo->vbocount++);

    iveclistpush(&vbo->buffers, (int *)&buf);
}

void
vbufbindebo(VBuffer *vbo)
{
    uint ebo;

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            vbo->indxs.size * sizeof(int) * 3,
            vbo->indxs.data, GL_STATIC_DRAW);

    iveclistpush(&vbo->buffers, (int *)&ebo);
}

void
vbufbindvao(VBuffer *vbo)
{
    glBindVertexArray(vbo->vao);
}

void
vbufbind(VBuffer *vbo)
{
    if (vbo->vao != 0)
        vbufreset(vbo);

    glGenVertexArrays(1, &vbo->vao);
    vbufbindvao(vbo);
    vbufbindvbo(vbo, &vbo->verts);
    vbufbindvbo(vbo, &vbo->uvs);
    vbufbindvbo(vbo, &vbo->norms);
    vbufbindebo(vbo);
}

void
vbufclear(VBuffer *vbo)
{
    veclistclear(&vbo->verts);
    veclistclear(&vbo->uvs);
    veclistclear(&vbo->norms);
    iveclistclear(&vbo->indxs);

    vbo->indxcount = 0;
}

void
vbufreset(VBuffer *vbo)
{
    uint i;

    glDeleteVertexArrays(1, &vbo->vao);
    
    for (i = 0; i < vbo->buffers.size; i++) {
        glDeleteBuffers(1, (uint *)iveclistat(&vbo->buffers, i));
    }
    iveclistclear(&vbo->buffers);
    veclistclear(&vbo->verts);
    veclistclear(&vbo->uvs);
    veclistclear(&vbo->norms);
    iveclistclear(&vbo->indxs);

    vbo->vao = 0;
    vbo->vbocount = 0;
    vbo->indxcount = 0;
}
/*
void
vbufresetnoclear(vbuf *vbo)
{

}
*/
void
vbufdelete(VBuffer *vbo)
{
    uint i;
    glDeleteVertexArrays(1, &vbo->vao);

    for (i = 0; i < vbo->buffers.size; i++) {
        glDeleteBuffers(1, (uint *)iveclistat(&vbo->buffers, i));
    }

    iveclistdelete(&vbo->buffers);
    veclistdelete(&vbo->verts);
    veclistdelete(&vbo->uvs);
    veclistdelete(&vbo->norms);
    iveclistdelete(&vbo->indxs);

    vbo->vao = 0;
    vbo->vbocount = 0;
    vbo->indxcount = 0;
}

void
fbufinit(FBuffer *buffer, uint width, uint height, uint comp, uint attach)
{
    float bordercolor[] = {1.0f, 1.0f, 1.0f, 1.0f};

    buffer->width = width;
    buffer->height = height;

    glGenFramebuffers(1, &buffer->fbo);
    glGenTextures(1, &buffer->map);
    glBindTexture(GL_TEXTURE_2D, buffer->map);
    glTexImage2D(GL_TEXTURE_2D, 0, comp, width, height,
            0, comp, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bordercolor);
    
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, attach,
            GL_TEXTURE_2D, buffer->map, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void
fbufbind(FBuffer *buffer)
{
    glBindFramebuffer(GL_FRAMEBUFFER, buffer->fbo);
}
/*
void
fbufreset(FBuffer *buffer)
{

}*/

void
fbufdelete(FBuffer *buffer)
{
    glDeleteTextures(1, &buffer->map);
    glDeleteFramebuffers(1, &buffer->fbo);
}

void
shaderinit(Shader *shader, const char *vertsrc, const char *fragsrc)
{
    shader->prog = shaderload(vertsrc, fragsrc);
    shadergetuniforms(shader);
}

void
shaderdelete(Shader *shader)
{
    glDeleteProgram(shader->prog);
}

GLuint
shaderload(const char *vertpath, const char *fragpath)
{
    GLuint vertshaderid = shadercompile(vertpath, GL_VERTEX_SHADER);
    GLuint fragshaderid = shadercompile(fragpath, GL_FRAGMENT_SHADER);
    GLuint progid = shaderlink(vertshaderid, fragshaderid);
    return progid;
}

GLuint
shadercompile(const char *path, GLenum type)
{
    GLuint shaderid = glCreateShader(type);
    char *code = loadfile(path);
    const char *data = code;
    GLint result = GL_FALSE;
    int loglength = 0;

    logprint(LOG_INFO, "Compiling shader: $s", path);
    glShaderSource(shaderid, 1, &data, NULL);
    glCompileShader(shaderid);

    glGetShaderiv(shaderid, GL_COMPILE_STATUS, &result);
    glGetShaderiv(shaderid, GL_INFO_LOG_LENGTH, &loglength);

    if (loglength > 0) {
        char *info = calloc(loglength + 1, sizeof(char));
        glGetProgramInfoLog(shaderid, loglength, NULL, info);
        logprint(LOG_FATAL, "shader: Unable to compile shader: $s", info);
        free(info);
    }
    free(code);

    return shaderid;
}

GLuint
shaderlink(GLuint vertshaderid, GLuint fragshaderid)
{
    GLuint progid = glCreateProgram();
    GLint result = GL_FALSE;
    int loglength = 0;

    logprint(LOG_INFO, "Linking program");
    glAttachShader(progid, vertshaderid);
    glAttachShader(progid, fragshaderid);
    glLinkProgram(progid);

    glGetShaderiv(progid, GL_LINK_STATUS, &result);
    glGetShaderiv(progid, GL_INFO_LOG_LENGTH, &loglength);

    if (loglength > 0) {
        char *info = calloc(loglength, sizeof(char));
        glGetProgramInfoLog(progid, loglength, NULL, info);
        logprint(LOG_ERROR, "shader: Unable to link shaders: $s", info);
        free(info);
    }
    glDetachShader(progid, vertshaderid);
    glDetachShader(progid, fragshaderid);
    glDeleteShader(vertshaderid);
    glDeleteShader(fragshaderid);

    return progid;
}

void
shaderbind(Shader *shader)
{
    glUseProgram(shader->prog);
}

void
shaderunbind()
{
    glUseProgram(0);
}
void
shaderloadint(uint loc, int data)
{
    glUniform1i(loc, data);
}

void
shaderloadfloat(uint loc, float data)
{
    glUniform1f(loc, data);
}

void
shaderloadvec2(uint loc, const vec2 vec)
{
    glUniform2f(loc, vec[0], vec[1]);
}

void
shaderloadvec3(uint loc, const vec3 vec)
{
    glUniform3f(loc, vec[0], vec[1], vec[2]);
}

void
shaderloadvec4(uint loc, const vec4 vec)
{

    glUniform4f(loc, vec[0], vec[1], vec[2], vec[3]);
}

void
shaderloadmat4(uint loc, const mat4 mat)
{
    glUniformMatrix4fv(loc, 1, GL_FALSE, mat[0]);
}

void
shaderloadprojviewmat(Shader *shader, const mat4 matrix)
{
    shaderloadmat4(shader->projviewmat, matrix);
}

void
shaderloadmodelmat(Shader *shader, const mat4 matrix)
{
    shaderloadmat4(shader->modelmat, matrix);
}

void
shaderloadviewmat(Shader *shader, const mat4 matrix)
{
    shaderloadmat4(shader->viewmat, matrix);
}

void
shaderaddint(Shader *shader, const char *name, int data)
{
    uint loc = glGetUniformLocation(shader->prog, name);
    shaderloadint(loc, data);
}

void
shaderaddfloat(Shader *shader, const char *name, float data)
{
    uint loc = glGetUniformLocation(shader->prog, name);
    shaderloadfloat(loc, data);
}

void
shaderaddvec2(Shader *shader, const char *name, const vec2 vec)
{
    uint loc = glGetUniformLocation(shader->prog, name);
    shaderloadvec2(loc, vec);
}

void
shaderaddvec3(Shader *shader, const char *name, const vec3 vec)
{
    uint loc = glGetUniformLocation(shader->prog, name);
    shaderloadvec3(loc, vec);
}

void
shaderaddvec4(Shader *shader, const char *name, const vec4 vec)
{
    uint loc = glGetUniformLocation(shader->prog, name);
    shaderloadvec4(loc, vec);
}

void
shaderaddmat4(Shader *shader, const char *name, const mat4 mat)
{
    uint loc = glGetUniformLocation(shader->prog, name);
    shaderloadmat4(loc, mat);
}

void
shadergetuniforms(Shader *shader)
{
    shaderbind(shader);

    shader->projviewmat = glGetUniformLocation(shader->prog, "projviewmat");
    shader->modelmat = glGetUniformLocation(shader->prog, "modelmat");
    shader->viewmat = glGetUniformLocation(shader->prog, "viewmat");
}

void
texinit(void)
{
    textures = htablecreate(htablestringhash, htablestringequal);
}

void
texload(Texture *tex, const char *path)
{
    SDL_Surface *surface = IMG_Load(path);
    int mode = GL_RGB;

    if (!surface) {
        logprint(LOG_ERROR, "texture: Unable to load texture file $s: " \
                            "SDL_image error: $s", path, IMG_GetError());
        return;
    }
    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_2D, tex->id);

    if (surface->format->BytesPerPixel == 4) {
        mode = GL_RGBA;
    }
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 mode,
                 surface->w,
                 surface->h,
                 0,
                 mode,
                 GL_UNSIGNED_BYTE,
                 surface->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MIN_FILTER,
                    GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MAG_FILTER,
                    GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    tex->width = surface->w;
    tex->height = surface->h;

    SDL_FreeSurface(surface);
}

void
texbind(Texture *tex)
{
    glBindTexture(GL_TEXTURE_2D, tex->id);
}

void
texfind(Texture *tex, const char *path)
{
    int len;
    char *name = strrchr(path, '/');
    char *texname;
    Texture *texentry;

    if (!name) {
        name = (char *)path;
    }
    name++;
    texentry = htableget(textures, name);

    if (texentry) {
        *tex = *texentry;
        return;
    }
    len = strlen(name);
    texentry = malloc(sizeof(Texture));
    texname = calloc(len + 1, sizeof(char));
    memcpy(texname, name, len);
    texload(texentry, path);
    htableadd(textures, texname, texentry);
    *tex = *texentry;
}

void
texget(Texture *tex, int x, int y, float *uvs)
{
    float texperrow = tex->imagesize / (float)tex->texsize;
    float texsize = 1.0f / texperrow;
    float pixsize = texsize / (float)tex->texsize;
    float pixoffset = pixsize * 0.001f;

    float xmin = x * texsize + pixoffset;
    float ymin = y * texsize + pixoffset;
    float xmax = (xmin + texsize) - (pixoffset * 2);
    float ymax = (ymin + texsize) - (pixoffset * 2);

    uvs[0] = xmax; uvs[1] = ymax;
    uvs[2] = xmin; uvs[3] = ymax;
    uvs[4] = xmin; uvs[5] = ymin;
    uvs[6] = xmax; uvs[7] = ymin;
}

void
texgetrect(Texture *tex, TextureRect *rect, float *uvs)
{
    float pixwidth = 1.0f / tex->width;
    float pixheight = 1.0f / tex->height;
    float pixoffset_x = pixwidth * 0.001f;
    float pixoffset_y = pixheight * 0.001f;

    float xmin = rect->x * pixwidth + pixoffset_x;
    float ymin = rect->y * pixheight + pixoffset_y;
    float xmax = (rect->x + rect->w) * pixwidth - pixoffset_x;
    float ymax = (rect->y + rect->h) * pixheight - pixoffset_y;

    uvs[0] = xmax; uvs[1] = ymax;
    uvs[2] = xmin; uvs[3] = ymax;
    uvs[4] = xmin; uvs[5] = ymin;
    uvs[6] = xmax; uvs[7] = ymin;
}

void
texsetactivetexture(uint texunit)
{
    if (texunit < GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) {
        glActiveTexture(GL_TEXTURE0 + texunit);
    } else {
        logprint(LOG_ERROR, "tex: Attempted to enable invalid texture unit");
    }
}

void
texsetsampleruniform(uint program, const char *name, uint texunit)
{
    int uniform = glGetUniformLocation(program, name);
    glUniform1i(uniform, texunit);
}

void
texquit(void)
{
    htabledealloc(textures);
    htabledelete(textures);
    textures = NULL;
}

void
fontinit(void)
{
    fonts = htablecreate(htablestringhash, htablestringequal);
}

void
fontbind(FontTex *font)
{
    glBindTexture(GL_TEXTURE_2D, font->id);
}

void
fontload(FontTex *font, const char *path, int width, int height)
{
    SDL_Surface *surface = IMG_Load(path);
    int mode = GL_RGB;

    if (!surface) {
        logprint(LOG_ERROR, "texture: Unable to load texture file $s: " \
                            "SDL_image error: $s", path, IMG_GetError());
        return;
    }
    glGenTextures(1, &font->id);
    glBindTexture(GL_TEXTURE_2D, font->id);

    if (surface->format->BytesPerPixel == 4) {
        mode = GL_RGBA;
    }
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 mode,
                 surface->w,
                 surface->h,
                 0,
                 mode,
                 GL_UNSIGNED_BYTE,
                 surface->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MIN_FILTER,
                    GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MAG_FILTER,
                    GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    font->width = surface->w;
    font->height = surface->h;
    font->fontwidth = width;
    font->fontheight = height;

    SDL_FreeSurface(surface);
}

void
fontfind(FontTex *font, const char *path, int width, int height)
{
    int len;
    char *name = strrchr(path, '/');
    char *fontname;
    FontTex *fontentry;

    if (!name) {
        name = (char *)path;
    }
    name++;
    fontentry = htableget(fonts, name);
    
    if (fontentry) {
        *font = *fontentry;
        return;
    }
    len = strlen(name);
    fontentry = malloc(sizeof(FontTex));
    fontname = malloc(sizeof(char) * len);
    memcpy(fontname, name, len);
    fontload(fontentry, path, width, height);
    htableadd(fonts, fontname, fontentry);
    *font = *fontentry;
}

void
fontcalctexcoords(FontTex *font, uchar charcount)
{
    int i;

    for (i = 0; i < charcount; i++) {
        fontget(font, i, font->uvs[i]);
    }
}

void
fontget(FontTex *font, const char chr, float *uvs)
{
    int texperrow = font->width / font->fontwidth;
    int charpos_x = chr % texperrow;
    int charpos_y = chr / texperrow;
    int texwidth = charpos_x * font->fontwidth;
    int texheight = charpos_y * font->fontheight;
    float pixwidth = 1.0f / font->width;
    float pixheight = 1.0f / font->height;

    float xmin = texwidth * pixwidth;
    float ymin = texheight * pixheight;
    float xmax = (texwidth + font->fontwidth) * pixwidth;
    float ymax = (texheight + font->fontheight) * pixheight;

    uvs[0] = xmax; uvs[1] = ymax;
    uvs[2] = xmin; uvs[3] = ymax;
    uvs[4] = xmin; uvs[5] = ymin;
    uvs[6] = xmax; uvs[7] = ymin;
}

void
fontdelete(FontTex *font)
{
    if (font->id) {
        glDeleteTextures(1, &font->id);
    }
}

void
fontquit(void)
{
    htabledealloc(fonts);
    htabledelete(fonts);
    fonts = NULL;
}
