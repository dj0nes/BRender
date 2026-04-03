/*
 * vfx_poly_map.c - VFX_map_polygon
 *
 * Ported from VFX3D.ASM lines 3217-3934.
 *
 * Affine texture mapping. Interpolates U,V texture coordinates across
 * edges and scanlines, reads texels from a source WINDOW, writes to
 * the destination pane.
 *
 * Four modes selected by flags parameter:
 *   0              - plain copy
 *   MP_XLAT        - translate texels through map_lookaside table
 *   MP_XP          - skip transparent texels (color 255)
 *   MP_XLAT|MP_XP  - translate + skip transparent
 *
 * The original ASM uses a UV_step[4] table with carry-bit indexing
 * for the inner loop. We use straightforward fixed-point stepping
 * and direct texture indexing instead.
 */

#include <stddef.h>
#include "vfx_compat.h"
#include "vfx_port.h"

/* Declared in vfx_poly_xlat.c */
extern UBYTE *vfx_get_map_lookaside(void);

void VFX_map_polygon(VFX_PANE *pane, LONG vcnt, SCRNVERTEX *vlist,
                     VFX_WINDOW *texture, ULONG flags)
{
    VFX_CLIP_RESULT clip;
    LONG i, top_idx, top_y, bot_y;
    int and_flags, x_clipped;
    LONG vp_r, vp_b, line_size;
    UBYTE *buff_addr;
    UBYTE *txt_bitmap;
    LONG txt_width;
    UBYTE *lookaside;
    int do_xlat, do_xp;

    /* Edge state: X, U, V */
    LONG lcur, lnxt, rcur, rnxt;
    LONG lcnt, rcnt;
    FIXED16 lx, rx, ldx, rdx;
    FIXED16 lu, ru, ldu, rdu;
    FIXED16 lv, rv, ldv, rdv;
    LONG line_cnt, y;
    UBYTE *line_base;

    if (vcnt < 3 || vlist == NULL || pane == NULL || texture == NULL) return;
    if (texture->buffer == NULL) return;
    if (vfx_clip_pane(pane, &clip) != 0) return;

    vp_r      = clip.vp_r;
    vp_b      = clip.vp_b;
    buff_addr = clip.buff_addr;
    line_size = clip.line_size;

    txt_bitmap = texture->buffer;
    txt_width  = texture->x_max + 1;

    do_xlat = (flags & MP_XLAT) != 0;
    do_xp   = (flags & MP_XP) != 0;
    lookaside = do_xlat ? vfx_get_map_lookaside() : NULL;

    /* Vertex sort and S-C rejection */
    top_y = 32767;  bot_y = -32768;
    top_idx = 0;
    and_flags = 0xF;
    x_clipped = 0;

    for (i = 0; i < vcnt; i++) {
        int f = 0;
        if (vlist[i].x < 0)    f |= 8;
        if (vlist[i].x > vp_r) f |= 4;
        if (vlist[i].y < 0)    f |= 2;
        if (vlist[i].y > vp_b) f |= 1;
        if (f & 0xC) x_clipped = 1;
        and_flags &= f;
        if (vlist[i].y <= top_y) { top_y = vlist[i].y; top_idx = i; }
        if (vlist[i].y >= bot_y) { bot_y = vlist[i].y; }
    }

    if (and_flags != 0) return;
    if (bot_y == top_y)  return;
    y = top_y;

    /* Init left edge (backward) with X, U, V */
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
        ldu = (vlist[lnxt].u - vlist[lcur].u) / edge_h;
        ldv = (vlist[lnxt].v - vlist[lcur].v) / edge_h;
        lx  = (vlist[lcur].x << 16) + 0x8000L;
        lu  = vlist[lcur].u + 0x8000L;
        lv  = vlist[lcur].v + 0x8000L;
        break;
    }

    /* Init right edge (forward) with X, U, V */
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
        rdu = (vlist[rnxt].u - vlist[rcur].u) / edge_h;
        rdv = (vlist[rnxt].v - vlist[rcur].v) / edge_h;
        rx  = (vlist[rcur].x << 16) + 0x8000L;
        ru  = vlist[rcur].u + 0x8000L;
        rv  = vlist[rcur].v + 0x8000L;
        break;
    }

    line_cnt = vp_b - y;
    if (bot_y <= vp_b) line_cnt -= (vp_b - bot_y);

    /* Clip top */
    if (y < 0) {
        LONG skip_l = -vlist[lcur].y;
        LONG skip_r = -vlist[rcur].y;
        line_cnt -= (-y);
        y = 0;
        lcnt -= skip_l;
        lx += ldx * skip_l;  lu += ldu * skip_l;  lv += ldv * skip_l;
        rcnt -= skip_r;
        rx += rdx * skip_r;  ru += rdu * skip_r;  rv += rdv * skip_r;
    }

    line_base = buff_addr + (y * line_size);

    /* Main scanline loop */
    while (line_cnt >= 0) {
        LONG left_x, right_x, px_count;
        FIXED16 cur_lx, cur_rx, cur_lu, cur_ru, cur_lv, cur_rv;
        FIXED16 du_scan, dv_scan, cur_u, cur_v;
        UBYTE *dst;

        cur_lx = lx;  cur_rx = rx;
        cur_lu = lu;  cur_ru = ru;
        cur_lv = lv;  cur_rv = rv;

        /* Ensure left <= right, swap U/V too */
        if (cur_rx < cur_lx) {
            FIXED16 tmp;
            tmp = cur_lx; cur_lx = cur_rx; cur_rx = tmp;
            tmp = cur_lu; cur_lu = cur_ru; cur_ru = tmp;
            tmp = cur_lv; cur_lv = cur_rv; cur_rv = tmp;
        }

        left_x  = cur_lx >> 16;
        right_x = cur_rx >> 16;

        if (x_clipped) {
            if (left_x > vp_r || right_x < 0) goto next_line;
        }

        px_count = right_x - left_x;
        if (px_count < 0) goto next_line;

        /* Calculate U/V deltas across scanline */
        if (px_count > 0) {
            du_scan = (cur_ru - cur_lu) / px_count;
            dv_scan = (cur_rv - cur_lv) / px_count;
        } else {
            du_scan = 0;
            dv_scan = 0;
        }

        cur_u = cur_lu;
        cur_v = cur_lv;

        /* Clip left: advance U/V by clipped pixels */
        if (x_clipped && left_x < 0) {
            LONG clip_px = -left_x;
            cur_u += du_scan * clip_px;
            cur_v += dv_scan * clip_px;
            left_x = 0;
        }

        if (x_clipped && right_x > vp_r) {
            right_x = vp_r;
        }

        /* Fill scanline with texture samples */
        dst = line_base + left_x;
        {
            LONG x;
            for (x = left_x; x <= right_x; x++) {
                LONG u_int = (cur_u >> 16);
                LONG v_int = (cur_v >> 16);
                UBYTE texel;

                /* Bounds safety (no tiling in this port) */
                if (u_int >= 0 && v_int >= 0 &&
                    u_int <= texture->x_max && v_int <= texture->y_max) {
                    texel = txt_bitmap[v_int * txt_width + u_int];
                } else {
                    texel = 0;
                }

                if (do_xlat) {
                    texel = lookaside[texel];
                }

                if (do_xp && texel == VFX_TRANSPARENT) {
                    /* skip transparent pixel */
                } else {
                    *dst = texel;
                }

                dst++;
                cur_u += du_scan;
                cur_v += dv_scan;
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
            ldu = (vlist[lnxt].u - vlist[lcur].u) / edge_h;
            ldv = (vlist[lnxt].v - vlist[lcur].v) / edge_h;
            lx  = (vlist[lcur].x << 16) + 0x8000L;
            lu  = vlist[lcur].u + 0x8000L;
            lv  = vlist[lcur].v + 0x8000L;
        } else {
            lx += ldx;  lu += ldu;  lv += ldv;
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
            rdu = (vlist[rnxt].u - vlist[rcur].u) / edge_h;
            rdv = (vlist[rnxt].v - vlist[rcur].v) / edge_h;
            rx  = (vlist[rcur].x << 16) + 0x8000L;
            ru  = vlist[rcur].u + 0x8000L;
            rv  = vlist[rcur].v + 0x8000L;
        } else {
            rx += rdx;  ru += rdu;  rv += rdv;
        }
    }
}
