/*
 *  BREADBOARD (2020)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Structures and functions for the logical representation
 *  of circuit simulation.
 *
 *  */

#ifndef LOGIC_H
#define LOGIC_H

#include "util/list.h"
#include "util/htable.h"

#define GATE_NOT_W  2
#define GATE_NOT_H  1
#define GATE_OR_W   2
#define GATE_OR_H   2
#define GATE_AND_W  2
#define GATE_AND_H  2
#define GATE_XOR_W  2
#define GATE_XOR_H  2
#define PACK_IN_W   1
#define PACK_IN_H   1
#define PACK_OUT_W  1
#define PACK_OUT_H  1
#define PACK_CLK_W  2
#define PACK_CLK_H  1
#define PACK_MEM_W  3
#define PACK_MEM_H  19
#define PACK_CONNECT_W  1
#define PACK_CONNECT_H  1
#define WIRE_CONVERGE_LIMIT 100
#define WIRE_CONVERGE_EXTRA 110
#define DEFAULT_CLOCK_RATE  8

/*enum wire_flags {
    WIRE_ACTIVE  = BIT(0),
    WIRE_CHANGED = BIT(1)
};*/

enum pack_type {
    PACK_CIRCUIT,
    PACK_NOT,
    PACK_OR,
    PACK_AND,
    PACK_XOR,
    PACK_MEMORY,
    PACK_INPUT,
    PACK_OUTPUT,
    PACK_CLOCK,
    PACK_CONNECTOR
};

struct World;
struct Segment;

typedef struct TileData {
    float x, y;
    uint state;
} TileData;

typedef struct Wire {
    uint active;
    uint changed;
    uint conv;      /* Convergence counter */
    uint layer;
    uint sub;       /* Sub-circuit wire flag */
    List *ins;
    List *outs; 
    List *segs;
    HTable *tiles;   /* Related world-space tiles */
    DList tile_data; /* Data for rebuilding world from circuit */
} Wire;

typedef struct Memory {
    size_t size;
    uint *data;
} Memory;

typedef struct Circuit {
    List *wires;
    List *packs;
    uint error;
} Circuit;

typedef struct Package {
    uint x, y, w, h, seg_x, seg_y;
    uint type;
    uint clockrate; /* Ticks per second */
    float time;
    uint changed;
    uint error;
    uint state;     /* Used for input/output packs */
    void (*func)(struct Package *);
    Memory *memory;
    Circuit *circuit;
    struct Segment *seg;
    char *path;     /* Path of file of circuit template */
    char *name;
    DList in_pins;  /* Pin */
    DList out_pins; /* Pin */
} Package;

typedef struct Pin {
    int x, y;   /* Relative to Package origin */
    uint state;
    char *name;
    List *ins;  /* Wire */
    List *outs; /* Wire */
    Package *parent;
} Pin;

Wire * wirecreate(void);
void wiredelete(Wire *);
Wire * wirebuild(struct World *world, uint x, uint y, uint layer);
uint wireupdate(Wire *wire);

Circuit * circuitbuild(struct World *world, int sub);
void circuitdelete(Circuit *circuit);
Wire * circuitgetwireat(Circuit *circuit, uint x, uint y, uint layer);
void circuitclear(Circuit *circuit);
void circuitupdate(Circuit *circuit, float dt);
void circuitupdatefull(Circuit *circuit, float dt);
void circuitrebuild(struct World *world, Circuit **circuit);
void circuitclearerrors(Circuit *circuit);
void circuitresetwireconvs(Circuit *circuit);

Package * packcreate(uint type, const char *path);
void packdelete(Package *pack);
Package * packcopy(Package *pack);
uint packupdate(Package *pack, float dt);
void packbindcircuit(Package *pack, Circuit *circuit);

void pininit(Package *pack, Pin *pin, int x, int y, const char *name);
void pindelete(Pin *pin);
void pingetsig(Pin *pin);

void updateclocktimes(Circuit *circuit, float dt);
void schedulesegremeshing(Circuit *circuit);

void packfunc_not(Package *pack);
void packfunc_or(Package *pack);
void packfunc_and(Package *pack);
void packfunc_xor(Package *pack);
void packfunc_input(Package *pack);
void packfunc_output(Package *pack);
void packfunc_clock(Package *pack);
void packfunc_memory(Package *pack);
void packfunc_connector(Package *pack);

#endif /* LOGIC_H */
