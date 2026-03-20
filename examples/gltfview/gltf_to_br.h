/*
 * gltf_to_br.h - glTF to BRender conversion (C89-safe public interface)
 *
 * Loads a .glb/.gltf file and converts meshes + materials into
 * BRender br_model/br_material objects, registered via BrModelAdd/BrMaterialAdd.
 * Builds a nested BRender actor tree mirroring the glTF node hierarchy.
 * Supports animation playback (v1: first animation, STEP + LINEAR).
 * No cgltf types leak into this header.
 */
#ifndef GLTF_TO_BR_H
#define GLTF_TO_BR_H

#include <brender.h>

/* ------------------------------------------------------------------ */
/* Animation data                                                       */
/* ------------------------------------------------------------------ */

typedef struct gltf_keyframes {
    int count;
    float *times;      /* sorted timestamps */
    float *values;     /* T: 3 floats, R: 4 floats (quat xyzw), S: 3 floats per key */
} gltf_keyframes;

typedef struct gltf_channel {
    int node_index;    /* glTF node this channel targets */
    int path;          /* 0=translation, 1=rotation, 2=scale, -1=skip */
    int interpolation; /* 0=STEP, 1=LINEAR */
    gltf_keyframes keys;
} gltf_channel;

typedef struct gltf_animation {
    float duration;
    int nchannels;
    gltf_channel *channels;
} gltf_animation;

/* ------------------------------------------------------------------ */
/* Scene                                                                */
/* ------------------------------------------------------------------ */

typedef struct gltf_scene {
    br_model    **models;
    br_material **materials;
    int           nmodels;
    int           nmaterials;
    br_vector3    bbox_min;
    br_vector3    bbox_max;
    br_actor    **node_actors;  /* glTF node index -> BRender actor */
    int           nnodes;
    br_actor     *root_actor;   /* root of the actor tree */
    float        *rest_trs;     /* nnodes * 10: [tx ty tz qx qy qz qw sx sy sz] */
    gltf_animation anim;
} gltf_scene;

/*
 * Parse a .glb or .gltf file, convert meshes and materials,
 * register them with BrModelAdd/BrMaterialAdd, build actor tree.
 * Returns 1 on success, 0 on failure.
 */
int  gltf_load_scene(const char *filename, gltf_scene *out);

/*
 * Free arrays allocated by gltf_load_scene.
 * Does NOT free the br_model/br_material objects themselves
 * (those are owned by BRender's registry).
 */
void gltf_free_scene(gltf_scene *scene);

/*
 * Evaluate animation at the given time (seconds) and update
 * actor transforms. Loops automatically at animation duration.
 * No-op if the scene has no animation.
 */
void gltf_update_animation(gltf_scene *scene, float time);

#endif /* GLTF_TO_BR_H */
