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

static void simhandle(void);
static void simupdate(float dt);
static void simrender(void);

static Camera *camera;
static State *simstate;
static World *world;
static Player player;

static void createtestgui();

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
    //
    
    createtestgui();
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
    }

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
        guihandle(&e);
        /*  TODO: handle player
         *  handle world if GUI falls through
         *  */
    }

    if (!guigetkbdfocus()) {
        SDL_StopTextInput();
    }
}

void
simupdate(float dt)
{
    playerupdate(dt);
    cameraupdate(camera);

    guiupdate(dt);
}

void
simrender(void)
{
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    //worldrender(world, camera);
    /*  TODO: Render gates/ICs
     *  Render wire layers */

    worldrender(world, camera);
    guirender();
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

    elem[0] = guielemcreate(0.2f, 0.2f, 0.4f, 0.4f,
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
    guisetgridtext(elem[2], "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.");
    //guisetgridtext(elem[2]->textgrid, "Data *\ngetData(&frame)\n{\n\treturn frame->data;\n}");
    //guisetgridtext(elem[2]->textgrid, "Data * getData(&frame) { return frame->data; }");
    //guisetgridtext(elem[2]->textgrid, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18");

    guisetbordersize(elem[0], 16);
    guisetbordersize(elem[1], 8);
    guisetbordersize(elem[2], 8);

    guiaddelem(elem[0], elem[1]);
    guiaddelem(elem[0], elem[2]);
    guiaddelem(elem[1], elem[3]);
    guiaddelem(elem[1], elem[4]);
    guiaddelem(elem[1], elem[5]);

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
