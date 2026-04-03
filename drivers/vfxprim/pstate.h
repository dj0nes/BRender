/*
 * Private primitive state structure for VFX primitive library
 */
#ifndef _PSTATE_H_
#define _PSTATE_H_

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MASK_STATE_OUTPUT    = BR_STATE_OUTPUT,
    MASK_STATE_PRIMITIVE = BR_STATE_PRIMITIVE,
    MASK_STATE_CACHE     = BR_STATE_CACHE
};

/*
 * state.prim.flags - subset of pentprim's flags that VFX cares about
 */
enum {
    PRIMF_FORCE_FRONT_BIT,
    PRIMF_SMOOTH_BIT,
    PRIMF_DECAL_BIT,
    PRIMF_DITHER_COLOUR_BIT,
    PRIMF_DITHER_MAP_BIT,
    PRIMF_DEPTH_WRITE_BIT,
    PRIMF_COLOUR_WRITE_BIT,
    PRIMF_INDEXED_COLOUR_BIT,
    PRIMF_BLEND_BIT,
    PRIMF_MODULATE_BIT,
};

enum {
    PRIMF_FORCE_FRONT    = (1<<PRIMF_FORCE_FRONT_BIT),
    PRIMF_SMOOTH         = (1<<PRIMF_SMOOTH_BIT),
    PRIMF_DECAL          = (1<<PRIMF_DECAL_BIT),
    PRIMF_DITHER_COLOUR  = (1<<PRIMF_DITHER_COLOUR_BIT),
    PRIMF_DITHER_MAP     = (1<<PRIMF_DITHER_MAP_BIT),
    PRIMF_DEPTH_WRITE    = (1<<PRIMF_DEPTH_WRITE_BIT),
    PRIMF_COLOUR_WRITE   = (1<<PRIMF_COLOUR_WRITE_BIT),
    PRIMF_INDEXED_COLOUR = (1<<PRIMF_INDEXED_COLOUR_BIT),
    PRIMF_BLEND          = (1<<PRIMF_BLEND_BIT),
    PRIMF_MODULATE       = (1<<PRIMF_MODULATE_BIT),
};

struct input_buffer {
    struct br_buffer_stored *buffer;
    br_uint_32 width;
    br_uint_32 height;
    br_uint_32 stride;
    br_uint_32 type;
};

struct output_buffer {
    struct br_device_pixelmap *pixelmap;
    br_uint_32 width;
    br_uint_32 height;
    br_uint_32 stride;
    br_uint_32 type;
    br_boolean viewport_changed;
};

typedef struct br_primitive_state {
    const struct br_primitive_state_dispatch *dispatch;
    const char *identifier;
    struct br_device *device;
    struct br_primitive_library *plib;

    struct {
        br_uint_32 timestamp;
        br_uint_32 timestamp_major;
        br_uint_32 flags;
        br_int_32 index_base;
        br_int_32 index_range;
        br_token colour_type;

        struct input_buffer colour_map;
        struct input_buffer index_shade;
    } prim;

    struct {
        br_uint_32 timestamp;
        br_uint_32 timestamp_major;
        struct output_buffer colour;
        struct output_buffer depth;
    } out;

    struct {
        struct local_block *last_block;
        br_token last_type;
        br_scalar comp_offsets[NUM_COMPONENTS];
        br_scalar comp_scales[NUM_COMPONENTS];
        br_uint_32 timestamp_prim;
        br_uint_32 timestamp_out;
    } cache;

} br_primitive_state;

#ifdef __cplusplus
};
#endif
#endif
