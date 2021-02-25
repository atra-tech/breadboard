/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  General, useful tools that can find use in many parts of the program
 *
 *  */

#ifndef ATRA_UTIL_H
#define ATRA_UTIL_H

#include <stdlib.h>

#include "list.h"

#define PI 3.14159265359

#define BIT(a) (1 << a)
#define BIT_SET(a, b) a = (a | b)
#define BIT_CLR(a, b) a = (a & ~b)

#define UNUSED(a) (void)a
#define SIGN(a) ((a > 0) - (a < 0))

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

int checkmalloc(void *ptr);
void die(char *msg);

char * loadfile(const char *path);
void savefile(const char *path, const char *data, size_t nitems);
List * listfilesat(char *path, char *ext);
void removefile(const char *path);

char * strdup(const char *str);

#endif
