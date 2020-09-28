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
#include "input.h"
#include "core.h"
#include "util/log.h"

#define COPY_VEC(dst, src, row, size) \
    memcpy(dst, src + row * size, size * sizeof(float))
#define SET_INDEX(dst, icount, i1, i2, i3) \
    dst[0] = icount + i1; \
    dst[1] = icount + i2; \
    dst[2] = icount + i3

static void worldstart(void);
static void segmentinit(Segment *seg, uint x, uint y);
static void segmentdelete(Segment *seg);
static void segmentmesh(Segment *seg);
static void freeseginit(FreeSegment *seg, float x, float y, uint w, uint h);
static void freesegdelete(FreeSegment *seg);
static void freesegmesh(FreeSegment *seg);
static void tilehandlemouse(Tile *tile, float mx, float my);
static void layerrender(Layer *layer, uint type);

/*  
 *  VERTEX DATA
 *  */

static const float segmentverts[] = {
    SEGMENT_SIZE, 0.0f,         0.0f,
    0.0f,         0.0f,         0.0f,
    0.0f,         SEGMENT_SIZE, 0.0f,
    SEGMENT_SIZE, SEGMENT_SIZE, 0.0f
};

static const float segmentuvs[] = {
    SEGMENT_SIZE * 0.5, SEGMENT_SIZE * 0.5,
    0.0f,               SEGMENT_SIZE * 0.5,
    0.0f,               0.0f,
    SEGMENT_SIZE * 0.5, 0.0f
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


/* 
 *  WIRE COLOR DATA
 *  */

static const float wirecolordarkred[]    = {0.5f, 0.0f, 0.0f};
static const float wirecolordarkgreen[]  = {0.0f, 0.5f, 0.0f};
static const float wirecolordarkblue[]   = {0.0f, 0.0f, 0.5f};
static const float wirecolorlightred[]   = {1.0f, 0.0f, 0.0f};
static const float wirecolorlightgreen[] = {0.0f, 1.0f, 0.0f};
static const float wirecolorlightblue[]  = {0.0f, 0.0f, 1.0f};

static int init = 0;
static Shader worldshader;
static Shader bgshader;
static Texture circuittex;
static Texture bgtex;
static uint currentwire;
static uint mousemode;

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

    currentwire = WIRE_NONE;
    mousemode = MOUSE_NONE;
    //selectioninit();
}

void
worldinit(World *world)
{
    uint i, j;
    Segment *seg;

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
worlddelete(World *world)
{
    uint i, j;
    Segment *seg;
    
    for (i = 0; i < SEGMENTS_PER_ROW; i++) {
        for (j = 0; j < SEGMENTS_PER_ROW; j++) {
            seg = worldgetsegment(world, i, j);
            segmentdelete(seg);
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
segmentdelete(Segment *seg)
{
    vbufdelete(&seg->vbo);
    listdelete(seg->packs);
}

void
freeseginit(FreeSegment *seg, float x, float y, uint w, uint h)
{
    uint i;

    seg->x = x;
    seg->y = y;
    seg->w = w;
    seg->h = h;

    vbufinit(&seg->vbo);

    glm_mat4_identity(seg->modelmat);
    glm_translate_x(seg->modelmat, x);
    glm_translate_y(seg->modelmat, y);

    for (i = 0; i < LAYER_TOTAL; i++) {
        seg->tiles[i] = calloc(w * h, sizeof(Tile));
    }
    seg->packs = listcreate();
}

void
freesegdelete(FreeSegment *seg)
{
    uint i;

    vbufdelete(&seg->vbo);

    for (i = 0; i < LAYER_TOTAL; i++) {
        free(seg->tiles[i]);
    }
    listdelete(seg->packs);
}

void
worldhandle(World *world, Event *e)
{
    static int prev_mx = 0, prev_my = 0;
    int mx, my, diff_mx, diff_my;
    float mdist, tmp_mx, tmp_my;
    uint mstate;
    Camera *cam;
    Tile *tile;

    mstate = mousegetstate(&mx, &my);
    cam = getmaincamera();

    /*  TODO:   Any interaction with anything other than the currently active
     *          FreeSegment selection will rebind the contained elements to
     *          the world and delete it. */
    
    if (e->type == SDL_MOUSEBUTTONDOWN) {
        if (e->button.type == SDL_BUTTON_LEFT) {
            //  Possible paths:
            //      1.  Player does not drag mouse, leading to single IC or select-box selection check
            //      2.  Player drags mouse activating select-box mode.
            //      3.  Player clicks on open area with wires enabled, drawing wires on tiles.
            prev_mx = mx;
            prev_my = my;


            //Check for selection box under cursor
            //Check for IC pack under cursor
            //If either are true, skip the following mousemode changes

            if (currentwire != WIRE_NONE) {
                mousemode = MOUSE_DRAW;
            } else {
                mousemode = MOUSE_DRAGBOX;
            }
        } else if (e->button.type == SDL_BUTTON_RIGHT) {
            /*  Context menus and wire erasure */

            if (currentwire != WIRE_NONE) {
                mousemode = MOUSE_ERASE;
            }
        }
    } else if (e->type == SDL_MOUSEBUTTONUP) {
        if (e->button.type == SDL_BUTTON_LEFT) {
            if (mousemode == MOUSE_DRAGBOX) {
                //Construct FreeSegment with all captured wires and ICs
                //Remove wires and ICs from world segment
                mousemode = MOUSE_NONE;
            } else if (mousemode == MOUSE_DRAW) {
                mousemode = MOUSE_NONE;
            }
        }
    }

    /*if (mousemode == MOUSE_DRAG) {
        diff_mx = mx - prev_mx;
        diff_my = my - prev_my;
        mdist = diff_mx * diff_mx + diff_my * diff_my;

        if (mdist > 1.0) {
            mousemode = MOUSE_DRAGBOX;
        }
    }*/

    /*  Check if wire buttons are enabled
     *  If any are enabled, place wire at current mouse position */
    if (mousemode == MOUSE_DRAW) {
        my = cam->height - my;
        tmp_mx = cam->ent.x + (mx / cam->zoom);
        tmp_my = cam->ent.y + (my / cam->zoom);
        
        tile = worldgettileat(world, tmp_mx, tmp_my, currentwire);
        tilehandlemouse(tile, tmp_mx, tmp_my);
    }
}

void
tilehandlemouse(Tile *tile, float mx, float my)
{
    mx -= (int)mx;
    my -= (int)my;

    if (mousemode == MOUSE_DRAW) {
        if (mx < TILE_WIREGAP) {
            if (mx > my) {
                BIT_SET(tile->state, TILE_BOT);
            } else if (mx > 1.0f - my){
                BIT_SET(tile->state, TILE_TOP);
            } else {
                BIT_SET(tile->state, TILE_LEFT);
            }
        } else if (mx > 1.0f - TILE_WIREGAP) {
            if (mx <= my) {
                BIT_SET(tile->state, TILE_BOT);
            } else if (mx <= 1.0f - my){
                BIT_SET(tile->state, TILE_TOP);
            } else {
                BIT_SET(tile->state, TILE_RIGHT);
            }
        } else {
            if (my < TILE_WIREGAP) {
                BIT_SET(tile->state, TILE_BOT);
            } else if (my > 1.0f - TILE_WIREGAP) {
                BIT_SET(tile->state, TILE_TOP);
            }
        }
        BIT_SET(tile->state, TILE_CENTRE);
    }
    else {
        /* TODO: Figure out best control-feel for wire deletion */
        tile->state = 0;

        /*if (mx < TILE_WIREGAP) {
            if (mx > my) {
                BIT_CLR(tile->state, TILE_BOT);
            } else if (mx > 1.0f - my){
                BIT_CLR(tile->state, TILE_TOP);
            } else {
                BIT_CLR(tile->state, TILE_LEFT);
            }
        } else if (mx > 1.0f - TILE_WIREGAP) {
            if (mx <= my) {
                BIT_CLR(tile->state, TILE_BOT);
            } else if (mx <= 1.0f - my){
                BIT_CLR(tile->state, TILE_TOP);
            } else {
                BIT_CLR(tile->state, TILE_RIGHT);
            }
        } else {
            if (my < TILE_WIREGAP) {
                BIT_CLR(tile->state, TILE_BOT);
            } else if (my > 1.0f - TILE_WIREGAP) {
                BIT_CLR(tile->state, TILE_TOP);
            } else {
                BIT_CLR(tile->state, TILE_CENTRE);
            }
        }*/
    }
}

/* Manage updates between the world-space circuit and
 * the logical representation of the circuit */
void
worldupdate(World *world, float dt)
{
    
}

void
worldrender(World *world, Camera *cam)
{
    const int seg_x = cam->ent.x / SEGMENT_SIZE;
    const int seg_y = cam->ent.y / SEGMENT_SIZE;
    int i, j, k;
    Segment *seg;

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
            if (i < 0 || i > SEGMENTS_PER_ROW - 1 ||
                    j < 0 || j > SEGMENTS_PER_ROW - 1)  {
                continue;
            }
            seg = worldgetsegment(world, i, j);

            if (seg->dirty) {
                segmentmesh(seg);
                seg->dirty = 0;
            }
            vbufbindvao(&seg->vbo);
            shaderloadmodelmat(&worldshader, seg->modelmat);
            glDrawElements(GL_TRIANGLES,
                    seg->vbo.indxcount, GL_UNSIGNED_INT, NULL);

            /*texsetactivetexture(0);
            texbind(&circuittex);
            for (k = 0; k < LAYER_TOTAL; k++) {
                layerrender(&seg->layers[k], k);
            }*/
        }
    }

    shaderunbind();
}

void
layerrender(Layer *layer, uint type)
{
    vec3 wirecolor;

    switch (type) {
    case LAYER_RED:
        memcpy(wirecolor, wirecolordarkred, 3 * sizeof(float));
    case LAYER_GREEN:
        memcpy(wirecolor, wirecolordarkgreen, 3 * sizeof(float));
    case LAYER_BLUE:
        memcpy(wirecolor, wirecolordarkblue, 3 * sizeof(float));
    }

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

void
freesegmesh(FreeSegment *seg)
{

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
