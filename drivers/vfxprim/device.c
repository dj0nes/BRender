/*
 * Device methods for VFX primitive library
 */
#include <stddef.h>
#include <string.h>

#include "drv.h"
#include "shortcut.h"
#include "brassert.h"

static const struct br_device_dispatch deviceDispatch;

#define F(f) offsetof(br_device, f)

static struct br_tv_template_entry deviceTemplateEntries[] = {
    {BRT_IDENTIFIER_CSTR, 0, F(identifier), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, },
};
#undef F

br_device * DeviceVFXPrimAllocate(const char *identifier)
{
    br_device *self;

    self = BrResAllocate(NULL, sizeof(*self), BR_MEMORY_OBJECT);
    self->res = BrResAllocate(self, 0, BR_MEMORY_DRIVER);
    self->identifier = identifier;
    self->dispatch = &deviceDispatch;
    self->device = self;
    self->object_list = BrObjectListAllocate(self);

    return self;
}

static void BR_CMETHOD_DECL(br_device_vfxprim, free)(br_object *_self)
{
    br_device *self = (br_device*)_self;
    BrObjectContainerFree((br_object_container *)self, BR_NULL_TOKEN, NULL, NULL);
    BrResFreeNoCallback(self);
}

static br_token BR_CMETHOD_DECL(br_device_vfxprim, type)(br_object *self)
{
    return BRT_DEVICE;
}

static br_size_t BR_CMETHOD_DECL(br_device_vfxprim, space)(br_object *self)
{
    return sizeof(br_device);
}

static br_boolean BR_CMETHOD_DECL(br_device_vfxprim, isType)(br_object *self, br_token t)
{
    return (t == BRT_DEVICE) || (t == BRT_OBJECT_CONTAINER) || (t == BRT_OBJECT);
}

static struct br_tv_template * BR_CMETHOD_DECL(br_device_vfxprim, templateQuery)(br_object *_self)
{
    br_device *self = (br_device*)_self;

    if(self->templates.deviceTemplate == NULL)
        self->templates.deviceTemplate = BrTVTemplateAllocate(self,
            deviceTemplateEntries,
            BR_ASIZE(deviceTemplateEntries));

    return self->templates.deviceTemplate;
}

static void * BR_CMETHOD_DECL(br_device_vfxprim, listQuery)(br_object_container *self)
{
    return ((br_device*)self)->object_list;
}

static const struct br_device_dispatch deviceDispatch = {
    .__reserved0 = NULL,
    .__reserved1 = NULL,
    .__reserved2 = NULL,
    .__reserved3 = NULL,
    ._free       = BR_CMETHOD_REF(br_device_vfxprim, free),
    ._identifier = BR_CMETHOD_REF(br_object_vfxprim, identifier),
    ._type       = BR_CMETHOD_REF(br_device_vfxprim, type),
    ._isType     = BR_CMETHOD_REF(br_device_vfxprim, isType),
    ._device     = BR_CMETHOD_REF(br_object_vfxprim, device),
    ._space      = BR_CMETHOD_REF(br_device_vfxprim, space),

    ._templateQuery = BR_CMETHOD_REF(br_device_vfxprim, templateQuery),
    ._query         = BR_CMETHOD_REF(br_object, query),
    ._queryBuffer   = BR_CMETHOD_REF(br_object, queryBuffer),
    ._queryMany     = BR_CMETHOD_REF(br_object, queryMany),
    ._queryManySize = BR_CMETHOD_REF(br_object, queryManySize),
    ._queryAll      = BR_CMETHOD_REF(br_object, queryAll),
    ._queryAllSize  = BR_CMETHOD_REF(br_object, queryAllSize),

    ._listQuery        = BR_CMETHOD_REF(br_device_vfxprim, listQuery),
    ._tokensMatchBegin = BR_CMETHOD_REF(br_object_container, tokensMatchBegin),
    ._tokensMatch      = BR_CMETHOD_REF(br_object_container, tokensMatch),
    ._tokensMatchEnd   = BR_CMETHOD_REF(br_object_container, tokensMatchEnd),
    ._addFront         = BR_CMETHOD_REF(br_object_container, addFront),
    ._removeFront      = BR_CMETHOD_REF(br_object_container, removeFront),
    ._remove           = BR_CMETHOD_REF(br_object_container, remove),
    ._find             = BR_CMETHOD_REF(br_object_container, find),
    ._findMany         = BR_CMETHOD_REF(br_object_container, findMany),
    ._count            = BR_CMETHOD_REF(br_object_container, count),
};
