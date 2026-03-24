/*
 * camera_fps.c - FPS roaming camera for gltfview
 *
 * Pure C89, no platform deps. Compiles under Open Watcom and clang/gcc.
 */
#include "camera_fps.h"
#include <math.h>

#define FPS_PITCH_LIMIT  1.5f
#define FPS_FAST_MUL     5.0f

void fps_camera_init_look_at(fps_camera_t *cam,
                              float px, float py, float pz,
                              float tx, float ty, float tz,
                              float scene_radius)
{
    float dx, dy, dz, len;

    cam->pos_x = px;
    cam->pos_y = py;
    cam->pos_z = pz;

    dx = tx - px;
    dy = ty - py;
    dz = tz - pz;
    len = (float)sqrt((double)(dx * dx + dy * dy + dz * dz));
    if (len > 0.0001f) {
        dx /= len;
        dy /= len;
        dz /= len;
    } else {
        dx = 0.0f;
        dy = 0.0f;
        dz = -1.0f;
    }

    cam->yaw         = (float)atan2((double)dx, (double)dz);
    cam->pitch        = (float)asin((double)dy);
    cam->base_speed   = scene_radius * 0.5f;
    cam->sensitivity  = 0.003f;
}

void fps_camera_forward(const fps_camera_t *cam,
                        float *fx, float *fy, float *fz)
{
    float cp = (float)cos((double)cam->pitch);
    *fx = cp * (float)sin((double)cam->yaw);
    *fy = (float)sin((double)cam->pitch);
    *fz = cp * (float)cos((double)cam->yaw);
}

void fps_camera_update(fps_camera_t *cam, float dt, const fps_input_t *in)
{
    float speed, fwd_x, fwd_y, fwd_z, right_x, right_z;
    float cp, sy, cy;

    /* Mouse look */
    cam->yaw   -= in->mouse_dx * cam->sensitivity;
    cam->pitch -= in->mouse_dy * cam->sensitivity;
    if (cam->pitch >  FPS_PITCH_LIMIT) cam->pitch =  FPS_PITCH_LIMIT;
    if (cam->pitch < -FPS_PITCH_LIMIT) cam->pitch = -FPS_PITCH_LIMIT;

    /* Speed: base * dt, 5x with shift */
    speed = cam->base_speed * dt;
    if (in->fast)
        speed *= FPS_FAST_MUL;

    /* Forward vector (full 3D, so W flies toward where you look) */
    cp    = (float)cos((double)cam->pitch);
    sy    = (float)sin((double)cam->yaw);
    cy    = (float)cos((double)cam->yaw);
    fwd_x = cp * sy;
    fwd_y = (float)sin((double)cam->pitch);
    fwd_z = cp * cy;

    /* Right vector (horizontal only): up x forward, normalized */
    right_x =  cy;
    right_z = -sy;

    if (in->fwd) {
        cam->pos_x += fwd_x * speed;
        cam->pos_y += fwd_y * speed;
        cam->pos_z += fwd_z * speed;
    }
    if (in->back) {
        cam->pos_x -= fwd_x * speed;
        cam->pos_y -= fwd_y * speed;
        cam->pos_z -= fwd_z * speed;
    }
    if (in->left) {
        cam->pos_x -= right_x * speed;
        cam->pos_z -= right_z * speed;
    }
    if (in->right) {
        cam->pos_x += right_x * speed;
        cam->pos_z += right_z * speed;
    }
}
