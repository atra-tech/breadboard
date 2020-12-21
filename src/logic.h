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

enum wire_flags {
    WIRE_ACTIVE  = BIT(0),
    WIRE_CHANGED = BIT(1)
};

enum pin_type {
    PIN_IN,
    PIN_OUT
};

enum pack_type {
    PACK_CIRCUIT,
    PACK_NOT,
    PACK_OR,
    PACK_AND,
    PACK_XOR,
    PACK_MEMORY
};

typedef struct Wire {
    uint active;
    uint changed;
    List *ins;
    List *outs; 
    List *segs;     //Knows when to update
    HTable *tiles;  //Tiles it links to in world-space
} Wire;

typedef struct Memory {
    uint *data;
} Memory;

typedef struct Circuit {
    List *wires;
    List *packs;
} Circuit;

typedef struct Pin {
    int x, y;  /* Relative to Package origin */
    uint state;
    char *name;
    List *ins;  /* Wire */
    List *outs; /* Wire */
} Pin;

typedef struct Package {
    uint x, y, w, h;
    uint type;
    void (*func)(struct Package *);
    Memory *memory;
    Circuit *circuit;
    char *name;
    DList in_pins;  /* Pin */
    DList out_pins; /* Pin */
} Package;

Wire * wirecreate(void);
void wiredelete(Wire *);
void wireset(Wire *wire, uint value);

void circuitdelete(Circuit *circuit);

Package * packcreate(uint type, const char *path);
void packdelete(Package *pack);
void packload(Package *pack, const char *path);

void pininit(Pin *pin, int x, int y, const char *name);
void pindelete(Pin *pin);
void pingetsig(Pin *pin);
void pinputsig(Pin *pin);

void packfunc_not(Package *pack);
void packfunc_or(Package *pack);
void packfunc_and(Package *pack);
void packfunc_xor(Package *pack);

#endif
