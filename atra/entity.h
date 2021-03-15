/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Structures and functions described with a position and rotation.
 *
 *  */

#ifndef ATRA_ENTITY_H
#define ATRA_ENTITY_H

#include "cglm/cglm.h"

#define DEFAULT_CAM_ZOOM 128.0f
#define DEFAULT_CAM_FOV 70.0f

enum camera_mode {
    CAMERA_PROJECTION,
    CAMERA_ORTHOGRAPHIC
};

typedef struct {
    float x, y, z;
    float rx, ry, rz;
} Entity;

typedef struct {
    int width, height;
    float zoom, fov;
    int mode;
    Entity ent;
    Entity *boundent;
    mat4 projmat;
    mat4 viewmat;
    mat4 projviewmat;
    vec3 dirvec;
    vec3 rightvec;
    vec3 upvec;
} Camera;

void camerainit(Camera *cam, int width, int height, int mode);
void cameracalcprojmatrix(Camera *cam, int width, int height);
void cameracalcviewmatrix(Camera *cam);
void cameracalcprojviewmatrix(Camera *cam);
void cameracalcvectors(Camera *cam);
void camerabindtoentity(Camera *cam, Entity *ent);
void cameraupdate(Camera *cam);
void camerasetfov(Camera *cam, float fov);

#endif /* ATRA_ENTITY_H */
