/*
 * Shared object methods for VFX primitive library
 */
#include <stddef.h>
#include <string.h>

#include "drv.h"
#include "shortcut.h"
#include "brassert.h"

const char * BR_CMETHOD_DECL(br_object_vfxprim, identifier)(br_object *self)
{
    return self->identifier;
}

br_device * BR_CMETHOD_DECL(br_object_vfxprim, device)(br_object *self)
{
    return self->device;
}
