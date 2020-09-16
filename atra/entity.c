/*
 *  ATRA ENGINE (2019)
 *
 *  Copyright Robert Saarloos, All rights reserved.
 *
 *  Description:
 *  Structures and functions described with a position and rotation.
 *
 *  */

#include <math.h>
#include <stdlib.h>
#include <cglm/cglm.h>

#include "entity.h"
#include "util/log.h"
#include "util/util.h"

void
camerainit(Camera *cam, int width, int height, int mode)
{
    if (!cam) {
        logprint(LOG_ERROR, "camera: Unable to initialise camera: " \
                            "NULL camera pointer received");
        return;
    }
    cam->fov = DEFAULT_CAM_FOV;
    cam->zoom = DEFAULT_CAM_ZOOM;
    cam->mode = mode;
    cam->boundent = NULL;
    cam->ent.x = 0.0f;
    cam->ent.y = 0.0f;
    cam->ent.z = 0.0f;
    cam->ent.rx = 0.0f;
    cam->ent.ry = 0.0f;
    cam->ent.rz = 0.0f;

    cameracalcprojmatrix(cam, width, height);
}

void
cameracalcprojmatrix(Camera *cam, int width, int height)
{
    cam->width = width;
    cam->height = height;
    if (cam->mode == CAMERA_ORTHOGRAPHIC) {
        glm_ortho(-width / cam->zoom,
                  width / cam->zoom,
                  -height / cam->zoom,
                  height / cam->zoom,
                  0.1f, 1000.0f,
                  cam->projmat);
    } else {
        glm_perspective(cam->fov,
                        (float)cam->width / cam->height,
                        0.1f, 1000.0f,
                        cam->projmat);
    }
}

void
cameracalcviewmatrix(Camera *cam)
{
    float pos_data[3] = {cam->ent.x, cam->ent.y, cam->ent.z};
    float cen_data[3]; 

    glm_vec3_add(pos_data, cam->dirvec, cen_data);
    glm_lookat(pos_data, cen_data, cam->upvec, cam->viewmat);
}

void
cameracalcprojviewmatrix(Camera *cam)
{
    glm_mat4_mul_sse2(cam->projmat, cam->viewmat, cam->projviewmat);
}

void
cameracalcvectors(Camera *cam)
{
    cam->dirvec[0] = cosf(cam->ent.rx) * sinf(cam->ent.ry);
    cam->dirvec[1] = sinf(cam->ent.rx);
    cam->dirvec[2] = cosf(cam->ent.rx) * cosf(cam->ent.ry);

    cam->rightvec[0] = sinf(cam->ent.ry - (PI / 2.0f));
    cam->rightvec[1] = 0.0f;
    cam->rightvec[2] = cosf(cam->ent.ry - (PI / 2.0f));

    glm_vec3_cross(cam->rightvec, cam->dirvec, cam->upvec);
}

void
camerabindtoentity(Camera *cam, Entity *ent)
{
    cam->boundent = ent;
}

void
cameraupdate(Camera *cam)
{
    if (!cam->boundent) {
        logprint(LOG_ERROR, "camera: Unable to update camera: " \
                            "Camera not bound to entity.");
        return;
    }
    cam->ent.x = cam->boundent->x;
    cam->ent.y = cam->boundent->y;
    cam->ent.z = cam->boundent->z;
    cam->ent.rx = cam->boundent->rx;
    cam->ent.ry = cam->boundent->ry;
    cam->ent.rz = cam->boundent->rz;
    cameracalcvectors(cam);
    cameracalcviewmatrix(cam);
    cameracalcprojviewmatrix(cam);
}

void
camerasetfov(Camera *cam, float fov)
{
    cam->fov = fov;
    cameracalcprojmatrix(cam, cam->width, cam->height);
}
