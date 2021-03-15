/*
 *  BREADBOARD (2021)
 *
 *  Copyright (c), Robert Saarloos.
 *
 *  Full license can be found in the LICENSE file
 *
 *  Description:
 *  Controllable entity for navigating the simulator world.
 *
 *  */

#ifndef PLAYER_H
#define PLAYER_H

#include "cglm/cglm.h"
#include "render.h"

typedef struct Player {
    Entity ent;
    vec3 vel;
    uint locked;
    float speed;
} Player;

void playerinit(Player *player);
void playerhandle(Player *player, Event *e);
void playerupdate(Player *player, float dt);

#endif /* PLAYER_H */
