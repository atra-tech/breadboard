/*
 *  BREADBOARD (2020)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Structures for managing the grid-based world, and the visual
 *  representation of entities the user will build circuits with.
 *
 *  */

#include "world.h"
#include "input.h"
#include "core.h"
#include "ui.h"
#include "file.h"
#include "util/log.h"
#include "util/stack.h"

#define COPY_VEC(dst, src, row, size) \
    memcpy(dst, src + row * size, size * sizeof(float))
#define SET_INDEX(dst, icount, i1, i2, i3) \
    dst[0] = icount + i1; \
    dst[1] = icount + i2; \
    dst[2] = icount + i3

static void segmentinit(Segment *seg, uint x, uint y);
static void segmentdelete(Segment *seg);
static void segmentmesh(Segment *seg);

static void worldhandlekeys(Event *e);
static void tilehandlemouse(Tile *tile, float mx, float my);
static void layermesh(Segment *seg, Layer *layer, const float *color, uint depth);
static void wirecalcuvs(void);
static void wiremovetotop(uint wire);

static void guitoolbarcreate(void);
static void guifilebarcreate(void);
static void guipackbarcreate(void);

static void fullremesh(void);

static void selboxinit(SelectBox *box);
static void selboxmesh(SelectBox *box);
static void selboxdelete(SelectBox *box);

static void calcpackuvs(Package *pack, uint x, uint y, float **uvs);
static int pointintersectsquad(float px, float py,
        float x, float y, float w, float h);
static int quadintersectsquad(float ax, float ay, float aw, float ah,
        float bx, float by, float bw, float bh);
static void drawtext(VBuffer *vbo, VecList *colors, const char *text,
        uint fontsize, const float *color, float x_min, float x_max,
        float y_min, float y_max);
static void vbopushquad(VBuffer *vbo, float x_min, float y_min,
        float x_max, float y_max, float depth, float uvs[8]);
static void vbopushquadinverted(VBuffer *vbo, float x_min, float y_min,
        float x_max, float y_max, float depth, float uvs[8]);

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

static const float colorwhite[] = {1.0f, 1.0f, 1.0f};
static const float colorred[]   = {1.0f, 0.2f, 0.2f};
static const float colorgreen[] = {0.4f, 1.0f, 0.2f};
static const float colorblue[]  = {0.2f, 0.4f, 1.0f};
static const float *colors[] = {colorred, colorgreen, colorblue};
static const float packtextcolor[] = {0.93f, 0.90f, 0.90f};
static const float iotextcolor[] = {0.11f, 0.10f, 0.10f};
static const float selectboxcolor[] = {1.0f, 0.7f, 0.0f, 1.0f};
static const float tileerrorcolor[] = {1.0f, 0.1f, 0.0f, 0.4f};

static int init = 0;
static Stack *worldstack;
static World *mainworld;
static FreeSegment *mouseseg;
static FreeSegment *clipboardseg;
static Shader worldshader;
static Shader bgshader;
static Shader indicatorshader;
static Texture circuittex;
static Texture bgtex;
static FontTex fonttex;
static uint currentwire;
static uint mousemode;
static uint wireorder[3];
static float packuvs[PACKTEX_TOTAL * 8];
static float inpackuvs[2 * 8];
static float outpackuvs[2 * 8];
static float pinuvs[PINTEX_TOTAL * 8];
static float selboxedgeuvs[8];
static float selboxcorneruvs[8];
static SelectBox selbox;

void
worldstart(void)
{
    uint i, j;

    shaderinit(&worldshader,
            "res/shader/worldvert.glsl",
            "res/shader/worldfrag.glsl");
    shaderinit(&bgshader,
            "res/shader/bgvert.glsl",
            "res/shader/bgfrag.glsl");
    shaderinit(&indicatorshader,
            "res/shader/indicatorvert.glsl",
            "res/shader/indicatorfrag.glsl");
    texfind(&bgtex, "res/texture/background_small.png");
    texfind(&circuittex, "res/texture/circuits.png");
    fontfind(&fonttex, "res/font/maledict_12x12.png", 12, 12);

    circuittex.imagesize = 1024;
    circuittex.texsize = 64;
    wirecalcuvs();
    fontcalctexcoords(&fonttex, 255);

    currentwire = WIRE_NONE;
    mousemode = MOUSE_NONE;
    wireorder[0] = WIRE_RED;
    wireorder[1] = WIRE_GREEN;
    wireorder[2] = WIRE_BLUE;
    mouseseg = NULL;

    /* Populate circuit package texture UVs */
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            texget(&circuittex, j, i, packuvs + (j + i * 3) * 8);
        }
    }
    texget(&circuittex, 3, 0, packuvs + PACKTEX_1W_TOP * 8);
    texget(&circuittex, 3, 1, packuvs + PACKTEX_1W_CENTRE * 8);
    texget(&circuittex, 3, 2, packuvs + PACKTEX_1W_BOT * 8);
    texget(&circuittex, 4, 0, packuvs + PACKTEX_1H_LEFT * 8);
    texget(&circuittex, 5, 0, packuvs + PACKTEX_1H_CENTRE * 8);
    texget(&circuittex, 6, 0, packuvs + PACKTEX_1H_RIGHT * 8);
    texget(&circuittex, 4, 1, packuvs + PACKTEX_1W_1H * 8);

    /* Special Input/Output package texture UVs */
    texget(&circuittex, 4, 3, inpackuvs + 0);
    texget(&circuittex, 5, 3, inpackuvs + 8);
    texget(&circuittex, 6, 3, outpackuvs + 0);
    texget(&circuittex, 7, 3, outpackuvs + 8);

    /* Populate pin package texture UVs */
    for (i = 0; i < PINTEX_TOTAL; i++) {
        texget(&circuittex, i, 3, pinuvs + i * 8);
    }
    worldstack = stackcreate();
    selboxinit(&selbox);
    guitoolbarcreate();
    guifilebarcreate();
    guipackbarcreate();
    init = 1;
}

void
worldshutdown(void)
{
    int i;

    if (mouseseg) {
        freesegdelete(mouseseg);
    }

    while (worldstack->size) {
        worlddelete(stacktop(worldstack));
        stackpop(worldstack);
    }

    stackdelete(worldstack);
    shaderdelete(&worldshader);
    shaderdelete(&bgshader);
    shaderdelete(&indicatorshader);
    selboxdelete(&selbox);
    init = 0;
}

World *
worldcreate(void)
{
    uint i, j;
    World *world;
    Segment *seg;

    world = malloc(sizeof(World));

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
    world->circuit = circuitbuild(world, 0);
    world->name = NULL;

    return world;
}

/* Creates a world with the minimum initialisation required for
 * quick conversion of circuit data */
World *
worldcreatefake(void)
{
    uint i, j;
    World *world;
    Segment *seg;

    world = malloc(sizeof(World));

    for (i = 0; i < SEGMENTS_PER_ROW; i++) {
        for (j = 0; j < SEGMENTS_PER_ROW; j++) {
            seg = calloc(1, sizeof(Segment));
            seg->x = i;
            seg->y = j;
            seg->packs = listcreate();

            world->segs[i * SEGMENTS_PER_ROW + j] = seg;
        }
    }
    world->circuit = circuitbuild(world, 0);
    world->name = NULL;

    return world;

}

void
worldpush(World *world)
{
    stackpush(worldstack, world);
    mainworld = world;
}

World *
worldtop(void)
{
    return mainworld;
}

void
worldpop(void)
{
    worlddelete(stacktop(worldstack));
    stackpop(worldstack);

    if (worldstack->size == 0) {
        worldshutdown();
    } else {
        mainworld = stacktop(worldstack);
    }
}

/* Delete the current world and replace it with the given world */
void
worldswitch(World *world)
{
    worlddelete(stacktop(worldstack));
    stackpop(worldstack);
    worldpush(world);
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

    if (world->circuit) {
        circuitdelete(world->circuit);
    }

    if (world->name) {
        free(world->name);
    }
    free(world);
}

void
worlddeletefake(World *world)
{
    uint i, j;
    Segment *seg;
    
    for (i = 0; i < SEGMENTS_PER_ROW; i++) {
        for (j = 0; j < SEGMENTS_PER_ROW; j++) {
            seg = worldgetsegment(world, i, j);
            listdelete(seg->packs);
            free(seg);
        }
    }

    if (world->circuit) {
        circuitdelete(world->circuit);
    }

    if (world->name) {
        free(world->name);
    }
    free(world);
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
    vbufinit(&seg->textvbo);
    veclistinit(&seg->colors, 3);
    veclistinit(&seg->textcolors, 3);

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
    vbufdelete(&seg->textvbo);
    veclistdelete(&seg->colors);
    veclistdelete(&seg->textcolors);
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

    vbufinit(&seg->wirevbo);
    vbufinit(&seg->textvbo);
    veclistinit(&seg->colors, 3);
    veclistinit(&seg->textcolors, 3);

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

    vbufdelete(&seg->wirevbo);
    vbufdelete(&seg->textvbo);
    veclistdelete(&seg->colors);
    veclistdelete(&seg->textcolors);
    listdelete(seg->packs);

    for (i = 0; i < LAYER_TOTAL; i++) {
        free(seg->tiles[i]);
    }
    free(seg);
}

void
selboxinit(SelectBox *box)
{
    box->enabled = 0;
    glm_mat4_identity(box->modelmat);
    vbufinit(&box->vbo);

    texget(&circuittex, 4, 3, selboxedgeuvs);
    texget(&circuittex, 5, 3, selboxcorneruvs);
}

SelectBox *
selboxget(void)
{
    return &selbox;
}

void
selboxdelete(SelectBox *box)
{
    vbufdelete(&box->vbo);
}

/* TODO: Rework to use world stack via simstate.
 * Either that or remove all world params and make */
void
worldhandle(Event *e)
{
    static int prev_mx = 0, prev_my = 0;
    int mx, my;
    float wmx, wmy, px, py;
    uint i, mstate;
    Camera *cam;
    Segment *seg;
    Package *pack;

    mstate = mousegetstate(&mx, &my);
    cam = getmaincamera();

    wmx = cam->ent.x + (mx - (cam->width * 0.5f)) / (cam->zoom * 0.5f);
    wmy = cam->ent.y +
        ((cam->height - my) - (cam->height * 0.5f)) / (cam->zoom * 0.5f);

    worldhandlekeys(e);

    if (e->type == SDL_MOUSEBUTTONDOWN) {
        if (e->button.button == SDL_BUTTON_LEFT) {
            
            /* Handle input pack toggling */
            if (!selbox.enabled) {
                seg = worldgetsegmentat(mainworld, wmx, wmy);
                for (i = 0; i < seg->packs->size; i++) {
                    pack = listat(seg->packs, i);

                    if (pack->type == PACK_INPUT) {
                        px = seg->x * SEGMENT_SIZE + pack->x;
                        py = seg->y * SEGMENT_SIZE + pack->y;

                        if (pointintersectsquad(wmx, wmy, 
                                    px, py, pack->w, pack->h)) {
                            pack->state = !pack->state;
                            pack->changed = 1;
                            seg->dirty = 1;
                            prev_mx = mx;
                            prev_my = my;
                            return;
                        }
                    }
                }
            }

            if (currentwire != WIRE_NONE) {
                mousemode = MOUSE_DRAW;
            } else if (mousemode == MOUSE_GRAB) {
                if (freesegmerge(mouseseg, wmx - (mouseseg->w * 0.5f) + 0.5f,
                            wmy - (mouseseg->h * 0.5f) + 0.5f)) {
                    freesegdelete(mouseseg);
                    mouseseg = NULL;
                    mousemode = MOUSE_NONE;
                    circuitrebuild(mainworld, &mainworld->circuit);
                } else {
                    /* TODO: Indicate placement error in some way
                     *       (Do it inside freesegmerge, maybe?)
                     *          Floating tooltip following mouse?
                     *          Highlight offending element in red for a time? */
                }
            } else if (mousemode == MOUSE_BOXREADY) {
                if (pointintersectsquad(wmx, wmy,
                            selbox.x, selbox.y, selbox.w, selbox.h)) {
                    
                    mouseseg = freesegbuild(
                            selbox.x, selbox.y, selbox.w, selbox.h, 1);
                    if (mouseseg) {
                        freesegmesh(mouseseg);
                        mousemode = MOUSE_GRAB;
                        selbox.enabled = 0;
                    }
                    circuitrebuild(mainworld, &mainworld->circuit);
                } else {
                    mousemode = MOUSE_NONE;
                    selbox.enabled = 0;
                }
            } else if (mousemode == MOUSE_NONE) {
                mousemode = MOUSE_DRAGBOX;
                selbox.x = wmx + 0.5f;
                selbox.y = wmy + 0.5f;
                selbox.w = 0;
                selbox.h = 0;
                selbox.enabled = 1;
                selboxmesh(&selbox);
            }
        } else if (e->button.button == SDL_BUTTON_RIGHT) {
            
            /* Handle I/O pack renaming */
            if (!selbox.enabled) {
                seg = worldgetsegmentat(mainworld, wmx, wmy);
                for (i = 0; i < seg->packs->size; i++) {
                    pack = listat(seg->packs, i);

                    if (pack->type == PACK_INPUT ||
                            pack->type == PACK_OUTPUT) {
                        px = seg->x * SEGMENT_SIZE + pack->x;
                        py = seg->y * SEGMENT_SIZE + pack->y;

                        if (pointintersectsquad(wmx, wmy, 
                                    px, py, pack->w, pack->h)) {
                            ui_settargetpack(pack);
                            ui_iorename_show();
                            prev_mx = mx;
                            prev_my = my;
                            return;
                        }
                    }
                }
            }

            /* Wire erasure */
            if (currentwire != WIRE_NONE) {
                mousemode = MOUSE_ERASE;
            }
        }
    } else if (e->type == SDL_MOUSEBUTTONUP) {
        if (e->button.button == SDL_BUTTON_LEFT) {
            if (mousemode == MOUSE_DRAGBOX) {
                mousemode = MOUSE_BOXREADY;
                if (selbox.w < 0) {
                    selbox.x += selbox.w;
                    selbox.w = abs(selbox.w);
                } else if (selbox.w == 0) {
                    selbox.enabled = 0;
                    mousemode = MOUSE_NONE;
                }

                if (selbox.h < 0) {
                    selbox.y += selbox.h;
                    selbox.h = abs(selbox.h);
                } else if (selbox.h == 0) {
                    selbox.enabled = 0;
                    mousemode = MOUSE_NONE;
                }
            } else if (mousemode == MOUSE_DRAW) {
                mousemode = MOUSE_NONE;
                circuitrebuild(mainworld, &mainworld->circuit);
            }
        } else if (e->button.button == SDL_BUTTON_RIGHT) {
            if (mousemode == MOUSE_ERASE) {
                mousemode = MOUSE_NONE;
                circuitrebuild(mainworld, &mainworld->circuit);
            }
        }
    } else if (e->type == SDL_MOUSEMOTION) {
        if (mousemode == MOUSE_DRAGBOX) {
            selbox.w = wmx - selbox.x;
            selbox.h = wmy - selbox.y;
            
            if (selbox.x + selbox.w < 0) {
                selbox.w = 0;
            } else if (selbox.x + selbox.w >= WORLD_TILES_PER_ROW) {
                selbox.w = WORLD_TILES_PER_ROW - 1;
            }

            if (selbox.y + selbox.h < 0) {
                selbox.h = 0;
            } else if (selbox.y + selbox.h >= WORLD_TILES_PER_ROW) {
                selbox.h = WORLD_TILES_PER_ROW - 1;
            }
            selboxmesh(&selbox);
        }
    }


    if (!(mstate & MOUSE_BUTTON(LEFT)) && mousemode == MOUSE_DRAW) {
        mousemode = MOUSE_NONE;
        circuitrebuild(mainworld, &mainworld->circuit);
    } else if (!(mstate & MOUSE_BUTTON(RIGHT)) && mousemode == MOUSE_ERASE) {
        mousemode = MOUSE_NONE;
        circuitrebuild(mainworld, &mainworld->circuit);
    } /*else if (!(mstate & MOUSE_BUTTON(LEFT)) && mousemode == MOUSE_DRAGBOX) {
        mousemode = MOUSE_NONE;
        selbox.enabled = 0;
    }*/

    prev_mx = mx;
    prev_my = my;
}

void
worldhandlekeys(Event *e)
{
    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == KEYCODE(DELETE) ||
                e->key.keysym.sym == KEYCODE(ESCAPE)) {
            if (mouseseg) {                     /* Delete held circuitry */
                freesegdelete(mouseseg);
                mouseseg = NULL;
                mousemode = MOUSE_NONE;
            }

        } else if (e->key.keysym.sym == KEYCODE(c) &&
                keystate(KEYCODE(LCTRL)) &&
                selbox.enabled) {               /* Copy to clipboard */
            if (clipboardseg) {
                freesegdelete(clipboardseg);
            }
            clipboardseg = freesegbuild(
                    selbox.x, selbox.y, selbox.w, selbox.h, 0);
            selbox.enabled = 0;
        } else if (e->key.keysym.sym == KEYCODE(v) &&
                keystate(KEYCODE(LCTRL)) &&
                clipboardseg && !mouseseg) {    /* Paste from clipboard */
            mouseseg = freesegcopy(clipboardseg);
            freesegmesh(mouseseg);
            mousemode = MOUSE_GRAB;
        }
    }
}

FreeSegment *
freesegcopy(FreeSegment *src)
{
    uint i, j, l, tile;
    FreeSegment *dest;
    Package *packsrc;

    dest = calloc(1, sizeof(FreeSegment));
    freeseginit(dest, 0, 0, src->w, src->h);

    for (l = 0; l < LAYER_TOTAL; l++) {
        for (i = 0; i < dest->w; i++) {
            for (j = 0; j < dest->h; j++) {
                tile = src->tiles[l][j + i * dest->h].state;
                dest->tiles[l][j + i * dest->h].state = tile; 
            }
        }
    }

    for (i = 0; i < src->packs->size; i++) {
        packsrc = listat(src->packs, i);
        listpushback(dest->packs, packcopy(packsrc));
    }

    return dest;
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
worldupdate(float dt)
{
    int mx, my;
    float tmp_mx, tmp_my;
    Camera *cam = getmaincamera();
    Segment *seg;
    Tile *tile;

    mousegetstate(&mx, &my);

    if (mousemode == MOUSE_DRAW || mousemode == MOUSE_ERASE) {
        my = cam->height - my;
        tmp_mx = cam->ent.x + (mx - (cam->width * 0.5)) / (cam->zoom * 0.5);
        tmp_my = cam->ent.y + (my - (cam->height * 0.5)) / (cam->zoom * 0.5);

        seg = worldgetsegmentat(mainworld, tmp_mx, tmp_my);
        tile = worldgettileat(mainworld, tmp_mx, tmp_my, currentwire);
        tilehandlemouse(tile, tmp_mx, tmp_my);
        seg->dirty = 1;
    } else if (mousemode == MOUSE_GRAB && mouseseg) {
        my = cam->height - my;
        tmp_mx = cam->ent.x + (mx - (cam->width * 0.5)) / (cam->zoom * 0.5);
        tmp_my = cam->ent.y + (my - (cam->height * 0.5)) / (cam->zoom * 0.5);

        glm_mat4_identity(mouseseg->modelmat);
        glm_translate_x(mouseseg->modelmat, tmp_mx - (mouseseg->w * 0.5));
        glm_translate_y(mouseseg->modelmat, tmp_my - (mouseseg->h * 0.5));
    }
    circuitresetwireconvs(mainworld->circuit);
    updateclocktimes(mainworld->circuit, dt);
    circuitupdate(mainworld->circuit, dt);
}

void
worldrender(Camera *cam)
{
    int seg_x = cam->ent.x / SEGMENT_SIZE;
    int seg_y = cam->ent.y / SEGMENT_SIZE;
    int i, j;
    Segment *seg;

    /* Render background */
    texsetactivetexture(0);
    texbind(&bgtex);
    shaderbind(&bgshader);
    shaderloadprojviewmat(&bgshader, cam->projviewmat);
    shaderloadviewmat(&bgshader, cam->viewmat);
    shaderaddint(&bgshader, "albedo_map", 0);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    /* TODO: Change segment render distance to be based on camera zoom 
     * TODO: Rework to use common background VBuffer, since this data is identical for each seg
     * */
    for (i = seg_x - VIEW_DIST; i <= seg_x + VIEW_DIST; i++) {
        for (j = seg_y - VIEW_DIST; j <= seg_y + VIEW_DIST; j++) {
            if (i < 0 || i > SEGMENTS_PER_ROW - 1 ||
                    j < 0 || j > SEGMENTS_PER_ROW - 1) {
                continue;
            }
            seg = worldgetsegment(mainworld, i, j);

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

    /* Render packages and wires */
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
            seg = worldgetsegment(mainworld, i, j);

            vbufbindvao(&seg->wirevbo);
            shaderloadmodelmat(&worldshader, seg->modelmat);
            glDrawElements(GL_TRIANGLES,
                    seg->wirevbo.indxcount, GL_UNSIGNED_INT, NULL);
        }
    }

    /* Render text */
    fontbind(&fonttex);

    for (i = seg_x - VIEW_DIST; i <= seg_x + VIEW_DIST; i++) {
        for (j = seg_y - VIEW_DIST; j <= seg_y + VIEW_DIST; j++) {
            if (i < 0 || i > SEGMENTS_PER_ROW - 1 ||
                    j < 0 || j > SEGMENTS_PER_ROW - 1)  {
                continue;
            }
            seg = worldgetsegment(mainworld, i, j);

            vbufbindvao(&seg->textvbo);
            shaderloadmodelmat(&worldshader, seg->modelmat);
            glDrawElements(GL_TRIANGLES,
                    seg->textvbo.indxcount, GL_UNSIGNED_INT, NULL);
        }
    }

    /* Render selection box */
    if (selbox.enabled) {
        vbufbindvao(&selbox.vbo);
        shaderbind(&indicatorshader);
        shaderloadprojviewmat(&indicatorshader, cam->projviewmat);
        shaderloadviewmat(&indicatorshader, cam->viewmat);
        shaderloadmodelmat(&indicatorshader, selbox.modelmat);
        shaderaddvec4(&indicatorshader, "color", selectboxcolor);
        glLineWidth(SELBOX_THICKNESS);
        glDrawElements(GL_LINES,
                selbox.vbo.indxcount, GL_UNSIGNED_INT, NULL);
    }
    shaderunbind();
}

void
worldrendermouseseg(Camera *cam)
{
    if (mouseseg) {
        /* Render packages and wires */
        glDisable(GL_DEPTH_TEST);

        texbind(&circuittex);
        shaderbind(&worldshader);
        shaderloadmodelmat(&worldshader, mouseseg->modelmat);

        vbufbindvao(&mouseseg->wirevbo);
        glDrawElements(GL_TRIANGLES,
                mouseseg->wirevbo.indxcount, GL_UNSIGNED_INT, NULL);
    
        /* Render text */
        fontbind(&fonttex);

        vbufbindvao(&mouseseg->textvbo);
        glDrawElements(GL_TRIANGLES,
                mouseseg->textvbo.indxcount, GL_UNSIGNED_INT, NULL);
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

/* Construct a world from circuit data.  Used for data serialisation.
 * NOTE: World must already be initialised */
void
worldbuild(World *world, Circuit *circuit)
{
    uint i, j;
    Wire *wire;
    TileData *data;
    Tile *tile;
    Segment *seg;
    Package *pack;

    /* Write wires */
    for (i = 0; i < circuit->wires->size; i++) {
        wire = listat(circuit->wires, i);

        for (j = 0; j < wire->tile_data.size; j++) {
            data = dlistat(&wire->tile_data, j);
            tile = worldgettileat(world, data->x, data->y, wire->layer);
            tile->state = data->state;
        }
    }

    /* Write packages */
    for (i = 0; i < circuit->packs->size; i++) {
        pack = listat(circuit->packs, i);
        seg = worldgetsegment(world, pack->seg_x, pack->seg_y);
        listpushback(seg->packs, packcopy(pack));
    }
}

/* Populate segment VBO and prepare for rendering 
 * TODO: Convert to thread worker job */
void
segmentmesh(Segment *seg)
{
    uint l, x, y, i, j, idxcount, len;
    float x_min, x_max, y_min, y_max;
    vec3 vert, norm, textcolor;
    vec2 uv;
    ivec3 index;
    Package *pack;
    Pin *pin;
    float *uvptr;
    const float *color;
    VBuffer *vbo;

    vbufreset(&seg->vbo);
    vbufreset(&seg->wirevbo);
    vbufreset(&seg->textvbo);
    veclistclear(&seg->colors);
    veclistclear(&seg->textcolors);
    
    /* Draw background */
    idxcount = 0;
    vbo = &seg->vbo;
    vert[2] = 0.0f;

    for (i = 0; i < 4; i++) {
        COPY_VEC(vert, segmentverts, i, 3);
        COPY_VEC(uv, segmentuvs, i, 2);
        COPY_VEC(norm, quadnorms, i, 3);
        vbufaddvertex(vbo, vert, uv, norm);
    }
    SET_INDEX(index, idxcount, 0, 1, 2);
    vbufaddindex(vbo, index);
    SET_INDEX(index, idxcount, 2, 3, 0);
    vbufaddindex(vbo, index);

    idxcount += 4;
    vbo->indxcount += 12;
    
    /* Draw wires */
    layermesh(seg, &seg->layers[wireorder[2]], colors[wireorder[2]], 1);
    layermesh(seg, &seg->layers[wireorder[1]], colors[wireorder[1]], 2);
    layermesh(seg, &seg->layers[wireorder[0]], colors[wireorder[0]], 3);
    
    
    /* Draw packages */
    vert[2] = 4.0f;

    for (l = 0; l < seg->packs->size; l++) {
        vbo = &seg->wirevbo;
        idxcount = vbo->indxcount / 3;
        pack = listat(seg->packs, l);

        textcolor[0] = packtextcolor[0];
        textcolor[1] = packtextcolor[1];
        textcolor[2] = packtextcolor[2];

        if (pack->error) {
            color = tileerrorcolor;
            textcolor[0] *= tileerrorcolor[0];
            textcolor[1] *= tileerrorcolor[1];
            textcolor[2] *= tileerrorcolor[2];
        } else {
            color = colorwhite;
        }

        /* Draw body */
        for (y = 0; y < pack->h; y++) {
            for (x = 0; x < pack->w; x++) {
                calcpackuvs(pack, x, y, &uvptr);

                for (i = 0; i < 4; i++) {
                    vert[0] = (int)pack->x + x + tileverts[i * 3 + 0];
                    vert[1] = (int)pack->y + y + tileverts[i * 3 + 1];
                    uv[0] = uvptr[i * 2 + 0];
                    uv[1] = uvptr[i * 2 + 1];
                    vbufaddvertex(vbo, vert, uv, norm);
                    veclistpush(&seg->colors, color);
                }
                SET_INDEX(index, idxcount, 0, 1, 2);
                vbufaddindex(vbo, index);
                SET_INDEX(index, idxcount, 2, 3, 0);
                vbufaddindex(vbo, index);

                idxcount += 4;
                vbo->indxcount += 12;
            } 
        }

        /* Draw pins */
        for (i = 0; i < pack->in_pins.size; i++) {
            pin = dlistat(&pack->in_pins, i);

            if (pin->x < 0) {
                uvptr = pinuvs + PINTEX_LEFT * 8;
            } else {
                if (pin->y < 0) {
                    uvptr = pinuvs + PINTEX_DOWN * 8;
                } else if ((uint)pin->y >= pack->h) {
                    uvptr = pinuvs + PINTEX_UP * 8;
                } else {
                    uvptr = pinuvs + PINTEX_RIGHT * 8;
                }
            }

            for (j = 0; j < 4; j++) {
                vert[0] = (int)pack->x + pin->x + tileverts[j * 3 + 0];
                vert[1] = (int)pack->y + pin->y + tileverts[j * 3 + 1];
                uv[0] = uvptr[j * 2 + 0];
                uv[1] = uvptr[j * 2 + 1];
                vbufaddvertex(vbo, vert, uv, norm);
                veclistpush(&seg->colors, color);
            }
            SET_INDEX(index, idxcount, 0, 1, 2);
            vbufaddindex(vbo, index);
            SET_INDEX(index, idxcount, 2, 3, 0);
            vbufaddindex(vbo, index);
            
            idxcount += 4;
            vbo->indxcount += 12;
        }
        for (i = 0; i < pack->out_pins.size; i++) {
            pin = dlistat(&pack->out_pins, i);

            if (pin->x < 0) {
                uvptr = pinuvs + PINTEX_LEFT * 8;
            } else {
                if (pin->y < 0) {
                    uvptr = pinuvs + PINTEX_DOWN * 8;
                } else if ((uint)pin->y >= pack->h) {
                    uvptr = pinuvs + PINTEX_UP * 8;
                } else {
                    uvptr = pinuvs + PINTEX_RIGHT * 8;
                }
            }

            for (j = 0; j < 4; j++) {
                vert[0] = pack->x + pin->x + tileverts[j * 3 + 0];
                vert[1] = pack->y + pin->y + tileverts[j * 3 + 1];
                uv[0] = uvptr[j * 2 + 0];
                uv[1] = uvptr[j * 2 + 1];
                vbufaddvertex(vbo, vert, uv, norm);
                veclistpush(&seg->colors, color);
            }
            SET_INDEX(index, idxcount, 0, 1, 2);
            vbufaddindex(vbo, index);
            SET_INDEX(index, idxcount, 2, 3, 0);
            vbufaddindex(vbo, index);
            
            idxcount += 4;
            vbo->indxcount += 12;
        }

        /* Draw text */
        if (pack->type != PACK_INPUT && pack->type != PACK_OUTPUT) {
            vbo = &seg->textvbo;
            idxcount = vbo->indxcount / 3;

            /* Body text */
            len = strlen(pack->name);
            x_min = (pack->x + (pack->w * 0.5f)) - 
                ((PACK_FONT_SIZE + FONT_GAP) * len - FONT_GAP) *
                PIX_SIZE * 0.5f;
            y_min = (pack->y + (pack->h * 0.5f)) -
                PACK_FONT_SIZE * PIX_SIZE * 0.5f;
            x_max = x_min + PACK_FONT_SIZE * PIX_SIZE;
            y_max = y_min + PACK_FONT_SIZE * PIX_SIZE;

            drawtext(vbo, &seg->textcolors, pack->name,
                PACK_FONT_SIZE, textcolor, x_min, x_max, y_min, y_max);

            /* Pin Text */
            x_min = (pack->x + 6 * PIX_SIZE);
            x_max = x_min + PIN_FONT_SIZE * PIX_SIZE;
            y_min = (pack->y + pack->h) - 0.5f -
                PIN_FONT_SIZE * PIX_SIZE * 0.5f;
            y_max = y_min + PIN_FONT_SIZE * PIX_SIZE;

            for (i = 0; i < pack->in_pins.size; i++) {
                pin = dlistat(&pack->in_pins, i);
                len = strlen(pin->name);

                drawtext(vbo, &seg->textcolors, pin->name,
                    PIN_FONT_SIZE, textcolor, x_min, x_max, y_min, y_max);

                y_min -= 1.0f;
                y_max -= 1.0f;
            }
            y_min = (pack->y + pack->h) - 0.5f -
                PIN_FONT_SIZE * PIX_SIZE * 0.5f;
            y_max = y_min + PIN_FONT_SIZE * PIX_SIZE;

            for (i = 0; i < pack->out_pins.size; i++) {
                pin = dlistat(&pack->out_pins, i);
                len = strlen(pin->name);

                x_min = (pack->x + pack->w) - 6 * PIX_SIZE -
                    ((PIN_FONT_SIZE + FONT_GAP) * len - FONT_GAP) *
                    PIX_SIZE;
                x_max = x_min + PIN_FONT_SIZE * PIX_SIZE;

                drawtext(vbo, &seg->textcolors, pin->name,
                    PIN_FONT_SIZE, textcolor, x_min, x_max, y_min, y_max);

                y_min -= 1.0f;
                y_max -= 1.0f;
            }
        } else {
            /* Draw Input/Output pack text */
            vbo = &seg->textvbo;
            idxcount = vbo->indxcount / 3;

            len = strlen(pack->name);
            
            if (pack->type == PACK_INPUT) {
                x_min = pack->x - 8 * PIX_SIZE - 
                    ((IO_FONT_SIZE + FONT_GAP) * len - FONT_GAP) * PIX_SIZE;
            } else if (pack->type == PACK_OUTPUT) {
                x_min = pack->x + pack->w + 8 * PIX_SIZE; 
            }
            y_min = (pack->y + (pack->h * 0.5f)) -
                IO_FONT_SIZE * PIX_SIZE * 0.5f;
            x_max = x_min + IO_FONT_SIZE * PIX_SIZE;
            y_max = y_min + IO_FONT_SIZE * PIX_SIZE;

            drawtext(vbo, &seg->textcolors, pack->name,
                    IO_FONT_SIZE, iotextcolor, x_min, x_max, y_min, y_max);
        }

    }
    vbufbind(&seg->vbo);
    vbufbind(&seg->wirevbo);
    vbufbindvbo(&seg->wirevbo, &seg->colors);
    vbufbind(&seg->textvbo);
    vbufbindvbo(&seg->textvbo, &seg->textcolors);
}

void
layermesh(Segment *seg, Layer *layer, const float *color, uint depth)
{
    uint i, j, k, tile, tilenib, idxcount;
    vec3 vert, norm = {0.0f, 0.0f, 1.0f}, tilecolor;
    vec2 uv;
    ivec3 index;

    idxcount = seg->wirevbo.indxcount / 3;
    vert[2] = depth;

    for (i = 0; i < SEGMENT_SIZE; i++) {
        for (j = 0; j < SEGMENT_SIZE; j++) {
            tile = layer->tiles[j + i * SEGMENT_SIZE].state;

            if (!(tile & 0x1F)) continue;
            
            if (tile & TILE_POWERED) {
                tilecolor[0] = color[0] * 1.25f;
                tilecolor[1] = color[1] * 1.25f;
                tilecolor[2] = color[2] * 1.25f;
            } else {
                tilecolor[0] = color[0] * 0.5f;
                tilecolor[1] = color[1] * 0.5f;
                tilecolor[2] = color[2] * 0.5f;
            }
            
            if (tile == TILE_CENTRE) {
                for (k = 0; k < 4; k++) {
                    vert[0] = i + tileverts[k * 3 + 0];
                    vert[1] = j + tileverts[k * 3 + 1];
                    uv[0] = wireuvs[0][k * 2 + 0];
                    uv[1] = wireuvs[0][k * 2 + 1];
                    vbufaddvertex(&seg->wirevbo, vert, uv, norm);
                    veclistpush(&seg->colors, tilecolor);
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
                    veclistpush(&seg->colors, tilecolor);
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
    uint l, x, y, i, j, tile, tilenib, idxcount, layer, len;
    float x_min, x_max, y_min, y_max;
    vec3 vert, norm = {0.0f, 0.0f, 1.0f}, tilecolor;
    vec2 uv;
    ivec3 index;
    Package *pack;
    Pin *pin;
    float *uvptr;
    VBuffer *vbo;

    vbufreset(&seg->wirevbo);
    vbufreset(&seg->textvbo);
    veclistclear(&seg->colors);
    veclistclear(&seg->textcolors);
    
    vbo = &seg->wirevbo;
    idxcount = vbo->indxcount / 3;

    /* Draw wires */
    for (l = 0; l < LAYER_TOTAL; l++) {
        layer = wireorder[LAYER_TOTAL - 1 - l];
        vert[2] = l;
 
        for (x = 0; x < seg->w; x++) {
            for (y = 0; y < seg->h; y++) {
                tile = seg->tiles[layer][x * seg->h + y].state & 0x1F;

                if (!(tile & 0x1F)) continue;

                if (tile & TILE_POWERED) {
                    tilecolor[0] = colors[layer][0] * 1.25f;
                    tilecolor[1] = colors[layer][1] * 1.25f;
                    tilecolor[2] = colors[layer][2] * 1.25f;
                } else {
                    tilecolor[0] = colors[layer][0] * 0.5f;
                    tilecolor[1] = colors[layer][1] * 0.5f;
                    tilecolor[2] = colors[layer][2] * 0.5f;
                }

                if (tile == TILE_CENTRE) {
                    for (i = 0; i < 4; i++) {
                        vert[0] = x + tileverts[i * 3 + 0];
                        vert[1] = y + tileverts[i * 3 + 1];
                        uv[0] = wireuvs[0][i * 2 + 0];
                        uv[1] = wireuvs[0][i * 2 + 1];
                        vbufaddvertex(vbo, vert, uv, norm);
                        veclistpush(&seg->colors, tilecolor);
                    }
                    SET_INDEX(index, idxcount, 0, 1, 2);
                    vbufaddindex(vbo, index);
                    SET_INDEX(index, idxcount, 2, 3, 0);
                    vbufaddindex(vbo, index);

                    idxcount += 4;
                    vbo->indxcount += 12;
                } else {
                    tilenib = tile & 0xF;

                    for (i = 0; i < 4; i++) {
                        vert[0] = x + tileverts[i * 3 + 0];
                        vert[1] = y + tileverts[i * 3 + 1];
                        uv[0] = wireuvs[tilenib][i * 2 + 0];
                        uv[1] = wireuvs[tilenib][i * 2 + 1];
                        vbufaddvertex(vbo, vert, uv, norm);
                        veclistpush(&seg->colors, tilecolor);
                    }
                    SET_INDEX(index, idxcount, 0, 1, 2);
                    vbufaddindex(vbo, index);
                    SET_INDEX(index, idxcount, 2, 3, 0);
                    vbufaddindex(vbo, index);

                    idxcount += 4;
                    vbo->indxcount += 12;
                }
            }
        }
    }

    /* Draw packages */
    for (l = 0; l < seg->packs->size; l++) {
        vbo = &seg->wirevbo;
        idxcount = vbo->indxcount / 3;
        pack = listat(seg->packs, l);
        vert[2] = 4.0f;

        /* Draw body */
        for (y = 0; y < pack->h; y++) {
            for (x = 0; x < pack->w; x++) {
               calcpackuvs(pack, x, y, &uvptr);
                
                for (i = 0; i < 4; i++) {
                    vert[0] = (int)pack->x + x + tileverts[i * 3 + 0];
                    vert[1] = (int)pack->y + y + tileverts[i * 3 + 1];
                    uv[0] = uvptr[i * 2 + 0];
                    uv[1] = uvptr[i * 2 + 1];
                    vbufaddvertex(vbo, vert, uv, norm);
                    veclistpush(&seg->colors, colorwhite);
                }
                SET_INDEX(index, idxcount, 0, 1, 2);
                vbufaddindex(vbo, index);
                SET_INDEX(index, idxcount, 2, 3, 0);
                vbufaddindex(vbo, index);

                idxcount += 4;
                vbo->indxcount += 12;
            } 
        }

        /* Draw pins */
        for (i = 0; i < pack->in_pins.size; i++) {
            pin = dlistat(&pack->in_pins, i);

            if (pin->x < 0) {
                uvptr = pinuvs + PINTEX_LEFT * 8;
            } else {
                if (pin->y < 0) {
                    uvptr = pinuvs + PINTEX_DOWN * 8;
                } else if ((uint)pin->y >= pack->h) {
                    uvptr = pinuvs + PINTEX_UP * 8;
                } else {
                    uvptr = pinuvs + PINTEX_RIGHT * 8;
                }
            }

            for (j = 0; j < 4; j++) {
                vert[0] = (int)pack->x + pin->x + tileverts[j * 3 + 0];
                vert[1] = (int)pack->y + pin->y + tileverts[j * 3 + 1];
                uv[0] = uvptr[j * 2 + 0];
                uv[1] = uvptr[j * 2 + 1];
                vbufaddvertex(vbo, vert, uv, norm);
                veclistpush(&seg->colors, colorwhite);
            }
            SET_INDEX(index, idxcount, 0, 1, 2);
            vbufaddindex(vbo, index);
            SET_INDEX(index, idxcount, 2, 3, 0);
            vbufaddindex(vbo, index);
            
            idxcount += 4;
            vbo->indxcount += 12;
        }
        for (i = 0; i < pack->out_pins.size; i++) {
            pin = dlistat(&pack->out_pins, i);

            if (pin->x < 0) {
                uvptr = pinuvs + PINTEX_LEFT * 8;
            } else {
                if (pin->y < 0) {
                    uvptr = pinuvs + PINTEX_DOWN * 8;
                } else if ((uint)pin->y >= pack->h) {
                    uvptr = pinuvs + PINTEX_UP * 8;
                } else {
                    uvptr = pinuvs + PINTEX_RIGHT * 8;
                }
            }

            for (j = 0; j < 4; j++) {
                vert[0] = pack->x + pin->x + tileverts[j * 3 + 0];
                vert[1] = pack->y + pin->y + tileverts[j * 3 + 1];
                uv[0] = uvptr[j * 2 + 0];
                uv[1] = uvptr[j * 2 + 1];
                vbufaddvertex(vbo, vert, uv, norm);
                veclistpush(&seg->colors, colorwhite);
            }
            SET_INDEX(index, idxcount, 0, 1, 2);
            vbufaddindex(vbo, index);
            SET_INDEX(index, idxcount, 2, 3, 0);
            vbufaddindex(vbo, index);
            
            idxcount += 4;
            vbo->indxcount += 12;
        }

        /* Draw text */
        if (pack->type != PACK_INPUT && pack->type != PACK_OUTPUT) {
            vbo = &seg->textvbo;
            idxcount = vbo->indxcount / 3;

            /* Body text */
            len = strlen(pack->name);
            x_min = (pack->x + (pack->w * 0.5f)) - 
                ((PACK_FONT_SIZE + FONT_GAP) * len - FONT_GAP) *
                PIX_SIZE * 0.5f;
            y_min = (pack->y + (pack->h * 0.5f)) -
                PACK_FONT_SIZE * PIX_SIZE * 0.5f;
            x_max = x_min + PACK_FONT_SIZE * PIX_SIZE;
            y_max = y_min + PACK_FONT_SIZE * PIX_SIZE;

            drawtext(vbo, &seg->textcolors, pack->name,
                PACK_FONT_SIZE, packtextcolor, x_min, x_max, y_min, y_max);

            /* Pin Text */
            x_min = (pack->x + 6 * PIX_SIZE);
            x_max = x_min + PIN_FONT_SIZE * PIX_SIZE;
            y_min = (pack->y + pack->h) - 0.5f -
                PIN_FONT_SIZE * PIX_SIZE * 0.5f;
            y_max = y_min + PIN_FONT_SIZE * PIX_SIZE;

            for (i = 0; i < pack->in_pins.size; i++) {
                pin = dlistat(&pack->in_pins, i);
                len = strlen(pin->name);

                drawtext(vbo, &seg->textcolors, pin->name,
                    PIN_FONT_SIZE, packtextcolor, x_min, x_max, y_min, y_max);

                y_min -= 1.0f;
                y_max -= 1.0f;
            }
            y_min = (pack->y + pack->h) - 0.5f -
                PIN_FONT_SIZE * PIX_SIZE * 0.5f;
            y_max = y_min + PIN_FONT_SIZE * PIX_SIZE;

            for (i = 0; i < pack->out_pins.size; i++) {
                pin = dlistat(&pack->out_pins, i);
                len = strlen(pin->name);

                x_min = (pack->x + pack->w) - 6 * PIX_SIZE -
                    ((PIN_FONT_SIZE + FONT_GAP) * len - FONT_GAP) * PIX_SIZE;
                x_max = x_min + PIN_FONT_SIZE * PIX_SIZE;

                drawtext(vbo, &seg->textcolors, pin->name,
                    PIN_FONT_SIZE, packtextcolor, x_min, x_max, y_min, y_max);

                y_min -= 1.0f;
                y_max -= 1.0f;
            }
        } else {
            /* Draw Input/Output pack text */
            vbo = &seg->textvbo;
            idxcount = vbo->indxcount / 3;

            len = strlen(pack->name);
            
            if (pack->type == PACK_INPUT) {
                x_min = pack->x - 8 * PIX_SIZE - 
                    ((IO_FONT_SIZE + FONT_GAP) * len - FONT_GAP) * PIX_SIZE;
            } else if (pack->type == PACK_OUTPUT) {
                x_min = pack->x + pack->w + 8 * PIX_SIZE; 
            }
            y_min = (pack->y + (pack->h * 0.5f)) -
                IO_FONT_SIZE * PIX_SIZE * 0.5f;
            x_max = x_min + IO_FONT_SIZE * PIX_SIZE;
            y_max = y_min + IO_FONT_SIZE * PIX_SIZE;

            drawtext(vbo, &seg->textcolors, pack->name,
                    IO_FONT_SIZE, iotextcolor, x_min, x_max, y_min, y_max);
        }
    }
    vbufbind(&seg->wirevbo);
    vbufbindvbo(&seg->wirevbo, &seg->colors);
    vbufbind(&seg->textvbo);
    vbufbindvbo(&seg->textvbo, &seg->textcolors);
}

void
calcpackuvs(Package *pack, uint x, uint y, float **uvs)
{
    uint texpos;

    if (pack->type == PACK_INPUT) {
        if (pack->state) {
            *uvs = inpackuvs + 8;
        } else {
            *uvs = inpackuvs;
        }
        return;
    } else if (pack->type == PACK_OUTPUT) {
        if (pack->state) {
            *uvs = outpackuvs + 8;
        } else {
            *uvs = outpackuvs;
        }
        return;
    }

    if (pack->w == 1) {
        if (pack->h == 1) {
            texpos = PACKTEX_1W_1H;
        } else {
            if (y == 0) {
                texpos = PACKTEX_1W_BOT;
            } else if (y == pack->h - 1) {
                texpos = PACKTEX_1W_TOP;
            } else {
                texpos = PACKTEX_1W_CENTRE;
            }
        }
    } else if (pack->h == 1) {
        if (x == 0) {
            texpos = PACKTEX_1H_LEFT;
        } else if (x == pack->w - 1) {
            texpos = PACKTEX_1H_RIGHT;
        } else {
            texpos = PACKTEX_1H_CENTRE;
        }
    } else {
        if (x == 0) {
            if (y == 0) {
                texpos = PACKTEX_BOT_LEFT;
            } else if (y == pack->h - 1) {
                texpos = PACKTEX_TOP_LEFT;
            } else {
                texpos = PACKTEX_LEFT;
            }
        } else if (x == pack->w - 1) {
            if (y == 0) {
                texpos = PACKTEX_BOT_RIGHT;
            } else if (y == pack->h - 1) {
                texpos = PACKTEX_TOP_RIGHT;
            } else {
                texpos = PACKTEX_RIGHT;
            }
        } else {
            if (y == 0) {
                texpos = PACKTEX_BOT;
            } else if (y == pack->h - 1) {
                texpos = PACKTEX_TOP;
            } else {
                texpos = PACKTEX_CENTRE;
            }
        }
    }
    *uvs = packuvs + texpos * 8;
}

void
drawtext(VBuffer *vbo, VecList *colors, const char *text, uint fontsize,
        const float *color, float x_min, float x_max, float y_min, float y_max)
{
    uint i, len, idxcount;
    vec3 vert, norm = {0.0f, 0.0f, 1.0f};
    vec2 uv;
    ivec3 index;
    uchar chr;

    idxcount = vbo->indxcount / 3;
    len = strlen(text);
    vert[2] = 5.0f;

    for (i = 0; i < len; i++) {
        chr = text[i];

        vert[0] = x_max;
        vert[1] = y_min;
        uv[0] = fonttex.uvs[chr][0];
        uv[1] = fonttex.uvs[chr][1];
        vbufaddvertex(vbo, vert, uv, norm);
        veclistpush(colors, color);

        vert[0] = x_min;
        vert[1] = y_min;
        uv[0] = fonttex.uvs[chr][2];
        uv[1] = fonttex.uvs[chr][3];
        vbufaddvertex(vbo, vert, uv, norm);
        veclistpush(colors, color);

        vert[0] = x_min;
        vert[1] = y_max;
        uv[0] = fonttex.uvs[chr][4];
        uv[1] = fonttex.uvs[chr][5];
        vbufaddvertex(vbo, vert, uv, norm);
        veclistpush(colors, color);

        vert[0] = x_max;
        vert[1] = y_max;
        uv[0] = fonttex.uvs[chr][6];
        uv[1] = fonttex.uvs[chr][7];
        vbufaddvertex(vbo, vert, uv, norm);
        veclistpush(colors, color);

        x_min += (fontsize + FONT_GAP) * PIX_SIZE;
        x_max += (fontsize + FONT_GAP) * PIX_SIZE;

        SET_INDEX(index, idxcount, 0, 1, 2);
        vbufaddindex(vbo, index);
        SET_INDEX(index, idxcount, 2, 3, 0);
        vbufaddindex(vbo, index);

        idxcount += 4;
        vbo->indxcount += 12;
    }
}



void
selboxmesh(SelectBox *box)
{
    VBuffer *vbo;
    uint idxcount;
    float x_min, x_max, y_min, y_max;
    vec3 vert;
    vec2 uv = {0.0f, 0.0f};
    vec3 norm = {0.0f, 0.0f, 0.0f};
    ivec3 index;

    vbo = &box->vbo;
    idxcount = 0;
    vert[2] = 7.0f;

    vbufreset(vbo);

    x_min = box->x; //box->x + SELBOX_THICKNESS / 2.0f;
    x_max = x_min + box->w; //x_min + box->w - SELBOX_THICKNESS;
    y_min = box->y; //box->y + SELBOX_THICKNESS / 2.0f;
    y_max = y_min + box->h; //y_min + box->h - SELBOX_THICKNESS;

    vert[0] = x_max;
    vert[1] = y_min;
    vbufaddvertex(vbo, vert, uv, norm);
    
    vert[0] = x_min;
    vert[1] = y_min;
    vbufaddvertex(vbo, vert, uv, norm);
    
    vert[0] = x_min;
    vert[1] = y_max;
    vbufaddvertex(vbo, vert, uv, norm);
    
    vert[0] = x_max;
    vert[1] = y_max;
    vbufaddvertex(vbo, vert, uv, norm);

    index[0] = 0;
    index[1] = 1;
    index[2] = 1;
    vbufaddindex(vbo, index);

    index[0] = 2;
    index[1] = 2;
    index[2] = 3;
    vbufaddindex(vbo, index);
    
    index[0] = 3;
    index[1] = 0;
    vbufaddindex(vbo, index);

    vbo->indxcount = 8;

    vbufbind(vbo);
}

void
fullremesh(void)
{
    uint i;

    for (i = 0; i < SEGMENT_COUNT; i++) {
        mainworld->segs[i]->dirty = 1;
    }
}

/* Populates destination world with contents of selbox from source world.
 * NOTE: Worlds must already be initialised. */
void
worldbuildfromselbox(World *dest, World *src, uint clearsrc)
{
    const int x = selbox.x, y = selbox.y,
          w = selbox.w, h = selbox.h;

    int l, i, j;
    int wx, wy;
    Segment *seg_src, *seg_dest;
    Package *pack;
    
    uint *src_tile, *dest_tile;

    if (!selbox.enabled) return;

    /* Copy tile data to destination */
    for (l = 0; l < LAYER_TOTAL; l++) {
        for (i = 0; i < selbox.w; i++) {
            for (j = 0; j < selbox.h; j++) {
                src_tile = &worldgettileat(src,
                        selbox.x + i, selbox.y + j, l)->state;
                dest_tile = &worldgettileat(dest,
                        selbox.x + i, selbox.y + j, l)->state;
                *dest_tile = *src_tile;

                if (clearsrc) {
                    *src_tile = 0;
                }
            }
        }
    }

    /* Copy packs over to destination */
    for (i = x / SEGMENT_SIZE; i <= (x + w) / SEGMENT_SIZE; i++) {
        for (j = y / SEGMENT_SIZE; j <= (y + h) / SEGMENT_SIZE; j++) {
            seg_src = src->segs[j + i * SEGMENTS_PER_ROW];
            seg_dest = dest->segs[j + i * SEGMENTS_PER_ROW];

            for (l = 0; l < (int)seg_src->packs->size; l++) {
                if (clearsrc) {
                    pack = listat(seg_src->packs, l);
                } else {
                    pack = packcopy(listat(seg_src->packs, l));
                }
                wx = pack->x + seg_src->x * SEGMENT_SIZE;
                wy = pack->y + seg_src->y * SEGMENT_SIZE;

                if (x <= wx && x + w >= wx + (int)pack->w &&
                    y <= wy && y + h >= wy + (int)pack->h) {
                    listpushback(seg_dest->packs, pack);

                    if (clearsrc) {
                        listremove(seg_src->packs, l--);
                    }
                }
            }
            if (clearsrc) {
                seg_src->dirty = 1;
            }
        }
    }
    circuitrebuild(src, &src->circuit);
}

/* Creates a segment containing elements in given rectangle */
FreeSegment *
freesegbuild(uint x, uint y, uint w, uint h, uint clearsrc)
{
    uint l, i, j;
    uint wx, wy;
    FreeSegment *seg;
    Segment *tmpseg;
    Package *pack;
    uint *tile;

    seg = calloc(1, sizeof(FreeSegment));
    freeseginit(seg, 0.0f, 0.0f, w, h);
    
    /* Map wire tiles */
    for (l = 0; l < LAYER_TOTAL; l++) {
        for (i = 0; i < w; i++) {
            for (j = 0; j < h; j++) {
                tile = &worldgettileat(mainworld, x + i, y + j, l)->state;
                seg->tiles[l][j + i * h].state = *tile;
                
                if (clearsrc) {
                    *tile = 0;
                    worldgetsegmentat(mainworld, x + i, y + j)->dirty = 1;
                }
            }
        }
    }
    
    /* Map packages */
    for (i = x / SEGMENT_SIZE; i <= (x + w) / SEGMENT_SIZE; i++) {
        for (j = y / SEGMENT_SIZE; j <= (y + h) / SEGMENT_SIZE; j++) {
            tmpseg = worldgetsegment(mainworld, i, j);
            
            for (l = 0; l < tmpseg->packs->size; l++) {
                if (clearsrc) {
                    pack = listat(tmpseg->packs, l);
                } else {
                    pack = packcopy(listat(tmpseg->packs, l));
                }
                wx = pack->x + tmpseg->x * SEGMENT_SIZE;
                wy = pack->y + tmpseg->y * SEGMENT_SIZE;
               
                /* Selection counts only when pack is entirely inside bounds */
                if (x <= wx && x + w >= wx + pack->w &&
                    y <= wy && y + h >= wy + pack->h) {
                    pack->x = wx - x;
                    pack->y = wy - y;
                    listpushback(seg->packs, pack);

                    if (clearsrc) {
                        listremove(tmpseg->packs, l--);
                    }
                }
            }
            tmpseg->dirty = 1;
        }
    }
    
    return seg;
}

/* Copy free segment data into the currently active world */
int
freesegmerge(FreeSegment *seg, int x, int y)
{
    uint l, i, j, k, wx, wy;
    uint tile;
    Package *pack, *tmppack;
    Segment *tmpseg;

    /* Check bounds violations */
    if (x < 0 || x + seg->w >= SEGMENTS_PER_ROW * SEGMENT_SIZE ||
        y < 0 || y + seg->h >= SEGMENTS_PER_ROW * SEGMENT_SIZE) {
        return 0;
    }

    /* Check for package overlaps */
    for (l = 0; l < seg->packs->size; l++) {
        pack = listat(seg->packs, l);

        for (i = (x + pack->x) / SEGMENT_SIZE;
                i <= (x + pack->x + pack->w) / SEGMENT_SIZE; i++) {
            for (j = (y + pack->y) / SEGMENT_SIZE;
                    j <= (y + pack->y + pack->h) / SEGMENT_SIZE; j++) {
                tmpseg = worldgetsegment(mainworld, i, j);

                for (k = 0; k < tmpseg->packs->size; k++) {
                    tmppack = listat(tmpseg->packs, k);
                    wx = tmppack->x + tmpseg->x * SEGMENT_SIZE;
                    wy = tmppack->y + tmpseg->y * SEGMENT_SIZE;

                    if (quadintersectsquad(
                            x + pack->x, y + pack->y, pack->w, pack->h,
                            wx, wy, tmppack->w, tmppack->h)) {
                        return 0;
                    }
                }
            }
        }
    }

    for (l = 0; l < LAYER_TOTAL; l++) {
        for (i = 0; i < seg->w; i++) {
            for (j = 0; j < seg->h; j++) {
                tile = seg->tiles[l][j + i * seg->h].state;
                worldgettileat(mainworld, x + i, y + j, l)->state |= tile;
                worldgetsegmentat(mainworld, x + i, y + j)->dirty = 1;
            }
        }
    }

    /* Merge segment */
    for (l = 0; l < seg->packs->size; l++) {
        pack = listat(seg->packs, l);
        wx = x + pack->x;
        wy = y + pack->y;
        tmpseg = worldgetsegmentat(mainworld, wx, wy);
        
        pack->x = pack->x + (x - (tmpseg->x * SEGMENT_SIZE));
        pack->y = pack->y + (y - (tmpseg->y * SEGMENT_SIZE));
        pack->seg = tmpseg;
        pack->seg_x = tmpseg->x;
        pack->seg_y = tmpseg->y;
        listpushback(tmpseg->packs, pack);
        tmpseg->dirty = 1;
    }

    return 1;
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

int
pointintersectsquad(float px, float py, float x, float y, float w, float h)
{
    return (
        px >= x && px <= x + w &&
        py >= y && py <= y + h
    );
}

int
quadintersectsquad(float ax, float ay, float aw, float ah,
        float bx, float by, float bw, float bh)
{
    return (
        ax <= bx + bw && ax + aw >= bx &&
        ay < by + bh && ay + ah > by
    );
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

void
setmouseseg(FreeSegment *seg)
{
    if (mouseseg) {
        freesegdelete(mouseseg);
    }
    mouseseg = seg;
    mousemode = MOUSE_GRAB;
}

/*
 *  GUI DEFINITIONS
 *
 *  Here be dragons
 */

static void guibuttonmouseover(GUIElement *elem, ...);
static void guibuttonmouseleave(GUIElement *elem, ...);
static void guibuttonmouseup(GUIElement *elem, ...);

/* Toolbar callbacks */
static void guibuttonnotdown(GUIElement *elem, ...);
static void guibuttonordown(GUIElement *elem, ...);
static void guibuttonanddown(GUIElement *elem, ...);
static void guibuttonxordown(GUIElement *elem, ...);
static void guibuttonindown(GUIElement *elem, ...);
static void guibuttonoutdown(GUIElement *elem, ...);
static void guibuttonclockdown(GUIElement *elem, ...);
static void guibuttonmemorydown(GUIElement *elem, ...);
static void guibuttonconnectordown(GUIElement *elem, ...);

static void guibuttonreddown(GUIElement *elem, ...);
static void guibuttongreendown(GUIElement *elem, ...);
static void guibuttonbluedown(GUIElement *elem, ...);

static void guibuttonredup(GUIElement *elem, ...);
static void guibuttongreenup(GUIElement *elem, ...);
static void guibuttonblueup(GUIElement *elem, ...);

static void guibuttonredmouseleave(GUIElement *elem, ...);
static void guibuttongreenmouseleave(GUIElement *elem, ...);
static void guibuttonbluemouseleave(GUIElement *elem, ...);

/* Filebar callbacks */
static void guibuttonfilenewdown(GUIElement *elem, ...);
static void guibuttonfileopendown(GUIElement *elem, ...);
static void guibuttonfilesavedown(GUIElement *elem, ...);

/* Packbar callbacks */
static void guibuttonpackopendown(GUIElement *elem, ...);
static void guibuttonpacksavedown(GUIElement *elem, ...);

static void guicheckmouseseg(GUIElement *elem);

enum gui_toolbar_buttons {
    BUTTON_NOT,
    BUTTON_OR,
    BUTTON_AND,
    BUTTON_XOR,
    BUTTON_RED,
    BUTTON_GREEN,
    BUTTON_BLUE,
    BUTTON_INPUT,
    BUTTON_OUTPUT,
    BUTTON_CLOCK,
    BUTTON_MEMORY,
    BUTTON_CONNECTOR,
    BUTTON_TOTAL
};

#define TOOLBAR_W       0.05f * BUTTON_TOTAL
#define TOOLBAR_H       0.1f
#define TOOLBAR_X       0.5f - (TOOLBAR_W / 2.0f)
#define TOOLBAR_Y       0.0f
#define TOOLBAR_BTN_W   1.0f / BUTTON_TOTAL
#define TOOLBAR_BTN_H   1.0f

#define FILEBAR_NBTNS   3
#define FILEBAR_W       0.05f * FILEBAR_NBTNS
#define FILEBAR_H       0.1f
#define FILEBAR_X       0.0f
#define FILEBAR_Y       1.0f
#define FILEBAR_BTN_W   1.0f / FILEBAR_NBTNS
#define FILEBAR_BTN_H   1.0f

#define PACKBAR_NBTNS   2
#define PACKBAR_W       0.05f * PACKBAR_NBTNS
#define PACKBAR_H       0.1f
#define PACKBAR_X       1.0f - PACKBAR_W
#define PACKBAR_Y       1.0f
#define PACKBAR_BTN_W   1.0f / PACKBAR_NBTNS
#define PACKBAR_BTN_H   1.0f

#define TOOLBAR_OFFSET_W  TOOLBAR_BTN_W * 0.01f; 
#define TOOLBAR_OFFSET_H  TOOLBAR_BTN_H * 0.01f; 
#define FILEBAR_OFFSET_W  FILEBAR_BTN_W * 0.01f; 
#define FILEBAR_OFFSET_H  FILEBAR_BTN_H * 0.01f; 
#define PACKBAR_OFFSET_W  PACKBAR_BTN_W * 0.01f; 
#define PACKBAR_OFFSET_H  PACKBAR_BTN_H * 0.01f; 

const char *toolbarnames[] = {
    "toolbar_not",
    "toolbar_or",
    "toolbar_and",
    "toolbar_xor",
    "toolbar_red",
    "toolbar_green",
    "toolbar_blue",
    "toolbar_input",
    "toolbar_output",
    "toolbar_clock",
    "toolbar_memory",
    "toolbar_connector"
};

const char *toolbariconnames[] = {
    "toolbar_not_icon",
    "toolbar_or_icon",
    "toolbar_and_icon",
    "toolbar_xor_icon",
    "toolbar_red_icon",
    "toolbar_green_icon",
    "toolbar_blue_icon",
    "toolbar_input_icon",
    "toolbar_output_icon",
    "toolbar_clock_icon",
    "toolbar_memory_icon",
    "toolbar_connector_icon"
};

void
guibuttonmouseover(GUIElement *elem, ...)
{
    ui_tooltip_configtext(elem);
    ui_tooltip_show();
    guisetbacktexture(elem, 256, 320, 128, 128);
    elem->layer->dirty = 1;
}

void
guibuttonmouseleave(GUIElement *elem, ...)
{
    ui_tooltip_hide();
    guisetbacktexture(elem, 128, 320, 128, 128);
    elem->layer->dirty = 1;
}

void
guibuttonmouseup(GUIElement *elem, ...)
{
    if (elem->state & GUI_STATE_MOUSEOVER) {
        guisetbacktexture(elem, 256, 320, 128, 128);
    } else {
        guisetbacktexture(elem, 128, 320, 128, 128);
    }
    elem->layer->dirty = 1;
}

void
guibuttonnotdown(GUIElement *elem, ...)
{
    FreeSegment *seg;
    Package *pack;

    if ((mousemode == MOUSE_NONE || mousemode == MOUSE_BOXREADY) &&
            currentwire == WIRE_NONE) {
        guisetbacktexture(elem, 384, 320, 128, 128);
        seg = malloc(sizeof(FreeSegment));
        freeseginit(seg, 0, 0, GATE_NOT_W, GATE_NOT_H);
        pack = packcreate(PACK_NOT, NULL);
        listpushback(seg->packs, pack);
        freesegmesh(seg);
        mouseseg = seg;
        mousemode = MOUSE_GRAB;
        elem->layer->dirty = 1;
        selbox.enabled = 0;
    }
}

void
guibuttonordown(GUIElement *elem, ...)
{
    FreeSegment *seg;
    Package *pack;

    if ((mousemode == MOUSE_NONE || mousemode == MOUSE_BOXREADY) &&
            currentwire == WIRE_NONE) {
        guisetbacktexture(elem, 384, 320, 128, 128);
        seg = malloc(sizeof(FreeSegment));
        freeseginit(seg, 0, 0, GATE_OR_W, GATE_OR_H);
        pack = packcreate(PACK_OR, NULL);
        listpushback(seg->packs, pack);
        freesegmesh(seg);
        mouseseg = seg;
        mousemode = MOUSE_GRAB;
        elem->layer->dirty = 1;
        selbox.enabled = 0;
    }
}

void
guibuttonanddown(GUIElement *elem, ...)
{   
    FreeSegment *seg;
    Package *pack;

    if ((mousemode == MOUSE_NONE || mousemode == MOUSE_BOXREADY) &&
            currentwire == WIRE_NONE) {
        guisetbacktexture(elem, 384, 320, 128, 128);
        seg = malloc(sizeof(FreeSegment));
        freeseginit(seg, 0, 0, GATE_AND_W, GATE_AND_H);
        pack = packcreate(PACK_AND, NULL);
        listpushback(seg->packs, pack);
        freesegmesh(seg);
        mouseseg = seg;
        mousemode = MOUSE_GRAB;
        elem->layer->dirty = 1;
        selbox.enabled = 0;
    }
}

void
guibuttonxordown(GUIElement *elem, ...)
{
    FreeSegment *seg;
    Package *pack;

    if ((mousemode == MOUSE_NONE || mousemode == MOUSE_BOXREADY) &&
            currentwire == WIRE_NONE) {
        guisetbacktexture(elem, 384, 320, 128, 128);
        seg = malloc(sizeof(FreeSegment));
        freeseginit(seg, 0, 0, GATE_XOR_W, GATE_XOR_H);
        pack = packcreate(PACK_XOR, NULL);
        listpushback(seg->packs, pack);
        freesegmesh(seg);
        mouseseg = seg;
        mousemode = MOUSE_GRAB;
        elem->layer->dirty = 1;
        selbox.enabled = 0;
    }
}

void
guibuttonindown(GUIElement *elem, ...)
{
    FreeSegment *seg;
    Package *pack;

    if ((mousemode == MOUSE_NONE || mousemode == MOUSE_BOXREADY) &&
            currentwire == WIRE_NONE) {
        guisetbacktexture(elem, 384, 320, 128, 128);
        seg = malloc(sizeof(FreeSegment));
        freeseginit(seg, 0, 0, PACK_IN_W, PACK_IN_H);
        pack = packcreate(PACK_INPUT, NULL);
        listpushback(seg->packs, pack);
        freesegmesh(seg);
        mouseseg = seg;
        mousemode = MOUSE_GRAB;
        elem->layer->dirty = 1;
        selbox.enabled = 0;
    }
}

void
guibuttonoutdown(GUIElement *elem, ...)
{
    FreeSegment *seg;
    Package *pack;

    if ((mousemode == MOUSE_NONE || mousemode == MOUSE_BOXREADY) &&
            currentwire == WIRE_NONE) {
        guisetbacktexture(elem, 384, 320, 128, 128);
        seg = malloc(sizeof(FreeSegment));
        freeseginit(seg, 0, 0, PACK_OUT_W, PACK_OUT_H);
        pack = packcreate(PACK_OUTPUT, NULL);
        listpushback(seg->packs, pack);
        freesegmesh(seg);
        mouseseg = seg;
        mousemode = MOUSE_GRAB;
        elem->layer->dirty = 1;
        selbox.enabled = 0;
    }
}

void
guibuttonclockdown(GUIElement *elem, ...)
{
    FreeSegment *seg;
    Package *pack;

    if ((mousemode == MOUSE_NONE || mousemode == MOUSE_BOXREADY) &&
            currentwire == WIRE_NONE) {
        guisetbacktexture(elem, 384, 320, 128, 128);
        seg = malloc(sizeof(FreeSegment));
        freeseginit(seg, 0, 0, PACK_CLK_W, PACK_CLK_H);
        pack = packcreate(PACK_CLOCK, NULL);
        listpushback(seg->packs, pack);
        freesegmesh(seg);
        mouseseg = seg;
        mousemode = MOUSE_GRAB;
        elem->layer->dirty = 1;
        selbox.enabled = 0;
    }
}

void
guibuttonmemorydown(GUIElement *elem, ...)
{
    FreeSegment *seg;
    Package *pack;

    if ((mousemode == MOUSE_NONE || mousemode == MOUSE_BOXREADY) &&
            currentwire == WIRE_NONE) {
        guisetbacktexture(elem, 384, 320, 128, 128);
        seg = malloc(sizeof(FreeSegment));
        freeseginit(seg, 0, 0, PACK_MEM_W, PACK_MEM_H);
        pack = packcreate(PACK_MEMORY, NULL);
        listpushback(seg->packs, pack);
        freesegmesh(seg);
        mouseseg = seg;
        mousemode = MOUSE_GRAB;
        elem->layer->dirty = 1;
        selbox.enabled = 0;
    }
}

void
guibuttonconnectordown(GUIElement *elem, ...)
{
    FreeSegment *seg;
    Package *pack;

    if ((mousemode == MOUSE_NONE || mousemode == MOUSE_BOXREADY) &&
            currentwire == WIRE_NONE) {
        guisetbacktexture(elem, 384, 320, 128, 128);
        seg = malloc(sizeof(FreeSegment));
        freeseginit(seg, 0, 0, PACK_CONNECT_W, PACK_CONNECT_H);
        pack = packcreate(PACK_CONNECTOR, NULL);
        listpushback(seg->packs, pack);
        freesegmesh(seg);
        mouseseg = seg;
        mousemode = MOUSE_GRAB;
        elem->layer->dirty = 1;
        selbox.enabled = 0;
    }
}


void
guibuttonreddown(GUIElement *elem, ...)
{
    GUIElement *green, *blue;

    if (mousemode == MOUSE_BOXREADY) {
        mousemode = MOUSE_NONE;
        selbox.enabled = 0;
    }
    if (mousemode != MOUSE_NONE) {
        return;
    }

    green = guigetelem("toolbar_green");
    blue = guigetelem("toolbar_blue");
    
    if (currentwire == WIRE_RED) {
        currentwire = WIRE_NONE;
        guisetbacktexture(elem, 384, 320, 128, 128);
    } else {
        currentwire = WIRE_RED;
        wiremovetotop(WIRE_RED);
        fullremesh();
        guisetbacktexture(elem, 384, 320, 128, 128);
        guisetbacktexture(green, 128, 320, 128, 128);
        guisetbacktexture(blue, 128, 320, 128, 128);
    }
    elem->layer->dirty = 1;
}

void
guibuttongreendown(GUIElement *elem, ...)
{
    GUIElement *red, *blue;

    if (mousemode == MOUSE_BOXREADY) {
        mousemode = MOUSE_NONE;
        selbox.enabled = 0;
    }
    if (mousemode != MOUSE_NONE) {
        return;
    }
    
    red = guigetelem("toolbar_red");
    blue = guigetelem("toolbar_blue");

    if (currentwire == WIRE_GREEN) {
        currentwire = WIRE_NONE;
        guisetbacktexture(elem, 384, 320, 128, 128);
    } else {
        currentwire = WIRE_GREEN;
        wiremovetotop(WIRE_GREEN);
        fullremesh();
        guisetbacktexture(elem, 384, 320, 128, 128);
        guisetbacktexture(red, 128, 320, 128, 128);
        guisetbacktexture(blue, 128, 320, 128, 128);
    }
    elem->layer->dirty = 1;
}

void
guibuttonbluedown(GUIElement *elem, ...)
{
    GUIElement *red, *green;

    if (mousemode == MOUSE_BOXREADY) {
        mousemode = MOUSE_NONE;
        selbox.enabled = 0;
    }
    if (mousemode != MOUSE_NONE) {
        return;
    }

    red = guigetelem("toolbar_red");
    green = guigetelem("toolbar_green");

    if (currentwire == WIRE_BLUE) {
        currentwire = WIRE_NONE;
        guisetbacktexture(elem, 384, 320, 128, 128);
    } else {
        currentwire = WIRE_BLUE;
        wiremovetotop(WIRE_BLUE);
        fullremesh();
        guisetbacktexture(elem, 384, 320, 128, 128);
        guisetbacktexture(red, 128, 320, 128, 128);
        guisetbacktexture(green, 128, 320, 128, 128);
    }
    elem->layer->dirty = 1;
}

void 
guibuttonredup(GUIElement *elem, ...)
{
    if (currentwire == WIRE_RED) {
        guisetbacktexture(elem, 256, 320, 128, 128);
    } else {
        if (elem->state & GUI_STATE_MOUSEOVER) {
            guisetbacktexture(elem, 256, 320, 128, 128);
        } else {
            guisetbacktexture(elem, 128, 320, 128, 128);
        }
    }
    elem->layer->dirty = 1;
}

void
guibuttongreenup(GUIElement *elem, ...)
{
    if (currentwire == WIRE_GREEN) {
        guisetbacktexture(elem, 256, 320, 128, 128);
    } else {
        if (elem->state & GUI_STATE_MOUSEOVER) {
            guisetbacktexture(elem, 256, 320, 128, 128);
        } else {
            guisetbacktexture(elem, 128, 320, 128, 128);
        }
    }
    elem->layer->dirty = 1;
}

void
guibuttonblueup(GUIElement *elem, ...)
{
    if (currentwire == WIRE_BLUE) {
        guisetbacktexture(elem, 256, 320, 128, 128);
    } else {
        if (elem->state & GUI_STATE_MOUSEOVER) {
            guisetbacktexture(elem, 256, 320, 128, 128);
        } else {
            guisetbacktexture(elem, 128, 320, 128, 128);
        }
    }
    elem->layer->dirty = 1;
}

void
guibuttonredmouseleave(GUIElement *elem, ...)
{
    if (currentwire == WIRE_RED) {
        guisetbacktexture(elem, 256, 320, 128, 128);
    } else {
        guisetbacktexture(elem, 128, 320, 128, 128);
    }
    ui_tooltip_hide();
    elem->layer->dirty = 1;
}

void
guibuttongreenmouseleave(GUIElement *elem, ...)
{
    if (currentwire == WIRE_GREEN) {
        guisetbacktexture(elem, 256, 320, 128, 128);
    } else {
        guisetbacktexture(elem, 128, 320, 128, 128);
    }
    ui_tooltip_hide();
    elem->layer->dirty = 1;
}

void
guibuttonbluemouseleave(GUIElement *elem, ...)
{
    if (currentwire == WIRE_BLUE) {
        guisetbacktexture(elem, 256, 320, 128, 128);
    } else {
        guisetbacktexture(elem, 128, 320, 128, 128);
    }
    ui_tooltip_hide();
    elem->layer->dirty = 1;
}

void 
guibuttonfilenewdown(GUIElement *elem, ...)
{
    /* Open file name prompt */
    ui_confirmbox_fileload_preloadsave();
}

void
guibuttonfileopendown(GUIElement *elem, ...)
{
    /* Open file open window */
    ui_fileload_show();
}

void
guibuttonfilesavedown(GUIElement *elem, ...)
{
    /* Open file save window */
    ui_filesave_show();
}

void
guibuttonpackopendown(GUIElement *elem, ...)
{
    /* Open user pack open window */
    if (ui_packload_isopen()) {
        ui_packload_hide();
    } else {
        ui_packload_show();
    }
}

void
guibuttonpacksavedown(GUIElement *elem, ...)
{
    /* Open user pack name prompt */
    if (selbox.enabled) {
        ui_packsave_show();
    } else {
        ui_dialogbox_settext("Unable to create package. Please make a "
                "selection with the drag-drop box of the circuitry you wish "
                "to save as a package.");
        ui_dialogbox_show();
    }
}

void
guifilebarcreate(void)
{
    uint i;
    GUILayer *layer;
    GUIElement *base, *base_right;
    GUIElement *new, *open, *save;
    GUIElement *new_icon, *open_icon, *save_icon;

    layer = guilayercreate("res/shader/guivert.glsl",
            "res/shader/guifrag.glsl",
            "res/texture/gui.png");

    base = guielemcreate(FILEBAR_X, FILEBAR_Y + 0.001, FILEBAR_W, FILEBAR_H,
            GUI_CUSTOM, GUI_ATTR_HASTEXTURE, "filebar_base");
    
    base_right = guielemcreate(FILEBAR_BTN_W * (FILEBAR_NBTNS - 1), 0.0f,
            FILEBAR_BTN_W * 2, FILEBAR_BTN_H,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE | GUI_ATTR_NOCAPTURE,
            "filebar_base_right");

    new = guielemcreate(
            FILEBAR_BTN_W * 0, 0.0f, FILEBAR_BTN_W, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "filebar_new");

    open = guielemcreate(
            FILEBAR_BTN_W * 1, 0.0f, FILEBAR_BTN_W, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "filebar_open");
    
    save = guielemcreate(
            FILEBAR_BTN_W * 2, 0.0f, FILEBAR_BTN_W, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "filebar_save");

    new_icon = guielemcreate(0.1f, 0.1f, 0.8f, 0.8f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_NOCAPTURE,
            "filebar_new_icon");
    
    open_icon = guielemcreate(0.1f, 0.1f, 0.8f, 0.8f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_NOCAPTURE,
            "filebar_open_icon");

    save_icon = guielemcreate(0.1f, 0.1f, 0.8f, 0.8f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_NOCAPTURE,
            "filebar_save_icon");

    guilinklayer(base, layer);

    guiaddelem(base, base_right);
    guiaddelem(base, new);
    guiaddelem(base, open);
    guiaddelem(base, save);
    guiaddelem(new, new_icon);
    guiaddelem(open, open_icon);
    guiaddelem(save, save_icon);

    guisetbacktexture(base, 128, 8, 128, 128);
    guisetbacktexture(base_right, 128, 128 * 6, 256, 128);
    guienableratioresizing(base, 1.0f / FILEBAR_NBTNS);

    guisetbacktexture(new, 128, 320, 128, 128);
    guisetbacktexture(open, 128, 320, 128, 128);
    guisetbacktexture(save, 128, 320, 128, 128);
    
    guisetbacktexture(new_icon,  128 * 4, 128 * 2 + 64, 128, 128);
    guisetbacktexture(open_icon, 128 * 5, 128 * 2 + 64, 128, 128);
    guisetbacktexture(save_icon, 128 * 6, 128 * 2 + 64, 128, 128);

    guiseteventcallback(new, GUI_EVENT_MOUSEOVER, guibuttonmouseover);
    guiseteventcallback(open, GUI_EVENT_MOUSEOVER, guibuttonmouseover);
    guiseteventcallback(save, GUI_EVENT_MOUSEOVER, guibuttonmouseover);

    guiseteventcallback(new, GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(open, GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(save, GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);

    guiseteventcallback(new, GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(open, GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(save, GUI_EVENT_MOUSEUP, guibuttonmouseup);
    
    guiseteventcallback(new, GUI_EVENT_MOUSEDOWN, guibuttonfilenewdown);
    guiseteventcallback(open, GUI_EVENT_MOUSEDOWN, guibuttonfileopendown);
    guiseteventcallback(save, GUI_EVENT_MOUSEDOWN, guibuttonfilesavedown);
}

void
guipackbarcreate(void)
{
    GUILayer *layer;
    GUIElement *base, *base_left;
    GUIElement *open, *save;
    GUIElement *open_icon, *save_icon;
    
    layer = guilayercreate("res/shader/guivert.glsl",
            "res/shader/guifrag.glsl",
            "res/texture/gui.png");

    base = guielemcreate(PACKBAR_X + 0.001, PACKBAR_Y + 0.001, PACKBAR_W, PACKBAR_H,
            GUI_CUSTOM, GUI_ATTR_HASTEXTURE, "packbar_base");
    
    base_left = guielemcreate(0.0f - PACKBAR_BTN_W, 0.0f,
            PACKBAR_BTN_W * 2, PACKBAR_BTN_H,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE | GUI_ATTR_NOCAPTURE,
            "packbar_base_left");

    open = guielemcreate(
            PACKBAR_BTN_W * 0, 0.0f, PACKBAR_BTN_W, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "packbar_open");
    
    save = guielemcreate(
            PACKBAR_BTN_W * 1, 0.0f, PACKBAR_BTN_W, 1.0f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "packbar_save");

    open_icon = guielemcreate(0.1f, 0.1f, 0.8f, 0.8f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_NOCAPTURE,
            "packbar_open_icon");

    save_icon = guielemcreate(0.1f, 0.1f, 0.8f, 0.8f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_NOCAPTURE,
            "packbar_save_icon");

    guilinklayer(base, layer);
    guiaddelem(base, base_left);
    guiaddelem(base, open);
    guiaddelem(base, save);
    guiaddelem(open, open_icon);
    guiaddelem(save, save_icon);

    guisetbacktexture(base, 128, 8, 128, 128);
    guisetbacktexture(base_left, 0, 128 * 6, 256, 128);
    guienableratioresizing(base, 1.0f / PACKBAR_NBTNS);

    guisetbacktexture(open, 128, 320, 128, 128);
    guisetbacktexture(save, 128, 320, 128, 128);
    
    guisetbacktexture(open_icon, 128 * 7, 128 * 3 + 64, 128, 128);
    guisetbacktexture(save_icon, 128 * 7, 128 * 4 + 64, 128, 128);
    
    guiseteventcallback(open, GUI_EVENT_MOUSEOVER, guibuttonmouseover);
    guiseteventcallback(save, GUI_EVENT_MOUSEOVER, guibuttonmouseover);

    guiseteventcallback(open, GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(save, GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);

    guiseteventcallback(open, GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(save, GUI_EVENT_MOUSEUP, guibuttonmouseup);
    
    guiseteventcallback(open, GUI_EVENT_MOUSEDOWN, guibuttonpackopendown);
    guiseteventcallback(save, GUI_EVENT_MOUSEDOWN, guibuttonpacksavedown);

}

void
guitoolbarcreate(void) 
{
    uint i;
    GUILayer *layer;
    GUIElement *base, *base_left, *base_right;
    GUIElement *buttons[BUTTON_TOTAL];
    GUIElement *icons[BUTTON_TOTAL];

    layer = guilayercreate("res/shader/guivert.glsl",
            "res/shader/guifrag.glsl",
            "res/texture/gui.png");

    base = guielemcreate(TOOLBAR_X, TOOLBAR_Y, TOOLBAR_W, TOOLBAR_H,
            GUI_CUSTOM, GUI_ATTR_HASTEXTURE, "toolbar_base");
    
    base_left = guielemcreate(0.0f - TOOLBAR_BTN_W, 0.0f,
            TOOLBAR_BTN_W * 2, TOOLBAR_BTN_H,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE | GUI_ATTR_NOCAPTURE,
            "toolbar_base_left");

    base_right = guielemcreate(TOOLBAR_BTN_W * (BUTTON_TOTAL - 1), 0.0f,
            TOOLBAR_BTN_W * 2, TOOLBAR_BTN_H,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE | GUI_ATTR_NOCAPTURE,
            "toolbar_base_right");

    for (i = 0; i < BUTTON_TOTAL; i++) {
        buttons[i] = guielemcreate(
                TOOLBAR_BTN_W * i, 0.0f, TOOLBAR_BTN_W, 1.0f,
                GUI_CUSTOM,
                GUI_ATTR_HASTEXTURE |
                GUI_ATTR_CLICKABLE,
                toolbarnames[i]);
        icons[i] = guielemcreate(0.1f, 0.1f, 0.8f, 0.8f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_NOCAPTURE,
            toolbariconnames[i]);
    }
    
    guilinklayer(base, layer);
    guiaddelem(base, base_left);
    guiaddelem(base, base_right);

    for (i = 0; i < BUTTON_TOTAL; i++) {
        guiaddelem(base, buttons[i]);
    }
    guisetbacktexture(base, 128, 8, 128, 128);
    guisetbacktexture(base_left, 0, 896, 256, 128);
    guisetbacktexture(base_right, 128, 896, 256, 128);
    guienableratioresizing(base, 1.0f / BUTTON_TOTAL);

    for (i = 0; i < BUTTON_TOTAL; i++) {
        guiaddelem(buttons[i], icons[i]);
        guisetbacktexture(buttons[i], 128, 320, 128, 128);
        guiseteventcallback(buttons[i], GUI_EVENT_MOUSEOVER, guibuttonmouseover);
    }

    guisetbacktexture(icons[BUTTON_NOT],       2 * 128, 0 * 128 + 64, 128, 128);
    guisetbacktexture(icons[BUTTON_OR],        3 * 128, 0 * 128 + 64, 128, 128);
    guisetbacktexture(icons[BUTTON_AND],       4 * 128, 0 * 128 + 64, 128, 128);
    guisetbacktexture(icons[BUTTON_XOR],       5 * 128, 0 * 128 + 64, 128, 128);
    guisetbacktexture(icons[BUTTON_RED],       2 * 128, 1 * 128 + 64, 128, 128);
    guisetbacktexture(icons[BUTTON_GREEN],     3 * 128, 1 * 128 + 64, 128, 128);
    guisetbacktexture(icons[BUTTON_BLUE],      4 * 128, 1 * 128 + 64, 128, 128);
    guisetbacktexture(icons[BUTTON_INPUT],     6 * 128, 0 * 128 + 64, 128, 128);
    guisetbacktexture(icons[BUTTON_OUTPUT],    7 * 128, 0 * 128 + 64, 128, 128);
    guisetbacktexture(icons[BUTTON_CLOCK],     6 * 128, 1 * 128 + 64, 128, 128);
    guisetbacktexture(icons[BUTTON_MEMORY],    7 * 128, 1 * 128 + 64, 128, 128);
    guisetbacktexture(icons[BUTTON_CONNECTOR], 5 * 128, 1 * 128 + 64, 128, 128);

    guiseteventcallback(buttons[BUTTON_NOT], GUI_EVENT_MOUSEDOWN, guibuttonnotdown);
    guiseteventcallback(buttons[BUTTON_OR], GUI_EVENT_MOUSEDOWN, guibuttonordown);
    guiseteventcallback(buttons[BUTTON_AND], GUI_EVENT_MOUSEDOWN, guibuttonanddown);
    guiseteventcallback(buttons[BUTTON_XOR], GUI_EVENT_MOUSEDOWN, guibuttonxordown);
    guiseteventcallback(buttons[BUTTON_RED], GUI_EVENT_MOUSEDOWN, guibuttonreddown);
    guiseteventcallback(buttons[BUTTON_GREEN], GUI_EVENT_MOUSEDOWN, guibuttongreendown);
    guiseteventcallback(buttons[BUTTON_BLUE], GUI_EVENT_MOUSEDOWN, guibuttonbluedown);
    guiseteventcallback(buttons[BUTTON_INPUT], GUI_EVENT_MOUSEDOWN, guibuttonindown);
    guiseteventcallback(buttons[BUTTON_OUTPUT], GUI_EVENT_MOUSEDOWN, guibuttonoutdown);
    guiseteventcallback(buttons[BUTTON_CLOCK], GUI_EVENT_MOUSEDOWN, guibuttonclockdown);
    guiseteventcallback(buttons[BUTTON_MEMORY], GUI_EVENT_MOUSEDOWN, guibuttonmemorydown);
    guiseteventcallback(buttons[BUTTON_CONNECTOR], GUI_EVENT_MOUSEDOWN, guibuttonconnectordown);

    guiseteventcallback(buttons[BUTTON_NOT], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_OR], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_AND], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_XOR], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_RED], GUI_EVENT_MOUSEUP, guibuttonredup);
    guiseteventcallback(buttons[BUTTON_GREEN], GUI_EVENT_MOUSEUP, guibuttongreenup);
    guiseteventcallback(buttons[BUTTON_BLUE], GUI_EVENT_MOUSEUP, guibuttonblueup);
    guiseteventcallback(buttons[BUTTON_INPUT], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_OUTPUT], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_CLOCK], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_MEMORY], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_CONNECTOR], GUI_EVENT_MOUSEUP, guibuttonmouseup);

    guiseteventcallback(buttons[BUTTON_NOT], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_OR], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_AND], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_XOR], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_RED], GUI_EVENT_MOUSELEAVE, guibuttonredmouseleave);
    guiseteventcallback(buttons[BUTTON_GREEN], GUI_EVENT_MOUSELEAVE, guibuttongreenmouseleave);
    guiseteventcallback(buttons[BUTTON_BLUE], GUI_EVENT_MOUSELEAVE, guibuttonbluemouseleave);
    guiseteventcallback(buttons[BUTTON_INPUT], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_OUTPUT], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_CLOCK], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_MEMORY], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_CONNECTOR], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
}
