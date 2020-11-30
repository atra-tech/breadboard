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
#include "input.h"
#include "world.h"
#include "core.h"
#include "util/log.h"

#define PLAYER_SPEED 16

void
playerinit(Player *player)
{
    memset(player, 0, sizeof(Player));
    player->ent.x = 512;
    player->ent.y = 512;
    player->ent.z = 100.0f;
    player->ent.ry = PI;
    player->speed = PLAYER_SPEED;
}

void
playerhandle(Player *player, Event *e)
{
    glm_vec3_zero(player->vel);

    if (keystate(KEYCODE(LSHIFT)) ||
            keystate(KEYCODE(RSHIFT))) {
        player->speed = PLAYER_SPEED * 2;
    } else {
        player->speed = PLAYER_SPEED;
    }
    if (keystate(KEYCODE(w))) {
        player->vel[1] = player->speed;
    }
    if (keystate(KEYCODE(s))) {
        player->vel[1] = -player->speed;
    }
    if (keystate(KEYCODE(d))) {
        player->vel[0] = player->speed;
    }
    if (keystate(KEYCODE(a))) {
        player->vel[0] = -player->speed;
    }
}

void
playerupdate(Player *player, float dt)
{
    //Camera *cam = getmaincamera();
    vec3 dt_vec = {dt, dt, dt};
    vec3 tform;

    glm_vec3_mul(player->vel, dt_vec, tform);

    if (!player->locked) {
        player->ent.x += tform[0];
        player->ent.y += tform[1];
    }

    if (player->ent.x < 1.001) {
        player->ent.x = 1.001;
    } else if (player->ent.x > WORLD_SIZE - 1.001) {
        player->ent.x = WORLD_SIZE - 1.001;
    }
    
    if (player->ent.y < 1.001) {
        player->ent.y = 1.001;
    } else if (player->ent.y > WORLD_SIZE - 1.001) {
        player->ent.y = WORLD_SIZE - 1.001;
    }

}
