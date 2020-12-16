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
#include "gui.h"
#include "file.h"
#include "util/log.h"
#include "util/stack.h"

#define COPY_VEC(dst, src, row, size) \
    memcpy(dst, src + row * size, size * sizeof(float))
#define SET_INDEX(dst, icount, i1, i2, i3) \
    dst[0] = icount + i1; \
    dst[1] = icount + i2; \
    dst[2] = icount + i3

static void worldstart(void);
static void worldshutdown(void);
static void segmentinit(Segment *seg, uint x, uint y);
static void segmentdelete(Segment *seg);
static void segmentmesh(Segment *seg);
static void freeseginit(FreeSegment *seg, float x, float y, uint w, uint h);
static void freesegdelete(FreeSegment *seg);
static void freesegmesh(FreeSegment *seg);
static FreeSegment * freesegbuild(uint x, uint y, uint w, uint h, uint clearsrc);
static void freesegmerge(FreeSegment *seg, uint x, uint y);
static void tilehandlemouse(Tile *tile, float mx, float my);
static void layermesh(Segment *seg, Layer *layer, const float *color, uint depth);
static void wirecalcuvs(void);
static void wiremovetotop(uint wire);
static void guitoolbarcreate(void);
static void fullremesh(void);
static void selboxinit(SelectBox *box);
static void selboxmesh(SelectBox *box);
static void selboxdelete(SelectBox *box);
static void drawtext(VBuffer *vbo, VecList *colors, const char *text,
        uint fontsize, float x_min, float x_max, float y_min, float y_max);
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

static const float colorwhite[] = {1.0f, 1.0f, 1.0f};
static const float colorred[]   = {1.0f, 0.2f, 0.2f};
static const float colorgreen[] = {0.4f, 1.0f, 0.2f};
static const float colorblue[]  = {0.2f, 0.4f, 1.0f};
static const float *colors[] = {colorred, colorgreen, colorblue};
static const float packtextcolor[] = {0.83f, 0.80f, 0.80f};
static const float selectboxcolor[] = {1.0f, 0.7f, 0.0f, 1.0f};

static int init = 0;
static Stack *worldstack;
static World *mainworld;
static FreeSegment *mouseseg;
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
    fontfind(&fonttex, "res/font/maledict.png", 8, 8);

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

    /* Populate pin package texture UVs */
    for (i = 0; i < PINTEX_TOTAL; i++) {
        texget(&circuittex, i, 3, pinuvs + i * 8);
    }
    worldstack = stackcreate();
    filesysinit();
    selboxinit(&selbox);
    guitoolbarcreate();
    init = 1;

    /*
    FreeSegment *seg = calloc(1, sizeof(FreeSegment));
    mousemode = MOUSE_GRAB;
    freeseginit(seg, 0, 0, 4, 4);
    mouseseg = seg;
    listpushback(seg->packs, packcreate(PACK_AND, NULL));
    freesegmesh(seg);*/
}

void
worldshutdown(void)
{
    if (mouseseg) {
        freesegdelete(mouseseg);
    }
    filesysshutdown();
    stackdelete(worldstack);
    shaderdelete(&worldshader);
    shaderdelete(&bgshader);
    shaderdelete(&indicatorshader);
    selboxdelete(&selbox);
    init = 0;
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
    worldpush(world);
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
    vbufinit(&seg->textvbo);
    veclistinit(&seg->colors, 3);
    veclistinit(&seg->textcolors, 3);

    glm_mat4_identity(seg->modelmat);
    glm_translate_x(seg->modelmat, x * SEGMENT_SIZE);
    glm_translate_y(seg->modelmat, y * SEGMENT_SIZE);

    for (i = 0; i < LAYER_TOTAL; i++) {
        memset(seg->layers[i].tiles, 0, LAYER_TILECOUNT * sizeof(Tile));
    }

    listpushback(seg->packs, packcreate(PACK_AND, NULL));
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

void
selboxdelete(SelectBox *box)
{
    vbufdelete(&box->vbo);
}

void
worldhandle(World *world, Event *e)
{
    static int prev_mx = 0, prev_my = 0;
    int w, h, mx, my, diff_mx, diff_my;
    float mdist, tmp_mx, tmp_my, wmx, wmy;
    uint mstate;
    Camera *cam;
    Tile *tile;
    Segment *seg;

    mstate = mousegetstate(&mx, &my);
    cam = getmaincamera();

    /*  TODO:   Any interaction with anything other than the currently active
     *          FreeSegment selection will rebind the contained elements to
     *          the world and delete it. */
    
    wmx = cam->ent.x + (mx - (cam->width / 2.0f)) / (cam->zoom / 2.0f);
    wmy = cam->ent.y + ((cam->height - my) - (cam->height / 2.0f)) / (cam->zoom / 2.0f);

    if (e->type == SDL_MOUSEBUTTONDOWN) {
        if (e->button.button == SDL_BUTTON_LEFT) {
            //  Possible paths:
            //      1.  Player does not drag mouse, leading to single IC or select-box selection check
            //      2.  Player drags mouse activating select-box mode.
            //      3.  Player clicks on open area with wires enabled, drawing wires on tiles.


            //TODO: Check for selection box under cursor
            //Check for IC pack under cursor
            //If either are true, skip the following mousemode changes

            if (currentwire != WIRE_NONE) {
                mousemode = MOUSE_DRAW;
            } else if (mousemode == MOUSE_GRAB) {
                /* TODO: Ensure segment is within bounds */
                freesegmerge(mouseseg, wmx - 0.5f, wmy - 0.5f); 
            } else {
                mousemode = MOUSE_DRAGBOX;
                selbox.x = wmx + 0.5f;
                selbox.y = wmy + 0.5f;
                selbox.w = 0;
                selbox.h = 0;
                selbox.enabled = 1;
                selboxmesh(&selbox);
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
            } else if (mousemode == MOUSE_DRAGBOX) {
                mousemode = MOUSE_NONE;
                selbox.enabled = 0;
            }
        } else if (e->button.button == SDL_BUTTON_RIGHT) {
            if (mousemode == MOUSE_ERASE) {
                mousemode = MOUSE_NONE;
            }
        }
    } else if (e->type == SDL_MOUSEMOTION) {
        if (mousemode == MOUSE_DRAGBOX) {
            selbox.w = wmx - selbox.x;
            selbox.h = wmy - selbox.y;

            selboxmesh(&selbox);
        }
    }


    if (!(mstate & MOUSE_BUTTON(LEFT)) && mousemode == MOUSE_DRAW) {
        mousemode = MOUSE_NONE;
    } else if (!(mstate & MOUSE_BUTTON(RIGHT)) && mousemode == MOUSE_ERASE) {
        mousemode = MOUSE_NONE;
    } else if (!(mstate & MOUSE_BUTTON(LEFT)) && mousemode == MOUSE_DRAGBOX) {
        mousemode = MOUSE_NONE;
        selbox.enabled = 0;
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
    } else if (mousemode == MOUSE_GRAB) {
        my = cam->height - my;
        tmp_mx = cam->ent.x + (mx - (cam->width / 2.0)) / (cam->zoom / 2.0);
        tmp_my = cam->ent.y + (my - (cam->height / 2.0)) / (cam->zoom / 2.0);

        glm_mat4_identity(mouseseg->modelmat);
        glm_translate_x(mouseseg->modelmat, tmp_mx - 1.0f);
        glm_translate_y(mouseseg->modelmat, tmp_my - 1.0f);
    }
    prev_mx = mx;
    prev_my = my;
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

    /* Render background */
    texsetactivetexture(0);
    texbind(&bgtex);
    shaderbind(&bgshader);
    shaderloadprojviewmat(&bgshader, cam->projviewmat);
    shaderloadviewmat(&bgshader, cam->viewmat);
    shaderaddint(&bgshader, "albedo_map", 0);

    //glEnable(GL_DEPTH_TEST);
    //glEnable(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    /* TODO: Change segment render distance to be based on camera zoom 
     * TODO: Rework to use common background VBuffer, since this data is identical for each seg
     * */
    for (i = seg_x - VIEW_DIST; i <= seg_x + VIEW_DIST; i++) {
        for (j = seg_y - VIEW_DIST; j <= seg_y + VIEW_DIST; j++) {
            if (i < 0 || i > SEGMENTS_PER_ROW - 1 ||
                    j < 0 || j > SEGMENTS_PER_ROW - 1) {
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
            seg = worldgetsegment(world, i, j);

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
            seg = worldgetsegment(world, i, j);

            vbufbindvao(&seg->textvbo);
            shaderloadmodelmat(&bgshader, seg->modelmat);
            glDrawElements(GL_TRIANGLES,
                    seg->wirevbo.indxcount, GL_UNSIGNED_INT, NULL);
        }
    }

    /* Render selection box */
    if (selbox.enabled) {
        shaderbind(&indicatorshader);
        vbufbindvao(&selbox.vbo);
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
        glDisable(GL_CULL_FACE);
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
/*
void
worldaddpack(uint x, uint y, uint type, void *func)
{
    Segment *seg;
    

    seg = worldgetsegmentat(mainworld, x, y);

    if (type == PACK_CIRCUIT) {
    } else {
        //Fixed function
    }
}

*/
/* Populate segment VBO and prepare for rendering 
 * TODO: Convert to thread worker job */
void
segmentmesh(Segment *seg)
{
    uint l, x, y, i, j, idxcount, len;
    float x_min, x_max, y_min, y_max;
    vec3 vert, norm;
    vec2 uv;
    ivec3 index;
    Package *pack;
    Pin *pin;
    float *uvptr;
    VBuffer *vbo;

    /* Draw background */
    idxcount = 0;
    vbo = &seg->vbo;

    vbufreset(&seg->vbo);
    vbufreset(&seg->wirevbo);
    vbufreset(&seg->textvbo);
    veclistclear(&seg->colors);
    veclistclear(&seg->textcolors);
    
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
    
    vbufbind(vbo);

    /* Draw wires */
    layermesh(seg, &seg->layers[wireorder[2]], colors[wireorder[2]], 1);
    layermesh(seg, &seg->layers[wireorder[1]], colors[wireorder[1]], 2);
    layermesh(seg, &seg->layers[wireorder[0]], colors[wireorder[0]], 3);
    
    vbo = &seg->wirevbo;
    idxcount = vbo->indxcount / 3;
    
    /* Draw packages */
    for (l = 0; l < seg->packs->size; l++) {

        pack = listat(seg->packs, l);

        /* Draw body */
        for (y = 0; y < pack->h; y++) {
            for (x = 0; x < pack->w; x++) {

                /* TODO: Find flat, branchless means of doing this */ 
                if (x == 0) {
                    if (y == 0) {
                        //  BOTTOM-LEFT
                        uvptr = packuvs + 6 * 8;
                    } else if (y == pack->h - 1) {
                        //  TOP-LEFT
                        uvptr = packuvs + 0 * 8;
                    } else {
                        //  LEFT
                        uvptr = packuvs + 3 * 8;
                    }
                } else if (x == pack->w - 1) {
                    if (y == 0) {
                        //  BOTTOM-RIGHT
                        uvptr = packuvs + 8 * 8;
                    } else if (y == pack->h - 1) {
                        //  TOP-RIGHT
                        uvptr = packuvs + 2 * 8;
                    } else {
                        //  RIGHT
                        uvptr = packuvs + 5 * 8;
                    }
                } else {
                    if (y == 0) {
                        //  BOTTOM
                        uvptr = packuvs + 7 * 8;
                    } else if (y == pack->h - 1) {
                        //  TOP
                        uvptr = packuvs + 1 * 8;
                    } else {
                        //  CENTRE
                        uvptr = packuvs + 4 * 8;
                    }
                }

                for (i = 0; i < 4; i++) {
                    vert[0] = x + tileverts[i * 3 + 0];
                    vert[1] = y + tileverts[i * 3 + 1];
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
            seg->vbo.indxcount += 12;
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
        vbufbind(vbo);
        vbufbindvbo(vbo, &seg->colors);

        /* Draw text */
        vbo = &seg->textvbo;
        idxcount = vbo->indxcount / 3;

        /* Body text */
        len = strlen(pack->name);
        x_min = (pack->x + (pack->w * 0.5f)) - 
            ((PACK_FONT_SIZE + FONT_GAP) * len - FONT_GAP) * PIX_SIZE * 0.5f;
        y_min = (pack->y + (pack->h * 0.5f)) -
            PACK_FONT_SIZE * PIX_SIZE * 0.5f;
        x_max = x_min + PACK_FONT_SIZE * PIX_SIZE;
        y_max = y_min + PACK_FONT_SIZE * PIX_SIZE;

        drawtext(vbo, &seg->textcolors, pack->name,
                PACK_FONT_SIZE, x_min, x_max, y_min, y_max);
                
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
                    PIN_FONT_SIZE, x_min, x_max, y_min, y_max);

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
                    PIN_FONT_SIZE, x_min, x_max, y_min, y_max);

            y_min -= 1.0f;
            y_max -= 1.0f;
        }
        vbufbind(vbo);
        vbufbindvbo(vbo, &seg->textcolors);
    }
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
    uint l, x, y, i, j, tile, tilenib, idxcount, color, len;
    float x_min, x_max, y_min, y_max;
    vec3 vert, norm = {0.0f, 0.0f, 1.0f};
    vec2 uv;
    ivec3 index;
    Package *pack;
    Pin *pin;
    float *uvptr;
    VBuffer *vbo;

    vbo = &seg->wirevbo;
    idxcount = vbo->indxcount / 3;
    
    vbufreset(&seg->wirevbo);
    vbufreset(&seg->textvbo);
    veclistclear(&seg->colors);
    veclistclear(&seg->textcolors);

    /* Draw wires */
    for (l = 0; l < LAYER_TOTAL; l++) {
        color = wireorder[LAYER_TOTAL - 1 - l];
        vert[2] = l;
 
        for (x = 0; x < seg->w; x++) {
            for (y = 0; y < seg->h; y++) {
                tile = seg->tiles[l][x * y].state & 0x1F;

                if (!tile) continue;

                if (tile == TILE_CENTRE) {
                    for (i = 0; i < 4; i++) {
                        vert[0] = x + tileverts[i * 3 + 0];
                        vert[1] = y + tileverts[i * 3 + 1];
                        uv[0] = wireuvs[0][i * 2 + 0];
                        uv[1] = wireuvs[0][i * 2 + 1];
                        vbufaddvertex(vbo, vert, uv, norm);
                        veclistpush(&seg->colors, colors[color]);
                    }
                    SET_INDEX(index, idxcount, 0, 1, 2);
                    vbufaddindex(vbo, index);
                    SET_INDEX(index, idxcount, 2, 3, 0);
                    vbufaddindex(vbo, index);

                    idxcount += 4;
                    seg->wirevbo.indxcount += 12;
                } else {
                    tilenib = tile & 0xF;

                    for (i = 0; i < 4; i++) {
                        vert[0] = x + tileverts[i * 3 + 0];
                        vert[1] = y + tileverts[i * 3 + 1];
                        uv[0] = wireuvs[tilenib][i * 2 + 0];
                        uv[1] = wireuvs[tilenib][i * 2 + 1];
                        vbufaddvertex(vbo, vert, uv, norm);
                        veclistpush(&seg->colors, colors[color]);
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

        pack = listat(seg->packs, l);

        /* Draw body */
        for (y = 0; y < pack->w; y++) {
            for (x = 0; x < pack->h; x++) {

                /* TODO: Find flat, branchless means of doing this
                 *       At least use pointer arithmetic, memcpy here is bad. */
                if (x == 0) {
                    if (y == 0) {
                        //  BOTTOM-LEFT
                        uvptr = packuvs + 6 * 8;
                    } else if (y == pack->h - 1) {
                        //  TOP-LEFT
                        uvptr = packuvs + 0 * 8;
                    } else {
                        //  LEFT
                        uvptr = packuvs + 3 * 8;
                    }
                } else if (x == pack->w - 1) {
                    if (y == 0) {
                        //  BOTTOM-RIGHT
                        uvptr = packuvs + 8 * 8;
                    } else if (y == pack->h - 1) {
                        //  TOP-RIGHT
                        uvptr = packuvs + 2 * 8;
                    } else {
                        //  RIGHT
                        uvptr = packuvs + 5 * 8;
                    }
                } else {
                    if (y == 0) {
                        //  BOTTOM
                        uvptr = packuvs + 7 * 8;
                    } else if (y == pack->h - 1) {
                        //  TOP
                        uvptr = packuvs + 1 * 8;
                    } else {
                        //  CENTRE
                        uvptr = packuvs + 4 * 8;
                    }
                }

                
                for (i = 0; i < 4; i++) {
                    vert[0] = x + tileverts[i * 3 + 0];
                    vert[1] = y + tileverts[i * 3 + 1];
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
        vbufbind(vbo);
        vbufbindvbo(vbo, &seg->colors);

        /* Draw text */
        vbo = &seg->textvbo;
        idxcount = vbo->indxcount / 3;

        /* Body text */
        len = strlen(pack->name);
        x_min = (pack->x + (pack->w * 0.5f)) - 
            ((PACK_FONT_SIZE + FONT_GAP) * len - FONT_GAP) * PIX_SIZE * 0.5f;
        y_min = (pack->y + (pack->h * 0.5f)) -
            PACK_FONT_SIZE * PIX_SIZE * 0.5f;
        x_max = x_min + PACK_FONT_SIZE * PIX_SIZE;
        y_max = y_min + PACK_FONT_SIZE * PIX_SIZE;

        drawtext(vbo, &seg->textcolors, pack->name,
                PACK_FONT_SIZE, x_min, x_max, y_min, y_max);
                
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
                    PIN_FONT_SIZE, x_min, x_max, y_min, y_max);

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
                    PIN_FONT_SIZE, x_min, x_max, y_min, y_max);

            y_min -= 1.0f;
            y_max -= 1.0f;
        }
        vbufbind(vbo);
        vbufbindvbo(vbo, &seg->textcolors);
    }
}

void
drawtext(VBuffer *vbo, VecList *colors, const char *text, uint fontsize,
        float x_min, float x_max, float y_min, float y_max)
{
    uint i, len, idxcount;
    vec3 vert, norm = {0.0f, 0.0f, 1.0f};
    vec2 uv;
    ivec3 index;
    uchar chr;

    idxcount = vbo->indxcount / 3;
    len = strlen(text);
    vert[2] = 0.0f;

    for (i = 0; i < len; i++) {
        chr = text[i];

        vert[0] = x_max;
        vert[1] = y_min;
        uv[0] = fonttex.uvs[chr][0];
        uv[1] = fonttex.uvs[chr][1];
        vbufaddvertex(vbo, vert, uv, norm);
        veclistpush(colors, packtextcolor);

        vert[0] = x_min;
        vert[1] = y_min;
        uv[0] = fonttex.uvs[chr][2];
        uv[1] = fonttex.uvs[chr][3];
        vbufaddvertex(vbo, vert, uv, norm);
        veclistpush(colors, packtextcolor);

        vert[0] = x_min;
        vert[1] = y_max;
        uv[0] = fonttex.uvs[chr][4];
        uv[1] = fonttex.uvs[chr][5];
        vbufaddvertex(vbo, vert, uv, norm);
        veclistpush(colors, packtextcolor);

        vert[0] = x_max;
        vert[1] = y_max;
        uv[0] = fonttex.uvs[chr][6];
        uv[1] = fonttex.uvs[chr][7];
        vbufaddvertex(vbo, vert, uv, norm);
        veclistpush(colors, packtextcolor);

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
    vert[2] = 0.0f;

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
/*
void
vbopushquad(VBuffer *vbo, float x_min, float y_min,
        float x_max, float y_max, float depth, float uvs[8])
{
    vec3 vert;
    vec2 uv;
    vec3 norm = {0.0f, 0.0f, 1.0f};
    ivec3 index;
    int idxcount = vbo->indxcount / 3;

    vert[2] = depth;

    vert[0] = x_max;
    vert[1] = y_min;
    uv[0] = uvs[0];
    uv[1] = uvs[1];
    vbufaddvertex(vbo, vert, uv, norm);

    vert[0] = x_min;
    vert[1] = y_min;
    uv[0] = uvs[2];
    uv[1] = uvs[3];
    vbufaddvertex(vbo, vert, uv, norm);

    vert[0] = x_min;
    vert[1] = y_max;
    uv[0] = uvs[4];
    uv[1] = uvs[5];
    vbufaddvertex(vbo, vert, uv, norm);

    vert[0] = x_max;
    vert[1] = y_max;
    uv[0] = uvs[6];
    uv[1] = uvs[7];
    vbufaddvertex(vbo, vert, uv, norm);

    index[0] = idxcount + 0;
    index[1] = idxcount + 1;
    index[2] = idxcount + 2;
    vbufaddindex(vbo, index);

    index[0] = idxcount + 2;
    index[1] = idxcount + 3;
    index[2] = idxcount + 0;
    vbufaddindex(vbo, index);

    vbo->indxcount += 12;
}
    
void
vbopushquadinverted(VBuffer *vbo, float x_min, float y_min,
        float x_max, float y_max, float depth, float uvs[8])
{
    vec3 vert;
    vec2 uv;
    vec3 norm = {0, 0, 1};
    ivec3 index;
    int indexbase = vbo->indxcount / 3;

    vert[2] = depth;

    vert[1] = x_max;
    vert[0] = y_min;
    uv[0] = uvs[0];
    uv[1] = uvs[1];
    vbufaddvertex(vbo, vert, uv, norm);

    vert[1] = x_min;
    vert[0] = y_min;
    uv[0] = uvs[2];
    uv[1] = uvs[3];
    vbufaddvertex(vbo, vert, uv, norm);

    vert[1] = x_min;
    vert[0] = y_max;
    uv[0] = uvs[4];
    uv[1] = uvs[5];
    vbufaddvertex(vbo, vert, uv, norm);

    vert[1] = x_max;
    vert[0] = y_max;
    uv[0] = uvs[6];
    uv[1] = uvs[7];
    vbufaddvertex(vbo, vert, uv, norm);

    index[0] = indexbase + 0;
    index[1] = indexbase + 1;
    index[2] = indexbase + 2;
    vbufaddindex(vbo, index);

    index[0] = indexbase + 2;
    index[1] = indexbase + 3;
    index[2] = indexbase + 0;
    vbufaddindex(vbo, index);

    vbo->indxcount += 12;
}
*/

void
fullremesh(void)
{
    uint i;

    for (i = 0; i < SEGMENT_COUNT; i++) {
        mainworld->segs[i]->dirty = 1;
    }
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
    uint tile;

    seg = malloc(sizeof(FreeSegment));
    freeseginit(seg, 0.0f, 0.0f, w, h);
    
    /* Map wire tiles */
    for (l = 0; l < LAYER_TOTAL; l++) {
        for (j = 0; j < h; j++) {
            for (i = 0; i < w; i++) {
                tile = worldgettileat(mainworld, x + i, y + j, l)->state;
                seg->tiles[l][j * w + i].state = tile;
            }
        }
    }
    
    /* Map packages */
    for (i = x / SEGMENT_SIZE; i < (x + w) / SEGMENT_SIZE; i++) {
        for (j = y / SEGMENT_SIZE; j < (y + h) / SEGMENT_SIZE; j++) {
            tmpseg = worldgetsegment(mainworld, i, j);
            
            for (l = 0; l < tmpseg->packs->size; l++) {
                //listpushback(seg->packs, listat(tmpseg->packs, l));
                //Check intersection
                //  Add to list if intersecting pack
                pack = listat(tmpseg->packs, l);
                wx = pack->x + tmpseg->x * SEGMENT_SIZE;
                wy = pack->y + tmpseg->y * SEGMENT_SIZE;
               
                /* Selection counts only when pack is entirely inside bounds */
                if (x < wx && x + w > wx + pack->w &&
                    y < wy && y + h > wy + pack->h) {
                    pack->x = wx - x;
                    pack->y = wy - y;
                    listpushback(seg->packs, pack);

                    if (clearsrc) {
                        listremove(tmpseg->packs, l);
                    }
                }
            }
        }
    }

    return seg;
}

/* Copy free segment data into the currently active world */
void
freesegmerge(FreeSegment *seg, uint x, uint y)
{
    uint l, i, j, wx, wy;
    uint tile;
    Package *pack;
    Segment *tmpseg;

    for (l = 0; l < LAYER_TOTAL; l++) {
        for (j = 0; j < seg->h; j++) {
            for (i = 0; i < seg->w; i++) {
                tile = seg->tiles[l][j * seg->w + i].state;
                worldgettileat(mainworld, x + i, y + j, l)->state = tile;
            }
        }
    }

    for (l = 0; l < seg->packs->size; l++) {
        pack = listat(seg->packs, l);
        wx = x + pack->x;
        wy = y + pack->y;
        tmpseg = worldgetsegmentat(mainworld, wx, wy);
        
        pack->x = wx;
        pack->y = wy;
        listpushback(tmpseg->packs, pack);
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
    Camera *cam;
    FreeSegment *seg;
    int mx, my;

    guisetbacktexture(elem, 384, 0, 64, 64);

    if (mousemode == MOUSE_NONE) {
        mousegetstate(&mx, &my);
        cam = getmaincamera();
        mx = cam->ent.x + (mx - (cam->width / 2.0)) / (cam->zoom / 2.0);
        my = cam->ent.y + (my - (cam->height / 2.0)) / (cam->zoom / 2.0);
        seg = malloc(sizeof(FreeSegment));
        freeseginit(seg, mx, my, GATE_NOT_W, GATE_NOT_H);
        mouseseg = seg;
    }
    elem->layer->dirty = 1;
}

void
guibuttonordown(GUIElement *elem, ...)
{
    Camera *cam;
    FreeSegment *seg;
    int mx, my;

    guisetbacktexture(elem, 384, 0, 64, 64);

    if (mousemode == MOUSE_NONE) {
        mousegetstate(&mx, &my);
        cam = getmaincamera();
        mx = cam->ent.x + (mx - (cam->width / 2.0)) / (cam->zoom / 2.0);
        my = cam->ent.y + (my - (cam->height / 2.0)) / (cam->zoom / 2.0);
        seg = malloc(sizeof(FreeSegment));
        freeseginit(seg, mx, my, GATE_OR_W, GATE_OR_H);
        mouseseg = seg;
    }
    elem->layer->dirty = 1;
}

void
guibuttonanddown(GUIElement *elem, ...)
{   
    Camera *cam;
    FreeSegment *seg;
    int mx, my;

    guisetbacktexture(elem, 384, 0, 64, 64);

    if (mousemode == MOUSE_NONE) {
        mousegetstate(&mx, &my);
        cam = getmaincamera();
        mx = cam->ent.x + (mx - (cam->width / 2.0)) / (cam->zoom / 2.0);
        my = cam->ent.y + (my - (cam->height / 2.0)) / (cam->zoom / 2.0);
        seg = malloc(sizeof(FreeSegment));
        freeseginit(seg, mx, my, GATE_AND_W, GATE_AND_H);
        mouseseg = seg;
    }
    elem->layer->dirty = 1;
}

void
guibuttonxordown(GUIElement *elem, ...)
{    Camera *cam;
    FreeSegment *seg;
    int mx, my;

    guisetbacktexture(elem, 384, 0, 64, 64);

    if (mousemode == MOUSE_NONE) {
        mousegetstate(&mx, &my);
        cam = getmaincamera();
        mx = cam->ent.x + (mx - (cam->width / 2.0)) / (cam->zoom / 2.0);
        my = cam->ent.y + (my - (cam->height / 2.0)) / (cam->zoom / 2.0);
        seg = malloc(sizeof(FreeSegment));
        freeseginit(seg, mx, my, GATE_XOR_W, GATE_XOR_H);
        mouseseg = seg;
    }
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
    guiseteventcallback(buttons[BUTTON_RED], GUI_EVENT_MOUSEDOWN, guibuttonreddown);
    guiseteventcallback(buttons[BUTTON_GREEN], GUI_EVENT_MOUSEDOWN, guibuttongreendown);
    guiseteventcallback(buttons[BUTTON_BLUE], GUI_EVENT_MOUSEDOWN, guibuttonbluedown);

    guiseteventcallback(buttons[BUTTON_NOT], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_OR], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_AND], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_XOR], GUI_EVENT_MOUSEUP, guibuttonmouseup);
    guiseteventcallback(buttons[BUTTON_RED], GUI_EVENT_MOUSEUP, guibuttonredup);
    guiseteventcallback(buttons[BUTTON_GREEN], GUI_EVENT_MOUSEUP, guibuttongreenup);
    guiseteventcallback(buttons[BUTTON_BLUE], GUI_EVENT_MOUSEUP, guibuttonblueup);

    guiseteventcallback(buttons[BUTTON_NOT], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_OR], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_AND], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_XOR], GUI_EVENT_MOUSELEAVE, guibuttonmouseleave);
    guiseteventcallback(buttons[BUTTON_RED], GUI_EVENT_MOUSELEAVE, guibuttonredmouseleave);
    guiseteventcallback(buttons[BUTTON_GREEN], GUI_EVENT_MOUSELEAVE, guibuttongreenmouseleave);
    guiseteventcallback(buttons[BUTTON_BLUE], GUI_EVENT_MOUSELEAVE, guibuttonbluemouseleave);
}
