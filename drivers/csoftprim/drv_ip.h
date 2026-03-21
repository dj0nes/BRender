/*
 * Prototypes for functions internal to csoftprim driver
 *
 * Pure C -- no asm externs
 */
#ifndef _DRV_IP_H_
#define _DRV_IP_H_

#ifndef NO_PROTOTYPES

#ifdef __cplusplus
extern "C" {
#endif

/*
 * object.c
 */
const char * BR_CMETHOD_DECL(br_object_softprim, identifier)( br_object *self);
br_device *	BR_CMETHOD_DECL(br_object_softprim, device)( br_object *self);

/*
 * device.c
 */
br_device * DeviceSoftPrimAllocate(const char *identifier);

/*
 * plib.c
 */
extern const br_token PrimPartsTokens[];
struct br_primitive_library * PrimitiveLibrarySoftAllocate(struct br_device *dev, const char * identifier, const char *arguments);

/*
 * pstate.c
 */
struct br_primitive_state * PrimitiveStateSoftAllocate(struct br_primitive_library *plib);

/*
 * sbuffer.c
 */
void SetupRenderBuffer(struct render_buffer *rb, br_device_pixelmap *pm);

struct br_buffer_stored * BufferStoredSoftAllocate(struct br_primitive_library *plib,
	br_token use, struct br_device_pixelmap *pm, br_token_value *tv);

/*
 * rcp.c
 */
extern const unsigned long BR_ASM_DATA _reciprocal[2048];

/*
 * safediv.c
 */
extern int BR_ASM_CALL SafeFixedMac2Div(int,int,int,int,int);
extern int BR_ASM_CALL SafeFixedMac3Div(int,int,int,int,int,int,int);
extern br_int_32 BR_ASM_CALL _sar16(br_int_32 a);

/*
 * tri_render.c
 */
void BR_ASM_CALL TriangleRenderNull(brp_block *block, brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);
void BR_ASM_CALL TriangleRenderFlat_RGB_888_ZB(brp_block *block, brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);
void BR_ASM_CALL TriangleRenderSmooth_RGB_888_ZB(brp_block *block, brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);
void BR_ASM_CALL TriangleRenderTextured_RGB_888_ZB(brp_block *block, brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);
void BR_ASM_CALL TriangleRenderTexturedSmooth_RGB_888_ZB(brp_block *block, brp_vertex *v0, brp_vertex *v1, brp_vertex *v2);

/*
 * l_piz.c
 */
void BR_ASM_CALL LineRenderPIZ2I(brp_block *block, brp_vertex *v0,brp_vertex *v1);
void BR_ASM_CALL LineRenderPIZ2T(brp_block *block, brp_vertex *v0,brp_vertex *v1);
void BR_ASM_CALL LineRenderPIZ2TI(brp_block *block, brp_vertex *v0,brp_vertex *v1);

void BR_ASM_CALL LineRenderPFZ2I(brp_block *block, brp_vertex *v0,brp_vertex *v1);
void BR_ASM_CALL LineRenderPFZ2I555(brp_block *block, brp_vertex *v0,brp_vertex *v1);
void BR_ASM_CALL LineRenderPFZ2I888(brp_block *block, brp_vertex *v0,brp_vertex *v1);

void BR_ASM_CALL LineRenderPIZ2T_RGB_888(brp_block *block, brp_vertex *v0,brp_vertex *v1);
void BR_ASM_CALL LineRenderPIZ2I_RGB_888(brp_block *block, brp_vertex *v0,brp_vertex *v1);
void BR_ASM_CALL LineRenderPIZ2T_RGB_555(brp_block *block, brp_vertex *v0,brp_vertex *v1);
void BR_ASM_CALL LineRenderPIZ2I_RGB_555(brp_block *block, brp_vertex *v0,brp_vertex *v1);

void BR_ASM_CALL LineRenderPIZ2I_RGB_565(brp_block *block, brp_vertex *v0,brp_vertex *v1);

/*
 * p_piz.c
 */
void BR_ASM_CALL PointRenderPIZ2(brp_block *block, brp_vertex *v0);
void BR_ASM_CALL PointRenderPIZ2T(brp_block *block, brp_vertex *v0);
void BR_ASM_CALL PointRenderPIZ2TI(brp_block *block, brp_vertex *v0);

void BR_ASM_CALL PointRenderPIZ2_RGB_888(brp_block *block, brp_vertex *v0);
void BR_ASM_CALL PointRenderPIZ2T_RGB_888(brp_block *block, brp_vertex *v0);

void BR_ASM_CALL PointRenderPIZ2_RGB_555(brp_block *block, brp_vertex *v0);
void BR_ASM_CALL PointRenderPIZ2T_RGB_555(brp_block *block, brp_vertex *v0);

void BR_ASM_CALL PointRenderPIZ2_RGB_565(brp_block *block, brp_vertex *v0);

/*
 * match.c
 */
br_error BR_CMETHOD_DECL(br_primitive_state_soft, renderBegin)(
		struct br_primitive_state *self,
		struct brp_block **rpb,
		br_boolean *block_changed,
		br_boolean *ranges_changed,
		br_boolean no_render,
		br_token prim_type);

br_error BR_CMETHOD_DECL(br_primitive_state_soft, renderEnd)(
		struct br_primitive_state *self,
		struct brp_block *pb);

br_error BR_CMETHOD_DECL(br_primitive_state_soft, rangesQueryF)(
		struct br_primitive_state *self,
		br_float *offset,
		br_float *scale,
		br_int_32 max_comp);

br_error BR_CMETHOD_DECL(br_primitive_state_soft, rangesQueryX)(
		struct br_primitive_state *self,
		br_fixed_ls *offset,
		br_fixed_ls *scale,
		br_int_32 max_comp);

#ifdef __cplusplus
};
#endif

#endif
#endif
