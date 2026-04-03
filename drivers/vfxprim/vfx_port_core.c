/*
 * vfx_port_core.c - Window/pane management and pane clipping
 *
 * Ported from WINVFX.CPP (C wrapper) and VFX.INC (SET_DEST_PANE macro)
 */

#include <stdlib.h>
#include <string.h>
#include "vfx_compat.h"
#include "vfx_port.h"

/*
 * Allocate a window with an internal pixel buffer.
 * Width and height are the actual pixel dimensions.
 * x_max = width - 1, y_max = height - 1 (max valid coordinates).
 */

VFX_WINDOW *vfx_window_construct(LONG width, LONG height)
{
    VFX_WINDOW *w;
    UBYTE *buf;

    if (width <= 0 || height <= 0) {
        return NULL;
    }

    w = (VFX_WINDOW *)malloc(sizeof(VFX_WINDOW));
    if (w == NULL) {
        return NULL;
    }

    buf = (UBYTE *)calloc((ULONG)width * (ULONG)height, 1);
    if (buf == NULL) {
        free(w);
        return NULL;
    }

    w->buffer = buf;
    w->x_max  = width - 1;
    w->y_max  = height - 1;

    return w;
}

void vfx_window_destroy(VFX_WINDOW *window)
{
    if (window != NULL) {
        if (window->buffer != NULL) {
            free(window->buffer);
        }
        free(window);
    }
}

/*
 * Allocate a pane referencing a region of a window.
 * Coordinates are inclusive: (x0,y0) to (x1,y1).
 */

VFX_PANE *vfx_pane_construct(VFX_WINDOW *window,
                             LONG x0, LONG y0, LONG x1, LONG y1)
{
    VFX_PANE *p;

    if (window == NULL) {
        return NULL;
    }

    p = (VFX_PANE *)malloc(sizeof(VFX_PANE));
    if (p == NULL) {
        return NULL;
    }

    p->window = window;
    p->x0 = x0;
    p->y0 = y0;
    p->x1 = x1;
    p->y1 = y1;

    return p;
}

void vfx_pane_destroy(VFX_PANE *pane)
{
    if (pane != NULL) {
        free(pane);
    }
}

/*
 * Clip pane to its window and compute rendering parameters.
 *
 * This is the C equivalent of the SET_DEST_PANE macro from VFX.INC.
 * All polygon functions call this first.
 *
 * Output:
 *   out->vp_r      = clipped pane width - 1 (max X in pane coords)
 *   out->vp_b      = clipped pane height - 1 (max Y in pane coords)
 *   out->buff_addr = pointer to pane's (0,0) pixel in the window buffer
 *   out->line_size = window scanline stride in bytes
 *
 * Returns 0 on success, -1 if pane is degenerate or fully off-window.
 */

int vfx_clip_pane(VFX_PANE *pane, VFX_CLIP_RESULT *out)
{
    VFX_WINDOW *win;
    LONG clip_x0, clip_y0, clip_x1, clip_y1;

    if (pane == NULL || pane->window == NULL) {
        return -1;
    }

    win = pane->window;

    /* line_size = window width in bytes */
    out->line_size = win->x_max + 1;
    if (out->line_size <= 0) {
        return -1;
    }

    /* Clip pane edges to window bounds */
    clip_x0 = pane->x0;
    if (clip_x0 < 0) clip_x0 = 0;

    clip_y0 = pane->y0;
    if (clip_y0 < 0) clip_y0 = 0;

    clip_x1 = pane->x1;
    if (clip_x1 > win->x_max) clip_x1 = win->x_max;

    clip_y1 = pane->y1;
    if (clip_y1 > win->y_max) clip_y1 = win->y_max;

    /* vp_r and vp_b are the max pane-relative coordinates */
    out->vp_r = clip_x1 - clip_x0;
    out->vp_b = clip_y1 - clip_y0;

    if (out->vp_r < 0 || out->vp_b < 0) {
        return -1;
    }

    /* buff_addr points to the (0,0) pixel of the clipped pane */
    out->buff_addr = win->buffer + (clip_y0 * out->line_size) + clip_x0;

    return 0;
}
