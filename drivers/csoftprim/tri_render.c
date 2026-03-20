/*
 * csoftprim triangle renderers
 *
 * Pure C Gouraud-shaded and flat-shaded triangle rasterizers
 * for RGB_888 with 16-bit Z-buffer.
 *
 * Translated from the pentprim tt24_piz.asm algorithm.
 */
#include "drv.h"
#include "shortcut.h"
#include "brassert.h"

/*
 * Null triangle renderer (fallback)
 */
void BR_ASM_CALL TriangleRenderNull(brp_block *block, brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
	(void)block; (void)v0; (void)v1; (void)v2;
}

/*
 * Sort three vertices by screen Y ascending.
 * After sorting: a->sy <= b->sy <= c->sy
 */
#define SORT_VERTICES_BY_Y(a, b, c, v0, v1, v2) do { \
	brp_vertex *_tmp; \
	a = v0; b = v1; c = v2; \
	if (a->comp_x[C_SY] > b->comp_x[C_SY]) { _tmp = a; a = b; b = _tmp; } \
	if (b->comp_x[C_SY] > c->comp_x[C_SY]) { _tmp = b; b = c; c = _tmp; } \
	if (a->comp_x[C_SY] > b->comp_x[C_SY]) { _tmp = a; a = b; b = _tmp; } \
} while(0)

/*
 * PARAM_SETUP: compute gradient and per-scanline deltas for one parameter.
 *
 * This is the C version of the PARAM_SETUP macro from perspzi.h:
 *   grad_x = (dp1*main.y - dp2*top.y) / g_divisor
 *   grad_y = (dp2*top.x - dp1*main.x) / g_divisor
 *   current = a->p + half-pixel correction
 *   d_nocarry = floor(main.grad)*grad_x + grad_y
 *   d_carry = d_nocarry + grad_x
 */
#define PARAM_SETUP(param, comp) do { \
	br_int_32 _dp1 = b->comp - a->comp; \
	br_int_32 _dp2 = c->comp - a->comp; \
	param.grad_x = SafeFixedMac2Div(_dp1, work.main.y, -_dp2, work.top.y, g_divisor); \
	param.grad_y = SafeFixedMac2Div(_dp2, work.top.x, -_dp1, work.main.x, g_divisor); \
	param.current = a->comp + (g_divisor >= 0 ? param.grad_x / 2 : -param.grad_x / 2); \
	param.d_nocarry = BrFixedToInt(work.main.grad) * param.grad_x + param.grad_y; \
	param.d_carry = param.d_nocarry + param.grad_x; \
} while(0)

/*
 * Same as PARAM_SETUP but XORs 0x80000000 into current for unsigned params (Z)
 */
#define PARAM_SETUP_UNSIGNED(param, comp) do { \
	br_int_32 _dp1 = b->comp - a->comp; \
	br_int_32 _dp2 = c->comp - a->comp; \
	param.grad_x = SafeFixedMac2Div(_dp1, work.main.y, -_dp2, work.top.y, g_divisor); \
	param.grad_y = SafeFixedMac2Div(_dp2, work.top.x, -_dp1, work.main.x, g_divisor); \
	param.current = (a->comp + (g_divisor >= 0 ? param.grad_x / 2 : -param.grad_x / 2)) ^ 0x80000000; \
	param.d_nocarry = BrFixedToInt(work.main.grad) * param.grad_x + param.grad_y; \
	param.d_carry = param.d_nocarry + param.grad_x; \
} while(0)

/*
 * Common triangle setup: sort vertices, compute edges, gradients.
 * Returns non-zero if the triangle is degenerate (zero area).
 *
 * After this, work.main/top/bot are set up, and g_divisor is the 2x area.
 * a, b, c are sorted top-to-bottom by screen Y.
 */
static int triangle_setup(brp_vertex *v0, brp_vertex *v1, brp_vertex *v2,
                          brp_vertex **pa, brp_vertex **pb, brp_vertex **pc,
                          br_int_32 *p_g_divisor)
{
	brp_vertex *a, *b, *c;
	br_int_32 sxa, sya, sxb, syb, sxc, syc;
	br_int_32 g_divisor;

	SORT_VERTICES_BY_Y(a, b, c, v0, v1, v2);

	/* Integer screen coords from 16.16 fixed */
	sxa = a->comp_x[C_SX] >> 16;
	sya = a->comp_x[C_SY] >> 16;
	sxb = b->comp_x[C_SX] >> 16;
	syb = b->comp_x[C_SY] >> 16;
	sxc = c->comp_x[C_SX] >> 16;
	syc = c->comp_x[C_SY] >> 16;

	/* Edge deltas */
	work.main.x = sxc - sxa;
	work.main.y = syc - sya;
	work.top.x  = sxb - sxa;
	work.top.y  = syb - sya;
	work.bot.x  = sxc - sxb;
	work.bot.y  = syc - syb;

	/* 2x signed area (cross product of main and top edge) */
	g_divisor = work.main.x * work.top.y - work.top.x * work.main.y;

	if (g_divisor == 0)
		return 1; /* Degenerate */

	/* Main edge gradient (16.16 fixed) */
	work.main.grad = work.main.y ? work.main.x * _reciprocal[work.main.y] : 0;
	work.main.d_f = work.main.grad << 16;
	work.main.d_i = _sar16(work.main.grad);

	/* Top minor edge */
	work.top.grad = work.top.y ? work.top.x * _reciprocal[work.top.y] : 0;
	work.top.d_f = work.top.grad << 16;
	work.top.d_i = _sar16(work.top.grad);
	work.top.count = syb - sya;

	/* Bottom minor edge */
	work.bot.grad = work.bot.y ? work.bot.x * _reciprocal[work.bot.y] : 0;
	work.bot.d_f = work.bot.grad << 16;
	work.bot.d_i = _sar16(work.bot.grad);
	work.bot.count = syc - syb;

	/* Half-pixel shift for sub-pixel accuracy */
	work.main.f = work.top.f = work.bot.f = 0x80000000;

	/* Starting X positions */
	work.main.i = sxa;
	work.main.start = sya;
	work.top.i = sxa;
	work.top.start = sya;
	work.bot.i = sxb;
	work.bot.start = syb;

	*pa = a;
	*pb = b;
	*pc = c;
	*p_g_divisor = g_divisor;

	return 0;
}

/*
 * Inner scanline rasterizer for flat-shaded RGB_888 + Z16
 */
static void scanline_flat_zb(int y, int x_start, int x_end,
                             br_uint_32 z_start, br_int_32 dz,
                             char *colour_base, br_int_32 colour_stride,
                             char *depth_base, br_int_32 depth_stride,
                             int r, int g, int b_col)
{
	char *cline;
	unsigned short *zline;
	int x;
	br_uint_32 z;

	if (x_start >= x_end)
		return;

	cline = colour_base + y * colour_stride;
	zline = (unsigned short *)(depth_base + y * depth_stride);

	z = z_start;

	for (x = x_start; x < x_end; x++) {
		unsigned short zval = (unsigned short)(z >> 16);
		if (zline[x] > zval) {
			zline[x] = zval;
			cline[x * 3 + 0] = b_col;
			cline[x * 3 + 1] = g;
			cline[x * 3 + 2] = r;
		}
		z += dz;
	}
}

/*
 * Inner scanline rasterizer for Gouraud-shaded RGB_888 + Z16
 */
static void scanline_smooth_zb(int y, int x_start, int x_end,
                               br_uint_32 z_start, br_int_32 dz,
                               br_int_32 r_start, br_int_32 dr,
                               br_int_32 g_start, br_int_32 dg,
                               br_int_32 b_start, br_int_32 db,
                               char *colour_base, br_int_32 colour_stride,
                               char *depth_base, br_int_32 depth_stride)
{
	char *cline;
	unsigned short *zline;
	int x;
	br_uint_32 z;
	br_int_32 r, g, b;

	if (x_start >= x_end)
		return;

	cline = colour_base + y * colour_stride;
	zline = (unsigned short *)(depth_base + y * depth_stride);

	z = z_start;
	r = r_start;
	g = g_start;
	b = b_start;

	for (x = x_start; x < x_end; x++) {
		unsigned short zval = (unsigned short)(z >> 16);
		if (zline[x] > zval) {
			zline[x] = zval;
			/* Fixed-point truncation: >> 16 gives authentic Gouraud banding */
			cline[x * 3 + 0] = (unsigned char)(b >> 16);
			cline[x * 3 + 1] = (unsigned char)(g >> 16);
			cline[x * 3 + 2] = (unsigned char)(r >> 16);
		}
		z += dz;
		r += dr;
		g += dg;
		b += db;
	}
}

/*
 * Walk one half of the triangle (top or bottom trapezoid).
 *
 * major_edge walks the long edge; minor_edge walks the short edge.
 * g_divisor sign determines which is left vs right.
 */
static void walk_trapezoid_flat(struct scan_edge *major, struct scan_edge *minor,
                                int count, br_int_32 g_divisor,
                                char *colour_base, br_int_32 colour_stride,
                                char *depth_base, br_int_32 depth_stride,
                                int r, int g, int b_col)
{
	int y, x_left, x_right;
	br_uint_32 z_left;
	int scanline;

	for (scanline = 0; scanline < count; scanline++) {
		y = major->start + scanline;

		if (y < 0)
			goto step;
		if (y >= (int)work.colour.height)
			break;

		if (g_divisor >= 0) {
			/* major is right edge, minor is left */
			x_left = minor->i;
			x_right = major->i;
		} else {
			/* major is left edge, minor is right */
			x_left = major->i;
			x_right = minor->i;
		}

		/* Clamp to screen */
		if (x_left < 0) x_left = 0;
		if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;

		/* Compute Z at left edge of scanline */
		z_left = work.pz.current + work.pz.grad_x * x_left;

		scanline_flat_zb(y, x_left, x_right,
		                 z_left, work.pz.grad_x,
		                 colour_base, colour_stride,
		                 depth_base, depth_stride,
		                 r, g, b_col);

step:
		/* Step major edge */
		{
			br_uint_32 new_f = major->f + major->d_f;
			if (new_f < major->f) {
				/* Carry */
				major->i += major->d_i + 1;
			} else {
				major->i += major->d_i;
			}
			major->f = new_f;
		}

		/* Step minor edge */
		{
			br_uint_32 new_f = minor->f + minor->d_f;
			if (new_f < minor->f) {
				minor->i += minor->d_i + 1;
			} else {
				minor->i += minor->d_i;
			}
			minor->f = new_f;
		}

		/* Step Z along Y */
		{
			br_uint_32 new_f = major->f; /* just stepped */
			/* Use carry/nocarry from the major edge step */
			/* Actually, the parameter stepping uses the main edge carry */
		}
		/* Step parameters: check if major edge carried this scanline */
		{
			/* We already stepped the major edge. The carry was from the fractional add.
			 * For parameter stepping, we use d_carry if carry occurred, d_nocarry otherwise.
			 * But we need to detect whether the main edge (which is always the long edge)
			 * carried. Let me re-think this.
			 *
			 * In BRender's scan converter, parameters step along the main (long) edge.
			 * The carry/nocarry distinction is about the main edge's fractional X accumulator.
			 */
		}
	}
}

/*
 * Flat-shaded triangle renderer: RGB_888 + 16-bit Z-buffer
 *
 * Flat colour from vertex 0's R,G,B components.
 */
void BR_ASM_CALL TriangleRenderFlat_RGB_888_ZB(brp_block *block, brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
	brp_vertex *a, *b, *c;
	br_int_32 g_divisor;
	int r_flat, g_flat, b_flat;
	char *colour_base;
	char *depth_base;
	br_int_32 colour_stride, depth_stride;
	int y, scanline;
	int main_x_i, main_x_f;
	int top_x_i, top_x_f;
	int bot_x_i, bot_x_f;
	br_int_32 z_current;
	int x_left, x_right;

	(void)block;

	if (triangle_setup(v0, v1, v2, &a, &b, &c, &g_divisor))
		return;

	/* Setup Z parameter */
	PARAM_SETUP_UNSIGNED(work.pz, comp_x[C_SZ]);

	/* Flat colour from vertex 0 (the top vertex after sort) */
	r_flat = (v0->comp_x[C_R] >> 16) & 0xFF;
	g_flat = (v0->comp_x[C_G] >> 16) & 0xFF;
	b_flat = (v0->comp_x[C_B] >> 16) & 0xFF;

	colour_base = (char *)work.colour.base;
	depth_base = (char *)work.depth.base;
	colour_stride = work.colour.stride_b;
	depth_stride = work.depth.stride_b;

	/*
	 * Rasterize: walk main edge against top edge, then main edge against bot edge.
	 *
	 * The main edge accumulator runs the entire triangle height.
	 * Minor edges run their respective half.
	 */
	main_x_i = a->comp_x[C_SX] >> 16;
	main_x_f = 0x80000000;

	/* Top half: main vs top minor edge */
	{
		int minor_x_i = a->comp_x[C_SX] >> 16;
		int minor_x_f = 0x80000000;

		z_current = work.pz.current;

		for (scanline = 0; scanline < work.top.count; scanline++) {
			y = (a->comp_x[C_SY] >> 16) + scanline;

			if (y >= 0 && y < (int)work.colour.height) {
				if (g_divisor >= 0) {
					x_left = minor_x_i;
					x_right = main_x_i;
				} else {
					x_left = main_x_i;
					x_right = minor_x_i;
				}

				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;

				if (x_left < x_right) {
					br_uint_32 z_scan = z_current + work.pz.grad_x * x_left;
					scanline_flat_zb(y, x_left, x_right,
					                 z_scan, work.pz.grad_x,
					                 colour_base, colour_stride,
					                 depth_base, depth_stride,
					                 r_flat, g_flat, b_flat);
				}
			}

			/* Step main edge */
			{
				br_uint_32 new_f = (br_uint_32)main_x_f + (br_uint_32)work.main.d_f;
				if (new_f < (br_uint_32)main_x_f) {
					main_x_i += work.main.d_i + 1;
					z_current += work.pz.d_carry;
				} else {
					main_x_i += work.main.d_i;
					z_current += work.pz.d_nocarry;
				}
				main_x_f = new_f;
			}

			/* Step top minor edge */
			{
				br_uint_32 new_f = (br_uint_32)minor_x_f + (br_uint_32)work.top.d_f;
				if (new_f < (br_uint_32)minor_x_f) {
					minor_x_i += work.top.d_i + 1;
				} else {
					minor_x_i += work.top.d_i;
				}
				minor_x_f = new_f;
			}
		}
	}

	/* Bottom half: main vs bot minor edge */
	{
		int minor_x_i = b->comp_x[C_SX] >> 16;
		int minor_x_f = 0x80000000;

		for (scanline = 0; scanline < work.bot.count; scanline++) {
			y = (b->comp_x[C_SY] >> 16) + scanline;

			if (y >= 0 && y < (int)work.colour.height) {
				if (g_divisor >= 0) {
					x_left = minor_x_i;
					x_right = main_x_i;
				} else {
					x_left = main_x_i;
					x_right = minor_x_i;
				}

				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;

				if (x_left < x_right) {
					br_uint_32 z_scan = z_current + work.pz.grad_x * x_left;
					scanline_flat_zb(y, x_left, x_right,
					                 z_scan, work.pz.grad_x,
					                 colour_base, colour_stride,
					                 depth_base, depth_stride,
					                 r_flat, g_flat, b_flat);
				}
			}

			/* Step main edge */
			{
				br_uint_32 new_f = (br_uint_32)main_x_f + (br_uint_32)work.main.d_f;
				if (new_f < (br_uint_32)main_x_f) {
					main_x_i += work.main.d_i + 1;
					z_current += work.pz.d_carry;
				} else {
					main_x_i += work.main.d_i;
					z_current += work.pz.d_nocarry;
				}
				main_x_f = new_f;
			}

			/* Step bot minor edge */
			{
				br_uint_32 new_f = (br_uint_32)minor_x_f + (br_uint_32)work.bot.d_f;
				if (new_f < (br_uint_32)minor_x_f) {
					minor_x_i += work.bot.d_i + 1;
				} else {
					minor_x_i += work.bot.d_i;
				}
				minor_x_f = new_f;
			}
		}
	}
}

/*
 * Gouraud-shaded triangle renderer: RGB_888 + 16-bit Z-buffer
 *
 * Interpolates R,G,B across the triangle. The fixed-point >> 16 truncation
 * naturally produces the Gouraud color banding that's the hallmark of 90s
 * software rendering.
 */
void BR_ASM_CALL TriangleRenderSmooth_RGB_888_ZB(brp_block *block, brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
	brp_vertex *a, *b, *c;
	br_int_32 g_divisor;
	char *colour_base;
	char *depth_base;
	br_int_32 colour_stride, depth_stride;
	int y, scanline;
	int main_x_i, main_x_f;
	br_int_32 z_current, r_current, g_current, b_current;
	int x_left, x_right;

	(void)block;

	if (triangle_setup(v0, v1, v2, &a, &b, &c, &g_divisor))
		return;

	/* Setup parameters */
	PARAM_SETUP_UNSIGNED(work.pz, comp_x[C_SZ]);
	PARAM_SETUP(work.pr, comp_x[C_R]);
	PARAM_SETUP(work.pg, comp_x[C_G]);
	PARAM_SETUP(work.pb, comp_x[C_B]);

	colour_base = (char *)work.colour.base;
	depth_base = (char *)work.depth.base;
	colour_stride = work.colour.stride_b;
	depth_stride = work.depth.stride_b;

	main_x_i = a->comp_x[C_SX] >> 16;
	main_x_f = 0x80000000;

	z_current = work.pz.current;
	r_current = work.pr.current;
	g_current = work.pg.current;
	b_current = work.pb.current;

	/* Top half */
	{
		int minor_x_i = a->comp_x[C_SX] >> 16;
		int minor_x_f = 0x80000000;

		for (scanline = 0; scanline < work.top.count; scanline++) {
			y = (a->comp_x[C_SY] >> 16) + scanline;

			if (y >= 0 && y < (int)work.colour.height) {
				if (g_divisor >= 0) {
					x_left = minor_x_i;
					x_right = main_x_i;
				} else {
					x_left = main_x_i;
					x_right = minor_x_i;
				}

				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;

				if (x_left < x_right) {
					br_uint_32 z_scan = z_current + work.pz.grad_x * x_left;
					br_int_32 r_scan = r_current + work.pr.grad_x * x_left;
					br_int_32 g_scan = g_current + work.pg.grad_x * x_left;
					br_int_32 b_scan = b_current + work.pb.grad_x * x_left;

					scanline_smooth_zb(y, x_left, x_right,
					                   z_scan, work.pz.grad_x,
					                   r_scan, work.pr.grad_x,
					                   g_scan, work.pg.grad_x,
					                   b_scan, work.pb.grad_x,
					                   colour_base, colour_stride,
					                   depth_base, depth_stride);
				}
			}

			/* Step main edge */
			{
				br_uint_32 new_f = (br_uint_32)main_x_f + (br_uint_32)work.main.d_f;
				if (new_f < (br_uint_32)main_x_f) {
					main_x_i += work.main.d_i + 1;
					z_current += work.pz.d_carry;
					r_current += work.pr.d_carry;
					g_current += work.pg.d_carry;
					b_current += work.pb.d_carry;
				} else {
					main_x_i += work.main.d_i;
					z_current += work.pz.d_nocarry;
					r_current += work.pr.d_nocarry;
					g_current += work.pg.d_nocarry;
					b_current += work.pb.d_nocarry;
				}
				main_x_f = new_f;
			}

			/* Step top minor edge */
			{
				br_uint_32 new_f = (br_uint_32)minor_x_f + (br_uint_32)work.top.d_f;
				if (new_f < (br_uint_32)minor_x_f) {
					minor_x_i += work.top.d_i + 1;
				} else {
					minor_x_i += work.top.d_i;
				}
				minor_x_f = new_f;
			}
		}
	}

	/* Bottom half */
	{
		int minor_x_i = b->comp_x[C_SX] >> 16;
		int minor_x_f = 0x80000000;

		for (scanline = 0; scanline < work.bot.count; scanline++) {
			y = (b->comp_x[C_SY] >> 16) + scanline;

			if (y >= 0 && y < (int)work.colour.height) {
				if (g_divisor >= 0) {
					x_left = minor_x_i;
					x_right = main_x_i;
				} else {
					x_left = main_x_i;
					x_right = minor_x_i;
				}

				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;

				if (x_left < x_right) {
					br_uint_32 z_scan = z_current + work.pz.grad_x * x_left;
					br_int_32 r_scan = r_current + work.pr.grad_x * x_left;
					br_int_32 g_scan = g_current + work.pg.grad_x * x_left;
					br_int_32 b_scan = b_current + work.pb.grad_x * x_left;

					scanline_smooth_zb(y, x_left, x_right,
					                   z_scan, work.pz.grad_x,
					                   r_scan, work.pr.grad_x,
					                   g_scan, work.pg.grad_x,
					                   b_scan, work.pb.grad_x,
					                   colour_base, colour_stride,
					                   depth_base, depth_stride);
				}
			}

			/* Step main edge */
			{
				br_uint_32 new_f = (br_uint_32)main_x_f + (br_uint_32)work.main.d_f;
				if (new_f < (br_uint_32)main_x_f) {
					main_x_i += work.main.d_i + 1;
					z_current += work.pz.d_carry;
					r_current += work.pr.d_carry;
					g_current += work.pg.d_carry;
					b_current += work.pb.d_carry;
				} else {
					main_x_i += work.main.d_i;
					z_current += work.pz.d_nocarry;
					r_current += work.pr.d_nocarry;
					g_current += work.pg.d_nocarry;
					b_current += work.pb.d_nocarry;
				}
				main_x_f = new_f;
			}

			/* Step bot minor edge */
			{
				br_uint_32 new_f = (br_uint_32)minor_x_f + (br_uint_32)work.bot.d_f;
				if (new_f < (br_uint_32)minor_x_f) {
					minor_x_i += work.bot.d_i + 1;
				} else {
					minor_x_i += work.bot.d_i;
				}
				minor_x_f = new_f;
			}
		}
	}
}
