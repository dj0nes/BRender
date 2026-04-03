/*
 * Render function matching for VFX primitive library
 *
 * Simplified from pentprim's match.h: we only support INDEX_8 output,
 * no autoloading, no generic setup, no MMX.
 */
#ifndef _MATCH_H_
#define _MATCH_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Local extension of brp_block with matching info
 */
struct local_block {
    brp_block p;

    br_scalar colour_offsets[4];
    br_scalar colour_scales[4];
    br_uint_32 range_flags;

    struct vfx_work *work;

    br_uint_32 flags_mask;
    br_uint_32 flags_cmp;

    br_int_32 depth_type;
    br_int_32 texture_type;
    br_int_32 shade_type;

    br_token input_colour_type;
};

enum {
    RF_DECAL = 0x0001,
};

#define PMT_NONE 255

#ifdef __cplusplus
};
#endif
#endif
