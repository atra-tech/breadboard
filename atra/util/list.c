/* 
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  A dynamically growing/shrinking, general-purpose list type.
 *
 *  */

#include <string.h>

#include "list.h"
#include "log.h"

static void listgrow(List *list);
static void listshrink(List *list);

List * 
listcreate()
{
    List *list = malloc(sizeof(List));
    void **elems = malloc(INIT_LIST_CAP * sizeof(void *));

    if(checkmalloc(list) < 0 || checkmalloc(elems) < 0) {
        return NULL;
    }
    list->size = 0;
    list->cap = INIT_LIST_CAP;
    list->elem = elems;

    return list;
}

void *
listat(List *list, uint index)
{
    if (index >= list->size) {
        logprint(LOG_ERROR, "list: Unable to get element: "\
                            "Index out of range");
        return NULL;
    }
    return list->elem[index];
}

void
listgrow(List *list)
{
    void **newmem = realloc(list->elem, (list->cap * 2) * sizeof(void *));

    if (newmem) {
        list->elem = newmem;
        list->cap *= 2;
    } else {
        logprint(LOG_ERROR, "list: Unable to reallocate memory");
        list->size--;
        return;
    }
}

void
listshrink(List *list)
{
    void **newmem = realloc(list->elem, (list->cap / 2) * sizeof(void *));

    if (newmem) {
        list->elem = newmem;
        list->cap /= 2;
    } else {
        logprint(LOG_ERROR, "list: Unable to reallocate memory");
        list->size++;
        return;
    }
}

void
listpushback(List *list, void *elem)
{
    if (list->size == list->cap) {
        listgrow(list);
    }
    list->elem[list->size] = elem;
    list->size++;
}

void
listinsert(List *list, void *elem, uint index)
{
    uint i;
    
    if (list->size == list->cap) {
        listgrow(list);
    }
    list->size++;

    for (i = list->size; i > index; i--) {
        list->elem[i] = list->elem[i - 1];
    }
    list->elem[index] = elem;
}

void
listpopback(List *list)
{
    if (list->size <= 0) {
        list->size = 0;
        logprint(LOG_ERROR, "list: Unable to pop element: "\
                            "List is empty");
    }
    list->size--;

    if (list->size < list->cap / 2) {
        listshrink(list);
    }
}

void *
listback(List *list)
{
    if (list->size <= 0) {
        list->size = 0;
        logprint(LOG_ERROR, "list: Unable to get top element: "\
                            "List is empty");
    }

    return list->elem[list->size - 1];
}

void
listclear(List *list)
{
    if (list->size > 0)
    {
        void **newmem = realloc(list->elem, INIT_LIST_CAP * sizeof(void *));

        if (newmem) {
            list->elem = newmem;
            list->size = 0;
            list->cap = INIT_LIST_CAP;
        } else {
            logprint(LOG_ERROR, "list: Unable to reallocate memory");
        }
    }
}

void
listclearnoresize(List *list)
{
    list->size = 0;
}

void
listremove(List *list, uint index)
{
    uint i;

    for (i = index; i < list->size - 1; i++) {
        list->elem[i] = list->elem[i + 1];
    }
    list->size--;

    if (list->size < list->cap / 2) {
        listshrink(list);
    }
}

void
listremoverange(List *list, uint start, uint end)
{
    uint i, j;

    if (start > end) {
        return;
    }
    for (i = start; i <= end; i++) {
        list->elem[i] = NULL;
    }
    for (i = end + 1, j = start; i < list->size; i++, j++) {
        list->elem[j] = list->elem[i];
        list->elem[i] = NULL;
    }
    list->size -= end - start - 1;

    if (list->size < list->cap / 2) {
        listshrink(list);
    }
}

void
listfreemove(List *list, uint index)
{
    uint i;

    free(list->elem[index]);

    for (i = index; i < list->size - 1; i++) {
        list->elem[i] = list->elem[i + 1];
    }
    list->size--;

    if (list->size < list->cap / 2) {
        listshrink(list);
    }
}

void
listfreemoverange(List *list, uint start, uint end)
{
    uint i, j;

    if (start >= end) {
        return;
    }
    for (i = start; i <= end; i++) {
        free(list->elem[i]);
        list->elem[i] = NULL;
    }
    for (i = end + 1, j = start; i < list->size; i++, j++) {
        list->elem[j] = list->elem[i];
        free(list->elem[i]);
        list->elem[i] = NULL;
    }
    list->size -= end - start - 1;

    if (list->size < list->cap / 2) {
        listshrink(list);
    }
}

void
listmoveelem(List *list, uint index, uint dest)
{
    uint i;
    void *elem = list->elem[index];

    for (i = index; i < dest; i++) {
        list->elem[i] = list->elem[i + 1];
    }
    list->elem[dest] = elem;
}

void
listdealloc(List *list)
{
    uint i;

    for (i = 0; i < list->size; i++) {
        free(list->elem[i]);
    }
}

void
listdelete(List *list)
{
    free(list->elem);
    free(list);
}

void
veclistinit(VecList *list, size_t elemsize)
{
    list->data = malloc(INIT_LIST_CAP * elemsize * sizeof(float));
    list->elemsize = elemsize;
    list->cap = INIT_LIST_CAP;
    list->size = 0;
}

float *
veclistat(VecList *list, uint index)
{
    return list->data + index * list->elemsize;
}

void
veclistpush(VecList *list, float *vec)
{
    if (list->size >= list->cap) {
        veclistgrow(list);
    }
    memcpy(list->data + (list->size * list->elemsize),
            vec, list->elemsize * sizeof(float));
    list->size++;
}

void
veclistgrow(VecList *list)
{
    float *newmem;

    list->cap *= 2;
    newmem = realloc(list->data, list->cap * list->elemsize * sizeof(float));

    if (newmem) {
        list->data = newmem;
    } else {
        logprint(LOG_ERROR, "list: Unable to reallocate memory");
        die("Memory error");
    }
}

void
veclistclear(VecList *list)
{
    list->size = 0;
}

void
veclistdelete(VecList *list)
{
    free(list->data);
}

void
iveclistinit(IVecList *list, size_t elemsize)
{
    list->data = malloc(INIT_LIST_CAP * elemsize * sizeof(int));
    list->elemsize = elemsize;
    list->cap = INIT_LIST_CAP;
    list->size = 0;
}

int *
iveclistat(IVecList *list, uint index)
{
    return list->data + index * list->elemsize;
}

void
iveclistpush(IVecList *list, int *vec)
{
    if (list->size >= list->cap) {
        iveclistgrow(list);
    }
    memcpy(list->data + (list->size * list->elemsize),
            vec, list->elemsize * sizeof(float));
    list->size++;
}

void
iveclistgrow(IVecList *list)
{
    int *newmem;

    list->cap *= 2;
    newmem = realloc(list->data, list->cap * list->elemsize * sizeof(int));

    if (newmem) {
        list->data = newmem;
    } else {
        logprint(LOG_ERROR, "list: Unable to reallocate memory");
        die("Memory error");
    }
}

void
iveclistclear(IVecList *list)
{
    list->size = 0;
}

void
iveclistdelete(IVecList *list)
{
    free(list->data);
}
