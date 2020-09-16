/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Hash table module for associative arrays.
 *
 *  */

#ifndef ATRA_HTABLE_H
#define ATRA_HTABLE_H

#include "list.h"
#include "util.h"

typedef struct {
    uint size, occupied;
    void **keys;
    void **vals;
    uint (*hash)(const void *);
    uint (*equal)(const void *, const void*);
} HTable;

HTable *htablecreate(uint (*hash)(const void *),
        uint (*eq)(const void *, const void *));
void htableclear(HTable *);
int htableempty(HTable *);
int htableadd(HTable *, void *key, void *val);
void *htableget(HTable *, const void *key);
void htableremove(HTable *, const void *key);
void htabledeallocelem(HTable *, const void *key);
void htabledealloc(HTable *);
void htabledelete(HTable *);

uint htablestringhash(const void *);
uint htablestringequal(const void *, const void *);

#endif /* ATRA_HTABLE_H */
