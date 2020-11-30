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
#include "util/util.h"
#include "util/htable.h"

static void parsedatafile(const char *data);
static uint read_uint(char **data);
static int read_int(char **data);
static float read_float(char **data);

static HTable *worldtable;
static HTable *circuittable;

void
filesysinit(void)
{
    worldtable = htablecreate(htablestringhash, htablestringequal);
    circuittable = htablecreate(htablestringhash, htablestringequal);
}

void
filesysshutdown(void)
{
    htabledealloc(worldtable);
    htabledealloc(circuittable);
    htabledelete(worldtable);
    htabledelete(circuittable);
}

void
filegetworld(const char *path)
{

}

void
filegetcircuit(const char *path)
{

}

void
parsedatafile(const char *path)
{

    char *data = loadfile(path);

    /*  Convert raw data to world
     *  Convert world to circuit
     *  Store both in hash table
     * */

    

    free(data);
}

uint
read_uint(char **data)
{
    uint *p = (uint *) *data;
    *data += sizeof(uint);

    return *p;
}

int
read_int(char **data)
{
    int *p = (int *) *data;
    *data += sizeof(int);

    return *p;
}

float
read_float(char **data)
{
    float *p = (float *) *data;
    *data += sizeof(float);

    return *p;
}
