/*
 * Private device driver structure for VFX primitive library
 */
#ifndef _DEVICE_H_
#define _DEVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct br_device {
    const struct br_device_dispatch *dispatch;
    const char *identifier;
    struct br_device *device;
    void *object_list;
    void *res;
    struct device_templates templates;
} br_device;

#define DeviceVFXResource(d) (((br_device *)d)->res)

#ifdef __cplusplus
};
#endif
#endif
