/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Simple interface for SDL's timer utility.
 *
 *  */

#include <SDL2/SDL.h>

#include "timer.h"

static float globaltimeprev;
static float globaltimenow;
static float globaltimediff;

void
timerstart(Timer *t)
{
    t->prev = SDL_GetTicks();
    t->now = SDL_GetTicks();
    t->diff = 0;
}

void
timerupdate(Timer *t)
{
    t->now = SDL_GetTicks();
    t->diff = t->now - t->prev;
    t->prev = t->now;
}

void
timerglobalstart()
{
    globaltimeprev = SDL_GetTicks();
    globaltimenow = SDL_GetTicks();
    globaltimediff = 0;
}

void
timerglobalupdate()
{
    globaltimenow = SDL_GetTicks();
    globaltimediff = globaltimenow - globaltimeprev;
    globaltimeprev = globaltimenow;
}

float
timerglobalcurrent()
{
    return globaltimenow;
}

float
timerglobaldiff()
{
    return globaltimediff;
}
