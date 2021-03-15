/*
 *  BREADBOARD (2021)
 *
 *  Copyright (c), Robert Saarloos.
 *
 *  Full license can be found in the LICENSE file
 *
 *  Description:
 *  Provides a set of tools for world data serialisation, saving, and loading.
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

int filegetworld(const char *path, struct World *world);
int filesaveworld(const char *path, struct World *world);

#endif /* FILE_H */
