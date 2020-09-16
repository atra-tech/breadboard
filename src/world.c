/*
 *  BREADBOARD (2020)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Structures for managing the grid-based world, and the visual
 *  representation of entities the user will built circuits with.
 *
 *  */

#include "world.h"
#include "util/log.h"

#define COPY_VEC(dst, src, row, size) \
    memcpy(dst, src + row * size, size * sizeof(float))
#define SET_INDEX(dst, icount, i1, i2, i3) \
    dst[0] = icount + i1; \
    dst[1] = icount + i2; \
    dst[2] = icount + i3


static void worldstart(void);
static void segmentinit(Segment *seg, uint x, uint y);
static void segmentmesh(Segment *seg);

static const float segmentverts[] = {
    SEGMENT_SIZE, 0.0f,         0.0f,
    0.0f,         0.0f,         0.0f,
    0.0f,         SEGMENT_SIZE, 0.0f,
    SEGMENT_SIZE, SEGMENT_SIZE, 0.0f
};

static const float segmentuvs[] = {
    SEGMENT_SIZE, SEGMENT_SIZE,
    0.0f,         SEGMENT_SIZE,
    0.0f,         0.0f,
    SEGMENT_SIZE, 0.0f
};

static const float quadnorms[] = {
    0.0f, 0.0f, -1.0f,
    0.0f, 0.0f, -1.0f,
    0.0f, 0.0f, -1.0f,
    0.0f, 0.0f, -1.0f
};

static const float tileverts[] = {
    1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    1.0f, 1.0f, 0.0f
};

static int init = 0;
static Shader worldshader;
static Shader bgshader;
static Texture circuittex;
static Texture bgtex;

void
worldstart(void)
{
    shaderinit(&worldshader,
            "res/shader/worldvert.glsl",
            "res/shader/worldfrag.glsl");
    shaderinit(&bgshader,
            "res/shader/bgvert.glsl",
            "res/shader/bgfrag.glsl");
    texfind(&bgtex, "res/texture/background_small.png");
    texfind(&circuittex, "res/texture/circuits.png");
    //selectioninit();
}

void
worldinit(World *world)
{
    Segment *seg;
    uint i, j;

    if (!init) {
        worldstart();
        init = 1;
    }

    for (i = 0; i < SEGMENTS_PER_ROW; i++) {
        for (j = 0; j < SEGMENTS_PER_ROW; j++) {
            seg = worldgetsegment(world, i, j);
            segmentinit(seg, i, j);
        }
    }
}

void
segmentinit(Segment *seg, uint x, uint y)
{
    uint i;

    seg->x = x;
    seg->y = y;
    seg->dirty = 1;
    vbufinit(&seg->vbo);
    glm_mat4_identity(seg->modelmat);
    glm_translate_x(seg->modelmat, x * SEGMENT_SIZE);
    glm_translate_y(seg->modelmat, y * SEGMENT_SIZE);

    for (i = 0; i < LAYER_TOTAL; i++) {
        memset(seg->layers[i].tiles, 0, LAYER_TILECOUNT * sizeof(Tile));
    }
}

void
worldhandle(World *world, Event *e)
{
    /* Selection, context menus, drag-drop of elements, wire drawing, etc.
     * All input driven activities are handled here and all effects queued
     * for the update phase.
     * */
}

void
worldupdate(World *world, float dt)
{

}

void
worldrender(World *world, Camera *cam)
{
    /*  
     *  Loop through segments in view of camera
     *      If segment is dirty, remesh
     *      Render segment
     *  */

    const uint seg_x = cam->ent.x / SEGMENT_SIZE;
    const uint seg_y = cam->ent.y / SEGMENT_SIZE;
    uint i, j;
    Segment *seg;

    //cameracalcvectors(cam);
    //cameracalcviewmatrix(cam);
    //cameracalcprojviewmatrix(cam);

    texsetactivetexture(0);
    texbind(&bgtex); //TODO: Bind circuit.png texture

    shaderbind(&worldshader);
    shaderloadprojviewmat(&worldshader, cam->projviewmat);
    shaderloadviewmat(&worldshader, cam->viewmat);
    shaderaddint(&worldshader, "albedo_map", 0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    /* TODO: Change segment render distance to be based on camera zoom */
    for (i = seg_x - VIEW_DIST; i <= seg_x + VIEW_DIST; i++) {
        for (j = seg_y - VIEW_DIST; j <= seg_y + VIEW_DIST; j++) {
            seg = worldgetsegment(world, i, j);

            if (seg->dirty) {
                segmentmesh(seg);
                seg->dirty = 0;
            }
            
            //vbufbindvao(&seg->vbo);
            glBindVertexArray(seg->vbo.vao);
            shaderloadmodelmat(&worldshader, seg->modelmat);
            glDrawElements(GL_TRIANGLES,
                    seg->vbo.indxcount, GL_UNSIGNED_INT, NULL);
        }
    }
    shaderunbind();
}

void
worldsave(const char *path)
{
    /* TODO: Serialize world data and write to file */
}

void
worldload(const char *path)
{
    /* TODO: Read file into world structure */
}

/* Populate segment VBO and prepare for rendering 
 * TODO: Convert to thread worker job */
void
segmentmesh(Segment *seg)
{
    uint i, idxcount;
    vec3 vert, norm;
    vec2 uv;
    ivec3 index;

    idxcount = 0;
    vbufreset(&seg->vbo);

    
    for (i = 0; i < 4; i++) {
        COPY_VEC(vert, segmentverts, i, 3);
        COPY_VEC(uv, segmentuvs, i, 2);
        COPY_VEC(norm, quadnorms, i, 3);
        vbufaddvertex(&seg->vbo, vert, uv, norm);
    }
    SET_INDEX(index, idxcount, 0, 1, 2);
    vbufaddindex(&seg->vbo, index);
    SET_INDEX(index, idxcount, 2, 3, 0);
    vbufaddindex(&seg->vbo, index);

    idxcount += 4;
    seg->vbo.indxcount = 12;

    /* TODO: Mesh layers (IC packages, pins, and wires) */
    for (i = 0; i < LAYER_TOTAL; i++) {
        /*  
         *  
         *  */
    }
    vbufbind(&seg->vbo);
}

/* Returns the segment at given segment index coordinates */
Segment *
worldgetsegment(World *world, uint x, uint y)
{
    return world->segs + x + y * SEGMENTS_PER_ROW;
}

/* Returns the segment at given world-space coordinates */
Segment *
worldgetsegmentat(World *world, uint x, uint y)
{
    x /= SEGMENT_SIZE;
    y /= SEGMENT_SIZE;

    return world->segs + x + y * SEGMENTS_PER_ROW; 
}

/* Returns the layer at given world-space coordinates */
Layer *
worldgetlayerat(World *world, uint x, uint y, uint layer)
{
    x /= SEGMENT_SIZE;
    y /= SEGMENT_SIZE;

    return world->segs[x + y * SEGMENTS_PER_ROW].layers + layer;
}

/* Returns the tile from the layer at given world-space coordinates */
Tile *
worldgettileat(World *world, uint x, uint y, uint layer)
{
    Layer *l = worldgetlayerat(world, x, y, layer);
    
    x %= SEGMENT_SIZE;
    y %= SEGMENT_SIZE;

    return l->tiles + x + y * SEGMENT_SIZE;
}

/* Returns the tile from the layer at given segment-space coordinates */
Tile *
segmentgettile(Segment *seg, uint x, uint y, uint layer)
{
    return seg->layers[layer].tiles + x + y * SEGMENT_SIZE;
}
