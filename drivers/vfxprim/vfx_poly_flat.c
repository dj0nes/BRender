/*
 * vfx_poly_flat.c - VFX_flat_polygon, ported from VFX3D.ASM lines 136-625
 *
 * Solid-color convex polygon fill. Color taken from vlist[0].c (FIXED16,
 * rounded to nearest integer palette index).
 *
 * The scan-conversion skeleton here is shared by all polygon functions.
 * Left edge walks vertices backward, right edge walks forward, both
 * starting from the topmost vertex.
 */

#include <stddef.h>
#include <string.h>
#include "vfx_compat.h"
#include "vfx_port.h"

void VFX_flat_polygon(VFX_PANE *pane, LONG vcnt, SCRNVERTEX *vlist)
{
    VFX_CLIP_RESULT clip;
    LONG i, top_idx, top_y, bot_y;
    int and_flags, x_clipped;
    UBYTE color_byte;
    LONG vp_r, vp_b, line_size;
    UBYTE *buff_addr;

    /* Edge state */
    LONG lcur, lnxt, rcur, rnxt;
    LONG lcnt, rcnt;
    FIXED16 lx, rx, ldx, rdx;
    LONG line_cnt, y;
    UBYTE *line_base;

    if (vcnt < 3 || vlist == NULL || pane == NULL) {
        return;
    }

    if (vfx_clip_pane(pane, &clip) != 0) {
        return;
    }

    vp_r      = clip.vp_r;
    vp_b      = clip.vp_b;
    buff_addr = clip.buff_addr;
    line_size = clip.line_size;

    /* Read color from first vertex: round 16.16 to nearest integer */
    color_byte = (UBYTE)((vlist[0].c + 0x8000L) >> 16);

    /*
     * Find topmost and bottommost vertices.
     * Sutherland-Cohen trivial rejection: if all vertices are
     * on the same side of the viewport, skip the polygon.
     */
    top_y = 32767;
    bot_y = -32768;
    top_idx = 0;
    and_flags = 0xF;
    x_clipped = 0;

    for (i = 0; i < vcnt; i++) {
        int flags = 0;
        if (vlist[i].x < 0)    flags |= 8;
        if (vlist[i].x > vp_r) flags |= 4;
        if (vlist[i].y < 0)    flags |= 2;
        if (vlist[i].y > vp_b) flags |= 1;

        if (flags & 0xC) x_clipped = 1;   /* any X outside viewport */
        and_flags &= flags;

        if (vlist[i].y <= top_y) {
            top_y = vlist[i].y;
            top_idx = i;
        }
        if (vlist[i].y >= bot_y) {
            bot_y = vlist[i].y;
        }
    }

    if (and_flags != 0) return;   /* fully clipped */
    if (bot_y == top_y)  return;   /* degenerate (flat) polygon */

    y = top_y;

    /*
     * Initialize left edge (walks backward through vertex list)
     */
    lcur = top_idx;
    for (;;) {
        LONG nxt = (lcur - 1 + vcnt) % vcnt;
        LONG edge_h;
        lnxt = nxt;

        edge_h = vlist[lnxt].y - vlist[lcur].y;
        /* Skip edges above viewport */
        if (vlist[lcur].y < 0 && vlist[lnxt].y <= 0) {
            lcur = lnxt;
            continue;
        }
        if (edge_h == 0) {   /* flat edge, advance */
            lcur = lnxt;
            continue;
        }

        lcnt = edge_h;
        ldx = ((vlist[lnxt].x - vlist[lcur].x) << 16) / edge_h;
        lx  = (vlist[lcur].x << 16) + 0x8000L;
        break;
    }

    /*
     * Initialize right edge (walks forward through vertex list)
     */
    rcur = top_idx;
    for (;;) {
        LONG nxt = (rcur + 1) % vcnt;
        LONG edge_h;
        rnxt = nxt;

        edge_h = vlist[rnxt].y - vlist[rcur].y;
        if (vlist[rcur].y < 0 && vlist[rnxt].y <= 0) {
            rcur = rnxt;
            continue;
        }
        if (edge_h == 0) {
            rcur = rnxt;
            continue;
        }

        rcnt = edge_h;
        rdx = ((vlist[rnxt].x - vlist[rcur].x) << 16) / edge_h;
        rx  = (vlist[rcur].x << 16) + 0x8000L;
        break;
    }

    /*
     * Calculate scanline count, clip against bottom of viewport
     */
    line_cnt = vp_b - y;
    if (bot_y <= vp_b) {
        line_cnt = line_cnt - (vp_b - bot_y);
    }

    /*
     * Clip against top of viewport (y < 0)
     */
    if (y < 0) {
        LONG clip_top;

        clip_top = -y;
        line_cnt -= clip_top;
        y = 0;

        /* Advance left edge by clipped scanlines */
        {
            LONG skip = -vlist[lcur].y;
            lcnt -= skip;
            lx += ldx * skip;
        }

        /* Advance right edge by clipped scanlines */
        {
            LONG skip = -vlist[rcur].y;
            rcnt -= skip;
            rx += rdx * skip;
        }
    }

    /* Base address for current scanline */
    line_base = buff_addr + (y * line_size);

    /*
     * Main scanline loop
     */
    while (line_cnt >= 0) {
        LONG left_x, right_x;
        FIXED16 cur_lx, cur_rx;
        LONG px_count;
        UBYTE *dst;

        cur_lx = lx;
        cur_rx = rx;

        /* Ensure left <= right */
        if (cur_rx < cur_lx) {
            FIXED16 tmp = cur_lx;
            cur_lx = cur_rx;
            cur_rx = tmp;
        }

        /* Convert 16.16 to integer (arithmetic shift preserves sign) */
        left_x  = cur_lx >> 16;
        right_x = cur_rx >> 16;

        /* X clipping */
        if (x_clipped) {
            if (left_x > vp_r || right_x < 0) goto next_line;
            if (left_x < 0) left_x = 0;
            if (right_x > vp_r) right_x = vp_r;
        }

        px_count = right_x - left_x + 1;
        if (px_count > 0) {
            dst = line_base + left_x;
            memset(dst, color_byte, (ULONG)px_count);
        }

next_line:
        line_base += line_size;
        line_cnt--;

        if (line_cnt < 0) break;

        /* Step left edge */
        lcnt--;
        if (lcnt <= 0) {
            /* Advance to next vertex on left edge */
            LONG edge_h;
            lcur = lnxt;
            lnxt = (lcur - 1 + vcnt) % vcnt;
            edge_h = vlist[lnxt].y - vlist[lcur].y;
            if (edge_h < 1) edge_h = 1;  /* prevent div-by-zero at bottom */
            lcnt = edge_h;
            ldx = ((vlist[lnxt].x - vlist[lcur].x) << 16) / edge_h;
            lx  = (vlist[lcur].x << 16) + 0x8000L;
        } else {
            lx += ldx;
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
            rx  = (vlist[rcur].x << 16) + 0x8000L;
        } else {
            rx += rdx;
        }
    }
}
