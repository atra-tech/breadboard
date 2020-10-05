/*
 *  BREADBOARD (2020)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Provides obfuscation for engine initialisation, setup, and functions
 *  to get pointers to core variables (window and context).
 *
 * */

#include "core.h"
#include "util/log.h"
#include "util/timer.h"
#include "render.h"
#include "entity.h"
#include "state.h"
#include "input.h"
#include "gui.h"

static Window window;
static Context context;
static Camera camera;

void
initatraengine(void)
{
    atrainit("Breadboard", DEFAULT_WIDTH, DEFAULT_HEIGHT, &window, &context);
    texinit();
    fontinit();
    keyinit();

    camerainit(&camera, DEFAULT_WIDTH, DEFAULT_HEIGHT, CAMERA_ORTHOGRAPHIC);
    camera.zoom = 64;
    /*glm_ortho(0, camera.width / camera.zoom,
              0, camera.height / camera.zoom,
              0.1f, 1000.0f,
              camera.projmat);*/

    guiinit(&camera);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void
stopatraengine(void)
{
    guishutdown();
    fontquit();
    texquit();
    contextdelete(&context);
    windowdelete(&window);
    atrashutdown();
    logstop();
}

Window *
getmainwindow(void)
{
    return &window;
}

Context *
getmaincontext(void)
{
    return &context;
}

Camera *
getmaincamera(void)
{
    return &camera;
}
