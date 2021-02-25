/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  A collection of rendering tools, buffers, shaders, etc.
 *
 *  */

#ifndef ATRA_RENDER_H
#define ATRA_RENDER_H

#include <cglm/cglm.h>

#include "entity.h"
#include "opengl.h"
#include "util/list.h"
#include "util/util.h"

typedef SDL_Color Color;

typedef struct {
    uint vao;
    uint vbocount;
    uint indxcount;
    IVecList buffers;
    VecList verts;
    VecList uvs;
    VecList norms;
    IVecList indxs;
} VBuffer;

typedef struct {
    uint prog;
    uint projviewmat;
    uint modelmat;
    uint viewmat;
} Shader;

typedef struct {
    uint id;
    uint width;
    uint height;
    uint imagesize;
    uint texsize;
} Texture;

typedef struct {
    uint fbo;
    uint map;
    uint width, height;
} FBuffer;

typedef struct {
    uint id;
    uint width;
    uint height;
    uint fontwidth;
    uint fontheight;
    float uvs[256][8];
} FontTex;

typedef struct {
    int x, y;
    int w, h;
} TextureRect;

void vbufinit(VBuffer *);
void vbufaddvertex(VBuffer *, vec3 vertex, vec2 uv, vec3 normal);
void vbufaddindex(VBuffer *, ivec3 index);
void vbufbindvbo(VBuffer *, VecList *data);
void vbufbindebo(VBuffer *);
void vbufbindvao(VBuffer *);
void vbufbind(VBuffer *);
void vbufclear(VBuffer *);
void vbufreset(VBuffer *);
void vbufresetnoclear(VBuffer *);
void vbufdelete(VBuffer *);

void fbufinit(FBuffer *, uint width, uint height, uint comp, uint attach);
void fbufbind(FBuffer *);
//void fbufreset(FBuffer *);
void fbufdelete(FBuffer *);

void shaderinit(Shader *shader, const char *vertsrc, const char *fragsrc);
uint shaderload(const char *vertpath, const char *fragpath);
void shaderbind(Shader *shader);
void shaderunbind(void);
void shaderloadint(uint loc, const int data);
void shaderloadfloat(uint loc, const float data);
void shaderloadvec2(uint loc, const vec2 vec);
void shaderloadvec3(uint loc, const vec3 vec);
void shaderloadvec4(uint loc, const vec4 vec);
void shaderloadmat4(uint loc, mat4 mat);
void shaderloadprojviewmat(Shader *shader, mat4 matrix);
void shaderloadmodelmat(Shader *shader, mat4 matrix);
void shaderloadviewmat(Shader *shader, mat4 matrix);
void shaderaddint(Shader *shader, const char *name, int data);
void shaderaddfloat(Shader *shader, const char *name, float data);
void shaderaddvec2(Shader *shader, const char *name, const vec2 vec);
void shaderaddvec3(Shader *shader, const char *name, const vec3 vec);
void shaderaddvec4(Shader *shader, const char *name, const vec4 vec);
void shaderaddmat4(Shader *shader, const char *name, mat4 mat);
void shaderdelete(Shader *shader);

void texinit(void);
void texload(Texture *, const char* path);
void texbind(Texture *);
void texfind(Texture *, const char *path);
void texget(Texture *, int x, int y, float *uvs);
void texgetrect(Texture *, TextureRect *rect, float *uvs);
void texsetactivetexture(uint texunit);
void texsetsampleruniform(uint program, const char *name, uint texunit);
void texdelete(Texture *);
void texquit(void);
//int texloadfont(Texture *tex, const char *font, const char *text, int size);
//void texfindfont(Texture *tex, const char *font, const char *text, int size);
//int texgetfont(Texture *tex, const char *font, const char *text,
//               int size, Color color);
//void texdeletefonts(void);
void fontinit(void);
void fontload(FontTex *, const char *path, int width, int height);
void fontbind(FontTex *);
void fontfind(FontTex *, const char *path, int width, int height);
void fontget(FontTex *, char, float *uvs);
void fontcalctexcoords(FontTex *font, uchar charcount);
void fontdelete(FontTex *);
void fontquit(void);

#endif
