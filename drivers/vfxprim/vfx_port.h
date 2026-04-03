/*
 * vfx_port.h - VFX polygon primitives ported from VFX3D.ASM to C
 *
 * Original: Miles Design VFX v1.15, John Miles (open-source freeware, 2000)
 * Port: C90 for WATCOM 11 / Windows 98, 8bpp paletted mode
 *
 * This is a minimal port of the polygon fill functions and their
 * infrastructure dependencies. No sound, no SAL, no VFXREND.
 */

#ifndef VFX_PORT_H
#define VFX_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Basic types - match original VFX.H conventions.
 * Guard against conflicts with windows.h (which defines BYTE, WORD, LONG
 * as unsigned types). When windows.h is included first, skip conflicting
 * typedefs and use windows.h types for LONG/ULONG.
 */

#ifndef VFX_TYPES_DEFINED
#define VFX_TYPES_DEFINED

typedef unsigned char  UBYTE;
typedef unsigned short UWORD;

#ifndef _WINDEF_
typedef unsigned long  ULONG;
typedef signed   char  BYTE;
typedef signed   short WORD;
typedef signed   long  LONG;
#endif

typedef signed long FIXED16;   /* 16:16 fixed-point [-32K, +32K] */
typedef signed long FIXED30;   /* 2:30 fixed-point [-1.999, +1.999] */

#endif /* VFX_TYPES_DEFINED */

/*
 * Fixed-point conversion macros - verbatim from VFX.H
 */

#define INT_TO_FIXED16(x)       (((long)(int)(x)) << 16)
#define DOUBLE_TO_FIXED16(x)    ((long)((x) * 65536.0 + 0.5))
#define FIXED16_TO_DOUBLE(x)    (((double)(x)) / 65536.0)
#define FIXED16_TO_INT(x)       ((int)((x) < 0 ? -(-(x) >> 16) : (x) >> 16))
#define ROUND_FIXED16_TO_INT(x) ((int)((x) < 0 ? -((32768-(x)) >> 16) : ((x)+32768) >> 16))

#define FIXED16_TO_FIXED30(x)   ((x) << 14)
#define FIXED30_TO_FIXED16(x)   ((x) >> 14)

/*
 * VFX_map_polygon() flags
 */

#define MP_XLAT  0x0001   /* Use lookaside table (speed loss ~9%) */
#define MP_XP    0x0002   /* Enable transparency (speed loss ~6%) */

/*
 * Transparent color for 8bpp mode
 */

#define VFX_TRANSPARENT 255

/*
 * Core data structures
 */

typedef struct vfx_window {
    UBYTE *buffer;
    LONG   x_max;    /* max valid X coordinate (width - 1) */
    LONG   y_max;    /* max valid Y coordinate (height - 1) */
} VFX_WINDOW;

typedef struct vfx_pane {
    VFX_WINDOW *window;
    LONG x0;
    LONG y0;
    LONG x1;
    LONG y1;
} VFX_PANE;

typedef struct scrnvertex {
    LONG    x;       /* screen X (integer) */
    LONG    y;       /* screen Y (integer) */
    FIXED16 c;       /* color / intensity (16.16 fixed-point) */
    FIXED16 u;       /* texture source X (16.16 fixed-point) */
    FIXED16 v;       /* texture source Y (16.16 fixed-point) */
    FIXED30 w;       /* perspective divisor (unused by VFX3D) */
} SCRNVERTEX;

/*
 * Internal: pane clip result, used by all polygon functions.
 * Output of vfx_clip_pane().
 */

typedef struct vfx_clip_result {
    LONG   vp_r;       /* max pane-relative X (pane width - 1) */
    LONG   vp_b;       /* max pane-relative Y (pane height - 1) */
    UBYTE *buff_addr;  /* pointer to pane's (0,0) in window buffer */
    LONG   line_size;  /* bytes per scanline in window */
} VFX_CLIP_RESULT;

/*
 * Window / pane management
 */

VFX_WINDOW *vfx_window_construct(LONG width, LONG height);
void        vfx_window_destroy(VFX_WINDOW *window);

VFX_PANE   *vfx_pane_construct(VFX_WINDOW *window,
                               LONG x0, LONG y0, LONG x1, LONG y1);
void        vfx_pane_destroy(VFX_PANE *pane);

/*
 * Internal: clip pane to window, fill VFX_CLIP_RESULT.
 * Returns 0 on success, -1 if pane is degenerate or off-window.
 */

int vfx_clip_pane(VFX_PANE *pane, VFX_CLIP_RESULT *out);

/*
 * Polygon primitives
 */

void VFX_flat_polygon(VFX_PANE *pane, LONG vcnt, SCRNVERTEX *vlist);

void VFX_Gouraud_polygon(VFX_PANE *pane, LONG vcnt, SCRNVERTEX *vlist);

void VFX_dithered_Gouraud_polygon(VFX_PANE *pane, FIXED16 dither_amount,
                                  LONG vcnt, SCRNVERTEX *vlist);

void VFX_map_lookaside(UBYTE *table);

void VFX_map_polygon(VFX_PANE *pane, LONG vcnt, SCRNVERTEX *vlist,
                     VFX_WINDOW *texture, ULONG flags);

void VFX_translate_polygon(VFX_PANE *pane, LONG vcnt, SCRNVERTEX *vlist,
                           void *lookaside);

void VFX_illuminate_polygon(VFX_PANE *pane, FIXED16 dither_amount,
                            LONG vcnt, SCRNVERTEX *vlist);

#ifdef __cplusplus
}
#endif

#endif /* VFX_PORT_H */
