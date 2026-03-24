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

/* Snap plane XZ to camera and update UV offset so the texture stays
 * fixed in world space. Call each frame before rendering. */
void gs_update(gs_plane_t *plane, float cam_x, float cam_z);

#endif /* GROUND_SKY_H */
