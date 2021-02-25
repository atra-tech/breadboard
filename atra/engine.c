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

#include "engine.h"
#include "util/util.h"
#include "util/log.h"
#include "util/stack.h"

static void initsdl(void);
static void initimg(void);
static void initglew(void);

static int init = 0;

void
atrainit(const char* title, int width, int height, Window *win, Context *con)
{
    logclear();
    logprint(LOG_INFO, "Initialising Atra Engine " ENGINE_VERSION);

    initsdl();

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, ATRA_GL_VERSION_MAJOR);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, ATRA_GL_VERSION_MINOR);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 16);
    //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);

    windowinit(win, title, width, height);
    contextinit(con, win);
    windowsetcontext(win, con);

    SDL_SetWindowMinimumSize(win->wndw, 800, 600);

    initimg();
    initglew();

    if (SDL_GL_SetSwapInterval(1) < 0) {
        logprint(LOG_ERROR, "atra: Error setting vertical sync: " \
                            "SDL error: $s", SDL_GetError());
    }
    init = 1;
}

void
atrapushstate(State *state)
{
    if (!init) {
        logprint(LOG_ERROR, "atra: Unable to push state: " \
                            "Engine not initialised");
        return;
    }
    statepush(state);
}

void
atrapopstate(void)
{
    if (!init) {
        logprint(LOG_ERROR, "atra: Unable to pop state: " \
                            "Engine not initialised");
        return;
    }
    statepop();

    if (stateempty())
        atrashutdown();
}

void
atrastatehandle(void)
{
    State *state = statetop();

    state->handle();
}

void
atrastateupdate(float dt)
{
    State *state = statetop();

    state->update(dt);
}

void
atrastaterender(void)
{
    State *state = statetop();

    state->render();
}

void
atraswapbuffers(Window *win)
{ 
    SDL_GL_SwapWindow(win->wndw);
}

void
atrashutdown(void)
{
    if (!init) {
        logprint(LOG_ERROR, "atra: Unable to shut down engine: " \
                            "Engine not initialised");
        return;
    }
    logprint(LOG_INFO, "Shutting down...");
    statestop();
    IMG_Quit();
    SDL_Quit();
}

void
initsdl(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        logprint(LOG_FATAL, "atra: SDL initialisation failure: " \
                            "SDL error: $s", SDL_GetError());
        die("Fatal error. Terminating...");
    }
}

void
initimg(void)
{
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        logprint(LOG_FATAL, "atra: SDL_image initialisation failure: " \
                            "SDL error: $s", SDL_GetError());
        die("Fatal error. Terminating...");
    }
}

void
initglew(void)
{
    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();

    if (glewError != GLEW_OK) {
        logprint(LOG_FATAL, "atra: GLEW initialisation error: $s", 
                            glewGetErrorString(glewError));
        die("Fatal error. Terminating...");
    }
}

void
noiniterror(void)
{
    die("Pre-initialisation call-to-function error: Stopping");
}
