/*
 * camera_fps.h - FPS roaming camera for gltfview
 *
 * Pure C89, no platform deps. Same file used by Win98 and Mac builds.
 * Call fps_camera_update() each frame with dt and input state.
 */
#ifndef CAMERA_FPS_H
#define CAMERA_FPS_H

typedef struct {
    float pos_x, pos_y, pos_z;
    float yaw;            /* radians */
    float pitch;          /* radians, positive = up */
    float base_speed;     /* units/sec at normal speed */
    float sensitivity;    /* radians per pixel of mouse delta */
} fps_camera_t;

typedef struct {
    int fwd;              /* W */
    int back;             /* S */
    int left;             /* A */
    int right;            /* D */
    int fast;             /* Shift (5x speed) */
    float mouse_dx;       /* pixels right since last frame */
    float mouse_dy;       /* pixels down since last frame */
} fps_input_t;

/* Init at position, looking toward target point.
 * base_speed is derived from scene_radius. */
void fps_camera_init_look_at(fps_camera_t *cam,
                              float px, float py, float pz,
                              float tx, float ty, float tz,
                              float scene_radius);

/* Advance camera by dt seconds given current input. */
void fps_camera_update(fps_camera_t *cam, float dt, const fps_input_t *in);

/* Get unit forward direction vector. */
void fps_camera_forward(const fps_camera_t *cam,
                        float *fx, float *fy, float *fz);

#endif /* CAMERA_FPS_H */
