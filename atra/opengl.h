/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Functions and structures related to direct interaction
 *  between Atra Engine and OpenGL/SDL.
 *  Graphics contexts, windows, cameras, buffers, etc.
 *
 *  */

#ifndef ATRA_OPENGL_H
#define ATRA_OPENGL_H

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include "entity.h"

#define ATRA_GL_VERSION_MAJOR 3
#define ATRA_GL_VERSION_MINOR 3

enum window_callback {
    CB_SCROLLUP,
    CB_SCROLLDN
};

typedef struct {
    SDL_Window *wndw;
    char *name;
    int width;
    int height;
    int init;
    int vsync;
} Window;

typedef struct {
    SDL_GLContext ctxt;
    int init;
} Context;

typedef SDL_Event Event;

void windowinit(Window *win, const char *title, int width, int height);
void windowdelete(Window *win);
int windoweventhandle(Window *win, Event *e, Camera *cam);
void windowsetvsync(Window *win, int vsync);
void windowsetcontext(Window *win, Context *con);
void windowsetcallback(int type, void (*func)());

void contextinit(Context *con, Window *win);
void contextdelete(Context *con);

#endif /* ATRA_OPENGL_H */
