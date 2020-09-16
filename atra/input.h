/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Functions for polling input from keyboard and mouse
 *
 *  */

#ifndef ATRA_INPUT_H
#define ATRA_INPUT_H

#include <SDL2/SDL.h>
#include "util/util.h"

#define KEYCODE(key) SDLK_##key
#define SCANCODE(key) SDL_SCANCODE_##key
#define MOUSE_BUTTON(button) SDL_BUTTON(SDL_BUTTON_##button)

#define MOUSE_SENS_INIT 0.003f
#define MOUSE_BOUND_MIN 4.71f
#define MOUSE_BOUND_MAX 7.85f

typedef SDL_Keycode Keycode;
typedef SDL_Keymod Keymod;
typedef SDL_Scancode Scancode;

void keyinit(void);
uchar keystate(Keycode key);
const uchar *keygetstates(void);
char keygetascii(SDL_Event *e);

void mouseinitrelative(void);
void mousegetstaterelative(int *x, int *y);
void mousegetmoverelative(float *x, float *y);
uint mousegetstate(int *x, int *y);
void mouselock(void);
void mouseunlock(void);
int mouseislocked(void);
void mousesetsensitivity(float sens);

#endif
