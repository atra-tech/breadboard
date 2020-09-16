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

#define PI 3.14159265359

#define BIT(a) (1 << a)
#define BIT_SET(a, b) a = (a | b)
#define BIT_CLR(a, b) a = (a & ~b)

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

int checkmalloc(void *ptr);
char *loadfile(const char *path);
void die(char *msg);

#endif
