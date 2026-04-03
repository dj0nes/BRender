/*
 * VFX primitive library driver entry point
 *
 * Provides BrDrv1Begin() which creates a device + primitive library
 * using the Miles VFX polygon fill engine for INDEX_8 rasterization.
 */
#include <stddef.h>
#include <string.h>

#include "drv.h"
#include "shortcut.h"
#include "brassert.h"

br_uint_32 VFXPrimDriverTimestamp;

br_device * BR_EXPORT BrDrv1VFXPrimBegin(const char *arguments)
{
    br_device *device;

    if(VFXPrimDriverTimestamp == 0)
        VFXPrimDriverTimestamp = TIMESTAMP_START;

    device = DeviceVFXPrimAllocate("VFXPRIM");

    if(device == NULL)
        return NULL;

    if(PrimitiveLibraryVFXAllocate(device, "VFX-Primitives-Float", arguments) == NULL) {
        ObjectFree(device);
        return NULL;
    }

    return device;
}

#ifdef DEFINE_BR_ENTRY_POINT
br_device *BR_EXPORT BrDrv1Begin(const char *arguments)
{
    return BrDrv1VFXPrimBegin(arguments);
}
#endif
