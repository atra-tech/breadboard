/*
 *  BREADBOARD (2020)
 *
 *  Copyright Robert Saarloos, All rights reserved.
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

#define INV_16            0.0625
#define CAMERA_ZOOM_SPEED camera->zoom * INV_16
#define CAMERA_MAX_ZOOM   512
#define CAMERA_MIN_ZOOM   16

static void simhandle(void);
static void simupdate(float dt);
static void simrender(void);
static void createtestgui(void);
static void zoomcamerain(void);
static void zoomcameraout(void);
static void handlescrollwheel(Event *e);

static Camera *camera;
static State *simstate;
static World *world;
static Player player;


void
simstateinit(void)
{
    simstate = statecreate(simhandle, simupdate, simrender);
    camera = getmaincamera();
   
    world = malloc(sizeof(World));
    worldinit(world);
    playerinit(&player);
    camerabindtoentity(camera, &player.ent);

    //TODO: Create world (or load from file)
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
    //TODO: Delete resources and clean up
    atrapopstate();
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
            // TODO: Add quit confirmation dialog
            simstatestop();
            //stopatraengine();
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
                worldhandle(world, &e);
                handlescrollwheel(&e);
            }
        }
        /*  TODO: handle player
         *  handle world if GUI falls through
         *  */
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
    worldupdate(world, dt);

    guiupdate(dt);
}

void
simrender(void)
{
    glClearColor(0.784f, 0.761f, 0.725f, 1.0f);
    //glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    //worldrender(world, camera);
    /*  TODO: Render gates/ICs
     *  Render wire layers */
    worldrender(world, camera);
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

void
createtestgui(void)
{
    GUILayer *layer;
    GUIElement *elem[10];

    layer = guilayercreate("res/shader/guivert.glsl",
            "res/shader/guifrag.glsl",
            "res/texture/gui.png");

    elem[0] = guielemcreate(0.10f, 0.10f, 0.25f, 0.25f,
            GUI_CUSTOM, 
            GUI_ATTR_MOVABLE |
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_HASBORDER,
            "main_frame");

    elem[1] = guielemcreate(0.05f, 0.05f, 0.425f, 0.9f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_HASBORDER,
            "inner_left");

    elem[2] = guielemcreate(0.525f, 0.05f, 0.425f, 0.9f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_HASBORDER |
            GUI_ATTR_HASTEXT |
            GUI_ATTR_SCROLLABLE |
            GUI_ATTR_SCROLLBAR |
            GUI_ATTR_TEXTINPUT |
            GUI_ATTR_MULTILINE |
            GUI_ATTR_WRAPTEXT,
            "inner_right");

    elem[3] = guielemcreate(0.05f, 0.775f, 0.9f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "menuopt_0");
    
    elem[4] = guielemcreate(0.05f, 0.550f, 0.9f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "menuopt_1");
    
    elem[5] = guielemcreate(0.05f, 0.325f, 0.9f, 0.2f,
            GUI_CUSTOM,
            GUI_ATTR_HASTEXTURE |
            GUI_ATTR_CLICKABLE,
            "menuopt_2");

    guilinklayer(elem[0], layer);
    guienabletextlayer(layer, "res/shader/fontvert.glsl",
            "res/shader/fontfrag.glsl",
            "res/font/maledict.png",
            8, 8);
    guitextscrollenable(elem[2]);
    
    guitextgridadd(guitextgridcreate(8, 8, 0, 2), elem[2]);
    guisetgridtext(elem[2], "Example text.");

    guisetbordersize(elem[0], 16);
    guisetbordersize(elem[1], 8);
    guisetbordersize(elem[2], 8);

    guiaddelem(elem[0], elem[1]);
    guiaddelem(elem[0], elem[2]);
    guiaddelem(elem[1], elem[3]);
    guiaddelem(elem[1], elem[4]);
    guiaddelem(elem[2], elem[5]);

    guisetbacktexture(elem[0], 0, 8, 128, 128);
    guisetcornertexture(elem[0], 8, 0, 8, 8);
    guisetedgetexture(elem[0], 0, 0, 8, 8);

    guisetbacktexture(elem[1], 128, 8, 128, 128);
    guisetcornertexture(elem[1], 20, 0, 4, 4);
    guisetedgetexture(elem[1], 16, 0, 4, 4);

    guisetbacktexture(elem[2], 128, 8, 128, 128);
    guisetcornertexture(elem[2], 20, 0, 4, 4);
    guisetedgetexture(elem[2], 16, 0, 4, 4);
 
    guisetbacktexture(elem[3], 0, 168, 128, 32);
    guisetbacktexture(elem[4], 0, 168, 128, 32);
    guisetbacktexture(elem[5], 0, 168, 128, 32);
 
    GUIScrollBar *bar = guiscrollbarcreate(16, 8, 0, 0);
    guiscrollbaradd(bar, elem[2]);
    //guisetscrollbarbacktex(elem[2], 128, 8, 4, 32);
    //guisetscrollbarbartex(elem[2], 0, 8, 4, 32);
    guisetscrollbarbacktex(elem[2], 0, 296, 8, 64);
    guisetscrollbarbartex(elem[2], 8, 296, 8, 64);

    guisetcursortexture(elem[2], 0, 0, 8, 8);

    guiseteventcallback(elem[3], GUI_EVENT_MOUSEOVER, buttonmouseover);
    guiseteventcallback(elem[3], GUI_EVENT_MOUSELEAVE, buttonmouseleave);
    guiseteventcallback(elem[3], GUI_EVENT_MOUSEDOWN, buttonmousedown);
    guiseteventcallback(elem[3], GUI_EVENT_MOUSEUP, buttonmouseup);

    guiseteventcallback(elem[4], GUI_EVENT_MOUSEOVER, buttonmouseover);
    guiseteventcallback(elem[4], GUI_EVENT_MOUSELEAVE, buttonmouseleave);
    guiseteventcallback(elem[4], GUI_EVENT_MOUSEDOWN, buttonmousedown);
    guiseteventcallback(elem[4], GUI_EVENT_MOUSEUP, buttonmouseup);
    
    guiseteventcallback(elem[5], GUI_EVENT_MOUSEOVER, buttonmouseover);
    guiseteventcallback(elem[5], GUI_EVENT_MOUSELEAVE, buttonmouseleave);
    guiseteventcallback(elem[5], GUI_EVENT_MOUSEDOWN, buttonmousedown);
    guiseteventcallback(elem[5], GUI_EVENT_MOUSEUP, buttonmouseup);

}
