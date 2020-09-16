/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Hash table module for associative arrays.
 *
 *  */

#include <string.h>

#include "htable.h"
#include "list.h"
#include "log.h"
#include "util.h"

#define INIT_TABLE_SIZE 16
#define MAX_LOAD_FACTOR 0.7

static void htablegrow(HTable *table);
/*static void htableshrink(HTable *table);*/
static int htablenullcheck(HTable *table, const char *msg);

HTable *
htablecreate(uint (*hash)(const void *),
        uint (*eq)(const void *, const void *))
{
    HTable *table = calloc(1, sizeof(HTable));
    void **keys = calloc(INIT_TABLE_SIZE, sizeof(void *));
    void **vals = calloc(INIT_TABLE_SIZE, sizeof(void *));

    if (checkmalloc(table) < 0 ||
        checkmalloc(keys) < 0 ||
        checkmalloc(vals) < 0)
        return NULL;

    table->keys = keys;
    table->vals = vals;
    table->size = INIT_TABLE_SIZE;
    table->hash = hash;
    table->equal = eq;

    return table;
}

void
htablegrow(HTable *table)
{
    uint i;
    uint size = table->size;
    uint newsize = size * 2;
    void **keys = table->keys;
    void **vals = table->vals;
    void **newkeys = calloc(newsize, sizeof(void *));
    void **newvals = calloc(newsize, sizeof(void *));

    if (checkmalloc(newkeys) < 0 ||
        checkmalloc(newvals) < 0)
        return;

    table->keys = newkeys;
    table->vals = newvals;
    table->size = newsize;
    table->occupied = 0;

    for (i = 0; i < size; i++) {
        if (keys[i]) {
            htableadd(table, keys[i], vals[i]);
        }
    }
    free(keys);
    free(vals);
}
/*
void
htableshrink(HTable *table)
{
    uint i;
    uint size = table->size;
    uint newsize = size * 0.5;
    void **keys = table->keys;
    void **vals = table->vals;
    void **newkeys = calloc(newsize, sizeof(void *));
    void **newvals = calloc(newsize, sizeof(void *));

    if (checkmalloc(newkeys) < 0 ||
        checkmalloc(newvals) < 0)
        return;

    table->keys = newkeys;
    table->vals = newvals;
    table->size = newsize;
    table->occupied = 0;

    for (i = 0; i < size; i++) {
        if (keys[i]) {
            htableadd(table, keys[i], vals[i]);
        }
    }
    free(keys);
    free(vals);
}*/

/* Sets all key and value references to NULL. 
 * NOTE: Does not deallocate memory used by elements. */
void
htableclear(HTable *table)
{
    uint i;
    void **keys = table->keys;
    void **vals = table->vals;

    if (htablenullcheck(table, "Unable to clear table"))
        return;
    
    for (i = 0; i < table->size; i++) {
        if (keys[i]) {
            free(keys[i]);
            free(vals[i]);
            keys[i] = NULL;
            vals[i] = NULL;
        }
    }
    table->size = INIT_TABLE_SIZE;
    table->occupied = 0;
}

int
htableempty(HTable *table)
{
    return !table->occupied;
}

/* Adds a new element to the table.
 * Resizes if table exceeds load factor. */
int
htableadd(HTable *table, void *key, void *val)
{
    uint i, hash, size;
    double loadfactor = (table->occupied + 1) / (double)table->size;
    void **keys, **vals;

    if (loadfactor > MAX_LOAD_FACTOR)
        htablegrow(table);
   
    size = table->size;
    hash = table->hash(key) % size;
    keys = table->keys;
    vals = table->vals;

    for (i = hash; i <= size; i++) {
        if (i == size)
            i = 0;
        if (!keys[i]) {
            keys[i] = key;
            vals[i] = val;
            logprint(LOG_INFO, "HTable: Added Elem: Key=\"$s\"", (char *)key);
            break;
        } else {
            if (table->equal(key, keys[i]))
                return 0;
        }
    }
    table->occupied++;

    return 1;
}

/* Retrieves value associated with the key. */
void *
htableget(HTable *table, const void *key)
{
    uint i;
    uint size = table->size;
    uint hash = table->hash(key) % size;
    void **keys = table->keys;
    void **vals = table->vals;

    for (i = hash; i <= size; i++) {
        if (i == size)
            i = 0;
        if (!keys[i])
            return NULL;
        if (table->equal(key, keys[i]))
            return vals[i];
    }

    return NULL;
}

/* Removes the specified element from the table. */ 
void
htableremove(HTable *table, const void *key)
{
    uint i;
    uint size = table->size;
    uint hash = table->hash(key) % size;
    void **keys = table->keys;
    void **vals = table->vals;

    for (i = hash; i <= size; i++) {
        if (i == size)
            i = 0;
        if (!keys[i])
            return;
        if (table->equal(key, keys[i])) {
            keys[i] = NULL;
            vals[i] = NULL;
            table->occupied--;
            return;
        }
    }
}

/* Deallocate specific element and remove from table */
void
htabledeallocelem(HTable *table, const void *key)
{
    uint i;
    uint size = table->size;
    uint hash = table->hash(key) % size;
    void **keys = table->keys;
    void **vals = table->vals;

    for (i = hash; i <= size; i++) {
        if (i == size)
            i = 0;
        if (!keys[i])
            return;
        if (table->equal(key, keys[i])) {
            free(keys[i]);
            free(vals[i]);
            keys[i] = NULL;
            vals[i] = NULL;
            table->occupied--;
            return;
        }
    }
}

/* Deallocate memory owned by all keys and values in table. */
void
htabledealloc(HTable *table)
{
    uint i;
    uint size = table->size;
    void **keys = table->keys;
    void **vals = table->vals;

    if (table) {
        for (i = 0; i < size; i++) {
            if (keys[i]) {
                free(keys[i]);
                free(vals[i]);
                keys[i] = NULL;
                vals[i] = NULL;
            }
        }
    }
    table->occupied = 0;
}

/* Free the table and internal lists. 
 * NOTE: Does not deallocate key or value elements. */
void
htabledelete(HTable *table)
{
    if (table) {
        free(table->keys);
        free(table->vals);
        free(table);
    }
}

int
htablenullcheck(HTable *table, const char *msg)
{
    if (!table) {
        logprint(LOG_ERROR, "htable: $s: Received NULL pointer: $s", msg);
        return 1;
    }
    return 0;
}

uint
htablestringhash(const void *str)
{
    ulong hash = 0;
    int c;
    const uchar *pstr = str;

    for (c = *pstr++; c; c = *pstr++) {
        hash = c + (hash << 6) + (hash << 16) - hash;
    }

    return hash;
}

uint
htablestringequal(const void *str1, const void *str2)
{
    return !strcmp(str1, str2);
}
