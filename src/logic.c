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
static void packinit_memory(Package *pack);
static void packinit_input(Package *pack);
static void packinit_output(Package *pack);
static void packinit_clock(Package *pack);
static void packinit_connector(Package *pack);

static uint vec2_hash(const void *);
static uint vec2_eq(const void *, const void *);

Wire *
wirecreate(void)
{
    Wire *wire;

    wire = calloc(1, sizeof(Wire));
    wire->ins = listcreate();
    wire->outs = listcreate();
    wire->segs = listcreate();
    wire->tiles = htablecreate(vec2_hash, vec2_eq);
    dlistinit(&wire->tile_data, sizeof(TileData));

    return wire;
}

void
wiredelete(Wire *wire)
{
    listdelete(wire->ins);
    listdelete(wire->outs);
    listdelete(wire->segs);
    htabledeallockeys(wire->tiles);
    htabledelete(wire->tiles);
    dlistdelete(&wire->tile_data);
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
    TileData t_data;
    int *coord;
    uint i, found;

    wire = wirecreate();
    wire->layer = layer;
    agenda = listcreate();

    coord = malloc(2 * sizeof(int));
    coord[0] = x;
    coord[1] = y;
    listpushback(agenda, coord);

    t_data.x = coord[0];
    t_data.y = coord[1];
    t_data.state = worldgettileat(world, coord[0], coord[1], layer)->state;
    dlistpush(&wire->tile_data, &t_data);

    while (agenda->size > 0) {
        coord = listat(agenda, 0);
        tile = worldgettileat(world, coord[0], coord[1], layer);
        seg = worldgetsegmentat(world, coord[0], coord[1]);
        
        t_data.x = coord[0];
        t_data.y = coord[1];
        t_data.state = tile->state;
        dlistpush(&wire->tile_data, &t_data);

        htableadd(wire->tiles, coord, tile);
        x = coord[0];
        y = coord[1];
        found = 0;

        /* Add segment to list if it does not exist */
        for (i = 0; i < wire->segs->size; i++) {
            if (seg == listat(wire->segs, i)) {
                found = 1;
            }
        }
        if (!found) {
            listpushback(wire->segs, seg);
        }

        /* Queue connected tiles */
        if (tile->state & TILE_TOP && y < WORLD_SIZE - 1) {
            tileadj = worldgettileat(world, x, y + 1, layer);

            if (tileadj->state & TILE_BOT) {
                coord = malloc(2 * sizeof(int));
                coord[0] = x;
                coord[1] = y + 1;

                if (!htableget(wire->tiles, coord)) {
                    listpushback(agenda, coord);
                }
            }
        }
        if (tile->state & TILE_BOT && y > 0) {
            tileadj = worldgettileat(world, x, y - 1, layer);

            if (tileadj->state & TILE_TOP) {
                coord = malloc(2 * sizeof(int));
                coord[0] = x;
                coord[1] = y - 1;

                if (!htableget(wire->tiles, coord)) {
                    listpushback(agenda, coord);
                }
            }
        }
        if (tile->state & TILE_LEFT && x > 0) {
            tileadj = worldgettileat(world, x - 1, y, layer);

            if (tileadj->state & TILE_RIGHT) {
                coord = malloc(2 * sizeof(int));
                coord[0] = x - 1;
                coord[1] = y;

                if (!htableget(wire->tiles, coord)) {
                    listpushback(agenda, coord);
                }
            }
        }
        if (tile->state & TILE_RIGHT && x < WORLD_SIZE - 1) {
            tileadj = worldgettileat(world, x + 1, y, layer);

            if (tileadj->state & TILE_LEFT) {
                coord = malloc(2 * sizeof(int));
                coord[0] = x + 1;
                coord[1] = y;

                if (!htableget(wire->tiles, coord)) {
                    listpushback(agenda, coord);
                }
            }
        }
        listremove(agenda, 0);
    }
    listdelete(agenda);

    return wire;
}

/* Pulls wire state based on input pins */
uint
wireupdate(Wire *wire)
{
    uint i, prevstate, result;
    Pin *pin;
    Segment *seg;
    Tile *tile;

    prevstate = wire->active;
    wire->active = 0;
    wire->changed = 0;
    result = 0;

    /* Skip updating after other oscillation culprits have had 
     * extra cycles to be marked. */
    if (wire->conv > WIRE_CONVERGE_EXTRA) {
        return 1;
    }

    /* Flag connected packs as oscillating */
    if (wire->conv > WIRE_CONVERGE_LIMIT) {
        for (i = 0; i < wire->ins->size; i++) {
            pin = listat(wire->ins, i);
            pin->parent->error = 1;
        }
        result = 1;
    }

    /* Activate wire if any pins are enabled */
    for (i = 0; i < wire->ins->size; i++) {
        pin = listat(wire->ins, i);

        if (pin->state) {
            wire->active = pin->state;
            break;
        }
    }

    /* Flag connected packs for updating */
    if (wire->active != prevstate) {
        for (i = 0; i < wire->outs->size; i++) {
            pin = listat(wire->outs, i);
            pin->parent->changed = 1;
        }
        wire->conv++;
    }

    /* Update world-space tiles for visual highlighting */
    if (!wire->sub) {
        if (wire->active) {
            for (i = 0; i < wire->tiles->size; i++) {
                tile = wire->tiles->vals[i];

                if (!tile) continue;

                BIT_SET(tile->state, TILE_POWERED);
            }
        } else {
            for (i = 0; i < wire->tiles->size; i++) {
                tile = wire->tiles->vals[i];

                if (!tile) continue;

                BIT_CLR(tile->state, TILE_POWERED);
            }
        }
        /* Queue segment remeshes */
        for (i = 0; i < wire->segs->size; i++) {
            seg = listat(wire->segs, i);
            seg->dirty = 1;
        }
    }

    return result;
}

Package *
packcreate(uint type, const char *path)
{
    Package *pack;

    pack = calloc(1, sizeof(Package));
    pack->clockrate = DEFAULT_CLOCK_RATE;
    pack->changed = 1;
    pack->type = type;
    dlistinit(&pack->in_pins, sizeof(Pin));
    dlistinit(&pack->out_pins, sizeof(Pin));

    if (path) {
        pack->path = strdup(path);
    }

    switch (type) {
    case PACK_CIRCUIT:
        //packload(pack, path);
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
    case PACK_MEMORY:
        packinit_memory(pack);
        break;
    case PACK_INPUT:
        packinit_input(pack);
        break;
    case PACK_OUTPUT:
        packinit_output(pack);
        break;
    case PACK_CLOCK:
        packinit_clock(pack);
        break;
    case PACK_CONNECTOR:
        packinit_connector(pack);
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

Package *
packcopy(Package *src)
{
    size_t size;
    Package *dest;
    World *world;

    dest = packcreate(src->type, src->path);
    dest->x = src->x;
    dest->y = src->y;
    dest->w = src->w;
    dest->h = src->h;
    dest->seg_x = src->seg_x;
    dest->seg_y = src->seg_y;
    dest->clockrate = src->clockrate;
    dest->state = src->state;
    dest->seg = src->seg;
    dest->name = strdup(src->name);

    if (src->memory) {
        size = src->memory->size * sizeof(uint);
        dest->memory = malloc(sizeof(Memory));
        dest->memory->data = malloc(size);
        memcpy(dest->memory->data, src->memory->data, size);
        dest->memory->size = src->memory->size;
    }

    if (src->circuit) {
        world = worldcreatefake();
        worldbuild(world, src->circuit);
        dest->circuit = circuitbuild(world, 1);
        packbindcircuit(dest, dest->circuit);
        worlddeletefake(world);
    }

    return dest;
}

/* Calculate state of package output pins based on input pin states */
uint
packupdate(Package *pack, float dt)
{
    uint i, j;
    Pin *pin;
    Wire *wire;

    pack->changed = 0;

    if (pack->error) {
        return 1;
    }

    switch(pack->type) {
    case PACK_CIRCUIT:
        for (i = 0; i < pack->in_pins.size; i++) {
            pin = dlistat(&pack->in_pins, i);
            pingetsig(pin);

            for (j = 0; j < pin->outs->size; j++) {
                wire = listat(pin->outs, j);
                wire->changed = 1;
            }
        }
        circuitupdate(pack->circuit, dt);

        for (i = 0; i < pack->out_pins.size; i++) {
            pin = dlistat(&pack->out_pins, i);
            pingetsig(pin);
        }
        break;
    default:
        pack->func(pack);
    }

    /* Flag all output wires for updating */
    for (i = 0; i < pack->out_pins.size; i++) {
        pin = dlistat(&pack->out_pins, i);

        for (j = 0; j < pin->outs->size; j++) {
            wire = listat(pin->outs, j);
            wire->changed = 1;
        }
    }

    return 0;
}


/* Reorders a given list of packages by name */
void
sortpacklist(List *packs)
{
    uint i, min_idx;
    List *sorted;
    Package *pack, *pack_min;

    sorted = listcreate();

    while (packs->size > 0) {
        pack_min = listat(packs, 0);
        min_idx = 0;

        for (i = 1; i < packs->size; i++) {
            pack = listat(packs, i);

            if (strcmp(pack->name, pack_min->name) < 0 &&
                strlen(pack->name) <= strlen(pack_min->name)) {
                pack_min = pack;
                min_idx = i;
            }
        }
        listpushback(sorted, pack_min);
        listremove(packs, min_idx);
    }
    
    for (i = 0; i < sorted->size; i++) {
        listpushback(packs, listat(sorted, i));
    }
    listdelete(sorted);
}

/* Builds a package with circuit, basing size and 
 * pins on circuit's input and output packages */
void
packbindcircuit(Package *pack, Circuit *circuit)
{
    uint i, j, k;
    List *in_nodes, *out_nodes;
    Package *pack_tmp;
    Pin *pin, *pin_tmp, *wire_pin, new_pin;
    Wire *wire;

    in_nodes = listcreate();
    out_nodes = listcreate();

    /* Find and order input and output packs */
    for (i = 0; i < circuit->packs->size; i++) {
        pack_tmp = listat(circuit->packs, i);
        
        if (pack_tmp->type == PACK_INPUT) {
            listpushback(in_nodes, pack_tmp);
        } else if (pack_tmp->type == PACK_OUTPUT) {
            listpushback(out_nodes, pack_tmp);
        }
    }
    sortpacklist(in_nodes);
    sortpacklist(out_nodes);

    /* Set package dimensions appropriately */
    if (in_nodes->size > out_nodes->size) {
        pack->h = in_nodes->size;
    } else {
        pack->h = out_nodes->size;
    }
    pack->h = (pack->h < 1 ? 1 : pack->h);
    pack->w = 3 + strlen(pack->name) / 3;

    /* Create and configure pins, linking to internal wiring */
    for (i = 0; i < in_nodes->size; i++) {
        pack_tmp = listat(in_nodes, i);
        pin = dlistat(&pack_tmp->out_pins, 0);
        pininit(pack, &new_pin, -1, pack->h - i - 1, pack_tmp->name);

        for (j = 0; j < pin->outs->size; j++) {
            wire = listat(pin->outs, j);
            listpushback(new_pin.outs, wire);
        }
        dlistpush(&pack->in_pins, &new_pin);
    }

    for (i = 0; i < out_nodes->size; i++) {
        pack_tmp = listat(out_nodes, i);
        pin = dlistat(&pack_tmp->in_pins, 0);
        pininit(pack, &new_pin, pack->w, pack->h - i - 1, pack_tmp->name);
        
        for (j = 0; j < pin->ins->size; j++) {
            wire = listat(pin->ins, j);
            listpushback(new_pin.ins, wire);
        }
        dlistpush(&pack->out_pins, &new_pin);
    }

    /* Replace I/O node pin connections with external package pins */
    for (i = 0; i < pack->in_pins.size; i++) {
        pack_tmp = listat(in_nodes, i);
        pin_tmp = dlistat(&pack_tmp->out_pins, 0);
        pin = dlistat(&pack->in_pins, i);

        for (j = 0; j < pin->outs->size; j++) {
            wire = listat(pin->outs, j);

            for (k = 0; k < wire->ins->size; k++) {
                wire_pin = listat(wire->ins, k);
                if (wire_pin == pin_tmp) {
                    wire->ins->elem[k] = pin;
                }
            }
        }
    }

    for (i = 0; i < pack->out_pins.size; i++) {
        pack_tmp = listat(out_nodes, i);
        pin_tmp = dlistat(&pack_tmp->in_pins, 0);
        pin = dlistat(&pack->out_pins, i);

        for (j = 0; j < pin->ins->size; j++) {
            wire = listat(pin->ins, j);

            for (k = 0; k < wire->outs->size; k++) {
                wire_pin = listat(wire->outs, k);
                if (wire_pin == pin_tmp) {
                    wire->outs->elem[k] = pin;
                }
            }
        }
    }
    listdelete(in_nodes);
    listdelete(out_nodes);

    pack->circuit = circuit;
}

/* Build a logical circuit from a world-space circuit */
Circuit *
circuitbuild(World *world, int sub)
{
    uint x, y, l, s, p, n;
    Circuit *circuit;
    Segment *seg;
    Package *pack;
    Wire *wire;
    Pin *pin;
    Tile *tile;

    circuit = malloc(sizeof(Circuit));
    circuit->wires = listcreate();
    circuit->packs = listcreate();
    circuit->error = 0;

    /* Build wires from connections to output pins */
    for (s = 0; s < SEGMENT_COUNT; s++) {
        seg = world->segs[s];
        
        if (seg->packs->size == 0) {
            continue;
        } else {
            for (p = 0; p < seg->packs->size; p++) {
                pack = listat(seg->packs, p);

                if (pack->error) {
                    seg->dirty = 1;
                }

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
                            wire->sub = sub;
                            listpushback(pin->outs, wire);
                            listpushback(wire->ins, pin);
                        }
                    }
                }
                listpushback(circuit->packs, pack);
            }
        }
    }

    /* Connect input pins to existing wires */
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
                            listpushback(wire->outs, pin);
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
    circuitclear(circuit);
    listdelete(circuit->wires);
    listdelete(circuit->packs);
    free(circuit);
}

/* Returns wire if the tile at the given position is already part of
 * a wire in the given circuit.  Otherwise, returns NULL.  */
Wire *
circuitgetwireat(Circuit *circuit, uint x, uint y, uint layer)
{
    uint i;
    Wire *wire;
    int coord[2];

    for (i = 0; i < circuit->wires->size; i++) {
        wire = listat(circuit->wires, i);

        if (wire->layer != layer) {
            continue;
        }
        coord[0] = x;
        coord[1] = y;

        if (htableget(wire->tiles, coord)) {
            return wire;
        }
    }

    return NULL;
}

void
circuitclear(Circuit *circuit)
{
    uint i, j;
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
    listclear(circuit->packs);
}

void
circuitupdate(Circuit *circuit, float dt)
{
    uint i, found;
    Wire *wire;
    Package *pack;

    found = 1;

    while (found) {
        found = 0;

        for (i = 0; i < circuit->wires->size; i++) {
            wire = listat(circuit->wires, i);

            if (wire->changed) {
                circuit->error = wireupdate(wire);
                found = 1;
            }
        }

        for (i = 0; i < circuit->packs->size; i++) {
            pack = listat(circuit->packs, i);

            if (pack->changed) {
                circuit->error = packupdate(pack, dt);
                found = 1;
            }
        }
    }
}

/* Runs updates on the whole circuit, instead of
 * following pure event scheduling. */
void
circuitupdatefull(Circuit *circuit, float dt)
{
    uint i, j, k;
    World *world;
    Segment *seg;
    Layer *layer;
    Package *pack;
    Wire *wire;

    world = worldtop();

    /* Clear all world-space wire activation and error states */
    for (i = 0; i < SEGMENT_COUNT; i++) {
        seg = world->segs[i];

        for (j = 0; j < LAYER_TOTAL; j++) {
            layer = seg->layers + j;

            for (k = 0; k < LAYER_TILECOUNT; k++) {
                BIT_CLR(layer->tiles[k].state, TILE_POWERED);
            }
        }
    }
    
    for (i = 0; i < circuit->wires->size; i++) {
        wire = listat(circuit->wires, i);
        wireupdate(wire);
        //wire->changed = 1;
    }

    for (i = 0; i < circuit->packs->size; i++) {
        pack = listat(circuit->packs, i);

        /*if (pack->circuit) {
            circuitupdatefull(pack->circuit, dt);
        }*/
        pack->changed = 1;
    }

    circuitupdate(circuit, dt);
}

void
circuitrebuild(struct World *world, Circuit **circuit)
{
    schedulesegremeshing(*circuit);
    circuitclear(*circuit);
    *circuit = circuitbuild(world, 0);
    circuitclearerrors(*circuit);
    circuitupdatefull(*circuit, 0.0f);
}

void
circuitclearerrors(Circuit *circuit)
{
    uint i;
    Package *pack;
    Wire *wire;

    circuit->error = 0;

    for (i = 0; i < circuit->packs->size; i++) {
        pack = listat(circuit->packs, i);
        pack->error = 0;

        if (pack->type == PACK_CIRCUIT) {
            circuitclearerrors(pack->circuit);
        }
    }

    for (i = 0; i < circuit->wires->size; i++) {
        wire = listat(circuit->wires, i);
        wire->conv = 0;
    }
}

void
circuitresetwireconvs(Circuit *circuit)
{
    uint i;
    Wire *wire;

    for (i = 0; i < circuit->wires->size; i++) {
        wire = listat(circuit->wires, i);
        wire->conv = 0;
    }
}

void
pininit(Package *pack, Pin *pin, int x, int y, const char *name)
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
    pin->parent = pack;
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
        pin->state = pin->state || wire->active;
    }
}

/* Increments times of all clock packages by dt */
void
updateclocktimes(Circuit *circuit, float dt)
{
    uint i;
    Package *pack;

    for (i = 0; i < circuit->packs->size; i++) {
        pack = listat(circuit->packs, i);

        switch (pack->type) {
        case PACK_CIRCUIT:
            updateclocktimes(pack->circuit, dt);
            pack->changed = 1;
            break;
        case PACK_CLOCK:
            pack->time += dt;
            pack->changed = 1;
        }
    }
}

/* Determine segments to remesh before circuit is cleared */
void
schedulesegremeshing(Circuit *circuit)
{
    uint i, j;
    Wire *wire;
    Segment *seg;

    for (i = 0; i < circuit->wires->size; i++) {
        wire = listat(circuit->wires, i);

        for (j = 0; j < wire->segs->size; j++) {
            seg = listat(wire->segs, j);
            seg->dirty = 1;
        }
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
}

void
packfunc_input(Package *pack)
{
    Pin *out;

    out = dlistat(&pack->out_pins, 0);
    out->state = pack->state;
}

void
packfunc_output(Package *pack)
{
    Pin *in;

    in = dlistat(&pack->in_pins, 0);

    pingetsig(in);

    if (pack->state != in->state) {
        pack->state = in->state;

        if (pack->seg) {
            pack->seg->dirty = 1;
        }
    }
}

void
packfunc_clock(Package *pack)
{
    Pin *out;

    out = dlistat(&pack->out_pins, 0);
    
    if (pack->time >= (1.0f / pack->clockrate)) {
        pack->time -= (1.0f / pack->clockrate);
        out->state = !out->state;
    }
}

void
packfunc_memory(Package *pack)
{
    int i;
    uint dat, addr, out;
    Pin *d[8], *a[8], *o[8];
    Pin *ld, *st, *clk;

    dat = 0;
    addr = 0;

    ld = dlistat(&pack->in_pins, 16);
    st = dlistat(&pack->in_pins, 17);
    clk = dlistat(&pack->in_pins, 18);
    pingetsig(ld);
    pingetsig(st);
    pingetsig(clk);

    /* Pull memory address from pins */
    for (i = 7; i >= 0; i--) {
        a[i] = dlistat(&pack->in_pins, i + 8);
        pingetsig(a[i]);
        addr |= a[i]->state << i;
    }

    /* Pull data from pins and store in memory */
    if (st->state && !pack->state && clk->state) {

        for (i = 7; i >= 0; i--) {
            d[i] = dlistat(&pack->in_pins, i);
            pingetsig(d[i]);
            dat |= d[i]->state << i;
        }
        pack->memory->data[addr] = dat;
    }
    
    /* Enxure that data is only stored on rising clock signal */
    if (clk->state) {
        pack->state = 1;
    } else {
        pack->state = 0;
    }

    /* Output state to pins. Defaults to 0 if `load` pin is not active */
    if (ld->state) {
        out = pack->memory->data[addr];

        for (i = 0; i < 8; i++) {
            o[i] = dlistat(&pack->out_pins, i);
            o[i]->state = (out >> i) & 1;
        }
    } else {
        for (i = 0; i < 8; i++) {
            o[i] = dlistat(&pack->out_pins, i);
            o[i]->state = 0;
        }
    }
}

void
packfunc_connector(Package *pack)
{
    Pin *in, *out;

    in = dlistat(&pack->in_pins, 0);
    out = dlistat(&pack->out_pins, 0);

    pingetsig(in);
    out->state = in->state;
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

    pininit(pack, &pin, -1, 0, "A");
    dlistpush(&pack->in_pins, &pin);
 
    pininit(pack, &pin, GATE_NOT_W, 0, "!A");
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
    
    pininit(pack, &pin, -1, 1, "A");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 0, "B");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, GATE_OR_W, 1, "Q");
    dlistpush(&pack->out_pins, &pin);

    pininit(pack, &pin, GATE_OR_W, 0, "!Q");
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
    
    pininit(pack, &pin, -1, 1, "A");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 0, "B");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, GATE_AND_W, 1, "Q");
    dlistpush(&pack->out_pins, &pin);

    pininit(pack, &pin, GATE_AND_W, 0, "!Q");
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
    
    pininit(pack, &pin, -1, 1, "A");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 0, "B");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, GATE_XOR_W, 1, "Q");
    dlistpush(&pack->out_pins, &pin);

    pininit(pack, &pin, GATE_XOR_W, 0, "!Q");
    dlistpush(&pack->out_pins, &pin);
}

void
packinit_input(Package *pack)
{
    Pin pin;

    pack->w = PACK_IN_W;
    pack->h = PACK_IN_H;
    pack->func = packfunc_input;
    pack->name = malloc(3 * sizeof(char));
    memcpy(pack->name, "IN", 3 * sizeof(char));
    
    pininit(pack, &pin, PACK_IN_W, 0, "");
    dlistpush(&pack->out_pins, &pin);
}

void
packinit_output(Package *pack)
{
    Pin pin;

    pack->w = PACK_OUT_W;
    pack->h = PACK_OUT_H;
    pack->func = packfunc_output;
    pack->name = malloc(4 * sizeof(char));
    memcpy(pack->name, "OUT", 4 * sizeof(char));
    
    pininit(pack, &pin, -1, 0, "");
    dlistpush(&pack->in_pins, &pin);
}

void
packinit_clock(Package *pack)
{
    Pin pin;

    pack->w = PACK_CLK_W;
    pack->h = PACK_CLK_H;
    pack->func = packfunc_clock;
    pack->name = malloc(6 * sizeof(char));
    memcpy(pack->name, "CLOCK", 6 * sizeof(char));

    pininit(pack, &pin, PACK_CLK_W, 0, "");
    dlistpush(&pack->out_pins, &pin);
}

void
packinit_memory(Package *pack)
{
    Pin pin;

    pack->w = PACK_MEM_W;
    pack->h = PACK_MEM_H;
    pack->func = packfunc_memory;
    pack->name = malloc(4 * sizeof(char));
    memcpy(pack->name, "RAM", 4 * sizeof(char));
    
    pack->memory = malloc(sizeof(Memory)); 
    pack->memory->data = calloc(256, sizeof(uint));
    pack->memory->size = 256;
    
    pininit(pack, &pin, -1, 18, "D0");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 17, "D1");
    dlistpush(&pack->in_pins, &pin);

    pininit(pack, &pin, -1, 16, "D2");
    dlistpush(&pack->in_pins, &pin);

    pininit(pack, &pin, -1, 15, "D3");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 14, "D4");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 13, "D5");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 12, "D6");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 11, "D7");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 10, "A0");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 9, "A1");
    dlistpush(&pack->in_pins, &pin);

    pininit(pack, &pin, -1, 8, "A2");
    dlistpush(&pack->in_pins, &pin);

    pininit(pack, &pin, -1, 7, "A3");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 6, "A4");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 5, "A5");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 4, "A6");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 3, "A7");
    dlistpush(&pack->in_pins, &pin);

    pininit(pack, &pin, -1, 2, "LD");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 1, "ST");
    dlistpush(&pack->in_pins, &pin);
    
    pininit(pack, &pin, -1, 0, "CLK");
    dlistpush(&pack->in_pins, &pin);

    pininit(pack, &pin, PACK_MEM_W, 18, "O0");
    dlistpush(&pack->out_pins, &pin);
    
    pininit(pack, &pin, PACK_MEM_W, 17, "O1");
    dlistpush(&pack->out_pins, &pin);

    pininit(pack, &pin, PACK_MEM_W, 16, "O2");
    dlistpush(&pack->out_pins, &pin);
    
    pininit(pack, &pin, PACK_MEM_W, 15, "O3");
    dlistpush(&pack->out_pins, &pin);
    
    pininit(pack, &pin, PACK_MEM_W, 14, "O4");
    dlistpush(&pack->out_pins, &pin);
    
    pininit(pack, &pin, PACK_MEM_W, 13, "O5");
    dlistpush(&pack->out_pins, &pin);
    
    pininit(pack, &pin, PACK_MEM_W, 12, "O6");
    dlistpush(&pack->out_pins, &pin);
    
    pininit(pack, &pin, PACK_MEM_W, 11, "O7");
    dlistpush(&pack->out_pins, &pin);
}

void
packinit_connector(Package *pack)
{
    Pin pin;

    pack->w = PACK_CONNECT_W;
    pack->h = PACK_CONNECT_H;
    pack->func = packfunc_connector;
    pack->name = malloc(4 * sizeof(char));
    memcpy(pack->name, "", sizeof(char));

    pininit(pack, &pin, -1, 0, "I");
    dlistpush(&pack->in_pins, &pin);
 
    pininit(pack, &pin, PACK_CONNECT_W, 0, "O");
    dlistpush(&pack->out_pins, &pin);
}

uint
vec2_hash(const void *vec)
{
    const int *data = vec;

    return (data[0] ^ data[1]) >> 2;
}

uint
vec2_eq(const void *vec_a, const void *vec_b)
{
    const int *a = vec_a;
    const int *b = vec_b;
    
    return (a[0] == b[0]) && (a[1] == b[1]);
}
