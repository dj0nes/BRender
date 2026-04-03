/*
 * Work area for VFX primitive library
 *
 * Simplified from pentprim's work.h: we only need colour buffer info
 * and a VFX window/pane wrapper for zero-copy rendering.
 */
#ifndef _WORK_H_
#define _WORK_H_

#include "vfx_compat.h"
#include "vfx_port.h"

/*
 * Minimal render_buffer description (subset of pentprim's)
 */
struct render_buffer {
    void *base;
    br_uint_8 type;
    br_uint_8 bpp;
    br_uint_32 width_b;
    br_uint_32 width_p;
    br_uint_32 height;
    br_int_32 stride_b;
    br_int_32 stride_p;
    br_uint_32 size;

    br_uint_32 *palette;
    br_int_32 palette_size;

    br_uint_8 width_s;
    br_uint_8 height_s;
    br_uint_8 tile_s;
    br_uint_8 _pad0;
};

/*
 * VFX work area: wraps BRender pixelmaps as VFX windows/panes
 */
typedef struct vfx_work {
    struct render_buffer colour;
    struct render_buffer depth;
    struct render_buffer texture;

    /*
     * VFX window/pane that point into the BRender colour buffer (zero-copy)
     */
    VFX_WINDOW  vfx_colour_win;
    VFX_PANE    vfx_colour_pane;

    /*
     * VFX window wrapping a texture buffer
     */
    VFX_WINDOW  vfx_texture_win;

    /*
     * Shade/lookup tables
     */
    br_uint_8 *shade_table;
    br_uint_8 shade_type;

    br_int_32 index_base;
    br_int_32 index_range;

    /*
     * Timestamps
     */
    br_uint_32 timestamp_prim;
    br_uint_32 timestamp_out;
} vfx_work;

extern vfx_work vfx_work_area;

#ifdef __cplusplus
};
#endif
#endif
