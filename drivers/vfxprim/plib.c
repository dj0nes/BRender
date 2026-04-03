/*
 * Primitive library methods for VFX
 */
#include <stddef.h>
#include <string.h>

#include "drv.h"
#include "shortcut.h"
#include "brassert.h"

static const struct br_primitive_library_dispatch primitiveLibraryDispatch;

const br_token VFXPrimPartsTokens[] = {
    BRT_PRIMITIVE,
    BRT_OUTPUT,
    0
};

#define F(f)  offsetof(br_primitive_library, f)
#define P(f)  ((br_uintptr_t)(&(f)))

static struct br_tv_template_entry primitiveLibraryTemplateEntries[] = {
    {BRT(IDENTIFIER_CSTR), F(identifier),          BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, },
    {BRT(PARTS_TL),        P(VFXPrimPartsTokens),  BRTV_QUERY | BRTV_ALL | BRTV_ABS, BRTV_CONV_LIST },
    {BRT(PARTS_U32),       MASK_STATE_CACHE | MASK_STATE_OUTPUT | MASK_STATE_PRIMITIVE, BRTV_QUERY | BRTV_ABS, BRTV_CONV_COPY },
};
#undef F
#undef P

struct br_primitive_library * PrimitiveLibraryVFXAllocate(struct br_device *dev,
    const char *identifier, const char *arguments)
{
    struct br_primitive_library *self;

    self = BrResAllocate(NULL, sizeof(*self), BR_MEMORY_OBJECT);
    self->dispatch = (struct br_primitive_library_dispatch *)&primitiveLibraryDispatch;
    self->identifier = identifier;
    self->device = dev;
    self->colour_buffer = NULL;
    self->object_list = BrObjectListAllocate(dev);

    ObjectContainerAddFront(dev, (br_object *)self);

    return self;
}

static void BR_CMETHOD_DECL(br_primitive_library_vfx, free)(br_object *_self)
{
    br_primitive_library *self = (br_primitive_library*)_self;
    BrObjectContainerFree((br_object_container *)self, BR_NULL_TOKEN, NULL, NULL);
    BrResFreeNoCallback(self);
}

static br_token BR_CMETHOD_DECL(br_primitive_library_vfx, type)(struct br_object *self)
{
    return BRT_PRIMITIVE_LIBRARY;
}

static br_boolean BR_CMETHOD_DECL(br_primitive_library_vfx, isType)(br_object *self, br_token t)
{
    return (t == BRT_PRIMITIVE_LIBRARY) || (t == BRT_OBJECT_CONTAINER) || (t == BRT_OBJECT);
}

static br_size_t BR_CMETHOD_DECL(br_primitive_library_vfx, space)(br_object *self)
{
    return sizeof(br_primitive_library);
}

static struct br_tv_template * BR_CMETHOD_DECL(br_primitive_library_vfx, templateQuery)(br_object *_self)
{
    br_primitive_library *self = (br_primitive_library*)_self;

    if(self->device->templates.primitiveLibraryTemplate == NULL)
        self->device->templates.primitiveLibraryTemplate = BrTVTemplateAllocate(self->device,
            (br_tv_template_entry *)primitiveLibraryTemplateEntries,
            BR_ASIZE(primitiveLibraryTemplateEntries));

    return self->device->templates.primitiveLibraryTemplate;
}

static void * BR_CMETHOD_DECL(br_primitive_library_vfx, listQuery)(br_object_container *self)
{
    return ((br_primitive_library*)self)->object_list;
}

static br_error BR_CMETHOD_DECL(br_primitive_library_vfx, stateNew)(
    struct br_primitive_library *self,
    struct br_primitive_state **rps)
{
    struct br_primitive_state *ps;

    UASSERT(rps);

    ps = PrimitiveStateVFXAllocate(self);

    if(ps == NULL)
        return BRE_FAIL;

    *rps = ps;
    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_primitive_library_vfx, bufferStoredNew)(
    struct br_primitive_library *self, struct br_buffer_stored **psm,
    br_token use, struct br_device_pixelmap *pm, br_token_value *tv)
{
    struct br_buffer_stored *sm;

    UASSERT(psm);

    sm = BufferStoredVFXAllocate(self, use, pm, tv);

    if(sm == NULL)
        return BRE_FAIL;

    *psm = sm;
    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_primitive_library_vfx, bufferStoredAvail)(
    struct br_primitive_library *self,
    br_int_32 *space,
    br_token use,
    br_token_value *tv)
{
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_primitive_library_vfx, flush)(
    struct br_primitive_library *self,
    br_boolean wait)
{
    ASSERT(self);

    if(self->colour_buffer) {
        DevicePixelmapDirectUnlock(self->colour_buffer);
        self->colour_buffer = NULL;
    }

    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_primitive_library_vfx, synchronise)(
    struct br_primitive_library *self,
    br_token sync_type,
    br_boolean block)
{
    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_primitive_library_vfx, mask)(
    struct br_primitive_library *self,
    br_uint_32 *mask,
    br_token *parts,
    int n_parts)
{
    int i;
    br_uint_32 m = 0;

    for(i = 0; i < n_parts; i++) {
        switch(parts[i]) {
        case BRT_PRIMITIVE:
            m |= MASK_STATE_PRIMITIVE;
            break;
        case BRT_OUTPUT:
            m |= MASK_STATE_OUTPUT;
            break;
        }
    }

    *mask = m;
    return BRE_OK;
}

static const struct br_primitive_library_dispatch primitiveLibraryDispatch = {
    .__reserved0 = NULL,
    .__reserved1 = NULL,
    .__reserved2 = NULL,
    .__reserved3 = NULL,
    ._free       = BR_CMETHOD_REF(br_primitive_library_vfx, free),
    ._identifier = BR_CMETHOD_REF(br_object_vfxprim, identifier),
    ._type       = BR_CMETHOD_REF(br_primitive_library_vfx, type),
    ._isType     = BR_CMETHOD_REF(br_primitive_library_vfx, isType),
    ._device     = BR_CMETHOD_REF(br_object_vfxprim, device),
    ._space      = BR_CMETHOD_REF(br_primitive_library_vfx, space),

    ._templateQuery = BR_CMETHOD_REF(br_primitive_library_vfx, templateQuery),
    ._query         = BR_CMETHOD_REF(br_object, query),
    ._queryBuffer   = BR_CMETHOD_REF(br_object, queryBuffer),
    ._queryMany     = BR_CMETHOD_REF(br_object, queryMany),
    ._queryManySize = BR_CMETHOD_REF(br_object, queryManySize),
    ._queryAll      = BR_CMETHOD_REF(br_object, queryAll),
    ._queryAllSize  = BR_CMETHOD_REF(br_object, queryAllSize),

    ._listQuery        = BR_CMETHOD_REF(br_primitive_library_vfx, listQuery),
    ._tokensMatchBegin = BR_CMETHOD_REF(br_object_container, tokensMatchBegin),
    ._tokensMatch      = BR_CMETHOD_REF(br_object_container, tokensMatch),
    ._tokensMatchEnd   = BR_CMETHOD_REF(br_object_container, tokensMatchEnd),
    ._addFront         = BR_CMETHOD_REF(br_object_container, addFront),
    ._removeFront      = BR_CMETHOD_REF(br_object_container, removeFront),
    ._remove           = BR_CMETHOD_REF(br_object_container, remove),
    ._find             = BR_CMETHOD_REF(br_object_container, find),
    ._findMany         = BR_CMETHOD_REF(br_object_container, findMany),
    ._count            = BR_CMETHOD_REF(br_object_container, count),

    ._stateNew          = BR_CMETHOD_REF(br_primitive_library_vfx, stateNew),
    ._bufferStoredNew   = BR_CMETHOD_REF(br_primitive_library_vfx, bufferStoredNew),
    ._bufferStoredAvail = BR_CMETHOD_REF(br_primitive_library_vfx, bufferStoredAvail),
    ._flush             = BR_CMETHOD_REF(br_primitive_library_vfx, flush),
    ._synchronise       = BR_CMETHOD_REF(br_primitive_library_vfx, synchronise),
    ._mask              = BR_CMETHOD_REF(br_primitive_library_vfx, mask),
};
