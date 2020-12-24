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

typedef int ivec2[2];

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
    wire->changed = 0;
    wire->layer = 0;
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
    htabledealloc(wire->tiles);
    htabledelete(wire->tiles);
    free(wire);
}

/* Builds a new wire, scanning out from specified tile. */
Wire *
wirebuild(World *world, uint x, uint y, uint layer)
{
    Wire *wire;
    Segment *seg;
    List *agenda;
    Tile *tile, *tileadj;
    ivec2 *coord;
    uint i;

    wire = wirecreate();
    wire->layer = layer;
    agenda = listcreate();

    coord = malloc(sizeof(ivec2));
    *coord[0] = x;
    *coord[1] = y;
    listpushback(agenda, coord);

    while (agenda->size > 0) {
        coord = listat(agenda, 0);
        tile = worldgettileat(world, *coord[0], *coord[1], layer);
        seg = worldgetsegmentat(world, *coord[0], *coord[1]);
        htableadd(wire->tiles, coord, tile);
        x = *coord[0];
        y = *coord[1];

        /* Add segment to list if it does not exist */
        for (i = 0; i < wire->segs->size; i++) {
            if (seg == listat(wire->segs, i)) {
                goto skipsegadd;
            }
        }
        listpushback(wire->segs, seg);
skipsegadd:

        /* Queue connected tiles */
        if (tile->state & TILE_TOP && y < WORLD_TILES_PER_ROW - 1) {
            tileadj = worldgettileat(world, x, y + 1, layer);

            if (tileadj->state & TILE_BOT) {
                coord = malloc(sizeof(ivec2));
                *coord[0] = x;
                *coord[1] = y + 1;
                listpushback(agenda, coord);
            }
        }
        if (tile->state & TILE_BOT && y > 0) {
            tileadj = worldgettileat(world, x, y - 1, layer);

            if (tileadj->state & TILE_TOP) {
                coord = malloc(sizeof(ivec2));
                *coord[0] = x;
                *coord[1] = y - 1;
                listpushback(agenda, coord);
            }
        }
        if (tile->state & TILE_LEFT && x > 0) {
            tileadj = worldgettileat(world, x - 1, y, layer);

            if (tileadj->state & TILE_RIGHT) {
                coord = malloc(sizeof(ivec2));
                *coord[0] = x - 1;
                *coord[1] = y;
                listpushback(agenda, coord);
            }
        }
        if (tile->state & TILE_RIGHT && x < WORLD_TILES_PER_ROW - 1) {
            tileadj = worldgettileat(world, x + 1, y, layer);

            if (tileadj->state & TILE_LEFT) {
                coord = malloc(sizeof(ivec2));
                *coord[0] = x + 1;
                *coord[1] = y;
                listpushback(agenda, coord);
            }
        }
        listremove(agenda, 0);
    }

    return wire;
}

/* Set wire activation state and set flags */
void
wireset(Wire *wire, uint value)
{
    if (wire->active != value) {
        wire->active = value;
        wire->changed = 1;
    }
    /* TODO: Perhaps pass activation state to output pins? */
}

Package *
packcreate(uint type, const char *path)
{
    size_t len;
    Package *pack;

    pack = calloc(1, sizeof(Package));
    dlistinit(&pack->in_pins, sizeof(Pin));
    dlistinit(&pack->out_pins, sizeof(Pin));

    if (path) {
        len = strlen(path) + 1;
        pack->path = malloc(len * sizeof(char));
        memcpy(pack->path, path, len);
    }

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
        circuitdelete(pack->circuit);
    }
    
    if (pack->memory) {
        free(pack->memory->data);
        free(pack->memory);
    }

    if (pack->path) {
        free(pack->path);
    }

    for (i = 0; i < pack->in_pins.size; i++) {
        pindelete(dlistat(&pack->in_pins, i));
    }

    for (i = 0; i < pack->out_pins.size; i++) {
        pindelete(dlistat(&pack->out_pins, i));
    }
    free(pack->name);
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

Package *
packcopy(Package *src)
{
    size_t size;
    Package *dest;

    dest = packcreate(src->type, src->path);
    dest->x = src->x;
    dest->y = src->y;
    dest->w = src->w;
    dest->h = src->h;

    if (src->memory) {
        size = src->memory->size * sizeof(uint);
        dest->memory = malloc(sizeof(Memory));
        dest->memory->data = malloc(size);
        memcpy(dest->memory->data, src->memory->data, size);
        dest->memory->size = src->memory->size;
    }

    return dest;
}

/* Build a logical circuit from a world-space circuit */
Circuit *
circuitbuild(struct World *world)
{
    uint x, y, l, s, p, n;
    Circuit *circuit;
    Segment *seg;
    Package *pack;
    Wire *wire;
    Pin *pin;
    Tile *tile;

    /*  Circuit building algorithm
     *
     *  For each world segment
     *      Check for packages
     *      If no packages are present:
     *          Go to next segment.
     *      Else: 
     *          For each package:
     *              For each output pin on package:
     *                  For each layer in segment:
     *                  If a connected wire is present in world-space:
     *                      Check that wire at coordinate is not present in any other wire
     *                      If wire is not present in any other wire:
     *                          Begin building new logical wire:
     *                              + Cascades through connected wires in island.
     *                              + Adds tile coords to hash table
     *                              + Assigns layer (prevents overlaps)
     *                              + Does not check for input pins
     *                          Add the new wire to master circuit
     *                      Add the wire to pin output
     *  For each world segment
     *      Check for packages
     *      If no packages are present:
     *          Go to next segment.
     *      Else: 
     *          For each package:
     *              For each input pin on package:
     *                  If a connected wire is present in world-space:
     *                      Get logical wire that tile belongs to
     *                      If logical wire exists:
     *                          Add wire to pin input
     * */

    circuit = malloc(sizeof(Circuit));
    circuit->wires = listcreate();
    circuit->packs = listcreate();

    /* Build wires from connections to output pins */
    for (s = 0; s < SEGMENT_COUNT; s++) {
        seg = world->segs[s];
        
        if (seg->packs->size == 0) {
            continue;
        } else {
            for (p = 0; p < seg->packs->size; p++) {
                pack = listat(seg->packs, p);

                for (n = 0; n < pack->out_pins.size; n++) {
                    pin = dlistat(&pack->out_pins, n);
                    x = seg->x * SEGMENT_SIZE + pack->x + pin->x;
                    y = seg->y * SEGMENT_SIZE + pack->y + pin->y;

                    for (l = 0; l < LAYER_TOTAL; l++) {
                        tile = worldgettileat(world, x, y, l);

                        if (tile->state & TILE_LEFT) {
                            wire = circuitgetwireat(circuit, x, y, l);

                            if (!wire) {
                                wire = wirebuild(world, x, y, l);    
                                listpushback(circuit->wires, wire);
                            }
                            listpushback(pin->outs, wire);
                        }
                    }
                }
                listpushback(circuit->packs, pack);
            }
        }
    }

    /* Connect input pins to wires */
    for (s = 0; s < SEGMENT_COUNT; s++) {
        seg = world->segs[s];
        
        if (seg->packs->size == 0) {
            continue;
        } else {
            for (p = 0; p < seg->packs->size; p++) {
                pack = listat(seg->packs, p);

                for (n = 0; n < pack->in_pins.size; n++) {
                    pin = dlistat(&pack->in_pins, n);
                    x = seg->x * SEGMENT_SIZE + pack->x + pin->x;
                    y = seg->y * SEGMENT_SIZE + pack->y + pin->y;

                    for (l = 0; l < LAYER_TOTAL; l++) {
                        tile = worldgettileat(world, x, y, l);
                        wire = circuitgetwireat(circuit, x, y, l);

                        if (tile->state & TILE_RIGHT && wire) {
                            listpushback(pin->ins, wire);
                        }
                    }
                }
            }
        }
    }

    return circuit;
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

/* Returns wire if the tile at the given position is already part of
 * a wire in the given circuit.  Otherwise, returns NULL.  */
Wire *
circuitgetwireat(Circuit *circuit, uint x, uint y, uint layer)
{
    uint i;
    Wire *wire;
    ivec2 coord;

    for (i = 0; i < circuit->wires->size; i++) {
        wire = listat(circuit->wires, i);
        coord[0] = x;
        coord[1] = y;

        if (wire->layer != layer) {
            continue;
        }

        if (htableget(wire->tiles, coord)) {
            return wire;
        }
    }

    return NULL;
}

void
circuitclear(Circuit *circuit)
{
    uint i, j, k;
    Package *pack;
    Pin *pin;

    for (i = 0; i < circuit->wires->size; i++) {
        wiredelete(listat(circuit->wires, i));
    }
    listclear(circuit->wires);

    for (i = 0; i < circuit->packs->size; i++) {
        pack = listat(circuit->packs, i);
        
        for (j = 0; j < pack->in_pins.size; j++) {
            pin = dlistat(&pack->in_pins, j);
            listclear(pin->ins);
        }

        for (j = 0; j < pack->out_pins.size; j++) {
            pin = dlistat(&pack->out_pins, j);
            listclear(pin->outs);
        }
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
    
    pininit(&pin, GATE_NOT_W, 0, "!A");
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
    
    pininit(&pin, GATE_OR_W, 1, "Q");
    dlistpush(&pack->out_pins, &pin);

    pininit(&pin, GATE_OR_W, 0, "!Q");
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
    
    pininit(&pin, GATE_AND_W, 1, "Q");
    dlistpush(&pack->out_pins, &pin);

    pininit(&pin, GATE_AND_W, 0, "!Q");
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
    
    pininit(&pin, GATE_XOR_W, 1, "Q");
    dlistpush(&pack->out_pins, &pin);

    pininit(&pin, GATE_XOR_W, 0, "!Q");
    dlistpush(&pack->out_pins, &pin);
}

uint
vec2_hash(const void *vec)
{
    const ivec2 *data = vec;

    return ((int)*data[0] ^ (int)*data[1]) >> 2;
}

uint
vec2_eq(const void *vec_a, const void *vec_b)
{
    const ivec2 *a = vec_a;
    const ivec2 *b = vec_b;
    
    return (*a[0] == *b[0]) && (*a[1] == *b[1]);
}
