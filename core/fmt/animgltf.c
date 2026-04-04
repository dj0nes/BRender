/*
 * animgltf.c - glTF animation loading for BRender
 *
 * Parses glTF animation data (keyframes, channels, samplers) and binds
 * them to a BRender actor tree, producing a br_animation_set that can
 * be evaluated with BrAnimationSetUpdate() (core/v1db/animation.c).
 *
 * Designed to work alongside loadgltf.c: call BrFmtGLTFActorLoadMany()
 * first for scene geometry, then BrFmtGLTFAnimLoad() with the returned
 * actors to add animation support.
 *
 * Supports STEP and LINEAR interpolation with quaternion SLERP for
 * rotations. Animations named *_oneshot clamp at their end; others loop.
 */
#include <string.h>
#include <brender.h>

#include "cgltf.h"

/* ------------------------------------------------------------------ */
/* cgltf allocator callbacks (same pattern as loadgltf.c)              */
/* ------------------------------------------------------------------ */

static void *cgltf_alloc_anim(void *user, cgltf_size size)
{
    return BrResAllocate(user, size, BR_MEMORY_SCRATCH);
}

static void cgltf_free_anim(void *user, void *ptr)
{
    if(ptr == NULL)
        return;

    (void)user;
    BrResFree(ptr);
}

static cgltf_result cgltf_load_brfile_anim(const cgltf_memory_options *memory_options,
                                           const cgltf_file_options *file_options,
                                           const char *path, cgltf_size *size, void **data)
{
    (void)file_options;

    if((*data = BrFileLoad(memory_options->user_data, path, size)) == NULL)
        return cgltf_result_io_error;

    return cgltf_result_success;
}

static void cgltf_release_brfile_anim(const cgltf_memory_options *memory_options,
                                      const cgltf_file_options *file_options, void *data)
{
    (void)memory_options;
    (void)file_options;

    if(data != NULL)
        BrResFree(data);
}

/* ------------------------------------------------------------------ */
/* DFS actor-to-node binding                                           */
/* ------------------------------------------------------------------ */

/*
 * Walk the cgltf node tree and the br_actor tree in parallel,
 * establishing the actors[] and rest_trs[] mappings.
 *
 * This works because loadgltf.c builds actors in identical tree
 * order to cgltf: it adds children in reverse to compensate for
 * BrActorAdd's prepend behavior, so actor children end up in the
 * same order as cgltf children.
 */
static void bind_actors_dfs(const cgltf_node *node, const cgltf_data *data,
                            br_actor *actor, br_animation_set *set)
{
    br_int_32 node_idx = (br_int_32)(node - data->nodes);
    float    *trs;
    br_actor *child_actor;
    cgltf_size ci;

    if(node_idx < 0 || node_idx >= set->nactors)
        return;

    set->actors[node_idx] = actor;

    /*
     * Store rest-pose TRS. cgltf initialises defaults (T=0, R=identity, S=1)
     * even when the node uses a matrix form.
     */
    trs = &set->rest_trs[node_idx * 10];
    BrMemCpy(trs,     node->translation, 3 * sizeof(float));
    BrMemCpy(trs + 3, node->rotation,    4 * sizeof(float));
    BrMemCpy(trs + 7, node->scale,       3 * sizeof(float));

    /*
     * Recurse: match cgltf children to actor children in order.
     */
    child_actor = actor->children;
    for(ci = 0; ci < node->children_count && child_actor != NULL; ++ci) {
        bind_actors_dfs(node->children[ci], data, child_actor, set);
        child_actor = child_actor->next;
    }
}

/* ------------------------------------------------------------------ */
/* Animation parsing                                                   */
/* ------------------------------------------------------------------ */

static void parse_one_animation(const cgltf_animation *src, const cgltf_data *data,
                                br_animation_clip *dst, void *res)
{
    float      max_time = 0.0f;
    const char *name    = src->name ? src->name : "";
    br_size_t   name_len;
    cgltf_size  ci;

    name_len = BrStrLen(name);
    if(name_len >= sizeof(dst->identifier))
        name_len = sizeof(dst->identifier) - 1;
    BrMemCpy(dst->identifier, name, name_len);
    dst->identifier[name_len] = '\0';

    dst->loop = BR_TRUE;
    if(name_len >= 7 && BrStrCmp(name + name_len - 7, "oneshot") == 0)
        dst->loop = BR_FALSE;

    dst->nchannels = (br_int_32)src->channels_count;
    dst->channels  = BrResAllocate(res, src->channels_count * sizeof(br_animation_channel), BR_MEMORY_APPLICATION);
    BrMemSet(dst->channels, 0, src->channels_count * sizeof(br_animation_channel));

    for(ci = 0; ci < src->channels_count; ++ci) {
        const cgltf_animation_channel *ch   = &src->channels[ci];
        const cgltf_animation_sampler *samp = ch->sampler;
        br_animation_channel          *ac   = &dst->channels[ci];
        int                            components;
        br_int_32                      ki;

        if(!ch->target_node) {
            ac->path = -1;
            continue;
        }

        ac->target = (br_int_32)(ch->target_node - data->nodes);

        switch(ch->target_path) {
            case cgltf_animation_path_type_translation:
                ac->path = BR_ANIM_PATH_TRANSLATION;
                break;
            case cgltf_animation_path_type_rotation:
                ac->path = BR_ANIM_PATH_ROTATION;
                break;
            case cgltf_animation_path_type_scale:
                ac->path = BR_ANIM_PATH_SCALE;
                break;
            default:
                ac->path = -1;
                continue;
        }

        switch(samp->interpolation) {
            case cgltf_interpolation_type_step:
                ac->interpolation = BR_ANIM_INTERP_STEP;
                break;
            case cgltf_interpolation_type_linear:
                ac->interpolation = BR_ANIM_INTERP_LINEAR;
                break;
            default:
                ac->interpolation = BR_ANIM_INTERP_STEP;
                break;
        }

        ac->nkeys  = (br_int_32)samp->input->count;
        ac->times  = BrResAllocate(res, sizeof(float) * ac->nkeys, BR_MEMORY_APPLICATION);

        components = (ac->path == BR_ANIM_PATH_ROTATION) ? 4 : 3;
        ac->values = BrResAllocate(res, sizeof(float) * ac->nkeys * components, BR_MEMORY_APPLICATION);

        for(ki = 0; ki < ac->nkeys; ++ki) {
            cgltf_accessor_read_float(samp->input, ki, &ac->times[ki], 1);
            if(ac->times[ki] > max_time)
                max_time = ac->times[ki];
        }

        for(ki = 0; ki < ac->nkeys; ++ki) {
            cgltf_accessor_read_float(samp->output, ki, &ac->values[ki * components], components);
        }
    }

    dst->duration = max_time;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

br_animation_set *BR_PUBLIC_ENTRY BrFmtGLTFAnimLoad(const char *name, br_actor **actors, br_uint_32 nactors)
{
    void             *scratch;
    cgltf_data       *data;
    br_animation_set *set;
    cgltf_size        i;

    cgltf_options opts = {
        .type   = cgltf_file_type_invalid,
        .memory = {
            .alloc_func = cgltf_alloc_anim,
            .free_func  = cgltf_free_anim,
        },
        .file = {
            .read    = cgltf_load_brfile_anim,
            .release = cgltf_release_brfile_anim,
        },
    };

    /*
     * Scratch resource: parent for all cgltf allocations (freed at the end).
     */
    scratch = BrResAllocate(NULL, sizeof(int), BR_MEMORY_SCRATCH);
    opts.memory.user_data = scratch;

    if(cgltf_parse_file(&opts, name, &data) != cgltf_result_success) {
        BrResFree(scratch);
        return NULL;
    }

    if(cgltf_load_buffers(&opts, data, name) != cgltf_result_success) {
        BrResFree(scratch);
        return NULL;
    }

    if(data->animations_count == 0) {
        BrResFree(scratch);
        return NULL;
    }

    /*
     * Allocate the result structure. All sub-allocations are children of
     * this resource, so BrFmtGLTFAnimFree() is a single BrResFree().
     */
    set = BrResAllocate(NULL, sizeof(br_animation_set), BR_MEMORY_APPLICATION);
    BrMemSet(set, 0, sizeof(br_animation_set));

    set->nactors = (br_int_32)data->nodes_count;
    set->actors  = BrResAllocate(set, data->nodes_count * sizeof(br_actor *), BR_MEMORY_APPLICATION);
    set->rest_trs = BrResAllocate(set, data->nodes_count * 10 * sizeof(float), BR_MEMORY_APPLICATION);
    BrMemSet(set->actors, 0, data->nodes_count * sizeof(br_actor *));

    /*
     * DFS-walk the scene tree and actor tree in parallel to establish
     * the node-to-actor mapping and extract rest-pose TRS.
     */
    if(data->scene != NULL) {
        cgltf_size ri;
        cgltf_size nroots = data->scene->nodes_count;

        if(nroots > nactors)
            nroots = nactors;

        for(ri = 0; ri < nroots; ++ri) {
            bind_actors_dfs(data->scene->nodes[ri], data, actors[ri], set);
        }
    }

    /*
     * Parse animation channels.
     */
    set->nclips = (br_int_32)data->animations_count;
    set->active = 0;
    set->clips  = BrResAllocate(set, data->animations_count * sizeof(br_animation_clip), BR_MEMORY_APPLICATION);
    BrMemSet(set->clips, 0, data->animations_count * sizeof(br_animation_clip));

    for(i = 0; i < data->animations_count; ++i) {
        parse_one_animation(&data->animations[i], data, &set->clips[i], set);
    }

    BrResFree(scratch);
    return set;
}
