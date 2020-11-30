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

struct World;
struct Circuit;

void filesysinit(void);
void filesysshutdown(void);
void filegetworld(const char *path);
void filegetcircuit(const char *path);

#endif /* FILE_H */
