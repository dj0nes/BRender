/*
 * Private primitive library structure for VFX
 */
#ifndef _PLIB_H_
#define _PLIB_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct br_primitive_library {
    const struct br_primitive_library_dispatch *dispatch;
    const char *identifier;
    struct br_device *device;
    br_device_pixelmap *colour_buffer;
    void *object_list;
} br_primitive_library;

#ifdef __cplusplus
};
#endif
#endif
