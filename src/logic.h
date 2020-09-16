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

struct Circuit;

typedef struct Wire {
    uint active;
    List *ins;
    List *outs;
} Wire;

typedef struct Package {
    struct Circuit *circuit;
    List *ins;
    List *outs;
} Package;

typedef struct Circuit {
    List *wires;
    List *packs;
} Circuit;

#endif
