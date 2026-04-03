/*
 * vfx_poly_gouraud.c - VFX_Gouraud_polygon and VFX_dithered_Gouraud_polygon
 *
 * Ported from VFX3D.ASM lines 628-2024.
 *
 * Gouraud shading interpolates a palette index (FIXED16 vertex color)
 * across both edges and each scanline. The original ASM uses a clever
 * CL/CH register packing trick for the inner loop; we just step one
 * pixel at a time, which the compiler can optimize.
 *
 * The dithered variant adds an alternating +/- dither value to break
 * the visible color banding inherent in 8-bit paletted Gouraud shading.
 */

#include <stddef.h>
#include "vfx_compat.h"
#include "vfx_port.h"

/*
 * Internal: shared Gouraud scan-conversion with optional dithering.
 * If dither_1 and dither_2 are both 0, this is plain Gouraud.
 */

static void gouraud_polygon_internal(VFX_PANE *pane, LONG vcnt,
                                     SCRNVERTEX *vlist,
                                     FIXED16 dither_1, FIXED16 dither_2)
{
    VFX_CLIP_RESULT clip;
    LONG i, top_idx, top_y, bot_y;
    int and_flags, x_clipped;
    LONG vp_r, vp_b, line_size;
    UBYTE *buff_addr;

    /* Edge state */
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

    /* Find topmost/bottommost vertices, S-C rejection */
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

    /* Init left edge (backward) */
    lcur = top_idx;
    for (;;) {
        LONG edge_h;
        lnxt = (lcur - 1 + vcnt) % vcnt;
        edge_h = vlist[lnxt].y - vlist[lcur].y;
        if ((vlist[lcur].y < 0 && vlist[lnxt].y <= 0) || edge_h == 0) {
            lcur = lnxt;
            continue;
        }
        lcnt = edge_h;
        ldx = ((vlist[lnxt].x - vlist[lcur].x) << 16) / edge_h;
        ldc = (vlist[lnxt].c - vlist[lcur].c) / edge_h;
        lx  = (vlist[lcur].x << 16) + 0x8000L;
        lc  = vlist[lcur].c + 0x8000L;
        break;
    }

    /* Init right edge (forward) */
    rcur = top_idx;
    for (;;) {
        LONG edge_h;
        rnxt = (rcur + 1) % vcnt;
        edge_h = vlist[rnxt].y - vlist[rcur].y;
        if ((vlist[rcur].y < 0 && vlist[rnxt].y <= 0) || edge_h == 0) {
            rcur = rnxt;
            continue;
        }
        rcnt = edge_h;
        rdx = ((vlist[rnxt].x - vlist[rcur].x) << 16) / edge_h;
        rdc = (vlist[rnxt].c - vlist[rcur].c) / edge_h;
        rx  = (vlist[rcur].x << 16) + 0x8000L;
        rc  = vlist[rcur].c + 0x8000L;
        break;
    }

    /* Scanline count, clip bottom */
    line_cnt = vp_b - y;
    if (bot_y <= vp_b) line_cnt -= (vp_b - bot_y);

    /* Clip top (y < 0) */
    if (y < 0) {
        LONG clip_top = -y;
        LONG skip_l, skip_r;
        line_cnt -= clip_top;
        y = 0;

        skip_l = -vlist[lcur].y;
        lcnt -= skip_l;
        lx += ldx * skip_l;
        lc += ldc * skip_l;

        skip_r = -vlist[rcur].y;
        rcnt -= skip_r;
        rx += rdx * skip_r;
        rc += rdc * skip_r;
    }

    line_base = buff_addr + (y * line_size);

    /* Main scanline loop */
    while (line_cnt >= 0) {
        LONG left_x, right_x, px_count;
        FIXED16 cur_lx, cur_rx, cur_lc, cur_rc;
        FIXED16 dc, cur_c;
        UBYTE *dst;

        cur_lx = lx;  cur_rx = rx;
        cur_lc = lc;  cur_rc = rc;

        /* Ensure left <= right */
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
        if (px_count < 0) goto next_line;

        /* Calculate color delta across scanline */
        if (px_count > 0) {
            dc = (cur_rc - cur_lc) / px_count;
        } else {
            dc = 0;
        }

        cur_c = cur_lc;

        /* Clip left edge */
        if (x_clipped && left_x < 0) {
            cur_c -= dc * left_x;   /* left_x is negative, so this advances c */
            left_x = 0;
        }

        /* Clip right edge */
        if (x_clipped && right_x > vp_r) {
            right_x = vp_r;
        }

        /* Fill scanline with interpolated color */
        dst = line_base + left_x;
        {
            LONG x;
            FIXED16 c_val = cur_c;
            FIXED16 d1 = dither_1;
            FIXED16 d2 = dither_2;

            for (x = left_x; x <= right_x; x++) {
                FIXED16 dithered = c_val + ((x & 1) ? d2 : d1);
                *dst++ = (UBYTE)(dithered >> 16);
                c_val += dc;
            }
        }

next_line:
        line_base += line_size;
        line_cnt--;
        if (line_cnt < 0) break;

        /* Step left edge */
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

        /* Step right edge */
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

void VFX_Gouraud_polygon(VFX_PANE *pane, LONG vcnt, SCRNVERTEX *vlist)
{
    gouraud_polygon_internal(pane, vcnt, vlist, 0, 0);
}

void VFX_dithered_Gouraud_polygon(VFX_PANE *pane, FIXED16 dither_amount,
                                  LONG vcnt, SCRNVERTEX *vlist)
{
    /*
     * Original ASM uses two dither values that alternate per pixel:
     *   dither_1 = +dither_amount
     *   dither_2 = -dither_amount  (approximately; ASM uses 0 for second)
     *
     * The default VFX dither is 0x8000 (0.5 palette entry) and 0.
     * We follow the original behavior.
     */
    gouraud_polygon_internal(pane, vcnt, vlist, dither_amount, 0);
}
