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
#include "gui.h"
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
static void layermesh(Segment *seg, Layer *layer, const float *color, uint depth);
static void layerrender(Layer *layer, uint type);
static void wirecalcuvs(void);
static void wiremovetotop(uint wire);
static void guitoolbarcreate(void);
static void fullremesh(void);

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
    SEGMENT_SIZE / 2, SEGMENT_SIZE / 2,
    0.0f,             SEGMENT_SIZE / 2,
    0.0f,             0.0f,
    SEGMENT_SIZE / 2, 0.0f
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

static const uint wiretexcoords[WIRE_PERM_COUNT][2] = {
    {3, 4},     /* Centre           */
    {0, 7},     /* Top              */
    {2, 7},     /* Bot              */
    {0, 4},     /* Top + Bot        */
    {3, 7},     /* Left             */
    {0, 6},     /* Left + Top       */
    {3, 6},     /* Left + Bot       */
    {3, 5},     /* Left T-Junction  */
    {1, 7},     /* Right            */
    {1, 6},     /* Right + Top      */
    {2, 6},     /* Right + Bot      */
    {2, 5},     /* Right T-Junction */
    {1, 4},     /* Right + Left     */
    {0, 5},     /* Top T-Junction   */
    {1, 5},     /* Bot T-Junction   */
    {2, 4}      /* 4-way Junction   */
};

static float wireuvs[WIRE_PERM_COUNT][8];

static const float colorred[]   = {1.0f, 0.2f, 0.2f};
static const float colorgreen[] = {0.4f, 1.0f, 0.2f};
static const float colorblue[]  = {0.2f, 0.4f, 1.0f};
static const float *colors[] = {colorred, colorgreen, colorblue};

static int init = 0;
static World *mainworld;
static Shader worldshader;
static Shader bgshader;
static Texture circuittex;
static Texture bgtex;
static uint currentwire;
static uint mousemode;
static uint wireorder[3];

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

    circuittex.imagesize = 1024;
    circuittex.texsize = 64;
    wirecalcuvs();

    currentwire = WIRE_NONE;
    mousemode = MOUSE_NONE;
    wireorder[0] = WIRE_RED;
    wireorder[1] = WIRE_GREEN;
    wireorder[2] = WIRE_BLUE;
    
    guitoolbarcreate();
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
            seg = malloc(sizeof(Segment));
            segmentinit(seg, i, j);
            world->segs[i * SEGMENTS_PER_ROW + j] = seg;
        }
    }
    mainworld = world;
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
            free(seg);
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
    seg->packs = listcreate();

    vbufinit(&seg->vbo);
    vbufinit(&seg->wirevbo);
    veclistinit(&seg->colors, 3);
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
    vbufdelete(&seg->wirevbo);
    veclistdelete(&seg->colors);
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
    seg->packs = listcreate();

    vbufinit(&seg->vbo);
    veclistinit(&seg->colors, 3);

    glm_mat4_identity(seg->modelmat);
    glm_translate_x(seg->modelmat, x);
    glm_translate_y(seg->modelmat, y);

    for (i = 0; i < LAYER_TOTAL; i++) {
        seg->tiles[i] = calloc(w * h, sizeof(Tile));
    }
}

void
freesegdelete(FreeSegment *seg)
{
    uint i;

    vbufdelete(&seg->vbo);
    veclistdelete(&seg->colors);
    listdelete(seg->packs);

    for (i = 0; i < LAYER_TOTAL; i++) {
        free(seg->tiles[i]);
    }
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
    Segment *seg;

    mstate = mousegetstate(&mx, &my);
    cam = getmaincamera();

    /*  TODO:   Any interaction with anything other than the currently active
     *          FreeSegment selection will rebind the contained elements to
     *          the world and delete it. */
    
    if (e->type == SDL_MOUSEBUTTONDOWN) {
        if (e->button.button == SDL_BUTTON_LEFT) {
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
        } else if (e->button.button == SDL_BUTTON_RIGHT) {
            /*  Context menus and wire erasure */

            if (currentwire != WIRE_NONE) {
                mousemode = MOUSE_ERASE;
            }
        }
    } else if (e->type == SDL_MOUSEBUTTONUP) {
        if (e->button.button == SDL_BUTTON_LEFT) {
            if (mousemode == MOUSE_DRAGBOX) {
                //Construct FreeSegment with all captured wires and ICs
                //Remove wires and ICs from world segment
                mousemode = MOUSE_NONE;
            } else if (mousemode == MOUSE_DRAW) {
                mousemode = MOUSE_NONE;
            }
        } else if (e->button.button == SDL_BUTTON_RIGHT) {
            if (mousemode == MOUSE_ERASE) {
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
    if (mousemode == MOUSE_DRAW || mousemode == MOUSE_ERASE) {
        my = cam->height - my;
        tmp_mx = cam->ent.x + (mx - (cam->width / 2.0)) / (cam->zoom / 2.0);
        tmp_my = cam->ent.y + (my - (cam->height / 2.0)) / (cam->zoom / 2.0);

        seg = worldgetsegmentat(world, tmp_mx, tmp_my);
        tile = worldgettileat(world, tmp_mx, tmp_my, currentwire);
        tilehandlemouse(tile, tmp_mx, tmp_my);
        seg->dirty = 1;
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
                BIT_SET(tile->state, TILE_TOP);
            } else if (mx <= 1.0f - my){
                BIT_SET(tile->state, TILE_BOT);
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
    int seg_x = cam->ent.x / SEGMENT_SIZE;
    int seg_y = cam->ent.y / SEGMENT_SIZE;
    int i, j, k;
    Segment *seg;

    texsetactivetexture(0);
    texbind(&bgtex);
    shaderbind(&bgshader);
    shaderloadprojviewmat(&bgshader, cam->projviewmat);
    shaderloadviewmat(&bgshader, cam->viewmat);
    shaderaddint(&bgshader, "albedo_map", 0);

    glEnable(GL_DEPTH_TEST);
    //glEnable(GL_CULL_FACE);
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
            shaderloadmodelmat(&bgshader, seg->modelmat);
            glDrawElements(GL_TRIANGLES,
                    seg->vbo.indxcount, GL_UNSIGNED_INT, NULL);
        }
    }

    texbind(&circuittex);
    shaderbind(&worldshader);
    shaderloadprojviewmat(&worldshader, cam->projviewmat);
    shaderloadviewmat(&worldshader, cam->viewmat);
    shaderaddint(&worldshader, "albedo_map", 0);

    for (i = seg_x - VIEW_DIST; i <= seg_x + VIEW_DIST; i++) {
        for (j = seg_y - VIEW_DIST; j <= seg_y + VIEW_DIST; j++) {
            if (i < 0 || i > SEGMENTS_PER_ROW - 1 ||
                    j < 0 || j > SEGMENTS_PER_ROW - 1)  {
                continue;
            }
            seg = worldgetsegment(world, i, j);

            vbufbindvao(&seg->wirevbo);
            shaderloadmodelmat(&worldshader, seg->modelmat);
            glDrawElements(GL_TRIANGLES,
                    seg->wirevbo.indxcount, GL_UNSIGNED_INT, NULL);
        }
    }

    shaderunbind();
}

void
layerrender(Layer *layer, uint type)
{

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
    vbufreset(&seg->wirevbo);
    veclistclear(&seg->colors);
    
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
    seg->vbo.indxcount += 12;

    layermesh(seg, &seg->layers[wireorder[2]], colors[wireorder[2]], 1);
    layermesh(seg, &seg->layers[wireorder[1]], colors[wireorder[1]], 2);
    layermesh(seg, &seg->layers[wireorder[0]], colors[wireorder[0]], 3);

    /* TODO: Render ICs */
    
    vbufbind(&seg->vbo);
    vbufbind(&seg->wirevbo);
    vbufbindvbo(&seg->wirevbo, &seg->colors);
}

void
layermesh(Segment *seg, Layer *layer, const float *color, uint depth)
{
    uint i, j, k, tile, tilenib, idxcount;
    vec3 vert, norm = {0.0f, 0.0f, 1.0f};
    vec2 uv;
    ivec3 index;

    idxcount = seg->wirevbo.indxcount / 3;
    vert[2] = depth;

    for (i = 0; i < SEGMENT_SIZE; i++) {
        for (j = 0; j < SEGMENT_SIZE; j++) {
            tile = layer->tiles[j + i * SEGMENT_SIZE].state & 0x1F;

            if (!tile) continue;

            if (tile == TILE_CENTRE) {
                for (k = 0; k < 4; k++) {
                    vert[0] = i + tileverts[k * 3 + 0];
                    vert[1] = j + tileverts[k * 3 + 1];
                    uv[0] = wireuvs[0][k * 2 + 0];
                    uv[1] = wireuvs[0][k * 2 + 1];
                    vbufaddvertex(&seg->wirevbo, vert, uv, norm);
                    veclistpush(&seg->colors, color);
                }
                SET_INDEX(index, idxcount, 0, 1, 2);
                vbufaddindex(&seg->wirevbo, index);
                SET_INDEX(index, idxcount, 2, 3, 0);
                vbufaddindex(&seg->wirevbo, index);

                idxcount += 4;
                seg->wirevbo.indxcount += 12;
            } else {
                tilenib = tile & 0xF;

                for (k = 0; k < 4; k++) {
                    vert[0] = i + tileverts[k * 3 + 0];
                    vert[1] = j + tileverts[k * 3 + 1];
                    uv[0] = wireuvs[tilenib][k * 2 + 0];
                    uv[1] = wireuvs[tilenib][k * 2 + 1];
                    vbufaddvertex(&seg->wirevbo, vert, uv, norm);
                    veclistpush(&seg->colors, color);
                }
                SET_INDEX(index, idxcount, 0, 1, 2);
                vbufaddindex(&seg->wirevbo, index);
                SET_INDEX(index, idxcount, 2, 3, 0);
                vbufaddindex(&seg->wirevbo, index);
                idxcount += 4;
                seg->wirevbo.indxcount += 12;
            }
        }
    }
}

void
freesegmesh(FreeSegment *seg)
{

}

void
fullremesh(void)
{
    uint i;

    for (i = 0; i < SEGMENT_COUNT; i++) {
        mainworld->segs[i]->dirty = 1;
    }
}

void
wirecalcuvs(void)
{
    uint i;

    for (i = 0; i < WIRE_PERM_COUNT; i++) {
        texget(&circuittex,
                wiretexcoords[i][0],
                wiretexcoords[i][1],
                wireuvs[i]);
    }
}

/* Moves the selected wire type to the top of the rendering queue */
void
wiremovetotop(uint wire)
{
    if (wireorder[1] == wire) {
        wireorder[1] = wireorder[0];
        wireorder[0] = wire;
    } else if (wireorder[2] == wire) {
        wireorder[2] = wireorder[1];
        wireorder[1] = wireorder[0];
        wireorder[0] = wire;
    }
}

/* Returns the segment at given segment index coordinates */
Segment *
worldgetsegment(World *world, uint x, uint y)
{
    return world->segs[y + x * SEGMENTS_PER_ROW];
}

/* Returns the segment at given world-space coordinates */
Segment *
worldgetsegmentat(World *world, uint x, uint y)
{
    x /= SEGMENT_SIZE;
    y /= SEGMENT_SIZE;

    return world->segs[y + x * SEGMENTS_PER_ROW]; 
}

/* Returns the layer at given world-space coordinates */
Layer *
worldgetlayerat(World *world, uint x, uint y, uint layer)
{
    x /= SEGMENT_SIZE;
    y /= SEGMENT_SIZE;

    return &world->segs[y + x * SEGMENTS_PER_ROW]->layers[layer];
}

/* Returns the tile from the layer at given world-space coordinates */
Tile *
worldgettileat(World *world, uint x, uint y, uint layer)
{
    Layer *l = worldgetlayerat(world, x, y, layer);
    
    x %= SEGMENT_SIZE;
    y %= SEGMENT_SIZE;

    return &l->tiles[y + x * SEGMENT_SIZE];
}

/* Returns the tile from the layer at given segment-space coordinates */
Tile *
segmentgettile(Segment *seg, uint x, uint y, uint layer)
{
    return &seg->layers[layer].tiles[y + x * SEGMENT_SIZE];
}


/*
 *  GUI DEFINITIONS
 *
 *  Here be dragons
 */

static void guibuttonmouseover(GUIElement *elem, ...);
static void guibuttonmouseleave(GUIElement *elem, ...);
static void guibuttonmouseup(GUIElement *elem, ...);

static void guibuttonnotdown(GUIElement *elem, ...);
static void guibuttonordown(GUIElement *elem, ...);
static void guibuttonanddown(GUIElement *elem, ...);
static void guibuttonxordown(GUIElement *elem, ...);

static void guibuttonreddown(GUIElement *elem, ...);
static void guibuttongreendown(GUIElement *elem, ...);
static void guibuttonbluedown(GUIElement *elem, ...);

static void guibuttonredup(GUIElement *elem, ...);
static void guibuttongreenup(GUIElement *elem, ...);
static void guibuttonblueup(GUIElement *elem, ...);

static void guibuttonredmouseleave(GUIElement *elem, ...);
static void guibuttongreenmouseleave(GUIElement *elem, ...);
static void guibuttonbluemouseleave(GUIElement *elem, ...);

enum gui_toolbar_buttons {
    BUTTON_NOT,
    BUTTON_OR,
    BUTTON_AND,
    BUTTON_XOR,
    BUTTON_RED,
    BUTTON_GREEN,
    BUTTON_BLUE,
    BUTTON_TOTAL
};

void
guibuttonmouseover(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 320, 0, 64, 64);
    elem->layer->dirty = 1;
}

void
guibuttonmouseleave(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 256, 0, 64, 64);
    elem->layer->dirty = 1;
}

void
guibuttonmouseup(GUIElement *elem, ...)
{
    if (elem->state & GUI_STATE_MOUSEOVER) {
        guisetbacktexture(elem, 320, 0, 64, 64);
    } else {
        guisetbacktexture(elem, 256, 0, 64, 64);
    }
    elem->layer->dirty = 1;
}

void
guibuttonnotdown(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 384, 0, 64, 64);
    elem->layer->dirty = 1;
}

void
guibuttonordown(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 384, 0, 64, 64);
    elem->layer->dirty = 1;
}

void
guibuttonanddown(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 384, 0, 64, 64);
    elem->layer->dirty = 1;
}

void
guibuttonxordown(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 384, 0, 64, 64);
    elem->layer->dirty = 1;
}

void
guibuttonreddown(GUIElement *elem, ...)
{
    GUIElement *green, *blue;

    green = guigetelem("toolbar_green");
    blue = guigetelem("toolbar_blue");
    
    if (currentwire == WIRE_RED) {
        currentwire = WIRE_NONE;
        guisetbacktexture(elem, 384, 0, 64, 64);
    } else {
        currentwire = WIRE_RED;
        wiremovetotop(WIRE_RED);
        fullremesh();
        guisetbacktexture(elem, 384, 0, 64, 64);
        guisetbacktexture(green, 256, 0, 64, 64);
        guisetbacktexture(blue, 256, 0, 64, 64);
    }
    elem->layer->dirty = 1;
}

void
guibuttongreendown(GUIElement *elem, ...)
{
    GUIElement *red, *blue;

    red = guigetelem("toolbar_red");
    blue = guigetelem("toolbar_blue");

    if (currentwire == WIRE_GREEN) {
        currentwire = WIRE_NONE;
        guisetbacktexture(elem, 384, 0, 64, 64);
    } else {
        currentwire = WIRE_GREEN;
        wiremovetotop(WIRE_GREEN);
        fullremesh();
        guisetbacktexture(elem, 384, 0, 64, 64);
        guisetbacktexture(red, 256, 0, 64, 64);
        guisetbacktexture(blue, 256, 0, 64, 64);
    }
    elem->layer->dirty = 1;
}

void
guibuttonbluedown(GUIElement *elem, ...)
{
    GUIElement *red, *green;

    red = guigetelem("toolbar_red");
    green = guigetelem("toolbar_green");

    if (currentwire == WIRE_BLUE) {
        currentwire = WIRE_NONE;
        guisetbacktexture(elem, 384, 0, 64, 64);
    } else {
        currentwire = WIRE_BLUE;
        wiremovetotop(WIRE_BLUE);
        fullremesh();
        guisetbacktexture(elem, 384, 0, 64, 64);
        guisetbacktexture(red, 256, 0, 64, 64);
        guisetbacktexture(green, 256, 0, 64, 64);
    }
    elem->layer->dirty = 1;
}

void 
guibuttonredup(GUIElement *elem, ...)
{
    if (currentwire == WIRE_RED) {
        guisetbacktexture(elem, 320, 0, 64, 64);
    } else {
        if (elem->state & GUI_STATE_MOUSEOVER) {
            guisetbacktexture(elem, 320, 0, 64, 64);
        } else {
            guisetbacktexture(elem, 256, 0, 64, 64);
        }
    }
    elem->layer->dirty = 1;
}

void
guibuttongreenup(GUIElement *elem, ...)
{
    if (currentwire == WIRE_GREEN) {
        guisetbacktexture(elem, 320, 0, 64, 64);
    } else {
        if (elem->state & GUI_STATE_MOUSEOVER) {
            guisetbacktexture(elem, 320, 0, 64, 64);
        } else {
            guisetbacktexture(elem, 256, 0, 64, 64);
        }
    }
    elem->layer->dirty = 1;
}

void
guibuttonblueup(GUIElement *elem, ...)
{
    if (currentwire == WIRE_BLUE) {
        guisetbacktexture(elem, 320, 0, 64, 64);
    } else {
        if (elem->state & GUI_STATE_MOUSEOVER) {
            guisetbacktexture(elem, 320, 0, 64, 64);
        } else {
            guisetbacktexture(elem, 256, 0, 64, 64);
        }
    }
    elem->layer->dirty = 1;
}

void
guibuttonredmouseleave(GUIElement *elem, ...)
{
    if (currentwire == WIRE_RED) {
        guisetbacktexture(elem, 320, 0, 64, 64);
    } else {
        guisetbacktexture(elem, 256, 0, 64, 64);
    }
    elem->layer->dirty = 1;
}

void
guibuttongreenmouseleave(GUIElement *elem, ...)
{
    if (currentwire == WIRE_GREEN) {
        guisetbacktexture(elem, 320, 0, 64, 64);
    } else {
        guisetbacktexture(elem, 256, 0, 64, 64);
    }
    elem->layer->dirty = 1;
}

void
guibuttonbluemouseleave(GUIElement *elem, ...)
{
    if (currentwire == WIRE_BLUE) {
        guisetbacktexture(elem, 320, 0, 64, 64);
    } else {
        guisetbacktexture(elem, 256, 0, 64, 64);
    }
    elem->layer->dirty = 1;
}

void
guitoolbarcreate(void) 
{
    uint i;
    GUILayer *layer;
    GUIElement *base;
    GUIElement *buttons[BUTTON_TOTAL];
    GUIElement *icons[BUTTON_TOTAL];

    layer = guilayercreate("res/shader/guivert.glsl",
            "res/shader/guifrag.glsl",
            "res/texture/gui.png");

    base = guielemcreate(0.325f, 0.0f, 0.35f, 0.1f,
            GUI_CUSTOM, GUI_ATTR_HASTEXTURE, "toolbar_base");

    buttons[BUTTON_NOT] = guielemcreate(0.0f, 0.0f, 0.14285f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "toolbar_not");
    
    buttons[BUTTON_OR] = guielemcreate(0.14285f, 0.0f, 0.14285f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "toolbar_or");

    buttons[BUTTON_AND] = guielemcreate(0.28571f, 0.0f, 0.14285f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "toolbar_and");

    buttons[BUTTON_XOR] = guielemcreate(0.42857f, 0.0f, 0.14285f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "toolbar_xor");

    buttons[BUTTON_RED] = guielemcreate(0.57143f, 0.0f, 0.14285f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "toolbar_red");

    buttons[BUTTON_GREEN] = guielemcreate(0.71428f, 0.0f, 0.14285f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "toolbar_green");
    
    buttons[BUTTON_BLUE] = guielemcreate(0.85714f, 0.0f, 0.14285f, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "toolbar_blue");
    
    icons[BUTTON_NOT] = guielemcreate(0.1f, 0.1f, 0.8f, 0.8f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_NOCAPTURE,
            "toolbar_not_icon");
    
    icons[BUTTON_OR] = guielemcreate(0.1f, 0.1f, 0.8f, 0.8f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_NOCAPTURE,
            "toolbar_or_icon");

    icons[BUTTON_AND] = guielemcreate(0.1f, 0.1f, 0.8f, 0.8f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_NOCAPTURE,
            "toolbar_and_icon");

    icons[BUTTON_XOR] = guielemcreate(0.1f, 0.1f, 0.8f, 0.8f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_NOCAPTURE,
            "toolbar_xor_icon");

    icons[BUTTON_RED] = guielemcreate(0.1f, 0.1f, 0.8f, 0.8f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_NOCAPTURE,
            "toolbar_red_icon");

    icons[BUTTON_GREEN] = guielemcreate(0.1f, 0.1f, 0.8f, 0.8f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_NOCAPTURE,
            "toolbar_blue_icon");
    
    icons[BUTTON_BLUE] = guielemcreate(0.1f, 0.1f, 0.8f, 0.8f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_NOCAPTURE,
            "toolbar_green_icon");

    guilinklayer(base, layer);
    guiaddelem(base, buttons[BUTTON_NOT]);
    guiaddelem(base, buttons[BUTTON_OR]);
    guiaddelem(base, buttons[BUTTON_AND]);
    guiaddelem(base, buttons[BUTTON_XOR]);
    guiaddelem(base, buttons[BUTTON_RED]);
    guiaddelem(base, buttons[BUTTON_GREEN]);
    guiaddelem(base, buttons[BUTTON_BLUE]);

    guisetbacktexture(base, 256, 0, 1, 1);

    for (i = 0; i < BUTTON_TOTAL; i++) {
        guiaddelem(buttons[i], icons[i]);
        guisetbacktexture(buttons[i], 256, 0, 64, 64);
        guiseteventcallback(buttons[i], GUI_EVENT_MOUSEOVER, guibuttonmouseover);
    }
    guisetbacktexture(icons[BUTTON_NOT], 256, 64, 128, 128);
    guisetbacktexture(icons[BUTTON_OR], 384, 64, 128, 128);
    guisetbacktexture(icons[BUTTON_AND], 512, 64, 128, 128);
    guisetbacktexture(icons[BUTTON_XOR], 640, 64, 128, 128);
    guisetbacktexture(icons[BUTTON_RED], 256, 192, 128, 128);
    guisetbacktexture(icons[BUTTON_GREEN], 384, 192, 128, 128);
    guisetbacktexture(icons[BUTTON_BLUE], 512, 192, 128, 128);

    guiseteventcallback(buttons[BUTTON_NOT], GUI_EVENT_MOUSEDOWN, guibuttonnotdown);
    guiseteventcallback(buttons[BUTTON_OR], GUI_EVENT_MOUSEDOWN, guibuttonordown);
    guiseteventcallback(buttons[BUTTON_AND], GUI_EVENT_MOUSEDOWN, guibuttonanddown);
    guiseteventcallback(buttons[BUTTON_XOR], GUI_EVENT_MOUSEDOWN, guibuttonxordown);
    guiseteventcallback(buttons[BUTTON_NOT], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_OR], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_AND], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_XOR], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_NOT], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_OR], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_AND], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_XOR], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);

    guiseteventcallback(buttons[BUTTON_RED], GUI_EVENT_MOUSEDOWN, guibuttonreddown);
    guiseteventcallback(buttons[BUTTON_RED], GUI_EVENT_MOUSEUP, guibuttonredup);
    guiseteventcallback(buttons[BUTTON_RED], GUI_EVENT_MOUSELEAVE, guibuttonredmouseleave);

    guiseteventcallback(buttons[BUTTON_GREEN], GUI_EVENT_MOUSEDOWN, guibuttongreendown);
    guiseteventcallback(buttons[BUTTON_GREEN], GUI_EVENT_MOUSEUP, guibuttongreenup);
    guiseteventcallback(buttons[BUTTON_GREEN], GUI_EVENT_MOUSELEAVE, guibuttongreenmouseleave);

    guiseteventcallback(buttons[BUTTON_BLUE], GUI_EVENT_MOUSEDOWN, guibuttonbluedown);
    guiseteventcallback(buttons[BUTTON_BLUE], GUI_EVENT_MOUSEUP, guibuttonblueup);
    guiseteventcallback(buttons[BUTTON_BLUE], GUI_EVENT_MOUSELEAVE, guibuttonbluemouseleave);
}
