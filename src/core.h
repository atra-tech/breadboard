/*
 *  BREADBOARD (2021)
 *
 *  Copyright (c), Robert Saarloos.
 *
 *  Full license can be found in the LICENSE file
 *
 *  Description:
 *  Provides obfuscation for engine initialisation, setup, and functions
 *  to get pointers to core variables (window and context).
 *
 * */

#ifndef BREADBOARD_CORE_H
#define BREADBOARD_CORE_H

#include "engine.h"

#define DEFAULT_WIDTH   1280
#define DEFAULT_HEIGHT  720

void initatraengine(void);
void stopatraengine(void);
Window * getmainwindow(void);
Context * getmaincontext(void);
Camera * getmaincamera(void);

#endif /* CAELUM_H */
