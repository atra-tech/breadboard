/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Functions and structures related to direct interaction
 *  between Atra Engine and OpenGL/SDL.
 *
 *  */

#include "opengl.h"
#include "util/log.h"
#include "util/util.h"

#warning window: TODO: windoweventhandle() scroll functions must be \
deferred to dev-defined callback function

/* Event Callbacks */
static void (*cb_scrollup)();
static void (*cb_scrolldn)();

void
windowinit(Window *win, const char *title, int width, int height)
{
    if (!win) {
        logprint(LOG_FATAL, "window: Unable to create window: " \
                            "NULL pointer received");
        die("Fatal error! Terminating...");
    }
    win->wndw = SDL_CreateWindow(title,
                                 SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED,
                                 width,
                                 height,
                                 SDL_WINDOW_OPENGL | 
                                 SDL_WINDOW_SHOWN | 
                                 SDL_WINDOW_RESIZABLE);
    if (!win->wndw) {
        logprint(LOG_FATAL, "window: Unable to create window: " \
                            "SDL error: $s", SDL_GetError());
        die("Fatal error! Terminating...");
    }
}

void
windowdelete(Window *win)
{
    if (!win) {
        logprint(LOG_ERROR, "window: Unable to delete window: " \
                            "NULL pointer received");
        return;
    }
    SDL_DestroyWindow(win->wndw);
}

int
windoweventhandle(Window *win, Event *e, Camera *cam)
{
    int result = 0;

    if (!win || !e || !cam) {
        logprint(LOG_ERROR, "window: Unable to handle event: " \
                            "NULL pointer received");
        return result;
    }
    if (e->type == SDL_WINDOWEVENT) {
        if (e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            win->width = e->window.data1;
            win->height = e->window.data2;
            cameracalcprojmatrix(cam, win->width, win->height);
            glViewport(0, 0, win->width, win->height);
            result = 1;
        }
    }

    return result;
}

void
windowsetvsync(Window *win, int vsync)
{
    if (SDL_GL_SetSwapInterval(vsync) < 0) {
        logprint(LOG_INFO, "window: Unable to change vsync state: " \
                "SDL error: $s", SDL_GetError());
    } else {
        win->vsync = vsync;
    }
}

void
windowsetcontext(Window *win, Context *con)
{
    SDL_GL_MakeCurrent(win->wndw, con->ctxt);
}

void
windowsetcallback(int type, void (*func)())
{
    if (!func) {
        logprint(LOG_ERROR, "window: Unable to set window callback: " \
                            "NULL function pointer received");
        return;
    }
    switch (type) {
    case CB_SCROLLUP:
        cb_scrollup = func;
        break;
    case CB_SCROLLDN:
        cb_scrolldn = func;
        break;
    default:
        logprint(LOG_WARN, "window: Received invalid callback type");
        break;
    }
}

void
contextinit(Context *con, Window *win)
{
    if (!con || !win) {
        logprint(LOG_FATAL, "context: Unable to initialise context: " \
                            "NULL pointer received");
        die("Fatal error! Terminating...");
    }

    con->ctxt = SDL_GL_CreateContext(win->wndw);
    if (!con->ctxt) {
        logprint(LOG_FATAL, "context: Unable to initialise context: " \
                            "SDL error: $s", SDL_GetError());
        die("Fatal error! Terminating...");
    }
    con->init = 1;
}

void
contextdelete(Context *con)
{
    if (!con) {
        logprint(LOG_ERROR, "context: Unable to delete context: " \
                            "NULL pointer received");
        return;
    }
    SDL_GL_DeleteContext(con->ctxt);
    con->init = 0;
}
