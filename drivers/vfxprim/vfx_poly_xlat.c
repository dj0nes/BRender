/*
 * vfx_poly_xlat.c - VFX_translate_polygon and VFX_map_lookaside
 *
 * Ported from VFX3D.ASM lines 2027-2465 and 3194-3214.
 *
 * VFX_translate_polygon reads each existing pixel in the polygon area,
 * translates it through a 256-byte lookaside table, and writes it back.
 * Used for translucency effects and color overlays.
 *
 * VFX_map_lookaside copies a 256-byte LUT into static storage for use
 * by VFX_map_polygon's MP_XLAT mode.
 */

#include <stddef.h>
#include <string.h>
#include "vfx_compat.h"
#include "vfx_port.h"

/* Static lookaside table for VFX_map_polygon */
static UBYTE g_map_lookaside[256];

void VFX_map_lookaside(UBYTE *table)
{
    if (table != NULL) {
        memcpy(g_map_lookaside, table, 256);
    }
}

/* Accessor for map_polygon to read the lookaside table */
UBYTE *vfx_get_map_lookaside(void)
{
    return g_map_lookaside;
}

void VFX_translate_polygon(VFX_PANE *pane, LONG vcnt, SCRNVERTEX *vlist,
                           void *lookaside)
{
    VFX_CLIP_RESULT clip;
    UBYTE *lut = (UBYTE *)lookaside;
    LONG i, top_idx, top_y, bot_y;
    int and_flags, x_clipped;
    LONG vp_r, vp_b, line_size;
    UBYTE *buff_addr;

    LONG lcur, lnxt, rcur, rnxt;
    LONG lcnt, rcnt;
    FIXED16 lx, rx, ldx, rdx;
    LONG line_cnt, y;
    UBYTE *line_base;

    if (vcnt < 3 || vlist == NULL || pane == NULL || lut == NULL) return;
    if (vfx_clip_pane(pane, &clip) != 0) return;

    vp_r      = clip.vp_r;
    vp_b      = clip.vp_b;
    buff_addr = clip.buff_addr;
    line_size = clip.line_size;

    /* Vertex sort and S-C rejection */
    top_y = 32767;  bot_y = -32768;
    top_idx = 0;
    and_flags = 0xF;
    x_clipped = 0;

    for (i = 0; i < vcnt; i++) {
        int flags = 0;
        if (vlist[i].x < 0)    flags |= 8;
        if (vlist[i].x > vp_r) flags |= 4;
        if (vlist[i].y < 0)    flags |= 2;
        if (vlist[i].y > vp_b) flags |= 1;
        if (flags & 0xC) x_clipped = 1;
        and_flags &= flags;
        if (vlist[i].y <= top_y) { top_y = vlist[i].y; top_idx = i; }
        if (vlist[i].y >= bot_y) { bot_y = vlist[i].y; }
    }

    if (and_flags != 0) return;
    if (bot_y == top_y)  return;

    y = top_y;

    /* Init left edge */
    lcur = top_idx;
    for (;;) {
        LONG edge_h;
        lnxt = (lcur - 1 + vcnt) % vcnt;
        edge_h = vlist[lnxt].y - vlist[lcur].y;
        if ((vlist[lcur].y < 0 && vlist[lnxt].y <= 0) || edge_h == 0) {
            lcur = lnxt; continue;
        }
        lcnt = edge_h;
        ldx = ((vlist[lnxt].x - vlist[lcur].x) << 16) / edge_h;
        lx  = (vlist[lcur].x << 16) + 0x8000L;
        break;
    }

    /* Init right edge */
    rcur = top_idx;
    for (;;) {
        LONG edge_h;
        rnxt = (rcur + 1) % vcnt;
        edge_h = vlist[rnxt].y - vlist[rcur].y;
        if ((vlist[rcur].y < 0 && vlist[rnxt].y <= 0) || edge_h == 0) {
            rcur = rnxt; continue;
        }
        rcnt = edge_h;
        rdx = ((vlist[rnxt].x - vlist[rcur].x) << 16) / edge_h;
        rx  = (vlist[rcur].x << 16) + 0x8000L;
        break;
    }

    line_cnt = vp_b - y;
    if (bot_y <= vp_b) line_cnt -= (vp_b - bot_y);

    if (y < 0) {
        LONG skip_l, skip_r;
        line_cnt -= (-y);
        y = 0;
        skip_l = -vlist[lcur].y;
        lcnt -= skip_l;  lx += ldx * skip_l;
        skip_r = -vlist[rcur].y;
        rcnt -= skip_r;  rx += rdx * skip_r;
    }

    line_base = buff_addr + (y * line_size);

    while (line_cnt >= 0) {
        LONG left_x, right_x;
        FIXED16 cur_lx = lx, cur_rx = rx;
        UBYTE *dst;

        if (cur_rx < cur_lx) {
            FIXED16 tmp = cur_lx; cur_lx = cur_rx; cur_rx = tmp;
        }

        left_x  = cur_lx >> 16;
        right_x = cur_rx >> 16;

        if (x_clipped) {
            if (left_x > vp_r || right_x < 0) goto next_line;
            if (left_x < 0) left_x = 0;
            if (right_x > vp_r) right_x = vp_r;
        }

        /* Translate each pixel through the lookaside table */
        dst = line_base + left_x;
        {
            LONG x;
            for (x = left_x; x <= right_x; x++) {
                *dst = lut[*dst];
                dst++;
            }
        }

next_line:
        line_base += line_size;
        line_cnt--;
        if (line_cnt < 0) break;

        lcnt--;
        if (lcnt <= 0) {
            LONG edge_h;
            lcur = lnxt;
            lnxt = (lcur - 1 + vcnt) % vcnt;
            edge_h = vlist[lnxt].y - vlist[lcur].y;
            if (edge_h < 1) edge_h = 1;
            lcnt = edge_h;
            ldx = ((vlist[lnxt].x - vlist[lcur].x) << 16) / edge_h;
            lx  = (vlist[lcur].x << 16) + 0x8000L;
        } else {
            lx += ldx;
        }

        rcnt--;
        if (rcnt <= 0) {
            LONG edge_h;
            rcur = rnxt;
            rnxt = (rcur + 1) % vcnt;
            edge_h = vlist[rnxt].y - vlist[rcur].y;
            if (edge_h < 1) edge_h = 1;
            rcnt = edge_h;
            rdx = ((vlist[rnxt].x - vlist[rcur].x) << 16) / edge_h;
            rx  = (vlist[rcur].x << 16) + 0x8000L;
        } else {
            rx += rdx;
        }
    }
}
