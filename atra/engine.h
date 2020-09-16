/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  The main Atra Engine system.  Initialises all essential engine
 *  components and provides an interface for game state interaction.
 *
 *  */

#ifndef ATRA_ENGINE_H
#define ATRA_ENGINE_H

#include <SDL2/SDL_image.h>

#include "state.h"
#include "opengl.h"

#define ENGINE_VERSION "ALPHA 0.1.3"

void atrainit(const char* title, int width, int height, Window *, Context *);
void atrapushstate(State *state);
void atrapopstate(void);
void atrastatehandle(void);
void atrastateupdate(float dt);
void atrastaterender(void);
void atraswapbuffers(Window *);
void atrashutdown(void);

#endif
