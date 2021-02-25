/*
 *  BREADBOARD (2020)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Provides a set of tools for world, circuit, and lesson serialisation,
 *  saving, and loading.  Caches loaded files for later use.
 *
 *  */

#include "file.h"
#include "world.h"
#include "logic.h"
#include "util/util.h"
#include "util/htable.h"
#include "util/log.h"

/* General file functions */
static int read_header(byte **data);
static int read_body(byte **data, World *world);
static int write_header(FileData *data);
static int write_body(FileData *data, World *world);

/* Circuit-specific file functions */
static void read_segment(byte **data, Segment *seg);
static Package * read_pack(byte **data);
static Pin * read_pin(byte **data);
static Circuit * read_circuit(byte **data);
static Memory * read_memory(byte **data);
static void write_segment(FileData *data, Segment *seg);
static void write_pack(FileData *data, Package *pack);
static void write_pin(FileData *data, Pin *pin);
static void write_circuit(FileData *data, Circuit *circuit);
static void write_memory(FileData *data, Memory *memory);

/* Primitive type file functions */
static int read_bool(byte **data);
static char read_char(byte **data);
static int read_int(byte **data);
static uchar read_uchar(byte **data);
static uint read_uint(byte **data);
static float read_float(byte **data);
static void write_bool(FileData *data, int val);
static void write_char(FileData *data, char val);
static void write_int(FileData *data, int val);
static void write_uchar(FileData *data, uchar val);
static void write_uint(FileData *data, uint val);
static void write_float(FileData *data, float val);

/* Composite type file functions */
static char * read_string(byte **data, uint len);
static void read_array(byte **data, void *dest, size_t nitems, size_t size);
static void write_string(FileData *data, uint len, char *str);
static void write_array(FileData *data, void *src, size_t nitems, size_t size);

/* Run-length encoding file functions */
static void rle_decode(byte **data, uint *dest, size_t nitems);
static void rle_encode(FileData *dest, uint *data, size_t nitems);

/* FileData functions */
static void filedata_init(FileData *data);
static void filedata_push(FileData *data, void *src, size_t size);
static void filedata_grow(FileData *data);
static void filedata_delete(FileData *data);

int
filegetworld(const char *path, World *world)
{
    byte *data = (byte *)loadfile(path);
    byte *start = data;

    if (read_header(&data)) {
        logprint(LOG_ERROR, "file: Unable to load world: "
                "File header mismatch");
        free(start);
        return 1;
    }
    if (read_body(&data, world)) {
        logprint(LOG_ERROR, "file: Unable to load world: "
                "Corrupted/invalid circuit data");
        return 1;
    }
    free(start);

    return 0;
}

int
filesaveworld(const char *path, World *world)
{
    FileData data;

    filedata_init(&data);

    write_header(&data);
    write_body(&data, world);

    savefile(path, (char *)data.data, data.size);

    return 0;
}

void
filedata_init(FileData *data)
{
    data->size = 0;
    data->cap = DATA_INIT_CAP;
    data->data = malloc(DATA_INIT_CAP);
}

void
filedata_push(FileData *data, void *src, size_t size)
{
    if (data->size + size >= data->cap) {
        filedata_grow(data);
    }
    memcpy(data->data + data->size, src, size);
    data->size += size;
}

void
filedata_grow(FileData *data)
{
    byte *newmem = realloc(data->data, data->cap * 2);

    if (!newmem) {
        logprint(LOG_FATAL, "file: Unable to reallocate memory");
        die("Fatal error has occurred.  Shutting down...");
    }
    data->data = newmem;
    data->cap *= 2;
}

void
filedata_delete(FileData *data)
{
    free(data->data);
}

/*
 *  Serialized data reading function definitions
 *
 * */

/* Checks header and advances pointer.  Returns non-zero on mismatch. */
int
read_header(byte **data)
{
    int diff;
    const byte header[] = "BCF";

    diff = 0;

    diff += header[0] != (*data)[0];
    diff += header[1] != (*data)[1];
    diff += header[2] != (*data)[2];

    *data += 3;

    return diff;
}

/* Reads data from file body into `world`
 * Returns non-zero on failure. */
int
read_body(byte **data, World *world)
{
    uint x, y, tag;
    Segment *seg;

    tag = read_uint(data);

    while (tag != TAG_END) {
        if (tag != TAG_SEG) {
            return 1;
        }
        x = read_uint(data);
        y = read_uint(data);
        seg = worldgetsegment(world, x, y);
        read_segment(data, seg);
        tag = read_uint(data);
    }

    return 0;
}

void
read_segment(byte **data, Segment *seg)
{
    uint i, j, tag;
    uint *layer_dat;
    Package *pack;

    layer_dat = malloc(LAYER_TILECOUNT * sizeof(uint));

    /* Read layer data */
    for (i = 0; i < 3; i++) {
        rle_decode(data, layer_dat, LAYER_TILECOUNT);
        
        for (j = 0; j < LAYER_TILECOUNT; j++) {
            seg->layers[i].tiles[j].state = layer_dat[j];
        }
    }
    free(layer_dat);

    /* Read Package data */
    tag = read_uint(data);

    while (tag != TAG_END) {
        pack = read_pack(data);
        pack->seg = seg;
        pack->seg_x = seg->x;
        pack->seg_y = seg->y;
        listpushback(seg->packs, pack);
        tag = read_uint(data);
    }
}

Package *
read_pack(byte **data)
{
    uint tag;
    World *world; 
    Package *pack;
    Circuit *circuit;

    pack = packcreate(read_uint(data), NULL);

    pack->x = read_uint(data);
    pack->y = read_uint(data);
    pack->w = read_uint(data);
    pack->h = read_uint(data);
    pack->clockrate = read_uint(data);
    pack->state = read_uint(data);

    /* Read package template path and name */
    tag = read_uint(data);
    if (tag == TAG_STRING) {
        pack->path = read_string(data, read_uint(data));
    } else {
        pack->path = NULL;
    }
    pack->name = read_string(data, read_uint(data));

    /* Load memory, if it exists. */
    tag = read_uint(data);
    if (tag == TAG_MEMORY) {
        pack->memory = read_memory(data);
    } else {
        pack->memory = NULL;
    }

    /* Load user-defined circuit, if it exists. */
    tag = read_uint(data);
    if (tag == TAG_CIRCUIT) {
        world = worldcreatefake();
        read_body(data, world);
        circuit = circuitbuild(world, 1);
        packbindcircuit(pack, circuit);
        worlddeletefake(world);
    } else {
        pack->circuit = NULL;
    }

    return pack;
}

Pin *
read_pin(byte **data)
{
    Pin *pin;

    pin = malloc(sizeof(Pin));
    pin->x = read_int(data);
    pin->y = read_int(data);
    pin->state = read_uint(data);
    pin->name = read_string(data, read_uint(data));
    pin->ins = listcreate();
    pin->outs = listcreate();

    return pin;
}

Circuit *
read_circuit(byte **data)
{
    World *world;
    Circuit *circuit;

    world = worldcreatefake();
    read_body(data, world);
    circuit = circuitbuild(world, 1);
    worlddeletefake(world);

    return circuit;
}

Memory *
read_memory(byte **data)
{
    Memory *mem;

    mem = malloc(sizeof(Memory));
    mem->size = read_uint(data);
    mem->data = malloc(mem->size * sizeof(uint));
    rle_decode(data, mem->data, mem->size);

    return mem;
}

int
read_bool(byte **data)
{
    int p = *((byte *) *data);
    *data += sizeof(byte);

    return p;
}

char
read_char(byte **data)
{
    char p = *((char *) *data);
    *data += sizeof(char);

    return p;
}

int
read_int(byte **data)
{
    int p = *((int *) *data);
    *data += sizeof(int);

    return p;
}

uchar
read_uchar(byte **data)
{
    uchar p = *((uchar *) *data);
    *data += sizeof(uchar);

    return p;
}

uint
read_uint(byte **data)
{
    uint p = *((uint *) *data);
    *data += sizeof(uint);

    return p;
}

float
read_float(byte **data)
{
    float p = *((float *) *data);
    *data += sizeof(float);

    return p;
}

char *
read_string(byte **data, uint len)
{
    char *str;

    str = malloc((len + 1) * sizeof(char));

    memcpy(str, *data, len * sizeof(char));
    str[len] = '\0';

    *data += len * sizeof(char);

    return str;
}

void
read_array(byte **data, void *dest, size_t nitems, size_t size)
{
    uint len;

    len = nitems * size;
    memcpy(dest, data, len);
    *data += len;
}

void
rle_decode(byte **data, uint *dest, size_t nitems)
{
    uint len, val, i, j;

    i = 0;

    while (i < nitems) {
        len = read_uint(data);
        val = read_uint(data);

        for (j = 0; j < len; j++) {
            dest[i + j] = val;
        }
        i += len;
    }
}


/*
 *  Serialized data writing function definitions
 *
 * */

int
write_header(FileData *data)
{
    filedata_push(data, "BCF", 3);

    return 1;
}

int
write_body(FileData *data, World *world)
{
    uint i, j, k, hasdata;
    Segment *seg;

    for (i = 0; i < SEGMENT_COUNT; i++) {
        seg = world->segs[i];
        hasdata = 0;

        for (j = 0; j < 3; j++) {
            for (k = 0; k < LAYER_TILECOUNT; k++) {
                hasdata += seg->layers[j].tiles[k].state;
            }
        }
        hasdata += seg->packs->size;

        if (hasdata) {
            write_uint(data, TAG_SEG);
            write_uint(data, seg->x);
            write_uint(data, seg->y);
            write_segment(data, seg);
        }
    }
    write_uint(data, TAG_END);

    return 1;
}

void
write_segment(FileData *data, Segment *seg)
{
    uint i, j;
    uint *layer_dat;
    Layer *layer;
    Package *pack;

    /* Write segment tile data */
    for (i = 0; i < 3; i++) {
        layer = &seg->layers[i];
        layer_dat = malloc(LAYER_TILECOUNT * sizeof(uint));

        for (j = 0; j < LAYER_TILECOUNT; j++) {
            layer_dat[j] = layer->tiles[j].state;
        }
        rle_encode(data, layer_dat, LAYER_TILECOUNT);
        free(layer_dat);
    }

    /* Write packages */
    for (i = 0; i < seg->packs->size; i++) {
        pack = listat(seg->packs, i);
        write_uint(data, TAG_PACK);
        write_pack(data, pack);
    }
    write_uint(data, TAG_END);
}

void
write_pack(FileData *data, Package *pack)
{
    write_uint(data, pack->type);
    write_uint(data, pack->x);
    write_uint(data, pack->y);
    write_uint(data, pack->w);
    write_uint(data, pack->h);
    write_uint(data, pack->clockrate);
    write_uint(data, pack->state);

    if (pack->path) {
        write_uint(data, TAG_STRING);
        write_string(data, strlen(pack->path), pack->path);
    } else {
        write_uint(data, TAG_NULL);
    }
    write_string(data, strlen(pack->name), pack->name);

    if (pack->memory) {
        write_uint(data, TAG_MEMORY);
        write_memory(data, pack->memory);
    } else {
        write_uint(data, TAG_NULL);
    }

    if (pack->circuit) {
        write_uint(data, TAG_CIRCUIT);
        write_circuit(data, pack->circuit);
    } else {
        write_uint(data, TAG_NULL);
    }
}

void
write_pin(FileData *data, Pin *pin)
{
    write_uint(data, pin->x);
    write_uint(data, pin->y);
    write_uint(data, pin->state);
    write_string(data, strlen(pin->name), pin->name);
}

void
write_circuit(FileData *data, Circuit *circuit)
{
    World *world;

    world = worldcreatefake();
    worldbuild(world, circuit);
    write_body(data, world);
    worlddeletefake(world);
}

void
write_memory(FileData *data, Memory *memory)
{
    write_uint(data, memory->size);
    rle_encode(data, memory->data, memory->size);
}

void
write_bool(FileData *data, int val)
{
    char x = val != 0;
    filedata_push(data, &x, sizeof(char));
}

void
write_char(FileData *data, char val)
{
    filedata_push(data, &val, sizeof(char));
}

void
write_int(FileData *data, int val)
{
    filedata_push(data, &val, sizeof(int));
}

void
write_uchar(FileData *data, uchar val)
{
    filedata_push(data, &val, sizeof(uchar));
}

void
write_uint(FileData *data, uint val)
{
    filedata_push(data, &val, sizeof(uint));
}

void
write_float(FileData *data, float val)
{
    filedata_push(data, &val, sizeof(float));
}

void
write_string(FileData *data, uint len, char *str)
{
    uint i;

    write_uint(data, len);

    for (i = 0; i < len; i++) {
        write_char(data, str[i]);
    }
}

void
write_array(FileData *data, void *src, size_t nitems, size_t size)
{
    uint i;
    byte *src_bytes;

    src_bytes = src;

    for (i = 0; i < nitems; i++) {
        filedata_push(data, src_bytes, size);
        src_bytes += size;
    }
}

void
rle_encode(FileData *dest, uint *data, size_t nitems)
{
    uint len, val, i;

    val = data[0];
    len = 0;

    for (i = 0; i < nitems; i++) {
        if (val != data[i]) {
            write_uint(dest, len);
            write_uint(dest, val);
            len = 0;
            val = data[i]; 
        }
        len++;
    }
    write_uint(dest, len);
    write_uint(dest, val);
}
