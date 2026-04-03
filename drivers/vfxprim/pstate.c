/*
 * Primitive state methods for VFX primitive library
 *
 * Adapted from pentprim/pstate.c with simplified state (no fog, no blend
 * tables, no screendoor, no bump -- just the bits VFX can render).
 */
#include <stddef.h>
#include <string.h>

#include "drv.h"
#include "brassert.h"

static const struct br_primitive_state_dispatch primitiveStateDispatch;

#define S BRTV_SET
#define Q BRTV_QUERY
#define A BRTV_ALL

#define F(f)  offsetof(struct br_primitive_state, f)
#define P(f)  ((br_uintptr_t)(&(f)))

static struct br_tv_template_entry primitiveStateTemplateEntries[] = {
    {BRT(IDENTIFIER_CSTR), F(identifier), Q | A, BRTV_CONV_COPY,},
    {BRT(PARTS_TL),        P(VFXPrimPartsTokens), Q | A | BRTV_ABS, BRTV_CONV_LIST, },
};
#undef F
#undef P

struct br_primitive_state * PrimitiveStateVFXAllocate(struct br_primitive_library *plib)
{
    struct br_primitive_state *self;

    self = BrResAllocate(plib->device, sizeof(*self), BR_MEMORY_OBJECT);

    if(self == NULL)
        return NULL;

    self->plib = plib;
    self->dispatch = &primitiveStateDispatch;
    self->device = plib->device;

    self->out.colour.viewport_changed = BR_TRUE;

    ObjectContainerAddFront(plib, (br_object *)self);

    return self;
}

static void BR_CMETHOD_DECL(br_primitive_state_vfx, free)(br_object *_self)
{
    br_primitive_state *self = (br_primitive_state*)_self;
    ObjectContainerRemove(self->plib, (br_object *)self);
    BrResFreeNoCallback(self);
}

static br_token BR_CMETHOD_DECL(br_primitive_state_vfx, type)(br_object *self)
{
    return BRT_PRIMITIVE_STATE;
}

static br_boolean BR_CMETHOD_DECL(br_primitive_state_vfx, isType)(br_object *self, br_token t)
{
    return (t == BRT_PRIMITIVE_STATE) || (t == BRT_OBJECT_CONTAINER) || (t == BRT_OBJECT);
}

static br_size_t BR_CMETHOD_DECL(br_primitive_state_vfx, space)(br_object *self)
{
    return sizeof(br_primitive_state);
}

static struct br_tv_template * BR_CMETHOD_DECL(br_primitive_state_vfx, templateQuery)(br_object *_self)
{
    br_primitive_state *self = (br_primitive_state*)_self;

    if(self->device->templates.primitiveStateTemplate == NULL)
        self->device->templates.primitiveStateTemplate = BrTVTemplateAllocate(self->device,
            primitiveStateTemplateEntries,
            BR_ASIZE(primitiveStateTemplateEntries));

    return self->device->templates.primitiveStateTemplate;
}

static br_boolean inputSet(struct input_buffer *ib, br_buffer_stored *new)
{
    br_buffer_stored *old = ib->buffer;

    if(old == NULL && new == NULL)
        return BR_FALSE;

    if(new == NULL) {
        ib->buffer = NULL;
        return BR_TRUE;
    }

    if(old != NULL) {
        if(ib->type == new->buffer.type &&
            ib->width == new->buffer.width_p &&
            ib->height == new->buffer.height &&
            ib->stride == new->buffer.stride_b) {
            ib->buffer = new;
            return BR_FALSE;
        }
    }

    ib->buffer = new;
    ib->type = new->buffer.type;
    ib->width = new->buffer.width_p;
    ib->height = new->buffer.height;
    ib->stride = new->buffer.stride_b;

    return BR_TRUE;
}

static br_boolean outputSet(struct output_buffer *ob, br_device_pixelmap *new)
{
    br_device_pixelmap *old = ob->pixelmap;

    if(old == NULL && new == NULL)
        return BR_FALSE;

    if(new == NULL) {
        ob->pixelmap = NULL;
        ob->width = 0;
        ob->height = 0;
        ob->viewport_changed = BR_TRUE;
        return BR_TRUE;
    }

    if(old != NULL) {
        if(ob->type == new->pm_type &&
            ob->width == new->pm_width &&
            ob->height == new->pm_height &&
            ob->stride == new->pm_row_bytes) {
            ob->pixelmap = new;
            return BR_FALSE;
        }
    }

    ob->pixelmap = new;
    ob->type = new->pm_type;
    ob->width = new->pm_width;
    ob->height = new->pm_height;
    ob->stride = new->pm_row_bytes;
    ob->viewport_changed = BR_TRUE;

    return BR_TRUE;
}

static br_error BR_CALLBACK customInputSet(void *block, const br_value *pvalue, const struct br_tv_template_entry *tep)
{
    if(inputSet(
        (struct input_buffer *)((char *)block + tep->offset),
        (br_buffer_stored *)pvalue->o))
        ((struct br_tv_template_entry*)tep)->mask = 1;
    else
        ((struct br_tv_template_entry*)tep)->mask = 0;
    return BRE_OK;
}

static struct br_tv_custom customInputConv = {
    NULL,
    customInputSet,
    NULL,
};

static br_error BR_CALLBACK customOutputSet(void *block, const br_value *pvalue, const struct br_tv_template_entry *tep)
{
    if(outputSet(
        (struct output_buffer *)((char *)block + tep->offset),
        (br_device_pixelmap *)pvalue->o))
        ((struct br_tv_template_entry*)tep)->mask = 1;
    else
        ((struct br_tv_template_entry*)tep)->mask = 0;
    return BRE_OK;
}

static const struct br_tv_custom customOutputConv = {
    NULL,
    customOutputSet,
    NULL,
};

#define F(f) offsetof(struct br_primitive_state, f)
#define P(f) ((br_uintptr_t)(&(f)))

static br_tv_template_entry partPrimitiveTemplateEntries[] = {
    {BRT(FORCE_FRONT_B),  F(prim.flags),          Q | S | A, BRTV_CONV_BIT, PRIMF_FORCE_FRONT,    1},
    {BRT(SMOOTH_B),       F(prim.flags),          Q | S | A, BRTV_CONV_BIT, PRIMF_SMOOTH,         1},
    {BRT(DECAL_B),        F(prim.flags),          Q | S | A, BRTV_CONV_BIT, PRIMF_DECAL,          1},
    {BRT(DITHER_COLOUR_B),F(prim.flags),          Q | S | A, BRTV_CONV_BIT, PRIMF_DITHER_COLOUR,  1},
    {BRT(DITHER_MAP_B),   F(prim.flags),          Q | S | A, BRTV_CONV_BIT, PRIMF_DITHER_MAP,     1},
    {BRT(DEPTH_WRITE_B),  F(prim.flags),          Q | S | A, BRTV_CONV_BIT, PRIMF_DEPTH_WRITE,    1},
    {BRT(COLOUR_WRITE_B), F(prim.flags),          Q | S | A, BRTV_CONV_BIT, PRIMF_COLOUR_WRITE,   1},
    {BRT(BLEND_B),        F(prim.flags),          Q | S | A, BRTV_CONV_BIT, PRIMF_BLEND,          1},
    {BRT(MODULATE_B),     F(prim.flags),          Q | S | A, BRTV_CONV_BIT, PRIMF_MODULATE,       1},

    {BRT(COLOUR_T),       F(prim.colour_type),    Q | S | A, BRTV_CONV_COPY, 0,                   1},
    {BRT(COLOUR_B),       F(prim.colour_type),    S,         BRTV_CONV_BOOL_TOKEN, BRT_DEFAULT,   1},

    {BRT(INDEX_BASE_I32), F(prim.index_base),     Q | S | A, BRTV_CONV_COPY, 0,                   1},
    {BRT(INDEX_RANGE_I32),F(prim.index_range),    Q | S | A, BRTV_CONV_COPY, 0,                   1},

    {BRT(COLOUR_MAP_O),   F(prim.colour_map.buffer), Q | A,  BRTV_CONV_COPY},
    {BRT(COLOUR_MAP_O),   F(prim.colour_map),     S,         BRTV_CONV_CUSTOM, P(customInputConv)},

    {BRT(TEXTURE_O),      F(prim.colour_map.buffer), Q,      BRTV_CONV_COPY},
    {BRT(TEXTURE_O),      F(prim.colour_map),     S,         BRTV_CONV_CUSTOM, P(customInputConv)},

    {BRT(INDEX_SHADE_O),  F(prim.index_shade.buffer), Q | A, BRTV_CONV_COPY},
    {BRT(INDEX_SHADE_O),  F(prim.index_shade),    S,         BRTV_CONV_CUSTOM, P(customInputConv)},
};

static br_tv_template_entry partOutputTemplateEntries[] = {
    {BRT(COLOUR_BUFFER_O), F(out.colour.pixelmap), Q | A,    BRTV_CONV_COPY},
    {BRT(COLOUR_BUFFER_O), F(out.colour),          S,        BRTV_CONV_CUSTOM, P(customOutputConv)},
    {BRT(DEPTH_BUFFER_O),  F(out.depth.pixelmap),  Q | A,    BRTV_CONV_COPY},
    {BRT(DEPTH_BUFFER_O),  F(out.depth),           S,        BRTV_CONV_CUSTOM, P(customOutputConv)},
};

#undef F
#undef P

static br_tv_template *findTemplate(struct br_primitive_state *self, br_token part)
{
    switch(part) {
    case BRT_PRIMITIVE:
        if(self->device->templates.partPrimitiveTemplate == NULL)
            self->device->templates.partPrimitiveTemplate = BrTVTemplateAllocate(self->device,
                partPrimitiveTemplateEntries, BR_ASIZE(partPrimitiveTemplateEntries));
        return self->device->templates.partPrimitiveTemplate;

    case BRT_OUTPUT:
        if(self->device->templates.partOutputTemplate == NULL)
            self->device->templates.partOutputTemplate = BrTVTemplateAllocate(self->device,
                partOutputTemplateEntries, BR_ASIZE(partOutputTemplateEntries));
        return self->device->templates.partOutputTemplate;
    }

    return NULL;
}

static br_error BR_CMETHOD_DECL(br_primitive_state_vfx, partSet)(
    struct br_primitive_state *self,
    br_token part, br_int_32 index, br_token t, br_value value)
{
    br_error r;
    br_tv_template *tp = findTemplate(self, part);
    br_uint_32 m;

    if(tp == NULL) return BRE_FAIL;

    m = 0;
    r = BrTokenValueSet(self, &m, t, value, tp);
    if(r != BRE_OK) return r;

    switch(part) {
    case BRT_PRIMITIVE:
        self->prim.timestamp = Timestamp();
        if(m) self->prim.timestamp_major = Timestamp();
        break;
    case BRT_OUTPUT:
        self->out.timestamp = Timestamp();
        if(m) self->out.timestamp_major = Timestamp();
        break;
    }

    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_primitive_state_vfx, partSetMany)(
    struct br_primitive_state *self,
    br_token part, br_int_32 index, br_token_value *tv, br_int_32 *pcount)
{
    br_error r;
    br_tv_template *tp = findTemplate(self, part);
    br_uint_32 m;
    br_int_32 c;

    if(tp == NULL) return BRE_FAIL;

    m = 0;
    r = BrTokenValueSetMany(self, &c, &m, tv, tp);
    if(r != BRE_OK || c == 0) return r;

    if(pcount) *pcount = c;

    switch(part) {
    case BRT_PRIMITIVE:
        self->prim.timestamp = Timestamp();
        if(m) self->prim.timestamp_major = Timestamp();
        break;
    case BRT_OUTPUT:
        self->out.timestamp = Timestamp();
        if(m) self->out.timestamp_major = Timestamp();
        break;
    }

    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_primitive_state_vfx, partQuery)(
    struct br_primitive_state *self,
    br_token part, br_int_32 index, void *pvalue, br_token t)
{
    br_tv_template *tp = findTemplate(self, part);
    if(tp == NULL) return BRE_FAIL;
    return BrTokenValueQuery(pvalue, NULL, 0, t, self, tp);
}

static br_error BR_CMETHOD_DECL(br_primitive_state_vfx, partQueryBuffer)(
    struct br_primitive_state *self,
    br_token part, br_int_32 index, void *pvalue,
    void *buffer, br_size_t buffer_size, br_token t)
{
    br_tv_template *tp = findTemplate(self, part);
    if(tp == NULL) return BRE_FAIL;
    return BrTokenValueQuery(pvalue, buffer, buffer_size, t, self, tp);
}

static br_error BR_CMETHOD_DECL(br_primitive_state_vfx, partQueryMany)(
    struct br_primitive_state *self,
    br_token part, br_int_32 index, br_token_value *tv,
    void *extra, br_size_t extra_size, br_int_32 *pcount)
{
    br_tv_template *tp = findTemplate(self, part);
    if(tp == NULL) return BRE_FAIL;
    return BrTokenValueQueryMany(tv, extra, extra_size, pcount, self, tp);
}

static br_error BR_CMETHOD_DECL(br_primitive_state_vfx, partQueryManySize)(
    struct br_primitive_state *self,
    br_token part, br_int_32 index, br_size_t *pextra_size, br_token_value *tv)
{
    br_tv_template *tp = findTemplate(self, part);
    if(tp == NULL) return BRE_FAIL;
    return BrTokenValueQueryManySize(pextra_size, tv, self, tp);
}

static br_error BR_CMETHOD_DECL(br_primitive_state_vfx, partQueryAll)(
    struct br_primitive_state *self,
    br_token part, br_int_32 index, br_token_value *buffer, br_size_t buffer_size)
{
    br_tv_template *tp = findTemplate(self, part);
    if(tp == NULL) return BRE_FAIL;
    return BrTokenValueQueryAll(buffer, buffer_size, self, tp);
}

static br_error BR_CMETHOD_DECL(br_primitive_state_vfx, partQueryAllSize)(
    struct br_primitive_state *self,
    br_token part, br_int_32 index, br_size_t *psize)
{
    br_tv_template *tp = findTemplate(self, part);
    if(tp == NULL) return BRE_FAIL;
    return BrTokenValueQueryAllSize(psize, self, tp);
}

static br_error BR_CMETHOD_DECL(br_primitive_state_vfx, partQueryCapability)(
    struct br_primitive_state *self,
    br_token part, br_int_32 index,
    br_token_value *buffer, br_size_t buffer_size)
{
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_primitive_state_vfx, partIndexQuery)(
    struct br_primitive_state *self,
    br_token part, br_int_32 *pnindex)
{
    int n;
    switch(part) {
    case BRT_OUTPUT:
    case BRT_PRIMITIVE: n = 1; break;
    default: n = 0;
    }
    if(pnindex) { *pnindex = n; return BRE_OK; }
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_primitive_state_vfx, stateQueryPerformance)(
    struct br_primitive_state *self, br_fixed_lu *speed)
{
    return BRE_FAIL;
}

static br_error BR_CMETHOD_DECL(br_primitive_state_vfx, stateDefault)(
    struct br_primitive_state *self, br_uint_32 mask)
{
    if(mask & MASK_STATE_PRIMITIVE) {
        self->prim.flags = PRIMF_COLOUR_WRITE;
        self->prim.colour_map.buffer = NULL;
        self->prim.index_shade.buffer = NULL;
        self->prim.colour_type = BRT_DEFAULT;
        self->prim.timestamp = Timestamp();
        self->prim.timestamp_major = Timestamp();
    }

    if(mask & MASK_STATE_OUTPUT) {
        self->out.colour.pixelmap = NULL;
        self->out.depth.pixelmap = NULL;
        self->out.timestamp = Timestamp();
        self->out.timestamp_major = Timestamp();
    }

    return BRE_OK;
}

static br_error BR_CMETHOD_DECL(br_primitive_state_vfx, stateCopy)(
    struct br_primitive_state *self,
    struct br_primitive_state *source,
    br_uint_32 mask)
{
    if(mask & (MASK_STATE_PRIMITIVE | MASK_STATE_OUTPUT))
        mask |= MASK_STATE_CACHE;

    if((mask & MASK_STATE_PRIMITIVE) && (self->prim.timestamp != source->prim.timestamp))
        self->prim = source->prim;

    if((mask & MASK_STATE_OUTPUT) && (self->out.timestamp != source->out.timestamp))
        self->out = source->out;

    if((mask & MASK_STATE_CACHE) &&
        ((self->cache.timestamp_prim != source->cache.timestamp_prim) ||
         (self->cache.timestamp_out != source->cache.timestamp_out) ||
         (self->cache.last_type != source->cache.last_type)))
        self->cache = source->cache;

    return BRE_OK;
}

static const struct br_primitive_state_dispatch primitiveStateDispatch = {
    .__reserved0 = NULL,
    .__reserved1 = NULL,
    .__reserved2 = NULL,
    .__reserved3 = NULL,
    ._free       = BR_CMETHOD_REF(br_primitive_state_vfx, free),
    ._identifier = BR_CMETHOD_REF(br_object_vfxprim, identifier),
    ._type       = BR_CMETHOD_REF(br_primitive_state_vfx, type),
    ._isType     = BR_CMETHOD_REF(br_primitive_state_vfx, isType),
    ._device     = BR_CMETHOD_REF(br_object_vfxprim, device),
    ._space      = BR_CMETHOD_REF(br_primitive_state_vfx, space),

    ._templateQuery = BR_CMETHOD_REF(br_primitive_state_vfx, templateQuery),
    ._query         = BR_CMETHOD_REF(br_object, query),
    ._queryBuffer   = BR_CMETHOD_REF(br_object, queryBuffer),
    ._queryMany     = BR_CMETHOD_REF(br_object, queryMany),
    ._queryManySize = BR_CMETHOD_REF(br_object, queryManySize),
    ._queryAll      = BR_CMETHOD_REF(br_object, queryAll),
    ._queryAllSize  = BR_CMETHOD_REF(br_object, queryAllSize),

    ._partSet               = BR_CMETHOD_REF(br_primitive_state_vfx, partSet),
    ._partSetMany           = BR_CMETHOD_REF(br_primitive_state_vfx, partSetMany),
    ._partQuery             = BR_CMETHOD_REF(br_primitive_state_vfx, partQuery),
    ._partQueryBuffer       = BR_CMETHOD_REF(br_primitive_state_vfx, partQueryBuffer),
    ._partQueryMany         = BR_CMETHOD_REF(br_primitive_state_vfx, partQueryMany),
    ._partQueryManySize     = BR_CMETHOD_REF(br_primitive_state_vfx, partQueryManySize),
    ._partQueryAll          = BR_CMETHOD_REF(br_primitive_state_vfx, partQueryAll),
    ._partQueryAllSize      = BR_CMETHOD_REF(br_primitive_state_vfx, partQueryAllSize),
    ._partIndexQuery        = BR_CMETHOD_REF(br_primitive_state_vfx, partIndexQuery),
    ._stateDefault          = BR_CMETHOD_REF(br_primitive_state_vfx, stateDefault),
    ._stateCopy             = BR_CMETHOD_REF(br_primitive_state_vfx, stateCopy),
    ._renderBegin           = BR_CMETHOD_REF(br_primitive_state_vfx, renderBegin),
    ._renderEnd             = BR_CMETHOD_REF(br_primitive_state_vfx, renderEnd),
    ._rangesQuery           = BR_CMETHOD_REF(br_primitive_state_vfx, rangesQueryF),
    ._partQueryCapability   = BR_CMETHOD_REF(br_primitive_state_vfx, partQueryCapability),
    ._stateQueryPerformance = BR_CMETHOD_REF(br_primitive_state_vfx, stateQueryPerformance),
};
