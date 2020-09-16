#ifndef ATRA_STATE_H
#define ATRA_STATE_H

#include "opengl.h"

typedef struct {
    void(*handle)();
    void(*update)(float dt);
    void(*render)();
} State;

State *statecreate(void(*handlefunc)(),
                   void(*updatefunc)(float dt),
                   void(*renderfunc)());
int statepush(State *);
void statepop(void);
State* statetop(void);
void statedelete(State *);
int stateempty(void);
void statestop(void);

#endif
