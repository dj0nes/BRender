/*
 * Internal prototypes for VFX primitive library
 */
#ifndef _DRV_IP_H_
#define _DRV_IP_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * object.c
 */
const char * BR_CMETHOD_DECL(br_object_vfxprim, identifier)(br_object *self);
br_device *  BR_CMETHOD_DECL(br_object_vfxprim, device)(br_object *self);

/*
 * device.c
 */
br_device * DeviceVFXPrimAllocate(const char *identifier);

/*
 * plib.c
 */
struct br_primitive_library * PrimitiveLibraryVFXAllocate(struct br_device *dev,
    const char *identifier, const char *arguments);

extern const br_token VFXPrimPartsTokens[];

/*
 * pstate.c
 */
struct br_primitive_state * PrimitiveStateVFXAllocate(struct br_primitive_library *plib);

/*
 * match.c
 */
br_error BR_CMETHOD_DECL(br_primitive_state_vfx, renderBegin)(
    struct br_primitive_state *self,
    struct brp_block **rpb,
    br_boolean *block_changed,
    br_boolean *ranges_changed,
    br_boolean no_render,
    br_token prim_type);

br_error BR_CMETHOD_DECL(br_primitive_state_vfx, renderEnd)(
    struct br_primitive_state *self,
    struct brp_block *pb);

br_error BR_CMETHOD_DECL(br_primitive_state_vfx, rangesQueryF)(
    struct br_primitive_state *self,
    br_float *offset,
    br_float *scale,
    br_int_32 max_comp);

/*
 * sbuffer.c
 */
struct br_buffer_stored * BufferStoredVFXAllocate(struct br_primitive_library *plib,
    br_token use, struct br_device_pixelmap *pm, br_token_value *tv);

void SetupRenderBuffer(struct render_buffer *rb, br_device_pixelmap *pm);

/*
 * tri_vfx.c
 */
void BR_ASM_CALL TriangleRenderVFXFlat(brp_block *block, brp_vertex *v0,
    brp_vertex *v1, brp_vertex *v2);
void BR_ASM_CALL TriangleRenderVFXGouraud(brp_block *block, brp_vertex *v0,
    brp_vertex *v1, brp_vertex *v2);
void BR_ASM_CALL TriangleRenderVFXDitheredGouraud(brp_block *block, brp_vertex *v0,
    brp_vertex *v1, brp_vertex *v2);
void BR_ASM_CALL TriangleRenderVFXTextured(brp_block *block, brp_vertex *v0,
    brp_vertex *v1, brp_vertex *v2);
void BR_ASM_CALL TriangleRenderVFXTexturedXP(brp_block *block, brp_vertex *v0,
    brp_vertex *v1, brp_vertex *v2);
void BR_ASM_CALL TriangleRenderVFXNull(brp_block *block, brp_vertex *v0,
    brp_vertex *v1, brp_vertex *v2);

#ifdef __cplusplus
};
#endif
#endif
