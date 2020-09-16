/*
 *  BREADBOARD (2020)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
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
