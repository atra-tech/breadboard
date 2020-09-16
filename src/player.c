/*
 *  BREADBOARD (2020)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Controllable entity for navigating the simulator world.
 *
 *  */

#include "player.h"


void
playerinit(Player *player)
{
    memset(player, 0, sizeof(Player));
    player->ent.x = 512;
    player->ent.y = 512;
    player->ent.z = -100.0f;
}

void
playerhandle(Event *e)
{
    //Input handling
}

void
playerupdate(float dt)
{
    //Update position based on player velocity
}
