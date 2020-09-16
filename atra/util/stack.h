/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  A dynamically growing/shrinking, general-purpose stack type.
 *
 *  */

#ifndef ATRA_STACK_H
#define ATRA_STACK_H

#define INIT_STACK_CAP 8

typedef struct {
    void **elem;
    int size;
    int cap;
} Stack;

Stack *stackcreate();
void stackpush(Stack *stack, void *elem);
void stackpop(Stack *stack);
void *stacktop(Stack *stack);
void stackclear(Stack *stack);
void stackdelete(Stack *stack);

#endif /* ATRA_STACK_H */
