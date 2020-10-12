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

struct Circuit;

typedef struct Wire {
    uint active;
    uint changed;
    List *ins;
    List *outs;
    List *segs;
    HTable *tiles;
} Wire;

typedef struct Package {
    struct Circuit *circuit;
    List *ins;
    List *outs;
} Package;

typedef struct SpecialPack {
    void (*packfunc)(struct SpecialPack *, List *, List *);
    List *ins;
    List *outs;
} SpecialPack;

typedef struct Circuit {
    List *wires;
    List *packs;
    List *specpacks;
} Circuit;


Wire * wirecreate(void);
void wiredelete(Wire *);

#endif
