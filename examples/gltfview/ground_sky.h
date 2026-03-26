/*
 * ground_sky.h - Ground and sky plane utilities for gltfview
 *
 * Creates textured quad actors for ground/sky planes that track the
 * camera position. Works with all rendering backends (software, ATI, PVR).
 */
#ifndef GROUND_SKY_H
#define GROUND_SKY_H

#include "brender.h"

typedef struct {
    br_actor    *actor;       /* add to g_world with BrActorAdd */
    br_material *material;
    float        uv_scale;   /* texture repeats per world unit */
    float        scroll_u;   /* sky UV scroll accumulator (U axis) */
    float        scroll_v;   /* sky UV scroll accumulator (V axis) */
    float        scroll_rate; /* UV units per second of diagonal drift */
    float        tile_sky;   /* sky UV span (for scroll_v starting offset) */
} gs_plane_t;

/* Load a texture from BMP/PNG/JPG path. Returns NULL on failure.
 * Result is registered with BrMapAdd. */
br_pixelmap *gs_load_texture(const char *path);

/* Create a ground plane quad at height y.
 * uv_scale: texture repeats per world unit (e.g. 1.0 = one tile per unit).
 * Quad size is 20x scene_radius in each direction. */
gs_plane_t gs_create_ground(br_pixelmap *texture, float scene_radius,
                             float y, float uv_scale);

/* Create a sky plane at the given height.
 * uv_scale: texture repeats per scene_radius (e.g. 0.5 = gentle tiling).
 * Full-bright (ka=1, kd=0), no scene lighting. */
gs_plane_t gs_create_sky(br_pixelmap *texture, float scene_radius,
                          float height, float uv_scale);

/* Set the sky scroll rate in UV units per second.
 * MW2 used tile_sky * anim_rate * framerate (e.g. 4.0 * 0.0004 * 30 = 0.048).
 * Pass 0 to disable scrolling (default). */
void gs_set_sky_scroll_rate(gs_plane_t *plane, float rate);

/* Snap plane XZ to camera and update UV offset so the texture stays
 * fixed in world space. For sky planes, also advances the diagonal
 * scroll animation by dt seconds. Call each frame before rendering. */
void gs_update(gs_plane_t *plane, float cam_x, float cam_z, float dt);

#endif /* GROUND_SKY_H */
