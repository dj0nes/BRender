/*
 * gltf_to_br.c - glTF to BRender mesh/material conversion + animation
 *
 * Ported to BlazingRenderer (CMake build, float-based BRender).
 * Uses cgltf for glTF parsing and stb_image for texture loading.
 *
 * Scene loading builds a nested BRender actor tree mirroring the glTF
 * node hierarchy. Vertices stay in local mesh space; BRender composes
 * transforms down the tree during rendering.
 *
 * Animation: parses the first glTF animation (STEP + LINEAR interp),
 * evaluates per-channel keyframes, and writes TRS-composed matrices
 * to actor transforms each frame.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <brender.h>
#include <brv1db.h>
#include "cgltf.h"

/* stb_image: include as static to avoid symbol conflicts with BRender's
 * internal copy (which uses STBI_NO_STDIO and custom allocators). */
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "gltf_to_br.h"

#define LOGF(fmt, ...) do { printf("LOG: " fmt "\n", __VA_ARGS__); fflush(stdout); } while(0)
#define LOG(msg)       do { printf("LOG: %s\n", msg); fflush(stdout); } while(0)

#define MAX_ANIM_NODES 256

/* Base directory of the glTF file, for resolving relative image URIs */
static char g_base_dir[260];

static void set_base_dir(const char *gltf_path) {
    const char *last_slash = NULL, *p;
    for (p = gltf_path; *p; p++) {
        if (*p == '/' || *p == '\\') last_slash = p;
    }
    if (last_slash) {
        int len = (int)(last_slash - gltf_path + 1);
        if (len >= (int)sizeof(g_base_dir)) len = (int)sizeof(g_base_dir) - 1;
        memcpy(g_base_dir, gltf_path, len);
        g_base_dir[len] = '\0';
    } else {
        g_base_dir[0] = '\0';
    }
}

/* ------------------------------------------------------------------ */
/* Textures                                                             */
/* ------------------------------------------------------------------ */

static br_pixelmap *load_texture(cgltf_image *image) {
    br_pixelmap *pm;
    unsigned char *pixels;
    int w, h, channels, x, y;
    char path[520];

    if (!image || !image->uri) return NULL;

    /* Build full path from base dir + URI */
    snprintf(path, sizeof(path), "%s%s", g_base_dir, image->uri);

    pixels = stbi_load(path, &w, &h, &channels, 4); /* force RGBA */
    if (!pixels) {
        LOGF("texture load failed: %s", path);
        return NULL;
    }

    LOGF("texture loaded: %s (%dx%d)", image->uri, w, h);

    /* Create RGB_888 pixelmap (BlazingRenderer GL path prefers 24-bit) */
    pm = BrPixelmapAllocate(BR_PMT_RGB_888, w, h, NULL, 0);
    if (!pm) {
        stbi_image_free(pixels);
        return NULL;
    }

    /* Convert RGBA8888 to RGB888 */
    for (y = 0; y < h; y++) {
        unsigned char *dst = (unsigned char *)((char *)pm->pixels + y * pm->row_bytes);
        unsigned char *src = pixels + y * w * 4;
        for (x = 0; x < w; x++) {
            dst[x*3 + 0] = src[x*4 + 0];
            dst[x*3 + 1] = src[x*4 + 1];
            dst[x*3 + 2] = src[x*4 + 2];
        }
    }

    stbi_image_free(pixels);

    pm->origin_x = 0;
    pm->origin_y = 0;

    BrMapAdd(pm);
    return pm;
}

/* ------------------------------------------------------------------ */
/* Materials                                                            */
/* ------------------------------------------------------------------ */

static br_material *convert_material(cgltf_material *gmat, cgltf_data *data) {
    br_material *mat;
    float *bc;
    unsigned char r, g, b;
    float roughness;

    mat = BrMaterialAllocate(gmat->name ? (char *)gmat->name : "gltf_mat");

    /* Extract base color from PBR metallic-roughness */
    bc = gmat->pbr_metallic_roughness.base_color_factor;
    r = (unsigned char)(bc[0] * 255.0f);
    g = (unsigned char)(bc[1] * 255.0f);
    b = (unsigned char)(bc[2] * 255.0f);
    mat->colour = BR_COLOUR_RGB(r, g, b);

    /* Lighting coefficients */
    mat->ka = BR_UFRACTION(0.10);
    mat->kd = BR_UFRACTION(0.70);
    roughness = gmat->pbr_metallic_roughness.roughness_factor;
    mat->ks = BR_UFRACTION(1.0f - roughness);
    mat->power = BR_SCALAR(20.0);

    mat->flags = BR_MATF_LIGHT | BR_MATF_SMOOTH;
    if (gmat->double_sided)
        mat->flags |= BR_MATF_TWO_SIDED;

    /* Load base color texture if present */
    if (gmat->pbr_metallic_roughness.base_color_texture.texture) {
        cgltf_texture *tex = gmat->pbr_metallic_roughness.base_color_texture.texture;
        if (tex->image) {
            br_pixelmap *tpm = load_texture(tex->image);
            if (tpm) {
                mat->colour_map = tpm;
                mat->flags |= BR_MATF_MAP_COLOUR;
                LOGF("material '%s': texture '%s' (%dx%d)",
                     gmat->name ? gmat->name : "?",
                     tex->image->uri ? tex->image->uri : "?",
                     tpm->width, tpm->height);
            }
        }
    }

    BrMatrix23Identity(&mat->map_transform);
    BrMaterialUpdate(mat, BR_MATU_ALL);
    BrMaterialAdd(mat);

    return mat;
}

static br_material *get_default_material(void) {
    static br_material *def = NULL;
    if (!def) {
        def = BrMaterialAllocate("gltf_default");
        def->colour = BR_COLOUR_RGB(200, 200, 200);
        def->ka = BR_UFRACTION(0.10);
        def->kd = BR_UFRACTION(0.70);
        def->ks = BR_UFRACTION(0.30);
        def->power = BR_SCALAR(20.0);
        def->flags = BR_MATF_LIGHT | BR_MATF_SMOOTH;
        BrMatrix23Identity(&def->map_transform);
        BrMaterialUpdate(def, BR_MATU_ALL);
        BrMaterialAdd(def);
    }
    return def;
}

/* ------------------------------------------------------------------ */
/* Geometry helpers                                                     */
/* ------------------------------------------------------------------ */

/* Find an accessor by attribute type in a primitive's attribute list */
static cgltf_accessor *find_accessor(cgltf_primitive *prim,
                                     cgltf_attribute_type type) {
    for (cgltf_size i = 0; i < prim->attributes_count; i++) {
        if (prim->attributes[i].type == type)
            return prim->attributes[i].data;
    }
    return NULL;
}

/* Transform a position by a column-major 4x4 matrix (used for AABB only) */
static void transform_point(const float *m, const float *in, float *out) {
    out[0] = m[0]*in[0] + m[4]*in[1] + m[8]*in[2]  + m[12];
    out[1] = m[1]*in[0] + m[5]*in[1] + m[9]*in[2]  + m[13];
    out[2] = m[2]*in[0] + m[6]*in[1] + m[10]*in[2] + m[14];
}

/* Convert a primitive's geometry into a br_model.
 * Vertices stay in local mesh space (no world transform baking). */
static br_model *convert_primitive(cgltf_mesh *mesh, cgltf_primitive *prim,
                                   int prim_idx, br_material *face_mat) {
    cgltf_accessor *pos_acc, *norm_acc, *uv_acc, *idx_acc;
    cgltf_size nv, nf, vi, fi;
    br_model *model;
    int has_normals, has_uvs;
    char name[64];
    float tmp[3];

    if (prim->type != cgltf_primitive_type_triangles)
        return NULL;

    pos_acc = find_accessor(prim, cgltf_attribute_type_position);
    if (!pos_acc) return NULL;

    norm_acc = find_accessor(prim, cgltf_attribute_type_normal);
    uv_acc = find_accessor(prim, cgltf_attribute_type_texcoord);
    idx_acc = prim->indices;

    nv = pos_acc->count;
    if (nv > 65535) {
        LOGF("skipping primitive: %u verts exceeds br_uint_16 limit", (unsigned)nv);
        return NULL;
    }

    if (idx_acc)
        nf = idx_acc->count / 3;
    else
        nf = nv / 3;

    if (nf == 0 || nv == 0) return NULL;

    snprintf(name, sizeof(name), "%s_p%d",
             mesh->name ? mesh->name : "mesh", prim_idx);

    model = BrModelAllocate(name, (int)nv, (int)nf);
    if (!model) return NULL;

    has_normals = (norm_acc != NULL);
    has_uvs = (uv_acc != NULL);

    /* Fill vertices in local mesh space */
    for (vi = 0; vi < nv; vi++) {
        cgltf_accessor_read_float(pos_acc, vi, tmp, 3);
        model->vertices[vi].p.v[0] = BR_SCALAR(tmp[0]);
        model->vertices[vi].p.v[1] = BR_SCALAR(tmp[1]);
        model->vertices[vi].p.v[2] = BR_SCALAR(tmp[2]);

        if (has_normals) {
            cgltf_accessor_read_float(norm_acc, vi, tmp, 3);
            model->vertices[vi].n.v[0] = tmp[0];
            model->vertices[vi].n.v[1] = tmp[1];
            model->vertices[vi].n.v[2] = tmp[2];
        }

        if (has_uvs) {
            float uv[2];
            cgltf_accessor_read_float(uv_acc, vi, uv, 2);
            model->vertices[vi].map.v[0] = BR_SCALAR(uv[0]);
            model->vertices[vi].map.v[1] = BR_SCALAR(uv[1]);
        }
    }

    /* Fill faces */
    if (idx_acc) {
        for (fi = 0; fi < nf; fi++) {
            cgltf_size i0 = cgltf_accessor_read_index(idx_acc, fi * 3 + 0);
            cgltf_size i1 = cgltf_accessor_read_index(idx_acc, fi * 3 + 1);
            cgltf_size i2 = cgltf_accessor_read_index(idx_acc, fi * 3 + 2);
            model->faces[fi].vertices[0] = (br_uint_16)i0;
            model->faces[fi].vertices[1] = (br_uint_16)i1;
            model->faces[fi].vertices[2] = (br_uint_16)i2;
            model->faces[fi].smoothing = 1;
            model->faces[fi].material = face_mat;
        }
    } else {
        /* No index buffer: sequential triangles */
        for (fi = 0; fi < nf; fi++) {
            model->faces[fi].vertices[0] = (br_uint_16)(fi * 3 + 0);
            model->faces[fi].vertices[1] = (br_uint_16)(fi * 3 + 1);
            model->faces[fi].vertices[2] = (br_uint_16)(fi * 3 + 2);
            model->faces[fi].smoothing = 1;
            model->faces[fi].material = face_mat;
        }
    }

    if (has_normals)
        model->flags |= BR_MODF_CUSTOM_NORMALS;

    /* BrModelAdd calls BrModelUpdate internally -- do NOT call it separately.
     * BrModelUpdate consumes (NULLs) the faces/vertices arrays. */
    BrModelAdd(model);

    LOGF("model '%s': %d verts, %d faces%s", name, (int)nv, (int)nf,
         has_normals ? " (custom normals)" : "");

    return model;
}

/* ------------------------------------------------------------------ */
/* Transform helpers                                                    */
/* ------------------------------------------------------------------ */

/* Convert a column-major 4x4 glTF matrix to BRender's row-major 3x4.
 * BRender uses row-vector convention (v * M), so M_br = transpose(M_col) */
static void col_major_to_br34(const float *m, br_matrix34 *out) {
    out->m[0][0] = BR_SCALAR(m[0]);  out->m[0][1] = BR_SCALAR(m[1]);  out->m[0][2] = BR_SCALAR(m[2]);
    out->m[1][0] = BR_SCALAR(m[4]);  out->m[1][1] = BR_SCALAR(m[5]);  out->m[1][2] = BR_SCALAR(m[6]);
    out->m[2][0] = BR_SCALAR(m[8]);  out->m[2][1] = BR_SCALAR(m[9]);  out->m[2][2] = BR_SCALAR(m[10]);
    out->m[3][0] = BR_SCALAR(m[12]); out->m[3][1] = BR_SCALAR(m[13]); out->m[3][2] = BR_SCALAR(m[14]);
}

/* Compose translation + quaternion rotation + scale into BRender 3x4 matrix.
 * t[3] = translation, q[4] = quaternion (x,y,z,w), s[3] = scale.
 * Result: M = T * R * S in column-vector form, transposed for BRender. */
static void compose_trs_to_br34(const float *t, const float *q, const float *s,
                                 br_matrix34 *out) {
    float qx = q[0], qy = q[1], qz = q[2], qw = q[3];
    float xx = qx*qx, yy = qy*qy, zz = qz*qz;
    float xy = qx*qy, xz = qx*qz, yz = qy*qz;
    float wx = qw*qx, wy = qw*qy, wz = qw*qz;

    out->m[0][0] = BR_SCALAR((1.0f - 2.0f*(yy+zz)) * s[0]);
    out->m[0][1] = BR_SCALAR((2.0f*(xy+wz))         * s[0]);
    out->m[0][2] = BR_SCALAR((2.0f*(xz-wy))         * s[0]);

    out->m[1][0] = BR_SCALAR((2.0f*(xy-wz))         * s[1]);
    out->m[1][1] = BR_SCALAR((1.0f - 2.0f*(xx+zz)) * s[1]);
    out->m[1][2] = BR_SCALAR((2.0f*(yz+wx))         * s[1]);

    out->m[2][0] = BR_SCALAR((2.0f*(xz+wy))         * s[2]);
    out->m[2][1] = BR_SCALAR((2.0f*(yz-wx))         * s[2]);
    out->m[2][2] = BR_SCALAR((1.0f - 2.0f*(xx+yy)) * s[2]);

    out->m[3][0] = BR_SCALAR(t[0]);
    out->m[3][1] = BR_SCALAR(t[1]);
    out->m[3][2] = BR_SCALAR(t[2]);
}

/* ------------------------------------------------------------------ */
/* Animation helpers                                                    */
/* ------------------------------------------------------------------ */

/* Quaternion spherical linear interpolation (shortest path) */
static void quat_slerp(float *out, const float *a, const float *b, float t) {
    float dot = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
    float b2[4];
    float theta, sin_theta, wa, wb, len;
    int i;

    /* Ensure shortest path */
    if (dot < 0.0f) {
        dot = -dot;
        for (i = 0; i < 4; i++) b2[i] = -b[i];
    } else {
        for (i = 0; i < 4; i++) b2[i] = b[i];
    }

    if (dot > 0.9995f) {
        /* Nearly identical: lerp + normalize */
        for (i = 0; i < 4; i++) out[i] = a[i] + t * (b2[i] - a[i]);
        len = (float)sqrt(out[0]*out[0] + out[1]*out[1] + out[2]*out[2] + out[3]*out[3]);
        if (len > 1e-6f)
            for (i = 0; i < 4; i++) out[i] /= len;
    } else {
        theta = (float)acos(dot);
        sin_theta = (float)sin(theta);
        wa = (float)sin((1.0f - t) * theta) / sin_theta;
        wb = (float)sin(t * theta) / sin_theta;
        for (i = 0; i < 4; i++) out[i] = wa * a[i] + wb * b2[i];
    }
}

/* Evaluate a single animation channel at the given time */
static void evaluate_channel(gltf_channel *ch, float time, float *out) {
    gltf_keyframes *keys = &ch->keys;
    int components = (ch->path == 1) ? 4 : 3;
    int lo, hi, mid;

    if (keys->count == 0) return;

    /* Before first or single keyframe: snap to first */
    if (keys->count == 1 || time <= keys->times[0]) {
        memcpy(out, keys->values, components * sizeof(float));
        return;
    }

    /* After last keyframe: snap to last */
    if (time >= keys->times[keys->count - 1]) {
        memcpy(out, &keys->values[(keys->count - 1) * components],
               components * sizeof(float));
        return;
    }

    /* Binary search for interval [lo, hi) containing time */
    lo = 0;
    hi = keys->count - 1;
    while (lo + 1 < hi) {
        mid = (lo + hi) / 2;
        if (keys->times[mid] <= time)
            lo = mid;
        else
            hi = mid;
    }

    if (ch->interpolation == 0) {
        /* STEP: snap to left keyframe */
        memcpy(out, &keys->values[lo * components], components * sizeof(float));
    } else {
        /* LINEAR: interpolate between lo and hi */
        float t_range = keys->times[hi] - keys->times[lo];
        float t_factor = (t_range > 0.0f)
            ? (time - keys->times[lo]) / t_range : 0.0f;

        if (ch->path == 1) {
            quat_slerp(out,
                       &keys->values[lo * 4],
                       &keys->values[hi * 4],
                       t_factor);
        } else {
            float *va = &keys->values[lo * 3];
            float *vb = &keys->values[hi * 3];
            int i;
            for (i = 0; i < 3; i++)
                out[i] = va[i] + t_factor * (vb[i] - va[i]);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Scene building                                                       */
/* ------------------------------------------------------------------ */

/* Count total primitives across all meshes (for array sizing) */
static int count_total_prims(cgltf_data *data) {
    int total = 0;
    for (cgltf_size mi = 0; mi < data->meshes_count; mi++)
        total += (int)data->meshes[mi].primitives_count;
    return total;
}

/* Recursively build a BRender actor tree from a glTF node.
 * Each node becomes a BR_ACTOR_NONE with its local transform.
 * Mesh primitives become BR_ACTOR_MODEL children with identity transforms. */
static br_actor *build_node_actor(cgltf_node *node, cgltf_data *data,
                                   br_material **mat_map, gltf_scene *out) {
    br_actor *actor;
    float local_m[16];
    int node_idx;
    float *trs;

    actor = BrActorAllocate(BR_ACTOR_NONE, NULL);
    actor->t.type = BR_TRANSFORM_MATRIX34;

    /* Set local transform from glTF node */
    cgltf_node_transform_local(node, local_m);
    col_major_to_br34(local_m, &actor->t.t.mat);

    /* Map glTF node index to BRender actor */
    node_idx = (int)(node - data->nodes);
    out->node_actors[node_idx] = actor;

    /* Store rest-pose TRS for animation fallback.
     * cgltf initializes defaults (T=0, R=identity, S=1) even
     * when the node uses a matrix form. */
    trs = &out->rest_trs[node_idx * 10];
    memcpy(trs,     node->translation, 3 * sizeof(float));
    memcpy(trs + 3, node->rotation,    4 * sizeof(float));
    memcpy(trs + 7, node->scale,       3 * sizeof(float));

    LOGF("node[%d] '%s': %s",
         node_idx, node->name ? node->name : "?",
         node->mesh ? "mesh" : "transform");

    /* If node has a mesh, create model actors for each primitive */
    if (node->mesh) {
        cgltf_mesh *mesh = node->mesh;
        float world_m[16];

        /* World transform for AABB computation only */
        cgltf_node_transform_world(node, world_m);

        for (cgltf_size pi = 0; pi < mesh->primitives_count; pi++) {
            cgltf_primitive *prim = &mesh->primitives[pi];
            br_material *face_mat;
            br_model *model;
            br_actor *model_actor;
            cgltf_accessor *pos_acc;

            /* Resolve material */
            if (prim->material && mat_map) {
                cgltf_size mat_i = (cgltf_size)(prim->material - data->materials);
                face_mat = mat_map[mat_i];
            } else {
                face_mat = get_default_material();
            }

            model = convert_primitive(mesh, prim, (int)pi, face_mat);
            if (!model) continue;

            out->models[out->nmodels++] = model;

            /* Attach as child model actor with identity transform */
            model_actor = BrActorAdd(actor, BrActorAllocate(BR_ACTOR_MODEL, NULL));
            model_actor->model = model;
            model_actor->t.type = BR_TRANSFORM_MATRIX34;
            BrMatrix34Identity(&model_actor->t.t.mat);

            /* Update scene AABB from accessor bounds transformed to world space */
            pos_acc = find_accessor(prim, cgltf_attribute_type_position);
            if (pos_acc && pos_acc->has_min && pos_acc->has_max) {
                int ci;
                for (ci = 0; ci < 8; ci++) {
                    float corner[3], wc[3];
                    corner[0] = (ci & 1) ? (float)pos_acc->max[0] : (float)pos_acc->min[0];
                    corner[1] = (ci & 2) ? (float)pos_acc->max[1] : (float)pos_acc->min[1];
                    corner[2] = (ci & 4) ? (float)pos_acc->max[2] : (float)pos_acc->min[2];
                    transform_point(world_m, corner, wc);
                    if (BR_SCALAR(wc[0]) < out->bbox_min.v[0]) out->bbox_min.v[0] = BR_SCALAR(wc[0]);
                    if (BR_SCALAR(wc[1]) < out->bbox_min.v[1]) out->bbox_min.v[1] = BR_SCALAR(wc[1]);
                    if (BR_SCALAR(wc[2]) < out->bbox_min.v[2]) out->bbox_min.v[2] = BR_SCALAR(wc[2]);
                    if (BR_SCALAR(wc[0]) > out->bbox_max.v[0]) out->bbox_max.v[0] = BR_SCALAR(wc[0]);
                    if (BR_SCALAR(wc[1]) > out->bbox_max.v[1]) out->bbox_max.v[1] = BR_SCALAR(wc[1]);
                    if (BR_SCALAR(wc[2]) > out->bbox_max.v[2]) out->bbox_max.v[2] = BR_SCALAR(wc[2]);
                }
            }
        }
    }

    /* Recurse into children */
    for (cgltf_size ci = 0; ci < node->children_count; ci++) {
        br_actor *child = build_node_actor(node->children[ci], data, mat_map, out);
        BrActorAdd(actor, child);
    }

    return actor;
}

/* ------------------------------------------------------------------ */
/* Animation parsing                                                    */
/* ------------------------------------------------------------------ */

static void parse_animation(cgltf_data *data, gltf_scene *out) {
    cgltf_animation *anim;
    float max_time = 0.0f;

    if (data->animations_count == 0) return;

    anim = &data->animations[0];
    out->anim.nchannels = (int)anim->channels_count;
    out->anim.channels = (gltf_channel *)calloc(anim->channels_count, sizeof(gltf_channel));

    LOGF("animation '%s': %d channels",
         anim->name ? anim->name : "?", (int)anim->channels_count);

    for (cgltf_size ci = 0; ci < anim->channels_count; ci++) {
        cgltf_animation_channel *ch = &anim->channels[ci];
        cgltf_animation_sampler *samp = ch->sampler;
        gltf_channel *gc = &out->anim.channels[ci];
        int components;

        /* Target node */
        if (!ch->target_node) {
            gc->path = -1;
            continue;
        }
        gc->node_index = (int)(ch->target_node - data->nodes);

        /* Target path */
        switch (ch->target_path) {
            case cgltf_animation_path_type_translation: gc->path = 0; break;
            case cgltf_animation_path_type_rotation:    gc->path = 1; break;
            case cgltf_animation_path_type_scale:       gc->path = 2; break;
            default: gc->path = -1; continue;
        }

        /* Interpolation */
        switch (samp->interpolation) {
            case cgltf_interpolation_type_step:   gc->interpolation = 0; break;
            case cgltf_interpolation_type_linear: gc->interpolation = 1; break;
            default:
                /* Cubic spline: not supported, treat as step */
                gc->interpolation = 0;
                break;
        }

        /* Read keyframe data */
        gc->keys.count = (int)samp->input->count;
        gc->keys.times = (float *)malloc(sizeof(float) * gc->keys.count);

        components = (gc->path == 1) ? 4 : 3;
        gc->keys.values = (float *)malloc(sizeof(float) * gc->keys.count * components);

        for (int ki = 0; ki < gc->keys.count; ki++) {
            cgltf_accessor_read_float(samp->input, ki, &gc->keys.times[ki], 1);
            if (gc->keys.times[ki] > max_time)
                max_time = gc->keys.times[ki];
        }

        for (int ki = 0; ki < gc->keys.count; ki++) {
            cgltf_accessor_read_float(samp->output, ki,
                                       &gc->keys.values[ki * components], components);
        }
    }

    out->anim.duration = max_time;
    LOGF("animation duration: %.3fs", (double)max_time);
}

/* ------------------------------------------------------------------ */
/* Animation update (called each frame)                                 */
/* ------------------------------------------------------------------ */

void gltf_update_animation(gltf_scene *scene, float time) {
    float trs[MAX_ANIM_NODES * 10];
    int dirty[MAX_ANIM_NODES];
    float wrapped;
    int ci, ni;

    if (scene->anim.nchannels == 0 || scene->anim.duration <= 0.0f) return;
    if (scene->nnodes > MAX_ANIM_NODES) return;

    wrapped = (float)fmod(time, scene->anim.duration);

    /* Start from rest pose */
    memcpy(trs, scene->rest_trs, scene->nnodes * 10 * sizeof(float));
    memset(dirty, 0, scene->nnodes * sizeof(int));

    /* Evaluate each channel and collect per-node TRS */
    for (ci = 0; ci < scene->anim.nchannels; ci++) {
        gltf_channel *ch = &scene->anim.channels[ci];
        float value[4];
        float *node_trs;

        if (ch->path < 0 || ch->node_index < 0 || ch->node_index >= scene->nnodes)
            continue;

        node_trs = &trs[ch->node_index * 10];
        dirty[ch->node_index] = 1;

        evaluate_channel(ch, wrapped, value);

        switch (ch->path) {
            case 0: /* translation */
                node_trs[0] = value[0];
                node_trs[1] = value[1];
                node_trs[2] = value[2];
                break;
            case 1: /* rotation */
                node_trs[3] = value[0];
                node_trs[4] = value[1];
                node_trs[5] = value[2];
                node_trs[6] = value[3];
                break;
            case 2: /* scale */
                node_trs[7] = value[0];
                node_trs[8] = value[1];
                node_trs[9] = value[2];
                break;
        }
    }

    /* Compose TRS and write to actor transforms */
    for (ni = 0; ni < scene->nnodes; ni++) {
        float *n;
        if (!dirty[ni] || !scene->node_actors[ni]) continue;
        n = &trs[ni * 10];
        compose_trs_to_br34(n, n + 3, n + 7, &scene->node_actors[ni]->t.t.mat);
    }
}

/* ------------------------------------------------------------------ */
/* Scene loading                                                        */
/* ------------------------------------------------------------------ */

int gltf_load_scene(const char *filename, gltf_scene *out) {
    cgltf_options options;
    cgltf_data *data = NULL;
    cgltf_result result;
    int total_prims;
    int mat_idx = 0;
    br_material **mat_map = NULL;  /* cgltf material index -> br_material* */

    memset(&options, 0, sizeof(options));
    memset(out, 0, sizeof(*out));

    set_base_dir(filename);
    LOGF("loading glTF: %s (base_dir='%s')", filename, g_base_dir);

    result = cgltf_parse_file(&options, filename, &data);
    if (result != cgltf_result_success) {
        LOGF("cgltf_parse_file failed: %d", (int)result);
        return 0;
    }

    result = cgltf_load_buffers(&options, data, filename);
    if (result != cgltf_result_success) {
        LOGF("cgltf_load_buffers failed: %d", (int)result);
        cgltf_free(data);
        return 0;
    }

    LOGF("glTF: %d meshes, %d materials, %d nodes, %d animations",
         (int)data->meshes_count, (int)data->materials_count,
         (int)data->nodes_count, (int)data->animations_count);

    /* Convert materials */
    out->nmaterials = (int)data->materials_count + 1; /* +1 for default */
    out->materials = (br_material **)malloc(sizeof(br_material *) * out->nmaterials);

    /* Default material at index 0 */
    out->materials[0] = get_default_material();
    mat_idx = 1;

    /* Map from cgltf material index to br_material pointer */
    if (data->materials_count > 0) {
        mat_map = (br_material **)malloc(sizeof(br_material *) * data->materials_count);
        for (cgltf_size i = 0; i < data->materials_count; i++) {
            mat_map[i] = convert_material(&data->materials[i], data);
            out->materials[mat_idx++] = mat_map[i];
        }
    }

    /* Allocate node tracking arrays */
    out->nnodes = (int)data->nodes_count;
    out->node_actors = (br_actor **)calloc(out->nnodes, sizeof(br_actor *));
    out->rest_trs = (float *)malloc(out->nnodes * 10 * sizeof(float));

    /* Size model array from total primitives across all meshes */
    total_prims = count_total_prims(data);
    out->models = (br_model **)malloc(sizeof(br_model *) * (total_prims > 0 ? total_prims : 1));
    out->nmodels = 0;

    /* Initialize bounding box */
    out->bbox_min.v[0] = out->bbox_min.v[1] = out->bbox_min.v[2] = BR_SCALAR(1e30f);
    out->bbox_max.v[0] = out->bbox_max.v[1] = out->bbox_max.v[2] = BR_SCALAR(-1e30f);

    /* Create root actor for the scene */
    out->root_actor = BrActorAllocate(BR_ACTOR_NONE, NULL);
    out->root_actor->t.type = BR_TRANSFORM_MATRIX34;
    BrMatrix34Identity(&out->root_actor->t.t.mat);

    /* Build actor tree from scene root nodes */
    if (data->scenes_count > 0) {
        cgltf_scene *scene = &data->scenes[data->scene ? (cgltf_size)(data->scene - data->scenes) : 0];
        for (cgltf_size ni = 0; ni < scene->nodes_count; ni++) {
            br_actor *subtree = build_node_actor(scene->nodes[ni], data, mat_map, out);
            BrActorAdd(out->root_actor, subtree);
        }
    } else {
        /* No scenes defined: fall back to processing all root-level nodes */
        for (cgltf_size ni = 0; ni < data->nodes_count; ni++) {
            if (data->nodes[ni].parent == NULL) {
                br_actor *subtree = build_node_actor(&data->nodes[ni], data, mat_map, out);
                BrActorAdd(out->root_actor, subtree);
            }
        }
    }

    /* Parse animation data (must happen before cgltf_free) */
    memset(&out->anim, 0, sizeof(out->anim));
    parse_animation(data, out);

    LOGF("converted %d models, %d materials, %d nodes",
         out->nmodels, out->nmaterials, out->nnodes);
    LOGF("scene AABB: (%.2f,%.2f,%.2f) - (%.2f,%.2f,%.2f)",
         (double)out->bbox_min.v[0], (double)out->bbox_min.v[1], (double)out->bbox_min.v[2],
         (double)out->bbox_max.v[0], (double)out->bbox_max.v[1], (double)out->bbox_max.v[2]);

    if (mat_map) free(mat_map);
    cgltf_free(data);
    return out->nmodels > 0 ? 1 : 0;
}

void gltf_free_scene(gltf_scene *scene) {
    int ci;

    if (scene->models)    free(scene->models);
    if (scene->materials) free(scene->materials);

    /* Free animation data */
    for (ci = 0; ci < scene->anim.nchannels; ci++) {
        if (scene->anim.channels[ci].keys.times)
            free(scene->anim.channels[ci].keys.times);
        if (scene->anim.channels[ci].keys.values)
            free(scene->anim.channels[ci].keys.values);
    }
    if (scene->anim.channels) free(scene->anim.channels);

    if (scene->node_actors) free(scene->node_actors);
    if (scene->rest_trs)    free(scene->rest_trs);

    memset(scene, 0, sizeof(*scene));
}
