/*
 * Private object structure for VFX primitive library
 */
#ifndef _OBJECT_H_
#define _OBJECT_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct br_object {
    struct br_object_dispatch *dispatch;
    char *identifier;
    struct br_device *device;
} br_object;

#define ObjectVFXPrimIdentifier(d) (((br_object *)d)->identifier)
#define ObjectVFXPrimDevice(d) (((br_object *)d)->device)

#ifdef __cplusplus
};
#endif
#endif
