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

#ifndef WORLD_H
#define WORLD_H

#include "util/util.h"
#include "render.h"
#include "logic.h"

#define WORLD_SIZE          1024
#define SEGMENT_SIZE        32
#define SEGMENTS_PER_ROW    WORLD_SIZE / SEGMENT_SIZE
#define SEGMENT_COUNT       SEGMENTS_PER_ROW * SEGMENTS_PER_ROW
#define LAYER_TILECOUNT     SEGMENT_SIZE * SEGMENT_SIZE
#define VIEW_DIST           4
#define TILE_WIREGAP        0.2f
#define WIRE_PERM_COUNT     16
#define PACK_MAX_SIZE       64
#define PIX_SIZE            0.015625f
#define FONT_GAP            0
#define PACK_FONT_SIZE      16
#define PIN_FONT_SIZE       12

enum layer_type {
    LAYER_RED,
    LAYER_GREEN,
    LAYER_BLUE,
    LAYER_TOTAL
};

enum tile_type {
    TILE_NONE,
    TILE_WIRE,
    TILE_PACK,
    TILE_PIN
};

enum tile_state {
    TILE_TOP     = BIT(0),
    TILE_BOT     = BIT(1),
    TILE_LEFT    = BIT(2),
    TILE_RIGHT   = BIT(3),
    TILE_CENTRE  = BIT(4),
    TILE_POWERED = BIT(5)
};

enum wire_type {
    WIRE_RED,
    WIRE_GREEN,
    WIRE_BLUE,
    WIRE_NONE,
    WIRE_TOTAL
};

enum mouse_mode {
    MOUSE_NONE,
    MOUSE_DRAW,
    MOUSE_ERASE,
    MOUSE_GRAB,
    MOUSE_DRAGBOX
};

enum pack_textures {
    PACKTEX_TOP_LEFT,
    PACKTEX_TOP,
    PACKTEX_TOP_RIGHT,
    PACKTEX_LEFT,
    PACKTEX_CENTRE,
    PACKTEX_RIGHT,
    PACKTEX_BOT_LEFT,
    PACKTEX_BOT,
    PACKTEX_BOT_RIGHT,
    PACKTEX_TOTAL
};

enum pin_textures {
    PINTEX_DOWN,
    PINTEX_LEFT,
    PINTEX_UP,
    PINTEX_RIGHT,
    PINTEX_TOTAL
};

typedef struct Tile {
    uint state;
} Tile;

typedef struct Layer {
    Tile tiles[LAYER_TILECOUNT];
} Layer;

typedef struct FreeSegment {
    float x, y;
    uint w, h;
    //VBuffer vbo;
    VBuffer wirevbo;
    VBuffer textvbo;
    VecList colors;
    VecList textcolors;
    mat4 modelmat;
    Tile *tiles[LAYER_TOTAL];   //Size based on layer dimensions
    List *packs;
} FreeSegment;

typedef struct Segment {
    int x, y, dirty;
    VBuffer vbo;
    VBuffer wirevbo;
    VBuffer textvbo;
    VecList colors;
    VecList textcolors;
    mat4 modelmat;
    Layer layers[LAYER_TOTAL];
    List *packs;
} Segment;

typedef struct World {
    Segment *segs[SEGMENT_COUNT];
} World;

typedef struct SelectBox {
    float x, y, w, h;
    int enabled;
} SelectBox;

void worldinit(World *world);
void worldpush(World *world);
World * worldtop(void);
void worldpop(void);
void worldhandle(World *world, Event *e);
void worldupdate(World *world, float dt);
void worldrender(World *world, Camera *cam);
void worldrendermouseseg(Camera *cam);
void worlddelete(World *world);

void worldsave(const char *path);
void worldload(const char *path);

//void worldaddpack(uint x, uint y, uint type, void *func);

Segment * worldgetsegment(World *world, uint x, uint y);
Segment * worldgetsegmentat(World *world, uint x, uint y);
Layer * worldgetlayerat(World *world, uint x, uint y, uint layer);
Tile * worldgettileat(World *world, uint x, uint y, uint layer);
Tile * segmentgettile(Segment *seg, uint x, uint y, uint layer);

#endif /* WORLD_H */
