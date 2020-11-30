/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  A dynamically growing/shrinking, general-purpose list type.
 *
 *  */

#ifndef ATRA_LIST_H
#define ATRA_LIST_H

#define INIT_LIST_CAP 8

#include "util.h"

typedef struct {
    void **elem;
    uint size;
    uint cap;
} List;

typedef struct {
    void *data;
    size_t elemsize;
    uint size;
    uint cap;
} DList;

typedef struct {
    float *data;
    size_t elemsize;
    uint size;
    uint cap;
} VecList;

typedef struct {
    int *data;
    size_t elemsize;
    uint size;
    uint cap;
} IVecList;

List *listcreate();
void *listat(List *list, uint index);
void listpushback(List *list, void *elem);
void listinsert(List *list, void *elem, uint index);
void liatpopback(List *list);
void *listback(List *list);
void listclear(List *list);
void listclearnoresize(List *list);
void listremove(List *list, uint index);
void listremoverange(List *list, uint start, uint end);
void listfreemove(List *list, uint index);
void listfreemoverange(List *list, uint start, uint end);
void listmoveelem(List *list, uint index, uint dest);
void listdealloc(List *list);
void listdelete(List *list);

void dlistinit(DList *list, size_t elemsize);
void *dlistat(DList *list, uint index);
void dlistpush(DList *list, void *elem);
void dlistinsert(DList *list, void *elem, uint index);
void dliatpop(DList *list);
void *dlistback(DList *list);
void dlistclear(DList *list);
void dlistclearnoresize(DList *list);
void dlistremove(DList *list, uint index);
void dlistremoverange(DList *list, uint start, uint end);
void dlistfreemove(DList *list, uint index);
void dlistfreemoverange(DList *list, uint start, uint end);
void dlistmoveelem(DList *list, uint index, uint dest);
void dlistdealloc(DList *list);

void veclistinit(VecList *list, size_t elemsize);
float * veclistat(VecList *list, uint index);
void veclistpush(VecList *list, const float *vec);
void veclistgrow(VecList *list);
void veclistclear(VecList *list);
void veclistdelete(VecList *list);

void iveclistinit(IVecList *list, size_t elemsize);
int * iveclistat(IVecList *list, uint index);
void iveclistpush(IVecList *list, const int *vec);
void iveclistgrow(IVecList *list);
void iveclistclear(IVecList *list);
void iveclistdelete(IVecList *list);

#endif /* ATRA_LIST_H */
