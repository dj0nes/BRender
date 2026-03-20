/*
 * Copyright (c) 1993-1995 Argonaut Technologies Limited. All rights reserved.
 *
 * Functions that match a primitive against the renderer state
 *
 * csoftprim: Hand-written match tables for pure C renderers.
 * No .ifg generated files, no autoload thunks, no MMX tables.
 */
#include "drv.h"
#include "shortcut.h"
#include "brassert.h"


/*
 * Invalid value for unknown pixelmap types
 */
#define PMT_NONE 255

/*
 * Hand-written primitive tables for RGB_888
 *
 * Triangle tables: smooth Gouraud (PRIMF_SMOOTH set) and flat (fallback)
 * Point and line tables: existing pure C renderers from p_piz.c / l_piz.c
 */

/*
 * RGB_888 Triangles
 */
static struct local_block primInfo_t24[] = {
	/* Smooth Gouraud, Z-buffered */
	{
		.p = {
			.render = (brp_render_fn *)TriangleRenderSmooth_RGB_888_ZB,
			.identifier = "Smooth RGB_888 Z16 (C)",
			.type = BRT_TRIANGLE,
			.flags = 0,
			.constant_components = 0,
			.vertex_components = CM_SX | CM_SY | CM_SZ | CM_R | CM_G | CM_B,
			.convert_mask_f = 0,
			.convert_mask_x = (1 << C_SX) | (1 << C_SY) | (1 << C_SZ) | (1 << C_R) | (1 << C_G) | (1 << C_B),
			.convert_mask_i = 0,
			.constant_mask = 0,
		},
		.colour_scales = { BR_SCALAR(255.0), BR_SCALAR(255.0), BR_SCALAR(255.0), BR_SCALAR(1.0) },
		.colour_offsets = { BR_SCALAR(0.0), BR_SCALAR(0.0), BR_SCALAR(0.0), BR_SCALAR(0.0) },
		.range_flags = 0,
		.work = &work,
		.flags_mask = PRIMF_SMOOTH,
		.flags_cmp = PRIMF_SMOOTH,
		.depth_type = BR_PMT_DEPTH_16,
		.texture_type = PMT_NONE,
		.shade_type = PMT_NONE,
		.blend_type = PMT_NONE,
		.screendoor_type = PMT_NONE,
		.lighting_type = PMT_NONE,
		.bump_type = PMT_NONE,
		.fog_type = PMT_NONE,
		.input_colour_type = BRT_RGB,
	},
	/* Flat, Z-buffered (fallback) */
	{
		.p = {
			.render = (brp_render_fn *)TriangleRenderFlat_RGB_888_ZB,
			.identifier = "Flat RGB_888 Z16 (C)",
			.type = BRT_TRIANGLE,
			.flags = 0,
			.constant_components = CM_R | CM_G | CM_B,
			.vertex_components = CM_SX | CM_SY | CM_SZ,
			.convert_mask_f = 0,
			.convert_mask_x = (1 << C_SX) | (1 << C_SY) | (1 << C_SZ) | (1 << C_R) | (1 << C_G) | (1 << C_B),
			.convert_mask_i = 0,
			.constant_mask = (1 << C_R) | (1 << C_G) | (1 << C_B),
		},
		.colour_scales = { BR_SCALAR(255.0), BR_SCALAR(255.0), BR_SCALAR(255.0), BR_SCALAR(1.0) },
		.colour_offsets = { BR_SCALAR(0.0), BR_SCALAR(0.0), BR_SCALAR(0.0), BR_SCALAR(0.0) },
		.range_flags = 0,
		.work = &work,
		.flags_mask = 0,
		.flags_cmp = 0,
		.depth_type = BR_PMT_DEPTH_16,
		.texture_type = PMT_NONE,
		.shade_type = PMT_NONE,
		.blend_type = PMT_NONE,
		.screendoor_type = PMT_NONE,
		.lighting_type = PMT_NONE,
		.bump_type = PMT_NONE,
		.fog_type = PMT_NONE,
		.input_colour_type = BRT_RGB,
	},
	/* Last-resort null renderer */
	{
		.p = {
			.render = (brp_render_fn *)TriangleRenderNull,
			.identifier = "Null Triangle (C)",
			.type = BRT_TRIANGLE,
			.flags = 0,
			.constant_components = 0,
			.vertex_components = CM_SX | CM_SY | CM_SZ,
			.convert_mask_f = 0,
			.convert_mask_x = (1 << C_SX) | (1 << C_SY) | (1 << C_SZ),
			.convert_mask_i = 0,
			.constant_mask = 0,
		},
		.colour_scales = { BR_SCALAR(255.0), BR_SCALAR(255.0), BR_SCALAR(255.0), BR_SCALAR(1.0) },
		.work = &work,
	},
};

/*
 * RGB_888 Lines
 */
static struct local_block primInfo_l24[] = {
#if PARTS & PART_24Z
	{
		.p = {
			.render = (brp_render_fn *)LineRenderPIZ2I_RGB_888,
			.identifier = "Line RGB_888 Z16 (C)",
			.type = BRT_LINE,
			.flags = 0,
			.constant_components = 0,
			.vertex_components = CM_SX | CM_SY | CM_SZ | CM_R | CM_G | CM_B,
			.convert_mask_f = 0,
			.convert_mask_x = (1 << C_SX) | (1 << C_SY) | (1 << C_SZ) | (1 << C_R) | (1 << C_G) | (1 << C_B),
			.convert_mask_i = 0,
			.constant_mask = 0,
		},
		.colour_scales = { BR_SCALAR(255.0), BR_SCALAR(255.0), BR_SCALAR(255.0), BR_SCALAR(1.0) },
		.work = &work,
		.depth_type = BR_PMT_DEPTH_16,
		.texture_type = PMT_NONE,
		.shade_type = PMT_NONE,
		.blend_type = PMT_NONE,
		.screendoor_type = PMT_NONE,
		.lighting_type = PMT_NONE,
		.bump_type = PMT_NONE,
		.fog_type = PMT_NONE,
		.input_colour_type = BRT_RGB,
	},
#endif
	/* Null fallback */
	{
		.p = {
			.render = (brp_render_fn *)TriangleRenderNull,
			.identifier = "Null Line (C)",
			.type = BRT_LINE,
		},
		.colour_scales = { BR_SCALAR(255.0), BR_SCALAR(255.0), BR_SCALAR(255.0), BR_SCALAR(1.0) },
		.work = &work,
	},
};

/*
 * RGB_888 Points
 */
static struct local_block primInfo_p24[] = {
#if PARTS & PART_24Z
	{
		.p = {
			.render = (brp_render_fn *)PointRenderPIZ2_RGB_888,
			.identifier = "Point RGB_888 Z16 (C)",
			.type = BRT_POINT,
			.flags = 0,
			.constant_components = 0,
			.vertex_components = CM_SX | CM_SY | CM_SZ | CM_R | CM_G | CM_B,
			.convert_mask_f = 0,
			.convert_mask_x = (1 << C_SX) | (1 << C_SY) | (1 << C_SZ) | (1 << C_R) | (1 << C_G) | (1 << C_B),
			.convert_mask_i = 0,
			.constant_mask = 0,
		},
		.colour_scales = { BR_SCALAR(255.0), BR_SCALAR(255.0), BR_SCALAR(255.0), BR_SCALAR(1.0) },
		.work = &work,
		.depth_type = BR_PMT_DEPTH_16,
		.texture_type = PMT_NONE,
		.shade_type = PMT_NONE,
		.blend_type = PMT_NONE,
		.screendoor_type = PMT_NONE,
		.lighting_type = PMT_NONE,
		.bump_type = PMT_NONE,
		.fog_type = PMT_NONE,
		.input_colour_type = BRT_RGB,
	},
#endif
	/* Null fallback */
	{
		.p = {
			.render = (brp_render_fn *)TriangleRenderNull,
			.identifier = "Null Point (C)",
			.type = BRT_POINT,
		},
		.colour_scales = { BR_SCALAR(255.0), BR_SCALAR(255.0), BR_SCALAR(255.0), BR_SCALAR(1.0) },
		.work = &work,
	},
};

/*
 * Empty tables for 8-bit, 15-bit, 16-bit (not supported in csoftprim yet)
 */
static struct local_block primInfo_t8[] = {
	{ .p = { .render = (brp_render_fn *)TriangleRenderNull, .identifier = "Null T8 (C)" }, .work = &work, },
};
static struct local_block primInfo_l8[] = {
	{ .p = { .render = (brp_render_fn *)TriangleRenderNull, .identifier = "Null L8 (C)" }, .work = &work, },
};
static struct local_block primInfo_p8[] = {
	{ .p = { .render = (brp_render_fn *)TriangleRenderNull, .identifier = "Null P8 (C)" }, .work = &work, },
};

static struct local_block primInfo_t15[] = {
	{ .p = { .render = (brp_render_fn *)TriangleRenderNull, .identifier = "Null T15 (C)" }, .work = &work, },
};
static struct local_block primInfo_l15[] = {
	{ .p = { .render = (brp_render_fn *)TriangleRenderNull, .identifier = "Null L15 (C)" }, .work = &work, },
};
static struct local_block primInfo_p15[] = {
	{ .p = { .render = (brp_render_fn *)TriangleRenderNull, .identifier = "Null P15 (C)" }, .work = &work, },
};

static struct local_block primInfo_t16[] = {
	{ .p = { .render = (brp_render_fn *)TriangleRenderNull, .identifier = "Null T16 (C)" }, .work = &work, },
};
static struct local_block primInfo_l16[] = {
	{ .p = { .render = (brp_render_fn *)TriangleRenderNull, .identifier = "Null L16 (C)" }, .work = &work, },
};
static struct local_block primInfo_p16[] = {
	{ .p = { .render = (brp_render_fn *)TriangleRenderNull, .identifier = "Null P16 (C)" }, .work = &work, },
};


struct prim_info_table {
	struct local_block *blocks;
	int nblocks;
};

static const struct prim_info_table  primInfoTables[4][3] = {
	{
		{ primInfo_p8, BR_ASIZE(primInfo_p8), },
		{ primInfo_l8, BR_ASIZE(primInfo_l8), },
		{ primInfo_t8, BR_ASIZE(primInfo_t8), },
	},{
		{ primInfo_p15, BR_ASIZE(primInfo_p15), },
		{ primInfo_l15, BR_ASIZE(primInfo_l15), },
		{ primInfo_t15, BR_ASIZE(primInfo_t15), },
	},{
		{ primInfo_p16, BR_ASIZE(primInfo_p16), },
		{ primInfo_l16, BR_ASIZE(primInfo_l16), },
		{ primInfo_t16, BR_ASIZE(primInfo_t16), },
	},{
		{ primInfo_p24, BR_ASIZE(primInfo_p24), },
		{ primInfo_l24, BR_ASIZE(primInfo_l24), },
		{ primInfo_t24, BR_ASIZE(primInfo_t24), },
	}
};


/*
 * Transcribe buffer info into static parameter area for renderers
 */

/*
 * Output buffers
 */
static void updateWorkOut(struct prim_work *pw, struct br_primitive_state *self)
{
	if(self->out.colour.pixelmap)
		SetupRenderBuffer(&pw->colour, self->out.colour.pixelmap);

	if(self->out.depth.pixelmap)
		SetupRenderBuffer(&pw->depth, self->out.depth.pixelmap);

	pw->timestamp_out = self->out.timestamp;
}

/*
 * Input buffers
 */
static void updateWorkPrim(struct prim_work *pw, struct br_primitive_state *self)
{
	if(self->prim.colour_map.buffer) {
		pw->texture = self->prim.colour_map.buffer->buffer;

		if(pw->texture.palette == NULL) {
			pw->texture.palette = pw->colour.palette;
			pw->texture.palette_size = pw->colour.palette_size;
		}
	}

	if(self->prim.bump.buffer)
		pw->bump = self->prim.bump.buffer->buffer;

	if(self->prim.index_shade.buffer) {
		pw->shade_type = self->prim.index_shade.buffer->buffer.type;
		pw->shade_table = self->prim.index_shade.buffer->buffer.base;
        pw->index_base = self->prim.index_base;
		pw->index_range = self->prim.index_range;
	}

	if(self->prim.index_blend.buffer) {
		pw->blend_type = self->prim.index_blend.buffer->buffer.type;
		pw->blend_table = self->prim.index_blend.buffer->buffer.base;
	}

	if(self->prim.screendoor.buffer) {
		pw->screendoor_type = self->prim.screendoor.buffer->buffer.type;
		pw->blend_table = self->prim.screendoor.buffer->buffer.base;
	}

	if(self->prim.lighting.buffer) {
		pw->lighting_type = self->prim.lighting.buffer->buffer.type;
		pw->blend_table = self->prim.lighting.buffer->buffer.base;
	}

    if(self->prim.index_fog.buffer) {
                pw->fog_type = self->prim.index_fog.buffer->buffer.type;
                pw->fog_table = self->prim.index_fog.buffer->buffer.base;
	}

	if(self->prim.flags & PRIMF_DECAL) {
		pw->decal_index_base = self->prim.index_base;
		pw->decal_index_range = self->prim.index_range;
		pw->decal_shade_height = self->prim.index_shade.height;
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

	for(i=0; i < NUM_COMPONENTS; i++) {
		self->cache.comp_offsets[i] = BR_SCALAR(0.0);
		self->cache.comp_scales[i] = BR_SCALAR(1.0);
	}

	/*
	 * SX,SY: half-screen centered
	 */
	self->cache.comp_offsets[C_SX] = BR_CONST_DIV(BrIntToScalar(self->out.colour.width),2) + BR_SCALAR(0.5);
	self->cache.comp_scales[C_SX] = BR_CONST_DIV(BrIntToScalar(self->out.colour.width),2);
	self->cache.comp_offsets[C_SY] = BR_CONST_DIV(BrIntToScalar(self->out.colour.height),2) + BR_SCALAR(0.5);
	self->cache.comp_scales[C_SY] = -BR_CONST_DIV(BrIntToScalar(self->out.colour.height),2);

	/*
	 * SZ: -1 to +1 remapped to unsigned 16-bit
	 */
	self->cache.comp_offsets[C_SZ] = BR_SCALAR(0);
	self->cache.comp_scales[C_SZ] = -BR_SCALAR(32767);

	/*
	 * U,V: texture dimensions
	 */
	if(m & (CM_U | CM_V)) {
		if(self->cache.last_block->texture_type != PMT_NONE) {
			if(self->cache.last_block->range_flags & RF_UNSCALED_TEXTURE_COORDS) {
				self->cache.comp_scales[C_U] = BR_SCALAR(1);
				self->cache.comp_scales[C_V] = BR_SCALAR(1);
			} else {
				self->cache.comp_scales[C_U] = BrIntToScalar(self->prim.colour_map.width);
				self->cache.comp_scales[C_V] = BrIntToScalar(self->prim.colour_map.height);
			}
		}
	}

	/*
	 * R,G,B
	 */
	if(m & (CM_R | CM_G | CM_B)) {
		if(self->cache.last_block->range_flags & RF_RGB_SHADE) {
			self->cache.comp_scales[C_R] = BrIntToScalar(self->prim.index_shade.height-1);
			self->cache.comp_scales[C_G] = BrIntToScalar(self->prim.index_shade.height-1);
			self->cache.comp_scales[C_B] = BrIntToScalar(self->prim.index_shade.height-1);
			self->cache.comp_offsets[C_R] = BR_SCALAR(0.5);
			self->cache.comp_offsets[C_G] = BR_SCALAR(0.5);
			self->cache.comp_offsets[C_B] = BR_SCALAR(0.5);
		} else {
			self->cache.comp_scales[C_R] = self->cache.last_block->colour_scales[0];
			self->cache.comp_scales[C_G] = self->cache.last_block->colour_scales[1];
			self->cache.comp_scales[C_B] = self->cache.last_block->colour_scales[2];
			self->cache.comp_offsets[C_R] = self->cache.last_block->colour_offsets[0];
			self->cache.comp_offsets[C_G] = self->cache.last_block->colour_offsets[1];
			self->cache.comp_offsets[C_B] = self->cache.last_block->colour_offsets[2];
		}
	}

	/*
	 * Alpha
	 */
	if(m & CM_A) {
		if(self->cache.last_block->blend_type != PMT_NONE) {
			self->cache.comp_scales[C_A] = BrIntToScalar(self->prim.index_blend.height-1);
			self->cache.comp_offsets[C_A] = BR_SCALAR(0.5);
		} else if(self->cache.last_block->screendoor_type != PMT_NONE) {
			self->cache.comp_scales[C_A] = BrIntToScalar(self->prim.screendoor.height-1);
			self->cache.comp_offsets[C_A] = BR_SCALAR(0.5);
		} else {
			self->cache.comp_scales[C_A] = self->cache.last_block->colour_scales[3];
			self->cache.comp_offsets[C_A] = self->cache.last_block->colour_offsets[3];
		}
	}

	/*
	 * I
	 */
	if(m & CM_I) {
		if(self->cache.last_block->range_flags & RF_DECAL) {
			self->cache.comp_offsets[C_I] = BR_SCALAR(0.5);
			self->cache.comp_scales[C_I] = BR_SCALAR(254.0);
		} else if(self->cache.last_block->shade_type != PMT_NONE) {
			self->cache.comp_scales[C_I] = BrIntToScalar(self->prim.index_shade.height-1);
			self->cache.comp_offsets[C_I] = BR_SCALAR(0.5);
		} else {
			self->cache.comp_scales[C_I] = BrIntToScalar(self->prim.index_range);
			self->cache.comp_offsets[C_I] = BrIntToScalar(self->prim.index_base) + BR_SCALAR(0.5);
		}
	}
}

static br_boolean isPowerof2(br_int_32 x)
{
	return !((x-1) & x);
}

br_error BR_CMETHOD_DECL(br_primitive_state_soft, renderBegin)(
		struct br_primitive_state *self,
		struct brp_block **rpb,
		br_boolean *block_changed,
		br_boolean *ranges_changed,
		br_boolean no_render,
		br_token prim_type)
{
	int i,j,b,nb;
	struct local_block *pb;
	br_uint_32 flags;
	br_token input_colour_type;

	ASSERT(rpb);
	ASSERT(self);
	ASSERT(self->plib);

	/*
	 * Lock destination pixelmap
	 */
	if (!no_render && self->plib->colour_buffer != self->out.colour.pixelmap)
	{
		if(self->plib->colour_buffer != NULL)
			DevicePixelmapDirectUnlock( self->plib->colour_buffer);

		self->plib->colour_buffer = self->out.colour.pixelmap ;
		DevicePixelmapDirectLock( self->plib->colour_buffer, BR_TRUE);
	}

	/*
	 * If previous match is still valid, return that
	 */
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

	/*
	 * Generate buffer info
	 */
	work.colour.type = PMT_NONE;
	work.depth.type = PMT_NONE;
	work.texture.type = PMT_NONE;
	work.bump.type = PMT_NONE;
	work.shade_type = PMT_NONE;
	work.blend_type = PMT_NONE;
	work.screendoor_type = PMT_NONE;
	work.lighting_type = PMT_NONE;
    work.fog_type = PMT_NONE;

	work.texture.width_p = 0;
	work.texture.height = 0;

	updateWorkOut(&work, self);
	updateWorkPrim(&work, self);

	if(self->prim.custom_block) {
		pb = self->prim.custom_block;
	} else {
		flags = self->prim.flags;

		if(work.index_range == 0)
			flags|=PRIMF_RANGE_ZERO;

		if(work.texture.type != PMT_NONE) {
			if(work.texture.width_b == work.texture.stride_b)
				flags |= PRIMF_NO_SKIP;

            if(work.texture.stride_b>0)
                    flags |= PRIMF_STRIDE_POSITIVE;

		    if(isPowerof2(work.texture.width_p)
		        && isPowerof2(work.texture.height)
			    && work.texture.width_p <= 1024
			    && work.texture.height <= 1024) {
			    flags |= PRIMF_POWER2;
		    }

		    if(work.texture.palette) {
			    int s = 0;
		        switch(work.texture.type) {
			    case BR_PMT_INDEX_1: s = 2; break;
			    case BR_PMT_INDEX_2: s = 4; break;
			    case BR_PMT_INDEX_4: s = 16; break;
			    case BR_PMT_INDEX_8: s = 256; break;
			    }
		        if(work.texture.palette_size >= s)
				    flags |= PRIMF_PALETTE;
		    }
	    }

	    if(self->prim.perspective_type != BRT_NONE &&
		    self->prim.perspective_type != BRT_SUBDIVIDE) {
			    flags |= PRIMF_PERSPECTIVE;
	    }

	    /*
	     * Pick tables by colour buffer type and primitive
	     */
	    switch(work.colour.type) {
	    case BR_PMT_INDEX_8:	i = 0;	input_colour_type = BRT_INDEX; break;
	    case BR_PMT_RGB_555:	i = 1;	input_colour_type = BRT_RGB; break;
	    case BR_PMT_RGB_565:	i = 2;	input_colour_type = BRT_RGB; break;
	    case BR_PMT_RGB_888:	i = 3;	input_colour_type = BRT_RGB; break;
	    default:
		    self->cache.last_block = NULL;
		    return BRE_FAIL;
	    }

	    if(self->prim.colour_type != BRT_DEFAULT)
		    input_colour_type = self->prim.colour_type;

	    switch(prim_type) {
	    case BRT_POINT:			j = 0;	break;
	    case BRT_LINE:			j = 1;	break;
	    case BRT_TRIANGLE:		j = 2;	break;
	    default:
		    self->cache.last_block = NULL;
		    return BRE_FAIL;
	    }

	    pb = primInfoTables[i][j].blocks;
	    nb = primInfoTables[i][j].nblocks;

	    for(b=0; b < nb; b++,pb++) {

		    if((flags & pb->flags_mask) != pb->flags_cmp)
			    continue;

		    if((pb->depth_type)  != PMT_NONE && (work.depth.type != pb->depth_type))
			    continue;

		    if((pb->texture_type) != PMT_NONE && (work.texture.type != pb->texture_type))
			    continue;

		    if((pb->shade_type) != PMT_NONE && (work.shade_type != pb->shade_type))
			    continue;

		    if((pb->bump_type) != PMT_NONE && (work.bump.type != pb->bump_type))
			    continue;

		    if((pb->lighting_type) != PMT_NONE && (work.lighting_type != pb->lighting_type))
			    continue;

		    if((pb->screendoor_type) != PMT_NONE && (work.screendoor_type != pb->screendoor_type))
			    continue;

                    if((pb->blend_type) != PMT_NONE && (work.blend_type != pb->blend_type))
			    continue;

                    if((pb->fog_type) != PMT_NONE && (work.fog_type != pb->fog_type))
			    continue;

		    if(pb->input_colour_type && (input_colour_type != pb->input_colour_type))
			    continue;

		    if(pb->map_width && (pb->map_width != work.texture.width_p))
			    continue;

		    if(pb->map_height && (pb->map_height != work.texture.height))
			    continue;

		    break;
	    }

	    if(b >= nb) {
		    pb--;
	    }
	}

	if(pb->work && (pb->work != &work))
		*pb->work = work;

	if(self->prim.perspective_type == BRT_SUBDIVIDE || (pb->range_flags & RF_NEED_SUBDIVIDE)) {
		pb->p.flags |= BR_PRIMF_SUBDIVIDE;
		pb->p.subdivide_tolerance = self->prim.subdivide_tolerance;
	} else {
		pb->p.flags &= ~BR_PRIMF_SUBDIVIDE;
	}

	*rpb = &pb->p;

	if(pb == self->cache.last_block) {
		*block_changed = BR_FALSE;
	} else {
		self->cache.last_block = pb;
	}

	updateRanges(self);

	self->cache.last_type = prim_type;
	self->cache.timestamp_prim = self->prim.timestamp_major;
	self->cache.timestamp_out = self->out.timestamp_major;

	return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_primitive_state_soft, renderEnd)(
		struct br_primitive_state *self,
		struct brp_block *pb)
{
	return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_primitive_state_soft, rangesQueryF)(
		struct br_primitive_state *self,
		br_float *offset,
		br_float *scale,
		br_int_32 max_comp)
{
	int i;

	if(self->cache.timestamp_prim != self->prim.timestamp_major ||
		self->cache.timestamp_out != self->out.timestamp_major)
		return BRE_FAIL;

	for(i=0; i < max_comp; i++) {
		offset[i] = BrScalarToFloat(self->cache.comp_offsets[i]);
		scale[i] = BrScalarToFloat(self->cache.comp_scales[i]);
	}

	return BRE_OK;
}

br_error BR_CMETHOD_DECL(br_primitive_state_soft, rangesQueryX)(
		struct br_primitive_state *self,
		br_fixed_ls *offset,
		br_fixed_ls *scale,
		br_int_32 max_comp)
{
	int i;

	if(self->cache.timestamp_prim != self->prim.timestamp_major ||
		self->cache.timestamp_out != self->out.timestamp_major)
		return BRE_FAIL;

	for(i=0; i < max_comp; i++) {
		offset[i] = BrScalarToFixed(self->cache.comp_offsets[i]);
		scale[i] = BrScalarToFixed(self->cache.comp_scales[i]);
	}

	return BRE_OK;
}
