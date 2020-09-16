#include <stdlib.h>

#include "state.h"
#include "util/util.h"
#include "util/stack.h"
#include "util/log.h"

static void stateinit();
static int checkstate(State *);

static Stack *statestack;
static int init = 0;

static void
stateinit()
{
    if (!init) {
        logprint(LOG_INFO, "Initialising state system...");
        statestack = stackcreate();
        init = 1;
    }
}

static int
checkstate(State *state)
{
    if (!state) {
        logprint(LOG_ERROR, "state: Received NULL state pointer.");
        return -1;
    }

    return 0;
}

State *
statecreate(void(*handlefunc)(),
            void(*updatefunc)(float dt),
            void(*renderfunc)())
{
    State *s;

    stateinit();
    s = (State *)malloc(sizeof(State));
    if (checkmalloc(s) < 0) {
        logprint(LOG_ERROR, "state: Unable to create state: "\
                            "Memory allocation error");
        return NULL;
    }
    s->handle = handlefunc;
    s->update = updatefunc;
    s->render = renderfunc;

    return s;
}

int
statepush(State *state)
{
    if (checkstate(state) < 0) {
        return -1;
    }
    stateinit();
    stackpush(statestack, state);

    return 0;
}

void
statepop()
{
    stateinit();
    stackpop(statestack);
}

State *
statetop()
{
    State *state;
    stateinit();
    state = stacktop(statestack);

    if (checkstate(state) < 0) {
        logprint(LOG_ERROR, "state: Unable to get state from stack.");
        return NULL;
    }

    return (State *)stacktop(statestack);
}

int
stateempty()
{
    stateinit();

    if (statestack->size <= 0) {
        return 1;
    }
    return 0;
}

void statedelete(State *state)
{
    if (checkstate(state) < 0) {
        logprint(LOG_ERROR, "state: Unable to delete state: "\
                           "NULL state pointer received");
        return;
    }
    free(state);
}

void
statestop()
{
    stateinit();
    stackdelete(statestack);
}
