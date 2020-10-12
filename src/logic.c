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

#include "logic.h"

Wire *
wirecreate(void)
{
    Wire *wire;

    wire = malloc(sizeof(Wire));
    wire->active = 0;
    wire->ins = listcreate();
    wire->outs = listcreate();
    wire->segs = listcreate();
    //wire->tiles = htablecreate(); /* Needs 2-Vector hash and equality functions */
    
    return wire;
}

void
wiredelete(Wire *wire)
{
    listdelete(wire->ins);
    listdelete(wire->outs);
    listdelete(wire->segs);
    htabledelete(wire->tiles);
}
