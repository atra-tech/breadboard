/*
 *  BREADBOARD (2020)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Entry point, game engine initialisation, and main program loop.
 *
 *  */

#include "util/util.h"
#include "util/log.h"
#include "util/timer.h"
#include "core.h"
#include "simstate.h"

int
main(int argc, char **argv)
{
    Window *window;
    float dt;

    UNUSED(argc);
    UNUSED(argv);

    initatraengine();
    window = getmainwindow();

    simstateinit();
    simstatestart();

    while (1) {
        timerglobalupdate();
        dt = timerglobaldiff() * 0.001;
        
        atrastatehandle();
        atrastateupdate(dt);
        atrastaterender();

        atraswapbuffers(window);
    }
    simstatestop();

    return 0;
}
