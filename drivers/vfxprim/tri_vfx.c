/*
 * tri_vfx.c - BRender-to-VFX triangle render adapters
 *
 * These functions implement brp_render_fn signatures and convert
 * BRender's brp_vertex (float screen coords) to VFX's SCRNVERTEX
 * (integer screen coords + FIXED16 attributes), then dispatch to
 * the appropriate VFX polygon function.
 */
#include <stddef.h>
#include <string.h>

#include "drv.h"
#include "shortcut.h"
#include "brassert.h"

static void brp_to_scrnvertex(const brp_vertex *src, SCRNVERTEX *dst)
{
    dst->x = (LONG)src->comp_f[C_SX];
    dst->y = (LONG)src->comp_f[C_SY];
    dst->c = (FIXED16)(src->comp_f[C_I] * 65536.0f);
    dst->u = (FIXED16)(src->comp_f[C_U] * 65536.0f);
    dst->v = (FIXED16)(src->comp_f[C_V] * 65536.0f);
    dst->w = 0;
}

void BR_ASM_CALL TriangleRenderVFXFlat(brp_block *block, brp_vertex *v0,
    brp_vertex *v1, brp_vertex *v2)
{
    SCRNVERTEX verts[3];

    brp_to_scrnvertex(v0, &verts[0]);
    brp_to_scrnvertex(v1, &verts[1]);
    brp_to_scrnvertex(v2, &verts[2]);

    VFX_flat_polygon(&vfx_work_area.vfx_colour_pane, 3, verts);
}

void BR_ASM_CALL TriangleRenderVFXGouraud(brp_block *block, brp_vertex *v0,
    brp_vertex *v1, brp_vertex *v2)
{
    SCRNVERTEX verts[3];

    brp_to_scrnvertex(v0, &verts[0]);
    brp_to_scrnvertex(v1, &verts[1]);
    brp_to_scrnvertex(v2, &verts[2]);

    VFX_Gouraud_polygon(&vfx_work_area.vfx_colour_pane, 3, verts);
}

void BR_ASM_CALL TriangleRenderVFXDitheredGouraud(brp_block *block, brp_vertex *v0,
    brp_vertex *v1, brp_vertex *v2)
{
    SCRNVERTEX verts[3];

    brp_to_scrnvertex(v0, &verts[0]);
    brp_to_scrnvertex(v1, &verts[1]);
    brp_to_scrnvertex(v2, &verts[2]);

    VFX_dithered_Gouraud_polygon(&vfx_work_area.vfx_colour_pane,
        0x8000, 3, verts);
}

void BR_ASM_CALL TriangleRenderVFXTextured(brp_block *block, brp_vertex *v0,
    brp_vertex *v1, brp_vertex *v2)
{
    SCRNVERTEX verts[3];

    brp_to_scrnvertex(v0, &verts[0]);
    brp_to_scrnvertex(v1, &verts[1]);
    brp_to_scrnvertex(v2, &verts[2]);

    VFX_map_polygon(&vfx_work_area.vfx_colour_pane, 3, verts,
        &vfx_work_area.vfx_texture_win, 0);
}

void BR_ASM_CALL TriangleRenderVFXTexturedXP(brp_block *block, brp_vertex *v0,
    brp_vertex *v1, brp_vertex *v2)
{
    SCRNVERTEX verts[3];

    brp_to_scrnvertex(v0, &verts[0]);
    brp_to_scrnvertex(v1, &verts[1]);
    brp_to_scrnvertex(v2, &verts[2]);

    VFX_map_polygon(&vfx_work_area.vfx_colour_pane, 3, verts,
        &vfx_work_area.vfx_texture_win, MP_XP);
}

void BR_ASM_CALL TriangleRenderVFXNull(brp_block *block, brp_vertex *v0,
    brp_vertex *v1, brp_vertex *v2)
{
}
