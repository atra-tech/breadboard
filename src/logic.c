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
#include "world.h"
#include "file.h"

static void packinit_not(Package *pack);
static void packinit_or(Package *pack);
static void packinit_and(Package *pack);
static void packinit_xor(Package *pack);

static uint vec2_hash(const void *);
static uint vec2_eq(const void *, const void *);

Wire *
wirecreate(void)
{
    Wire *wire;

    wire = malloc(sizeof(Wire));
    wire->active = 0;
    wire->ins = listcreate();
    wire->outs = listcreate();
    wire->segs = listcreate();
    wire->tiles = htablecreate(vec2_hash, vec2_eq);

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

Package *
packcreate(uint type, const char *path)
{
    Package *pack;

    pack = calloc(1, sizeof(Package));
    dlistinit(&pack->in_pins, sizeof(Pin));
    dlistinit(&pack->out_pins, sizeof(Pin));

    switch (type) {
    case PACK_CIRCUIT:
        packload(pack, path);
        break;
    case PACK_NOT:
        packinit_not(pack);
        break;
    case PACK_OR:
        packinit_or(pack);
        break;
    case PACK_AND:
        packinit_and(pack);
        break;
    case PACK_XOR:
        packinit_xor(pack);
        break;
    }

    return pack;
}

void
packdelete(Package *pack)
{
    uint i;

    if (pack->circuit) {
        //Free circuit
        //circuitdelete(pack->circuit);

    }

    for (i = 0; i < pack->in_pins.size; i++) {
        pindelete(dlistat(&pack->in_pins, i));

    }
}

void
packload(Package *pack, const char *path)
{
    /*  If file does not exist in hashtable
     *      Load file and add to hashtable
     *      Fetch loaded data
     *  Else
     *      Fetch matching hashtable entry
     *  Build new package from data
     *
     * */
}

void
wireset(Wire *wire, uint value)
{
    if (wire->active != value) {
        wire->active = value;
        wire->changed = 1;
    }
}

void
circuitdelete(Circuit *circuit)
{
    uint i;

    for (i = 0; i < circuit->packs->size; i++) {
        packdelete(listat(circuit->packs, i));
    }

    for (i = 0; i < circuit->wires->size; i++) {
        wiredelete(listat(circuit->wires, i));
    }
}

void
pininit(Pin *pin, int x, int y, const char *name)
{
    uint len;

    len = strlen(name) * sizeof(char);
    pin->x = x;
    pin->y = y;
    pin->state = 0;
    pin->name = malloc((len + 1) * sizeof(char));
    memcpy(pin->name, name, len + 1);
    pin->ins = listcreate();
    pin->outs = listcreate();
}

void
pindelete(Pin *pin)
{
    free(pin->name);
    listdelete(pin->ins);
    listdelete(pin->outs);
}

void
pingetsig(Pin *pin)
{
    uint i;
    Wire *wire;

    pin->state = 0;

    for (i = 0; i < pin->ins->size; i++) {
        wire = listat(pin->ins, i);
        pin->state += wire->active;
    }
}

void
pinputsig(Pin *pin)
{
    uint i;
    Wire *wire;

    for (i = 0; i < pin->outs->size; i++) {
        wire = listat(pin->outs, i);
        wireset(wire, pin->state);
    }
}

void
packfunc_not(Package *pack)
{
    Pin *in, *out;

    in = dlistat(&pack->in_pins, 0);
    out = dlistat(&pack->out_pins, 0);

    pingetsig(in);
    out->state = !in->state;
    pinputsig(out);
}

void
packfunc_or(Package *pack)
{
    Pin *a, *b, *out, *neg_out;

    a = dlistat(&pack->in_pins, 0);
    b = dlistat(&pack->in_pins, 1);
    out = dlistat(&pack->out_pins, 0);
    neg_out = dlistat(&pack->out_pins, 1);
   
    pingetsig(a);
    pingetsig(b);
    out->state = a->state + b->state;
    neg_out->state = !out->state;
    pinputsig(out);
    pinputsig(neg_out);
}

void
packfunc_and(Package *pack)
{
    Pin *a, *b, *out, *neg_out;
    
    a = dlistat(&pack->in_pins, 0);
    b = dlistat(&pack->in_pins, 1);
    out = dlistat(&pack->out_pins, 0);
    neg_out = dlistat(&pack->out_pins, 1);

    pingetsig(a);
    pingetsig(b);
    out->state = a->state * b->state;
    neg_out->state = !out->state;
    pinputsig(out);
    pinputsig(neg_out);
}

void
packfunc_xor(Package *pack)
{
    Pin *a, *b, *out, *neg_out;

    a = dlistat(&pack->in_pins, 0);
    b = dlistat(&pack->in_pins, 1);
    out = dlistat(&pack->out_pins, 0);
    neg_out = dlistat(&pack->out_pins, 1);
    
    pingetsig(a);
    pingetsig(b);
    out->state = (a->state != b->state);
    neg_out->state = !out->state;
    pinputsig(out);
    pinputsig(neg_out);
}

void
packinit_not(Package *pack)
{
    Pin pin;

    pack->w = GATE_NOT_W;
    pack->h = GATE_NOT_H;
    pack->func = packfunc_not;
    pack->name = malloc(4 * sizeof(char));
    memcpy(pack->name, "NOT", 4 * sizeof(char));

    pininit(&pin, -1, 0, "A");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(&pin, 1, 0, "!A");
    dlistpush(&pack->out_pins, &pin);
}

void
packinit_or(Package *pack)
{
    Pin pin;

    pack->w = GATE_OR_W;
    pack->h = GATE_OR_H;
    pack->func = packfunc_or;
    pack->name = malloc(3 * sizeof(char));
    memcpy(pack->name, "OR", 3 * sizeof(char));
    
    pininit(&pin, -1, 1, "A");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(&pin, -1, 0, "B");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(&pin, 1, 1, "Q");
    dlistpush(&pack->out_pins, &pin);

    pininit(&pin, 1, 0, "!Q");
    dlistpush(&pack->out_pins, &pin);
}

void
packinit_and(Package *pack)
{
    Pin pin;

    pack->w = GATE_AND_W;
    pack->h = GATE_AND_H;
    pack->func = packfunc_and;
    pack->name = malloc(4 * sizeof(char));
    memcpy(pack->name, "AND", 4 * sizeof(char));
    
    pininit(&pin, -1, 1, "A");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(&pin, -1, 0, "B");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(&pin, 2, 1, "Q");
    dlistpush(&pack->out_pins, &pin);

    pininit(&pin, 2, 0, "!Q");
    dlistpush(&pack->out_pins, &pin);
}

void
packinit_xor(Package *pack)
{
    Pin pin;

    pack->w = GATE_XOR_W;
    pack->h = GATE_XOR_H;
    pack->func = packfunc_xor;
    pack->name = malloc(4 * sizeof(char));
    memcpy(pack->name, "XOR", 4 * sizeof(char));
    
    pininit(&pin, -1, 1, "A");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(&pin, -1, 0, "B");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(&pin, 1, 1, "Q");
    dlistpush(&pack->out_pins, &pin);

    pininit(&pin, 1, 0, "!Q");
    dlistpush(&pack->out_pins, &pin);
}

uint
vec2_hash(const void *vec)
{
    const uint *data = vec;

    return (data[0] ^ data[1]) >> 2;
}

uint
vec2_eq(const void *vec_a, const void *vec_b)
{
    const uint *a = vec_a;
    const uint *b = vec_b;
    
    return (a[0] == b[0]) && (a[1] == b[1]);
}
