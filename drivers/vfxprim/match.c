/*
 * match.c - Render function matching for VFX primitive library
 *
 * Much simpler than pentprim: we only support INDEX_8 output and
 * a small set of rendering modes. No autoloading, no MMX, no
 * generic setup.
 */
#include "drv.h"
#include "shortcut.h"
#include "brassert.h"

/*
 * Table of available VFX renderers for INDEX_8 triangles.
 *
 * Ordered from most specific to least specific. The matcher walks
 * this list and picks the first entry whose flags_mask/flags_cmp
 * match the current state.
 */
static struct local_block vfxTriBlocks[] = {
    /* Textured (with colour map, no smooth shading) */
    {
        .p = {
            .render     = (brp_render_fn *)TriangleRenderVFXTextured,
            .identifier = "VFX Textured I8",
            .type       = BRT_TRIANGLE,
            .flags      = BR_PRIMF_SCISSOR,
            .constant_components = CM_SX | CM_SY,
            .vertex_components   = CM_SX | CM_SY | CM_U | CM_V,
        },
        .work           = &vfx_work_area,
        .flags_mask     = PRIMF_SMOOTH,
        .flags_cmp      = 0,
        .depth_type     = PMT_NONE,
        .texture_type   = BR_PMT_INDEX_8,
        .shade_type     = PMT_NONE,
        .input_colour_type = 0,
    },

    /* Gouraud with dither */
    {
        .p = {
            .render     = (brp_render_fn *)TriangleRenderVFXDitheredGouraud,
            .identifier = "VFX Dithered Gouraud I8",
            .type       = BRT_TRIANGLE,
            .flags      = BR_PRIMF_SCISSOR,
            .constant_components = 0,
            .vertex_components   = CM_SX | CM_SY | CM_I,
        },
        .work           = &vfx_work_area,
        .flags_mask     = PRIMF_SMOOTH | PRIMF_DITHER_COLOUR,
        .flags_cmp      = PRIMF_SMOOTH | PRIMF_DITHER_COLOUR,
        .depth_type     = PMT_NONE,
        .texture_type   = PMT_NONE,
        .shade_type     = PMT_NONE,
        .input_colour_type = BRT_INDEX,
    },

    /* Gouraud (smooth, no texture) */
    {
        .p = {
            .render     = (brp_render_fn *)TriangleRenderVFXGouraud,
            .identifier = "VFX Gouraud I8",
            .type       = BRT_TRIANGLE,
            .flags      = BR_PRIMF_SCISSOR,
            .constant_components = 0,
            .vertex_components   = CM_SX | CM_SY | CM_I,
        },
        .work           = &vfx_work_area,
        .flags_mask     = PRIMF_SMOOTH,
        .flags_cmp      = PRIMF_SMOOTH,
        .depth_type     = PMT_NONE,
        .texture_type   = PMT_NONE,
        .shade_type     = PMT_NONE,
        .input_colour_type = BRT_INDEX,
    },

    /* Flat (fallback for any unmatched INDEX_8 triangle) */
    {
        .p = {
            .render     = (brp_render_fn *)TriangleRenderVFXFlat,
            .identifier = "VFX Flat I8",
            .type       = BRT_TRIANGLE,
            .flags      = BR_PRIMF_SCISSOR,
            .constant_components = CM_SX | CM_SY | CM_I,
            .vertex_components   = CM_SX | CM_SY,
        },
        .work           = &vfx_work_area,
        .flags_mask     = 0,
        .flags_cmp      = 0,
        .depth_type     = PMT_NONE,
        .texture_type   = PMT_NONE,
        .shade_type     = PMT_NONE,
        .input_colour_type = BRT_INDEX,
    },
};

static struct local_block vfxNullBlock = {
    .p = {
        .render     = (brp_render_fn *)TriangleRenderVFXNull,
        .identifier = "VFX Null",
        .type       = BRT_TRIANGLE,
        .flags      = 0,
        .constant_components = 0,
        .vertex_components   = CM_SX | CM_SY,
    },
    .work = &vfx_work_area,
};

static void updateWorkOut(vfx_work *pw, struct br_primitive_state *self)
{
    if(self->out.colour.pixelmap) {
        SetupRenderBuffer(&pw->colour, self->out.colour.pixelmap);

        pw->vfx_colour_win.buffer = (UBYTE *)pw->colour.base;
        pw->vfx_colour_win.x_max = pw->colour.width_p - 1;
        pw->vfx_colour_win.y_max = pw->colour.height - 1;

        pw->vfx_colour_pane.window = &pw->vfx_colour_win;
        pw->vfx_colour_pane.x0 = 0;
        pw->vfx_colour_pane.y0 = 0;
        pw->vfx_colour_pane.x1 = pw->colour.width_p - 1;
        pw->vfx_colour_pane.y1 = pw->colour.height - 1;
    }

    if(self->out.depth.pixelmap)
        SetupRenderBuffer(&pw->depth, self->out.depth.pixelmap);

    pw->timestamp_out = self->out.timestamp;
}

static void updateWorkPrim(vfx_work *pw, struct br_primitive_state *self)
{
    if(self->prim.colour_map.buffer) {
        pw->texture = self->prim.colour_map.buffer->buffer;

        pw->vfx_texture_win.buffer = (UBYTE *)pw->texture.base;
        pw->vfx_texture_win.x_max = pw->texture.width_p - 1;
        pw->vfx_texture_win.y_max = pw->texture.height - 1;
    }

    if(self->prim.index_shade.buffer) {
        pw->shade_type = self->prim.index_shade.buffer->buffer.type;
        pw->shade_table = self->prim.index_shade.buffer->buffer.base;
        pw->index_base = self->prim.index_base;
        pw->index_range = self->prim.index_range;
    }

    pw->timestamp_prim = self->prim.timestamp;
}

static void updateRanges(struct br_primitive_state *self)
{
    int i;
    br_uint_32 m;

    if(self->cache.last_block == NULL)
        return;

    m = self->cache.last_block->p.constant_components | self->cache.last_block->p.vertex_components;

    for(i = 0; i < NUM_COMPONENTS; i++) {
        self->cache.comp_offsets[i] = BR_SCALAR(0.0);
        self->cache.comp_scales[i] = BR_SCALAR(1.0);
    }

    self->cache.comp_offsets[C_SX] = BR_CONST_DIV(BrIntToScalar(self->out.colour.width), 2) + BR_SCALAR(0.5);
    self->cache.comp_scales[C_SX]  = BR_CONST_DIV(BrIntToScalar(self->out.colour.width), 2);
    self->cache.comp_offsets[C_SY] = BR_CONST_DIV(BrIntToScalar(self->out.colour.height), 2) + BR_SCALAR(0.5);
    self->cache.comp_scales[C_SY]  = -BR_CONST_DIV(BrIntToScalar(self->out.colour.height), 2);

    self->cache.comp_offsets[C_SZ] = BR_SCALAR(0);
    self->cache.comp_scales[C_SZ]  = -BR_SCALAR(32767);

    if(m & CM_I) {
        if(self->prim.index_shade.buffer) {
            self->cache.comp_scales[C_I]  = BrIntToScalar(self->prim.index_shade.height - 1);
            self->cache.comp_offsets[C_I] = BR_SCALAR(0.5);
        } else {
            self->cache.comp_scales[C_I]  = BrIntToScalar(self->prim.index_range);
            self->cache.comp_offsets[C_I] = BrIntToScalar(self->prim.index_base) + BR_SCALAR(0.5);
        }
    }

    if(m & (CM_U | CM_V)) {
        if(self->prim.colour_map.buffer) {
            self->cache.comp_scales[C_U] = BrIntToScalar(self->prim.colour_map.width);
            self->cache.comp_scales[C_V] = BrIntToScalar(self->prim.colour_map.height);
        }
    }
}

br_error BR_CMETHOD_DECL(br_primitive_state_vfx, renderBegin)(
    struct br_primitive_state *self,
    struct brp_block **rpb,
    br_boolean *block_changed,
    br_boolean *ranges_changed,
    br_boolean no_render,
    br_token prim_type)
{
    int i, n;
    struct local_block *pb;
    br_uint_32 flags;

    ASSERT(rpb);
    ASSERT(self);
    ASSERT(self->plib);

    if(!no_render && self->plib->colour_buffer != self->out.colour.pixelmap) {
        if(self->plib->colour_buffer != NULL)
            DevicePixelmapDirectUnlock(self->plib->colour_buffer);

        self->plib->colour_buffer = self->out.colour.pixelmap;
        DevicePixelmapDirectLock(self->plib->colour_buffer, BR_TRUE);
    }

    if(self->cache.last_type == prim_type) {
        if(self->cache.timestamp_prim == self->prim.timestamp_major &&
           self->cache.timestamp_out == self->out.timestamp_major) {

            *rpb = &self->cache.last_block->p;
            *block_changed = BR_FALSE;
            *ranges_changed = BR_FALSE;

            if(self->cache.last_block->work->timestamp_prim != self->prim.timestamp)
                updateWorkPrim(self->cache.last_block->work, self);
            if(self->cache.last_block->work->timestamp_out != self->out.timestamp)
                updateWorkOut(self->cache.last_block->work, self);

            return BRE_OK;
        }
    }

    *block_changed = BR_TRUE;
    *ranges_changed = BR_TRUE;

    vfx_work_area.colour.type = PMT_NONE;
    vfx_work_area.depth.type = PMT_NONE;
    vfx_work_area.texture.type = PMT_NONE;
    vfx_work_area.shade_type = PMT_NONE;

    updateWorkOut(&vfx_work_area, self);
    updateWorkPrim(&vfx_work_area, self);

    if(vfx_work_area.colour.type != BR_PMT_INDEX_8 || prim_type != BRT_TRIANGLE) {
        self->cache.last_block = &vfxNullBlock;
        *rpb = &vfxNullBlock.p;
        return BRE_OK;
    }

    flags = self->prim.flags;
    pb = vfxTriBlocks;
    n = BR_ASIZE(vfxTriBlocks);

    for(i = 0; i < n; i++, pb++) {
        if((flags & pb->flags_mask) != pb->flags_cmp)
            continue;

        if(pb->texture_type != PMT_NONE && vfx_work_area.texture.type != (br_uint_8)pb->texture_type)
            continue;

        if(pb->shade_type != PMT_NONE && vfx_work_area.shade_type != pb->shade_type)
            continue;

        if(pb->input_colour_type && self->prim.colour_type != BRT_DEFAULT &&
           self->prim.colour_type != pb->input_colour_type)
            continue;

        break;
    }

    if(i >= n)
        pb = &vfxTriBlocks[n - 1];

    *rpb = &pb->p;

    if(pb == self->cache.last_block)
        *block_changed = BR_FALSE;
    else
        self->cache.last_block = pb;

    updateRanges(self);

    self->cache.last_type = prim_type;
    self->cache.timestamp_prim = self->prim.timestamp_major;
    self->cache.timestamp_out = self->out.timestamp_major;

    return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_primitive_state_vfx, renderEnd)(
    struct br_primitive_state *self,
    struct brp_block *pb)
{
    return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_primitive_state_vfx, rangesQueryF)(
    struct br_primitive_state *self,
    br_float *offset,
    br_float *scale,
    br_int_32 max_comp)
{
    int i;

    if(self->cache.timestamp_prim != self->prim.timestamp_major ||
       self->cache.timestamp_out != self->out.timestamp_major)
        return BRE_FAIL;

    for(i = 0; i < max_comp; i++) {
        offset[i] = BrScalarToFloat(self->cache.comp_offsets[i]);
        scale[i] = BrScalarToFloat(self->cache.comp_scales[i]);
    }

    return BRE_OK;
}
