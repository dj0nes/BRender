/*
 * Stored buffer structure for VFX primitive library
 */
#ifndef _SBUFFER_H_
#define _SBUFFER_H_

#ifdef __cplusplus
extern "C" {
#endif

enum {
    SBUFF_SHARED = 0x0001,
};

typedef struct br_buffer_stored {
    const struct br_buffer_stored_dispatch *dispatch;
    char *identifier;
    struct br_device *device;
    struct br_primitive_library *plib;
    br_uint_32 flags;
    struct render_buffer buffer;
    struct br_device_pixelmap *local_pm;
} br_buffer_stored;

#ifdef __cplusplus
};
#endif
#endif
