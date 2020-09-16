/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Functions for polling input from keyboard and mouse
 *
 *  */

#include "input.h"
#include "util/log.h"

static const uchar *keystates;
static int init_x, init_y;
static float mousesens;
static int mouselocked;

void
keyinit(void)
{
    keystates = SDL_GetKeyboardState(NULL);
    mouselocked = 0;
}

const uchar *
keygetstates(void)
{
    return keystates;
}

uchar
keystate(Keycode key)
{
    Scancode scan = SDL_GetScancodeFromKey(key);

    return keystates[scan];
}

void
mouseinitrelative(void)
{
    mousesens = MOUSE_SENS_INIT;
    SDL_GetRelativeMouseState(&init_x, &init_y);
}

void
mousegetstaterelative(int *x, int *y)
{
    SDL_GetRelativeMouseState(x, y);
}

void
mousegetmoverelative(float *x, float *y)
{
    int x_tmp, y_tmp;
    SDL_GetRelativeMouseState(&x_tmp, &y_tmp);
    *x = (init_x - x_tmp) * mousesens;
    *y = (init_y - y_tmp) * mousesens;
}

uint
mousegetstate(int *x, int *y)
{
    return SDL_GetMouseState(x, y);
}

void
mouselock(void)
{
    if (SDL_SetRelativeMouseMode(SDL_TRUE) < 0) {
        logprint(LOG_ERROR, "mouse: Relative mouse mode is not supported.");
    }
    else {
        mouselocked = 1;
    }
}

void
mouseunlock(void)
{
    if (SDL_SetRelativeMouseMode(SDL_FALSE) < 0) {
        logprint(LOG_ERROR, "mouse: Error unsetting relative mouse.");
    }
    else {
        mouselocked = 0;
    }
}

int mouseislocked(void) {
    return mouselocked;
}

void
mousesetsensitivity(float sens)
{
    mousesens = sens;
}
