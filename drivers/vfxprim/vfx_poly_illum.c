/*
 * vfx_poly_illum.c - VFX_illuminate_polygon
 *
 * Ported from VFX3D.ASM lines 2468-3191.
 *
 * Like dithered Gouraud, but ADDS the interpolated color value to the
 * existing pixel rather than replacing it. This implements lighting:
 * brighten existing texture/color by a spatially varying amount.
 *
 * No clamping is performed. If (existing + illumination) > 255, it wraps.
 * Games using this arrange their palettes so wrap produces acceptable results.
 */

#include <stddef.h>
#include "vfx_compat.h"
#include "vfx_port.h"

void VFX_illuminate_polygon(VFX_PANE *pane, FIXED16 dither_amount,
                            LONG vcnt, SCRNVERTEX *vlist)
{
    VFX_CLIP_RESULT clip;
    LONG i, top_idx, top_y, bot_y;
    int and_flags, x_clipped;
    LONG vp_r, vp_b, line_size;
    UBYTE *buff_addr;

    LONG lcur, lnxt, rcur, rnxt;
    LONG lcnt, rcnt;
    FIXED16 lx, rx, ldx, rdx;
    FIXED16 lc, rc, ldc, rdc;
    LONG line_cnt, y;
    UBYTE *line_base;

    if (vcnt < 3 || vlist == NULL || pane == NULL) return;
    if (vfx_clip_pane(pane, &clip) != 0) return;

    vp_r      = clip.vp_r;
    vp_b      = clip.vp_b;
    buff_addr = clip.buff_addr;
    line_size = clip.line_size;

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
        ldc = (vlist[lnxt].c - vlist[lcur].c) / edge_h;
        lx  = (vlist[lcur].x << 16) + 0x8000L;
        lc  = vlist[lcur].c + 0x8000L;
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
        rdc = (vlist[rnxt].c - vlist[rcur].c) / edge_h;
        rx  = (vlist[rcur].x << 16) + 0x8000L;
        rc  = vlist[rcur].c + 0x8000L;
        break;
    }

    line_cnt = vp_b - y;
    if (bot_y <= vp_b) line_cnt -= (vp_b - bot_y);

    if (y < 0) {
        LONG skip_l = -vlist[lcur].y;
        LONG skip_r = -vlist[rcur].y;
        line_cnt -= (-y);
        y = 0;
        lcnt -= skip_l;  lx += ldx * skip_l;  lc += ldc * skip_l;
        rcnt -= skip_r;  rx += rdx * skip_r;  rc += rdc * skip_r;
    }

    line_base = buff_addr + (y * line_size);

    while (line_cnt >= 0) {
        LONG left_x, right_x, px_count;
        FIXED16 cur_lx = lx, cur_rx = rx;
        FIXED16 cur_lc = lc, cur_rc = rc;
        FIXED16 dc, cur_c;
        UBYTE *dst;

        if (cur_rx < cur_lx) {
            FIXED16 tmp;
            tmp = cur_lx; cur_lx = cur_rx; cur_rx = tmp;
            tmp = cur_lc; cur_lc = cur_rc; cur_rc = tmp;
        }

        left_x  = cur_lx >> 16;
        right_x = cur_rx >> 16;

        if (x_clipped) {
            if (left_x > vp_r || right_x < 0) goto next_line;
        }

        px_count = right_x - left_x;
        dc = (px_count > 0) ? (cur_rc - cur_lc) / px_count : 0;
        cur_c = cur_lc;

        if (x_clipped && left_x < 0) {
            cur_c -= dc * left_x;
            left_x = 0;
        }
        if (x_clipped && right_x > vp_r) {
            right_x = vp_r;
        }

        /* ADD interpolated illumination to existing pixels */
        dst = line_base + left_x;
        {
            LONG x;
            FIXED16 c_val = cur_c;
            for (x = left_x; x <= right_x; x++) {
                UBYTE illum = (UBYTE)((c_val + ((x & 1) ? 0 : dither_amount)) >> 16);
                *dst = (UBYTE)(*dst + illum);   /* no clamp, wraps like original */
                dst++;
                c_val += dc;
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
            ldc = (vlist[lnxt].c - vlist[lcur].c) / edge_h;
            lx  = (vlist[lcur].x << 16) + 0x8000L;
            lc  = vlist[lcur].c + 0x8000L;
        } else {
            lx += ldx;
            lc += ldc;
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
            rdc = (vlist[rnxt].c - vlist[rcur].c) / edge_h;
            rx  = (vlist[rcur].x << 16) + 0x8000L;
            rc  = vlist[rcur].c + 0x8000L;
        } else {
            rx += rdx;
            rc += rdc;
        }
    }
}
