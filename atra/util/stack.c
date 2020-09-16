/* 
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  A dynamically growing/shrinking, general-purpose stack type.
 *
 *  */

#include "stack.h"
#include "util.h"
#include "log.h"

Stack * 
stackcreate()
{
    Stack *s = malloc(sizeof(Stack));
    s->elem = (void **) malloc(INIT_STACK_CAP * sizeof(void *));

    if(checkmalloc(s) < 0 || checkmalloc(s->elem) < 0)
        return NULL;
    s->size = 0;
    s->cap = INIT_STACK_CAP;

    return s;
}

void
stackpush(Stack *stack, void *elem)
{
    if (!stack || !elem) {
        logprint(LOG_ERROR, "stack: Unable to push element: "\
                            "Received NULL argument");
        return;
    }

    /* Double stack capacity if capacity is reached */
    if (stack->size == stack->cap) {
        void **newmem = 
            (void **)realloc(stack->elem, (stack->cap * 2) * sizeof(void *));
        if (newmem) {
            stack->elem = newmem;
            stack->cap *= 2;
        } else {
            logprint(LOG_ERROR, "stack: Unable to reallocate memory");
            stack->size--;
            return;
        }
    }
    stack->elem[stack->size] = elem;
    stack->size++;
}

void
stackpop(Stack *stack)
{
    if (!stack) {
        logprint(LOG_ERROR, "stack: Unable to pop element: "\
                            "Received NULL stack pointer");
        return;
    }

    if (stack->size <= 0) {
        stack->size = 0;
        logprint(LOG_ERROR, "stack: Unable to pop element: "\
                            "Stack is empty");
    }
    stack->size--;

    /* Half stack capacity if size falls under half capacity */
    if (stack->size < stack->cap / 2) {
        void **newmem = 
            (void **)realloc(stack->elem, (stack->cap / 2) * sizeof(void *));
        if (newmem) {
            stack->elem = newmem;
            stack->cap /= 2;
        } else {
            logprint(LOG_ERROR, "stack: Unable to reallocate memory");
            stack->size++;
            return;
        }
    }
}

void *
stacktop(Stack *stack)
{
    if (!stack) {
        logprint(LOG_ERROR, "stack: Unable to get top element: "\
                            "Received NULL stack pointer");
        return NULL;
    }

    if (stack->size <= 0) {
        stack->size = 0;
        logprint(LOG_ERROR, "stack: Unable to get top element: "\
                            "Stack is empty");
    }

    return stack->elem[stack->size - 1];
}

void
stackclear(Stack *stack)
{
    if (!stack) {
        logprint(LOG_ERROR, "stack: Unable to clear stack: "\
                            "Received NULL stack pointer");
        return;
    }

    /* Reset the stack to default parameters */
    if (stack->size > 0)
    {
        void **newmem = realloc(stack->elem, INIT_STACK_CAP * sizeof(void *));
        if (newmem) {
            stack->elem = newmem;
            stack->size = 0;
            stack->cap = INIT_STACK_CAP;
        } else {
            logprint(LOG_ERROR, "stack: Unable to reallocate memory");
        }
    }
}

void
stackdelete(Stack *stack)
{
    if (!stack) {
        logprint(LOG_ERROR, "stack: Unable to delete stack: "\
                            "Received NULL stack pointer");
        return;
    }
    free(stack->elem);
    free(stack);
}
