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

enum layer_type {
    LAYER_PACK,
    LAYER_RED,
    LAYER_GREEN,
    LAYER_BLUE,
    LAYER_TOTAL
};

enum tile_section {
    SIDE_CENTER,
    SIDE_TOP,
    SIDE_BOT,
    SIDE_LEFT,
    SIDE_RIGHT,
    SIDE_TOTAL
};

enum tile_type {
    TILE_NONE,
    TILE_WIRE,
    TILE_PACK,
    TILE_PIN
};

typedef struct Tile {
    uint type[SIDE_TOTAL];
} Tile;

typedef struct Layer {
    Tile tiles[LAYER_TILECOUNT];
} Layer;

typedef struct FreeSegment {
    float x, y, w, h;
    VBuffer vbo;
    mat4 modelmat;
    Tile *tiles[LAYER_TOTAL];    //Size based on layer dimensions
    List *packs;
} FreeSegment;

typedef struct Segment {
    int x, y, dirty;
    VBuffer vbo;
    mat4 modelmat;
    Layer layers[LAYER_TOTAL];
    List *packs;
} Segment;

typedef struct World {
    Segment segs[SEGMENT_COUNT];
} World;

void worldinit(World *world);
void worldhandle(World *world, Event *e);
void worldupdate(World *world, float dt);
void worldrender(World *world, Camera *cam);

void worldsave(const char *path);
void worldload(const char *path);

Segment * worldgetsegment(World *world, uint x, uint y);
Segment * worldgetsegmentat(World *world, uint x, uint y);
Layer * worldgetlayerat(World *world, uint x, uint y, uint layer);
Tile * worldgettileat(World *world, uint x, uint y, uint layer);
Tile * segmentgettile(Segment *seg, uint x, uint y, uint layer);

#endif /* WORLD_H */
