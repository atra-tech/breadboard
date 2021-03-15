/*
 *  BREADBOARD (2021)
 *
 *  Copyright (c), Robert Saarloos.
 *
 *  Full license can be found in the LICENSE file
 *
 *  Description:
 *  State data and functions for the logic simulator.
 *  Handles input, update and render stages for the simulator.
 *
 *  */

#include "simstate.h"
#include "util/log.h"
#include "util/util.h"
#include "core.h"
#include "world.h"
#include "player.h"
#include "gui.h"
#include "ui.h"

#define INV_16            0.0625
#define CAMERA_ZOOM_SPEED camera->zoom * INV_16
#define CAMERA_MAX_ZOOM   512
#define CAMERA_MIN_ZOOM   16

static void simhandle(void);
static void simupdate(float dt);
static void simrender(void);
static void zoomcamerain(void);
static void zoomcameraout(void);
static void handlescrollwheel(Event *e);

static Camera *camera;
static State *simstate;
static Player player;


void
simstateinit(void)
{
    simstate = statecreate(simhandle, simupdate, simrender);
    camera = getmaincamera();

    worldpush(worldcreate());
    playerinit(&player);
    camerabindtoentity(camera, &player.ent);
    ui_init();
}

void
simstatestart(void)
{
    atrapushstate(simstate);
}

void
simstatestop(void)
{
    if (statetop() != simstate) {
        logprint(LOG_FATAL, "simstate: Unable to pop state: " \
                "State not at top of stack");
        die("Fatal engine state stack error: Bad order");
    }
    ui_shutdown();
    worldshutdown();
    atrapopstate(0);
    stopatraengine();
}

void
simhandle(void)
{
    Event e;
    int redrawgui = 0;

    if (guigetkbdfocus()) {
        SDL_StartTextInput();
        player.locked = 1;
    }
    SDL_PumpEvents();
    
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            simstatestop();
            exit(EXIT_SUCCESS);
        }

        if (!redrawgui) {
            redrawgui = windoweventhandle(getmainwindow(), &e, camera);
        } else {
            windoweventhandle(getmainwindow(), &e, camera);
        }

        if (redrawgui) {
            guiredraw();
            redrawgui = 0;
        }
        
        if(!guihandle(&e))
        {
            if (!player.locked) {
                playerhandle(&player, &e);
                worldhandle(&e);
                handlescrollwheel(&e);
            }
        }
    }

    if (!guigetkbdfocus()) {
        SDL_StopTextInput();
        player.locked = 0;
    }
}

void
simupdate(float dt)
{
    playerupdate(&player, dt);
    cameraupdate(camera);
    guiupdate(dt);
    worldupdate(dt);
}

void
simrender(void)
{
    glClearColor(0.784f, 0.761f, 0.725f, 1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);

    worldrender(camera);
    guirender();
    worldrendermouseseg(camera);
}

void
handlescrollwheel(Event *e)
{
    if (e->type == SDL_MOUSEWHEEL) {
        if (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
            e->wheel.y *= -1;
        }

        if (e->wheel.y < 0) {
            zoomcameraout();
        } else if (e->wheel.y > 0) {
            zoomcamerain();
        }
        cameracalcprojmatrix(camera, camera->width, camera->height);
    }
}

void
zoomcamerain(void) {
    camera->zoom += CAMERA_ZOOM_SPEED;

    if (camera->zoom > CAMERA_MAX_ZOOM) {
        camera->zoom = CAMERA_MAX_ZOOM;
    }
}

void
zoomcameraout(void) {
    camera->zoom -= CAMERA_ZOOM_SPEED;

    if (camera->zoom < CAMERA_MIN_ZOOM) {
        camera->zoom = CAMERA_MIN_ZOOM;
    }
}

/*
 *  GUI TESTING
 */
void
buttonmouseover(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 0, 200, 128, 32);
    elem->layer->dirty = 1;
}

void
buttonmouseleave(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 0, 168, 128, 32);
    elem->layer->dirty = 1;
}

void
buttonmousedown(GUIElement *elem, ...)
{
    guisetbacktexture(elem, 0, 232, 128, 32);
    elem->layer->dirty = 1;
}

void
buttonmouseup(GUIElement *elem, ...)
{
    if (elem->state & GUI_STATE_MOUSEOVER) {
        guisetbacktexture(elem, 0, 200, 128, 32);
    } else {
        guisetbacktexture(elem, 0, 168, 128, 32);
    }
    elem->layer->dirty = 1;
}
