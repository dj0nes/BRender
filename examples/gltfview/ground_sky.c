/*
 * ground_sky.c - Ground and sky plane utilities for gltfview
 *
 * Compiled with C99 flags (uses stb_image for texture loading).
 * Same file used by Win98 and Mac builds.
 */
#include "ground_sky.h"
#include "stb_image.h"
#include <stdio.h>
#include <string.h>

#define LOGF(fmt, ...)                                                         \
  do {                                                                         \
    printf("LOG: " fmt "\n", __VA_ARGS__);                                     \
    fflush(stdout);                                                            \
  } while (0)

/* ------------------------------------------------------------------ */
/* Texture loading                                                      */
/* ------------------------------------------------------------------ */

br_pixelmap *gs_load_texture(const char *path)
{
    br_pixelmap *pm;
    unsigned char *pixels;
    int w, h, channels, x, y;

    pixels = stbi_load(path, &w, &h, &channels, 4);
    if (!pixels) {
        LOGF("gs_load_texture: failed to load %s", path);
        return NULL;
    }

    LOGF("gs_load_texture: %s (%dx%d)", path, w, h);

#ifdef GS_TEXTURE_RGB888
    /* BlazingRenderer GL path: 24-bit BGR */
    pm = BrPixelmapAllocate(BR_PMT_RGB_888, w, h, NULL, 0);
    if (!pm) {
        stbi_image_free(pixels);
        return NULL;
    }
    for (y = 0; y < h; y++) {
        unsigned char *dst = (unsigned char *)((char *)pm->pixels + y * pm->row_bytes);
        unsigned char *src = pixels + y * w * 4;
        for (x = 0; x < w; x++) {
            dst[x*3 + 0] = src[x*4 + 2]; /* B */
            dst[x*3 + 1] = src[x*4 + 1]; /* G */
            dst[x*3 + 2] = src[x*4 + 0]; /* R */
        }
    }
#else
    /* Win98 software renderer: 16-bit RGB565 */
    pm = BrPixelmapAllocate(BR_PMT_RGB_565, w, h, NULL, 0);
    if (!pm) {
        stbi_image_free(pixels);
        return NULL;
    }
    for (y = 0; y < h; y++) {
        unsigned short *dst = (unsigned short *)((char *)pm->pixels + y * pm->row_bytes);
        unsigned char *src = pixels + y * w * 4;
        for (x = 0; x < w; x++) {
            unsigned int r = src[x*4+0] >> 3;
            unsigned int g = src[x*4+1] >> 2;
            unsigned int b = src[x*4+2] >> 3;
            dst[x] = (unsigned short)((r << 11) | (g << 5) | b);
        }
    }
#endif

    stbi_image_free(pixels);

    pm->origin_x = 0;
    pm->origin_y = 0;

    BrMapAdd(pm);
    return pm;
}

/* ------------------------------------------------------------------ */
/* Grid model creation                                                  */
/* ------------------------------------------------------------------ */

#define GRID_SUBDIV 32

/*
 * Create a subdivided flat XZ grid model centered at the origin.
 * GRID_SUBDIV x GRID_SUBDIV cells, each cell = 2 triangles.
 * Subdivision fixes affine texture distortion on software renderers.
 */
static br_model *create_quad_model(const char *name, float half_extent,
                                    float y, float uv_extent)
{
    int nverts = (GRID_SUBDIV + 1) * (GRID_SUBDIV + 1);
    int nfaces = GRID_SUBDIV * GRID_SUBDIV * 2;
    br_model *model = BrModelAllocate((char *)name, nverts, nfaces);
    int ix, iz, fi;
    float step, uv_step;

    if (!model) return NULL;

    step = (half_extent * 2.0f) / (float)GRID_SUBDIV;
    uv_step = uv_extent / (float)GRID_SUBDIV;

    /* Vertices: (GRID_SUBDIV+1) x (GRID_SUBDIV+1) grid */
    for (iz = 0; iz <= GRID_SUBDIV; iz++) {
        for (ix = 0; ix <= GRID_SUBDIV; ix++) {
            int vi = iz * (GRID_SUBDIV + 1) + ix;
            model->vertices[vi].p.v[0] = BR_SCALAR(-half_extent + ix * step);
            model->vertices[vi].p.v[1] = BR_SCALAR(y);
            model->vertices[vi].p.v[2] = BR_SCALAR(-half_extent + iz * step);
            model->vertices[vi].map.v[0] = BR_SCALAR(ix * uv_step);
            model->vertices[vi].map.v[1] = BR_SCALAR(iz * uv_step);
            model->vertices[vi].n.v[0] = 0;
            model->vertices[vi].n.v[1] = 1;
            model->vertices[vi].n.v[2] = 0;
        }
    }

    /* Faces: 2 tris per cell, wound for upward normal */
    fi = 0;
    for (iz = 0; iz < GRID_SUBDIV; iz++) {
        for (ix = 0; ix < GRID_SUBDIV; ix++) {
            int v00 = iz * (GRID_SUBDIV + 1) + ix;
            int v10 = v00 + 1;
            int v01 = v00 + (GRID_SUBDIV + 1);
            int v11 = v01 + 1;

            model->faces[fi].vertices[0] = (br_uint_16)v00;
            model->faces[fi].vertices[1] = (br_uint_16)v11;
            model->faces[fi].vertices[2] = (br_uint_16)v10;
            model->faces[fi].smoothing = 1;
            fi++;

            model->faces[fi].vertices[0] = (br_uint_16)v00;
            model->faces[fi].vertices[1] = (br_uint_16)v01;
            model->faces[fi].vertices[2] = (br_uint_16)v11;
            model->faces[fi].smoothing = 1;
            fi++;
        }
    }

    model->flags |= BR_MODF_CUSTOM_NORMALS;

    return model;
}

/* ------------------------------------------------------------------ */
/* Ground plane                                                         */
/* ------------------------------------------------------------------ */

gs_plane_t gs_create_ground(br_pixelmap *texture, float scene_radius,
                             float y, float uv_scale)
{
    gs_plane_t plane;
    float half_extent = scene_radius * 100.0f;

    memset(&plane, 0, sizeof(plane));

    /* Material: textured, high ambient so always visible, two-sided */
    plane.material = BrMaterialAllocate("gs_ground");
    plane.material->colour = BR_COLOUR_RGB(255, 255, 255);
    plane.material->ka = BR_UFRACTION(0.90);
    plane.material->kd = BR_UFRACTION(0.10);
    plane.material->ks = BR_UFRACTION(0.0);
    plane.material->flags = BR_MATF_LIGHT | BR_MATF_SMOOTH | BR_MATF_TWO_SIDED;

    if (texture) {
        plane.material->colour_map = texture;
        plane.material->flags |= BR_MATF_MAP_COLOUR;
    }

    /* map_transform: scale controls tiling, translation tracks camera.
     * Vertex UVs are 0..1 across the grid; map_transform scales them up. */
    BrMatrix23Identity(&plane.material->map_transform);
    plane.material->map_transform.m[0][0] = BR_SCALAR(half_extent * 2.0f * uv_scale / scene_radius);
    plane.material->map_transform.m[1][1] = BR_SCALAR(half_extent * 2.0f * uv_scale / scene_radius);
    BrMaterialUpdate(plane.material, BR_MATU_ALL);
    BrMaterialAdd(plane.material);

    /* Model: subdivided grid (32x32), vertex UVs normalised 0..1 */
    {
        int fi;
        br_model *model = create_quad_model("gs_ground", half_extent, 0.0f, 1.0f);
        if (!model) {
            plane.actor = NULL;
            return plane;
        }
        for (fi = 0; fi < model->nfaces; fi++)
            model->faces[fi].material = plane.material;
        BrModelAdd(model);

        plane.actor = BrActorAllocate(BR_ACTOR_MODEL, NULL);
        plane.actor->model = model;
        plane.actor->material = plane.material;
        plane.actor->t.type = BR_TRANSFORM_MATRIX34;
        BrMatrix34Identity(&plane.actor->t.t.mat);
        plane.actor->t.t.mat.m[3][1] = BR_SCALAR(y);
    }

    /* uv_scale stored as texture-repeats per world unit for gs_update offset */
    plane.uv_scale = uv_scale / scene_radius;

    LOGF("gs_create_ground: half_extent=%.1f y=%.2f uv_scale=%.1f subdiv=%d",
         (double)half_extent, (double)y, (double)uv_scale, GRID_SUBDIV);

    return plane;
}

/* ------------------------------------------------------------------ */
/* Sky plane                                                            */
/* ------------------------------------------------------------------ */

gs_plane_t gs_create_sky(br_pixelmap *texture, float scene_radius,
                          float height, float uv_scale)
{
    gs_plane_t plane;
    float half_extent = scene_radius * 100.0f;
    float sky_tile;

    memset(&plane, 0, sizeof(plane));

    /* Sky tiling: uv_scale repeats per scene_radius, default 0.5 gives
     * gentle tiling that looks like a real sky dome */
    sky_tile = half_extent * 2.0f * uv_scale / scene_radius;

    /* Material: full-bright, lit for texture support, two-sided */
    plane.material = BrMaterialAllocate("gs_sky");
    plane.material->colour = BR_COLOUR_RGB(255, 255, 255);
    plane.material->ka = BR_UFRACTION(1.0);
    plane.material->kd = BR_UFRACTION(0.0);
    plane.material->ks = BR_UFRACTION(0.0);
    plane.material->flags = BR_MATF_LIGHT | BR_MATF_SMOOTH | BR_MATF_TWO_SIDED;

    if (texture) {
        plane.material->colour_map = texture;
        plane.material->flags |= BR_MATF_MAP_COLOUR;
    }

    BrMatrix23Identity(&plane.material->map_transform);
    plane.material->map_transform.m[0][0] = BR_SCALAR(sky_tile);
    plane.material->map_transform.m[1][1] = BR_SCALAR(sky_tile);
    BrMaterialUpdate(plane.material, BR_MATU_ALL);
    BrMaterialAdd(plane.material);

    /* Model: subdivided grid, vertex UVs normalised 0..1 */
    {
        int vi, fi;
        br_model *model = create_quad_model("gs_sky", half_extent, 0.0f, 1.0f);
        if (!model) {
            plane.actor = NULL;
            return plane;
        }

        /* Sky normals face down */
        for (vi = 0; vi < model->nvertices; vi++)
            model->vertices[vi].n.v[1] = -1;

        for (fi = 0; fi < model->nfaces; fi++)
            model->faces[fi].material = plane.material;
        BrModelAdd(model);

        plane.actor = BrActorAllocate(BR_ACTOR_MODEL, NULL);
        plane.actor->model = model;
        plane.actor->material = plane.material;
        plane.actor->t.type = BR_TRANSFORM_MATRIX34;
        BrMatrix34Identity(&plane.actor->t.t.mat);
        plane.actor->t.t.mat.m[3][1] = BR_SCALAR(height);
    }

    plane.uv_scale = uv_scale / scene_radius;

    LOGF("gs_create_sky: half_extent=%.1f height=%.2f uv_scale=%.1f subdiv=%d",
         (double)half_extent, (double)height, (double)uv_scale, GRID_SUBDIV);

    return plane;
}

/* ------------------------------------------------------------------ */
/* Per-frame camera tracking                                            */
/* ------------------------------------------------------------------ */

void gs_update(gs_plane_t *plane, float cam_x, float cam_z)
{
    if (!plane || !plane->actor) return;

    /* Snap actor XZ to camera (Y stays fixed) */
    plane->actor->t.t.mat.m[3][0] = BR_SCALAR(cam_x);
    plane->actor->t.t.mat.m[3][2] = BR_SCALAR(cam_z);

    /* Offset UV so texture stays fixed in world space.
     * Vertex UVs are in model space; adding (cam * uv_scale) to the
     * map_transform translation converts them to world-space UVs. */
    plane->material->map_transform.m[2][0] = BR_SCALAR(cam_x * plane->uv_scale);
    plane->material->map_transform.m[2][1] = BR_SCALAR(cam_z * plane->uv_scale);
    BrMaterialUpdate(plane->material, BR_MATU_ALL);
}
