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

#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include <stddef.h>

#define DATA_INIT_CAP 8

typedef unsigned char byte;

struct World;
struct Circuit;

enum data_tag {
    TAG_END,
    TAG_NULL,
    TAG_BOOL,
    TAG_CHAR,
    TAG_INT,
    TAG_UCHAR,
    TAG_UINT,
    TAG_FLOAT,
    TAG_STRING,
    TAG_ARRAY,
    TAG_SEG,
    TAG_PACK,
    TAG_PIN,
    TAG_CIRCUIT,
    TAG_MEMORY
};

typedef struct FileData{
    size_t size, cap;
    byte *data;
} FileData;

//void filesysinit(void);
//void filesysshutdown(void);
int filegetworld(const char *path, struct World *world);
int filesaveworld(const char *path, struct World *world);

#endif /* FILE_H */
