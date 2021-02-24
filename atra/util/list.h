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

#include <stdlib.h>

#define INIT_LIST_CAP 8

typedef struct {
    void **elem;
    unsigned int size;
    unsigned int cap;
} List;

typedef struct {
    void *data;
    size_t elemsize;
    unsigned int size;
    unsigned int cap;
} DList;

typedef struct {
    float *data;
    size_t elemsize;
    unsigned int size;
    unsigned int cap;
} VecList;

typedef struct {
    int *data;
    size_t elemsize;
    unsigned int size;
    unsigned int cap;
} IVecList;

List *listcreate();
void *listat(List *list, unsigned int index);
void listpushback(List *list, void *elem);
void listinsert(List *list, void *elem, unsigned int index);
void liatpopback(List *list);
void *listback(List *list);
void listclear(List *list);
void listclearnoresize(List *list);
void listremove(List *list, unsigned int index);
void listremoverange(List *list, unsigned int start, unsigned int end);
void listfreemove(List *list, unsigned int index);
void listfreemoverange(List *list, unsigned int start, unsigned int end);
void listmoveelem(List *list, unsigned int index, unsigned int dest);
void listdealloc(List *list);
void listdelete(List *list);

void dlistinit(DList *list, size_t elemsize);
void *dlistat(DList *list, unsigned int index);
void dlistpush(DList *list, void *elem);
void dlistinsert(DList *list, void *elem, unsigned int index);
void dlistpop(DList *list);
void *dlistback(DList *list);
void dlistclear(DList *list);
void dlistclearnoresize(DList *list);
void dlistremove(DList *list, unsigned int index);
void dlistremoverange(DList *list, unsigned int start, unsigned int end);
void dlistfreemove(DList *list, unsigned int index);
void dlistfreemoverange(DList *list, unsigned int start, unsigned int end);
void dlistmoveelem(DList *list, unsigned int index, unsigned int dest);
void dlistdealloc(DList *list);
void dlistdelete(DList *list);

void veclistinit(VecList *list, size_t elemsize);
float * veclistat(VecList *list, unsigned int index);
void veclistpush(VecList *list, const float *vec);
void veclistgrow(VecList *list);
void veclistclear(VecList *list);
void veclistdelete(VecList *list);

void iveclistinit(IVecList *list, size_t elemsize);
int * iveclistat(IVecList *list, unsigned int index);
void iveclistpush(IVecList *list, const int *vec);
void iveclistgrow(IVecList *list);
void iveclistclear(IVecList *list);
void iveclistdelete(IVecList *list);

#endif /* ATRA_LIST_H */
