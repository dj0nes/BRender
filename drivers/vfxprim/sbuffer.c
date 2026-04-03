/*
 * Stored buffer methods for VFX primitive library
 */
#include <stddef.h>
#include <string.h>

#include "drv.h"
#include "shortcut.h"
#include "brassert.h"

static const struct br_buffer_stored_dispatch bufferStoredDispatch;

#define F(f) offsetof(struct br_buffer_stored, f)

static struct br_tv_template_entry bufferStoredTemplateEntries[] = {
    {BRT_IDENTIFIER_CSTR, 0, F(identifier), BRTV_QUERY | BRTV_ALL, BRTV_CONV_COPY, },
};
#undef F

static br_uint_8 findShift(br_int_32 x)
{
    br_uint_8 b;

    for(b = (br_uint_8)-1; x; b++)
        x /= 2;

    return b;
}

void SetupRenderBuffer(struct render_buffer *rb, br_device_pixelmap *pm)
{
    int bpp = 1;

    switch(pm->pm_type) {
    case BR_PMT_RGB_555:
    case BR_PMT_RGB_565:
    case BR_PMT_DEPTH_16:
        bpp = 2;
        break;
    case BR_PMT_RGB_888:
        bpp = 3;
        break;
    case BR_PMT_RGBX_888:
    case BR_PMT_RGBA_8888:
        bpp = 4;
        break;
    }

    rb->type = pm->pm_type;
    rb->bpp = bpp;
    rb->width_b = pm->pm_width * bpp;
    rb->width_p = pm->pm_width;
    rb->height = pm->pm_height;
    rb->stride_b = pm->pm_row_bytes;
    rb->stride_p = pm->pm_row_bytes / bpp;
    rb->size = pm->pm_height * pm->pm_row_bytes;

    rb->base = (char *)(pm->pm_pixels) +
        pm->pm_base_y * pm->pm_row_bytes +
        pm->pm_base_x * bpp;

    if(pm->pm_map &&
        (pm->pm_map->type == BR_PMT_RGBX_888) &&
        pm->pm_map->width == 1 &&
        pm->pm_map->row_bytes == 4) {
        rb->palette_size = pm->pm_map->height;
        rb->palette = pm->pm_map->pixels;
    } else {
        rb->palette_size = 0;
        rb->palette = NULL;
    }

    rb->width_s = findShift(rb->width_p);
    rb->height_s = findShift(rb->height);
    rb->tile_s = 0;
}

struct br_buffer_stored * BufferStoredVFXAllocate(struct br_primitive_library *plib,
    br_token use, struct br_device_pixelmap *pm, br_token_value *tv)
{
    struct br_buffer_stored *self;
    char *ident;

    switch(use) {
    case BRT_TEXTURE_O:
    case BRT_COLOUR_MAP_O:
        ident = "Colour-Map";
        break;
    case BRT_INDEX_SHADE_O:
        ident = "Shade-Table";
        break;
    case BRT_INDEX_BLEND_O:
        ident = "Blend-Table";
        break;
    case BRT_SCREEN_DOOR_O:
        ident = "Screendoor-Table";
        break;
    case BRT_INDEX_LIGHT_O:
        ident = "Lighting-Table";
        break;
    case BRT_BUMP_O:
        ident = "Bump-Map";
        break;
    case BRT_UNKNOWN:
        ident = "Unknown";
        break;
    default:
        return NULL;
    }

    self = BrResAllocate(plib->device, sizeof(*self), BR_MEMORY_OBJECT);
    if(self == NULL)
        return NULL;

    self->dispatch = &bufferStoredDispatch;
    self->identifier = ident;
    self->device = plib->device;
    self->plib = plib;
    self->flags |= SBUFF_SHARED;

    SetupRenderBuffer(&self->buffer, pm);

    ObjectContainerAddFront(plib, (br_object *)self);

    return self;
}

static br_error BR_CMETHOD_DECL(br_buffer_stored_vfx, update)(
    struct br_buffer_stored *self,
    struct br_device_pixelmap *pm,
    br_token_value *tv)
{
    SetupRenderBuffer(&self->buffer, pm);
    return BRE_OK;
}

static void BR_CMETHOD_DECL(br_buffer_stored_vfx, free)(br_object *_self)
{
    br_buffer_stored *self = (br_buffer_stored*)_self;
    ObjectContainerRemove(self->plib, (br_object *)self);
    BrResFreeNoCallback(self);
}

static br_token BR_CMETHOD_DECL(br_buffer_stored_vfx, type)(br_object *self)
{
    return BRT_BUFFER_STORED;
}

static br_boolean BR_CMETHOD_DECL(br_buffer_stored_vfx, isType)(br_object *self, br_token t)
{
    return (t == BRT_BUFFER_STORED) || (t == BRT_OBJECT);
}

static br_size_t BR_CMETHOD_DECL(br_buffer_stored_vfx, space)(br_object *self)
{
    return BrResSizeTotal(self);
}

static struct br_tv_template * BR_CMETHOD_DECL(br_buffer_stored_vfx, templateQuery)(br_object *_self)
{
    br_buffer_stored *self = (br_buffer_stored*)_self;

    if(self->device->templates.bufferStoredTemplate == NULL)
        self->device->templates.bufferStoredTemplate = BrTVTemplateAllocate(self->device,
            bufferStoredTemplateEntries, BR_ASIZE(bufferStoredTemplateEntries));

    return self->device->templates.bufferStoredTemplate;
}

static const struct br_buffer_stored_dispatch bufferStoredDispatch = {
    .__reserved0 = NULL,
    .__reserved1 = NULL,
    .__reserved2 = NULL,
    .__reserved3 = NULL,
    ._free       = BR_CMETHOD_REF(br_buffer_stored_vfx, free),
    ._identifier = BR_CMETHOD_REF(br_object_vfxprim, identifier),
    ._type       = BR_CMETHOD_REF(br_buffer_stored_vfx, type),
    ._isType     = BR_CMETHOD_REF(br_buffer_stored_vfx, isType),
    ._device     = BR_CMETHOD_REF(br_object_vfxprim, device),
    ._space      = BR_CMETHOD_REF(br_buffer_stored_vfx, space),

    ._templateQuery = BR_CMETHOD_REF(br_buffer_stored_vfx, templateQuery),
    ._query         = BR_CMETHOD_REF(br_object, query),
    ._queryBuffer   = BR_CMETHOD_REF(br_object, queryBuffer),
    ._queryMany     = BR_CMETHOD_REF(br_object, queryMany),
    ._queryManySize = BR_CMETHOD_REF(br_object, queryManySize),
    ._queryAll      = BR_CMETHOD_REF(br_object, queryAll),
    ._queryAllSize  = BR_CMETHOD_REF(br_object, queryAllSize),

    ._update = BR_CMETHOD_REF(br_buffer_stored_vfx, update),
};
