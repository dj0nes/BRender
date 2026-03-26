/*
 * csoftprim triangle renderers
 *
 * Pure C flat, Gouraud, and textured triangle rasterizers
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
	param.current = a->comp + param.grad_x / 2; \
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
	param.current = (a->comp + param.grad_x / 2) ^ 0x80000000; \
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

	/* 2x signed area (cross product of top and main edge) */
	g_divisor = work.top.x * work.main.y - work.main.x * work.top.y;

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
		if (zline[x] >= zval) {
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
		if (zline[x] >= zval) {
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
					x_left = main_x_i;
					x_right = minor_x_i;
				} else {
					x_left = minor_x_i;
					x_right = main_x_i;
				}

				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;

				if (x_left < x_right) {
					br_uint_32 z_scan = z_current + work.pz.grad_x * (x_left - main_x_i);
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
					x_left = main_x_i;
					x_right = minor_x_i;
				} else {
					x_left = minor_x_i;
					x_right = main_x_i;
				}

				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;

				if (x_left < x_right) {
					br_uint_32 z_scan = z_current + work.pz.grad_x * (x_left - main_x_i);
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
					x_left = main_x_i;
					x_right = minor_x_i;
				} else {
					x_left = minor_x_i;
					x_right = main_x_i;
				}

				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;

				if (x_left < x_right) {
					br_uint_32 z_scan = z_current + work.pz.grad_x * (x_left - main_x_i);
					br_int_32 r_scan = r_current + work.pr.grad_x * (x_left - main_x_i);
					br_int_32 g_scan = g_current + work.pg.grad_x * (x_left - main_x_i);
					br_int_32 b_scan = b_current + work.pb.grad_x * (x_left - main_x_i);

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
					x_left = main_x_i;
					x_right = minor_x_i;
				} else {
					x_left = minor_x_i;
					x_right = main_x_i;
				}

				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;

				if (x_left < x_right) {
					br_uint_32 z_scan = z_current + work.pz.grad_x * (x_left - main_x_i);
					br_int_32 r_scan = r_current + work.pr.grad_x * (x_left - main_x_i);
					br_int_32 g_scan = g_current + work.pg.grad_x * (x_left - main_x_i);
					br_int_32 b_scan = b_current + work.pb.grad_x * (x_left - main_x_i);

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

/*
 * Inner scanline for textured RGB_888 + Z16 (no lighting modulation)
 */
static void scanline_textured_zb(int y, int x_start, int x_end,
                                  br_uint_32 z_start, br_int_32 dz,
                                  br_int_32 u_start, br_int_32 du,
                                  br_int_32 v_start, br_int_32 dv,
                                  char *colour_base, br_int_32 colour_stride,
                                  char *depth_base, br_int_32 depth_stride,
                                  char *tex_base, br_int_32 tex_stride,
                                  int tex_width, int tex_height)
{
	char *cline = colour_base + y * colour_stride;
	unsigned short *zline = (unsigned short *)(depth_base + y * depth_stride);
	br_uint_32 z = z_start;
	br_int_32 u = u_start, v = v_start;
	int x;

	for (x = x_start; x < x_end; x++) {
		unsigned short zval = (unsigned short)(z >> 16);
		if (zline[x] >= zval) {
			int tu = (u >> 16) % tex_width;
			int tv = (v >> 16) % tex_height;
			char *texel;
			if (tu < 0) tu += tex_width;
			if (tv < 0) tv += tex_height;
			texel = tex_base + tv * tex_stride + tu * 3;
			if (texel[0] | texel[1] | texel[2]) {
				zline[x] = zval;
				cline[x * 3 + 0] = texel[0];
				cline[x * 3 + 1] = texel[1];
				cline[x * 3 + 2] = texel[2];
			}
		}
		z += dz; u += du; v += dv;
	}
}

/*
 * Inner scanline for textured + Gouraud-lit RGB_888 + Z16
 * out = (texel * vertex_color) >> 8
 */
static void scanline_textured_smooth_zb(int y, int x_start, int x_end,
                                         br_uint_32 z_start, br_int_32 dz,
                                         br_int_32 u_start, br_int_32 du,
                                         br_int_32 v_start, br_int_32 dv,
                                         br_int_32 r_start, br_int_32 dr,
                                         br_int_32 g_start, br_int_32 dg,
                                         br_int_32 b_start, br_int_32 db,
                                         char *colour_base, br_int_32 colour_stride,
                                         char *depth_base, br_int_32 depth_stride,
                                         char *tex_base, br_int_32 tex_stride,
                                         int tex_width, int tex_height)
{
	char *cline = colour_base + y * colour_stride;
	unsigned short *zline = (unsigned short *)(depth_base + y * depth_stride);
	br_uint_32 z = z_start;
	br_int_32 u = u_start, v = v_start;
	br_int_32 r = r_start, g = g_start, b = b_start;
	int x;

	for (x = x_start; x < x_end; x++) {
		unsigned short zval = (unsigned short)(z >> 16);
		if (zline[x] >= zval) {
			int tu = (u >> 16) % tex_width;
			int tv = (v >> 16) % tex_height;
			unsigned char *texel;
			if (tu < 0) tu += tex_width;
			if (tv < 0) tv += tex_height;
			texel = (unsigned char *)(tex_base + tv * tex_stride + tu * 3);
			if (texel[0] | texel[1] | texel[2]) {
				int vr = (r >> 16) & 0xFF, vg = (g >> 16) & 0xFF, vb = (b >> 16) & 0xFF;
				zline[x] = zval;
				cline[x * 3 + 0] = (unsigned char)((texel[0] * vb) >> 8);
				cline[x * 3 + 1] = (unsigned char)((texel[1] * vg) >> 8);
				cline[x * 3 + 2] = (unsigned char)((texel[2] * vr) >> 8);
			}
		}
		z += dz; u += du; v += dv;
		r += dr; g += dg; b += db;
	}
}

/*
 * Textured triangle: RGB_888 + Z16 (no per-vertex lighting)
 */
void BR_ASM_CALL TriangleRenderTextured_RGB_888_ZB(brp_block *block, brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
	brp_vertex *a, *b, *c;
	br_int_32 g_divisor;
	char *colour_base, *depth_base, *tex_base;
	br_int_32 colour_stride, depth_stride, tex_stride;
	int tex_width, tex_height, y, scanline, main_x_i, main_x_f, x_left, x_right;
	br_int_32 z_current, u_current, v_current;

	(void)block;
	if (triangle_setup(v0, v1, v2, &a, &b, &c, &g_divisor)) return;

	PARAM_SETUP_UNSIGNED(work.pz, comp_x[C_SZ]);
	PARAM_SETUP(work.pu, comp_x[C_U]);
	PARAM_SETUP(work.pv, comp_x[C_V]);

	colour_base = (char *)work.colour.base; depth_base = (char *)work.depth.base;
	colour_stride = work.colour.stride_b;   depth_stride = work.depth.stride_b;
	tex_base = (char *)work.texture.base;    tex_stride = work.texture.stride_b;
	tex_width = (int)work.texture.width_p;   tex_height = (int)work.texture.height;
	if (tex_width <= 0 || tex_height <= 0) return;

	main_x_i = a->comp_x[C_SX] >> 16; main_x_f = 0x80000000;
	z_current = work.pz.current; u_current = work.pu.current; v_current = work.pv.current;

#define TEX_STEP_MAIN(carry_label) \
	{ br_uint_32 nf = (br_uint_32)main_x_f + (br_uint_32)work.main.d_f; \
	  if (nf < (br_uint_32)main_x_f) { \
		main_x_i += work.main.d_i + 1; \
		z_current += work.pz.d_carry; u_current += work.pu.d_carry; v_current += work.pv.d_carry; \
	  } else { \
		main_x_i += work.main.d_i; \
		z_current += work.pz.d_nocarry; u_current += work.pu.d_nocarry; v_current += work.pv.d_nocarry; \
	  } main_x_f = nf; }

	/* Top half */
	{
		int minor_x_i = a->comp_x[C_SX] >> 16, minor_x_f = 0x80000000;
		for (scanline = 0; scanline < work.top.count; scanline++) {
			y = (a->comp_x[C_SY] >> 16) + scanline;
			if (y >= 0 && y < (int)work.colour.height) {
				x_left  = (g_divisor >= 0) ? main_x_i  : minor_x_i;
				x_right = (g_divisor >= 0) ? minor_x_i : main_x_i;
				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;
				if (x_left < x_right)
					scanline_textured_zb(y, x_left, x_right,
						z_current + work.pz.grad_x * (x_left - main_x_i), work.pz.grad_x,
						u_current + work.pu.grad_x * (x_left - main_x_i), work.pu.grad_x,
						v_current + work.pv.grad_x * (x_left - main_x_i), work.pv.grad_x,
						colour_base, colour_stride, depth_base, depth_stride,
						tex_base, tex_stride, tex_width, tex_height);
			}
			TEX_STEP_MAIN(top)
			{ br_uint_32 nf = (br_uint_32)minor_x_f + (br_uint_32)work.top.d_f;
			  minor_x_i += (nf < (br_uint_32)minor_x_f) ? work.top.d_i + 1 : work.top.d_i;
			  minor_x_f = nf; }
		}
	}
	/* Bottom half */
	{
		int minor_x_i = b->comp_x[C_SX] >> 16, minor_x_f = 0x80000000;
		for (scanline = 0; scanline < work.bot.count; scanline++) {
			y = (b->comp_x[C_SY] >> 16) + scanline;
			if (y >= 0 && y < (int)work.colour.height) {
				x_left  = (g_divisor >= 0) ? main_x_i  : minor_x_i;
				x_right = (g_divisor >= 0) ? minor_x_i : main_x_i;
				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;
				if (x_left < x_right)
					scanline_textured_zb(y, x_left, x_right,
						z_current + work.pz.grad_x * (x_left - main_x_i), work.pz.grad_x,
						u_current + work.pu.grad_x * (x_left - main_x_i), work.pu.grad_x,
						v_current + work.pv.grad_x * (x_left - main_x_i), work.pv.grad_x,
						colour_base, colour_stride, depth_base, depth_stride,
						tex_base, tex_stride, tex_width, tex_height);
			}
			TEX_STEP_MAIN(bot)
			{ br_uint_32 nf = (br_uint_32)minor_x_f + (br_uint_32)work.bot.d_f;
			  minor_x_i += (nf < (br_uint_32)minor_x_f) ? work.bot.d_i + 1 : work.bot.d_i;
			  minor_x_f = nf; }
		}
	}
#undef TEX_STEP_MAIN
}

/*
 * Textured + Gouraud-lit triangle: RGB_888 + Z16
 */
void BR_ASM_CALL TriangleRenderTexturedSmooth_RGB_888_ZB(brp_block *block, brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
	brp_vertex *a, *b, *c;
	br_int_32 g_divisor;
	char *colour_base, *depth_base, *tex_base;
	br_int_32 colour_stride, depth_stride, tex_stride;
	int tex_width, tex_height, y, scanline, main_x_i, main_x_f, x_left, x_right;
	br_int_32 z_current, u_current, v_current, r_current, g_current, b_current;

	(void)block;
	if (triangle_setup(v0, v1, v2, &a, &b, &c, &g_divisor)) return;

	PARAM_SETUP_UNSIGNED(work.pz, comp_x[C_SZ]);
	PARAM_SETUP(work.pu, comp_x[C_U]);
	PARAM_SETUP(work.pv, comp_x[C_V]);
	PARAM_SETUP(work.pr, comp_x[C_R]);
	PARAM_SETUP(work.pg, comp_x[C_G]);
	PARAM_SETUP(work.pb, comp_x[C_B]);

	colour_base = (char *)work.colour.base; depth_base = (char *)work.depth.base;
	colour_stride = work.colour.stride_b;   depth_stride = work.depth.stride_b;
	tex_base = (char *)work.texture.base;    tex_stride = work.texture.stride_b;
	tex_width = (int)work.texture.width_p;   tex_height = (int)work.texture.height;
	if (tex_width <= 0 || tex_height <= 0) return;

	main_x_i = a->comp_x[C_SX] >> 16; main_x_f = 0x80000000;
	z_current = work.pz.current; u_current = work.pu.current; v_current = work.pv.current;
	r_current = work.pr.current; g_current = work.pg.current; b_current = work.pb.current;

#define TEXLIT_STEP_MAIN \
	{ br_uint_32 nf = (br_uint_32)main_x_f + (br_uint_32)work.main.d_f; \
	  if (nf < (br_uint_32)main_x_f) { \
		main_x_i += work.main.d_i + 1; \
		z_current += work.pz.d_carry; u_current += work.pu.d_carry; v_current += work.pv.d_carry; \
		r_current += work.pr.d_carry; g_current += work.pg.d_carry; b_current += work.pb.d_carry; \
	  } else { \
		main_x_i += work.main.d_i; \
		z_current += work.pz.d_nocarry; u_current += work.pu.d_nocarry; v_current += work.pv.d_nocarry; \
		r_current += work.pr.d_nocarry; g_current += work.pg.d_nocarry; b_current += work.pb.d_nocarry; \
	  } main_x_f = nf; }

	/* Top half */
	{
		int minor_x_i = a->comp_x[C_SX] >> 16, minor_x_f = 0x80000000;
		for (scanline = 0; scanline < work.top.count; scanline++) {
			y = (a->comp_x[C_SY] >> 16) + scanline;
			if (y >= 0 && y < (int)work.colour.height) {
				x_left  = (g_divisor >= 0) ? main_x_i  : minor_x_i;
				x_right = (g_divisor >= 0) ? minor_x_i : main_x_i;
				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;
				if (x_left < x_right)
					scanline_textured_smooth_zb(y, x_left, x_right,
						z_current + work.pz.grad_x * (x_left - main_x_i), work.pz.grad_x,
						u_current + work.pu.grad_x * (x_left - main_x_i), work.pu.grad_x,
						v_current + work.pv.grad_x * (x_left - main_x_i), work.pv.grad_x,
						r_current + work.pr.grad_x * (x_left - main_x_i), work.pr.grad_x,
						g_current + work.pg.grad_x * (x_left - main_x_i), work.pg.grad_x,
						b_current + work.pb.grad_x * (x_left - main_x_i), work.pb.grad_x,
						colour_base, colour_stride, depth_base, depth_stride,
						tex_base, tex_stride, tex_width, tex_height);
			}
			TEXLIT_STEP_MAIN
			{ br_uint_32 nf = (br_uint_32)minor_x_f + (br_uint_32)work.top.d_f;
			  minor_x_i += (nf < (br_uint_32)minor_x_f) ? work.top.d_i + 1 : work.top.d_i;
			  minor_x_f = nf; }
		}
	}
	/* Bottom half */
	{
		int minor_x_i = b->comp_x[C_SX] >> 16, minor_x_f = 0x80000000;
		for (scanline = 0; scanline < work.bot.count; scanline++) {
			y = (b->comp_x[C_SY] >> 16) + scanline;
			if (y >= 0 && y < (int)work.colour.height) {
				x_left  = (g_divisor >= 0) ? main_x_i  : minor_x_i;
				x_right = (g_divisor >= 0) ? minor_x_i : main_x_i;
				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;
				if (x_left < x_right)
					scanline_textured_smooth_zb(y, x_left, x_right,
						z_current + work.pz.grad_x * (x_left - main_x_i), work.pz.grad_x,
						u_current + work.pu.grad_x * (x_left - main_x_i), work.pu.grad_x,
						v_current + work.pv.grad_x * (x_left - main_x_i), work.pv.grad_x,
						r_current + work.pr.grad_x * (x_left - main_x_i), work.pr.grad_x,
						g_current + work.pg.grad_x * (x_left - main_x_i), work.pg.grad_x,
						b_current + work.pb.grad_x * (x_left - main_x_i), work.pb.grad_x,
						colour_base, colour_stride, depth_base, depth_stride,
						tex_base, tex_stride, tex_width, tex_height);
			}
			TEXLIT_STEP_MAIN
			{ br_uint_32 nf = (br_uint_32)minor_x_f + (br_uint_32)work.bot.d_f;
			  minor_x_i += (nf < (br_uint_32)minor_x_f) ? work.bot.d_i + 1 : work.bot.d_i;
			  minor_x_f = nf; }
		}
	}
#undef TEXLIT_STEP_MAIN
}

/* ===================================================================
 * Perspective-correct textured triangle renderers
 *
 * Uses span-based correction: every PERSP_SPAN pixels, compute true
 * u,v via float division, then affinely interpolate within the span.
 * Reads comp_f[C_W] (clip-space W, left as float by the convert step).
 * =================================================================== */

#define PERSP_SPAN 16

/*
 * Perspective-correct scanline: textured, no lighting, RGB_888 + Z16
 */
static void scanline_persp_textured_zb(int y, int x_start, int x_end,
                                        br_int_32 z_start, br_int_32 dz,
                                        float uw0, float vw0, float qw0,
                                        float uw_dx, float vw_dx, float qw_dx,
                                        char *colour_base, br_int_32 colour_stride,
                                        char *depth_base, br_int_32 depth_stride,
                                        char *tex_base, br_int_32 tex_stride,
                                        int tex_width, int tex_height)
{
	unsigned char *cline = (unsigned char *)(colour_base + y * colour_stride);
	unsigned short *zline = (unsigned short *)(depth_base + y * depth_stride);
	br_int_32 z = z_start;
	int x = x_start;

	while (x < x_end) {
		int span_end = x + PERSP_SPAN;
		int span_len, sx;
		float inv_q0, u0, v0, inv_q1, u1, v1, du, dv, fu, fv;

		if (span_end > x_end) span_end = x_end;
		span_len = span_end - x;

		inv_q0 = 1.0f / qw0;
		u0 = uw0 * inv_q0;
		v0 = vw0 * inv_q0;

		{
			float uw1 = uw0 + uw_dx * span_len;
			float vw1 = vw0 + vw_dx * span_len;
			float qw1 = qw0 + qw_dx * span_len;
			inv_q1 = 1.0f / qw1;
			u1 = uw1 * inv_q1;
			v1 = vw1 * inv_q1;
		}

		du = (u1 - u0) / (float)span_len;
		dv = (v1 - v0) / (float)span_len;
		fu = u0;
		fv = v0;

		for (sx = x; sx < span_end; sx++) {
			unsigned short zval = (unsigned short)(z >> 16);
			if (zline[sx] >= zval) {
				int tu = (int)fu % tex_width;
				int tv = (int)fv % tex_height;
				unsigned char *texel;
				if (tu < 0) tu += tex_width;
				if (tv < 0) tv += tex_height;
				texel = (unsigned char *)(tex_base + tv * tex_stride + tu * 3);
				if (texel[0] | texel[1] | texel[2]) {
					zline[sx] = zval;
					cline[sx * 3 + 0] = texel[0];
					cline[sx * 3 + 1] = texel[1];
					cline[sx * 3 + 2] = texel[2];
				}
			}
			z += dz;
			fu += du;
			fv += dv;
		}

		uw0 += uw_dx * span_len;
		vw0 += vw_dx * span_len;
		qw0 += qw_dx * span_len;
		x = span_end;
	}
}

/*
 * Perspective-correct scanline: textured + Gouraud, RGB_888 + Z16
 */
static void scanline_persp_textured_smooth_zb(int y, int x_start, int x_end,
                                               br_int_32 z_start, br_int_32 dz,
                                               float uw0, float vw0, float qw0,
                                               float uw_dx, float vw_dx, float qw_dx,
                                               br_int_32 r_start, br_int_32 dr,
                                               br_int_32 g_start, br_int_32 dg,
                                               br_int_32 b_start, br_int_32 db,
                                               char *colour_base, br_int_32 colour_stride,
                                               char *depth_base, br_int_32 depth_stride,
                                               char *tex_base, br_int_32 tex_stride,
                                               int tex_width, int tex_height)
{
	unsigned char *cline = (unsigned char *)(colour_base + y * colour_stride);
	unsigned short *zline = (unsigned short *)(depth_base + y * depth_stride);
	br_int_32 z = z_start;
	br_int_32 r = r_start, g = g_start, b = b_start;
	int x = x_start;

	while (x < x_end) {
		int span_end = x + PERSP_SPAN;
		int span_len, sx;
		float inv_q0, u0, v0, inv_q1, u1, v1, du, dv, fu, fv;

		if (span_end > x_end) span_end = x_end;
		span_len = span_end - x;

		inv_q0 = 1.0f / qw0;
		u0 = uw0 * inv_q0;
		v0 = vw0 * inv_q0;

		{
			float uw1 = uw0 + uw_dx * span_len;
			float vw1 = vw0 + vw_dx * span_len;
			float qw1 = qw0 + qw_dx * span_len;
			inv_q1 = 1.0f / qw1;
			u1 = uw1 * inv_q1;
			v1 = vw1 * inv_q1;
		}

		du = (u1 - u0) / (float)span_len;
		dv = (v1 - v0) / (float)span_len;
		fu = u0;
		fv = v0;

		for (sx = x; sx < span_end; sx++) {
			unsigned short zval = (unsigned short)(z >> 16);
			if (zline[sx] >= zval) {
				int tu = (int)fu % tex_width;
				int tv = (int)fv % tex_height;
				unsigned char *texel;
				if (tu < 0) tu += tex_width;
				if (tv < 0) tv += tex_height;
				texel = (unsigned char *)(tex_base + tv * tex_stride + tu * 3);
				if (texel[0] | texel[1] | texel[2]) {
					int vr = (r >> 16) & 0xFF, vg = (g >> 16) & 0xFF, vb = (b >> 16) & 0xFF;
					zline[sx] = zval;
					cline[sx * 3 + 0] = (unsigned char)((texel[0] * vb) >> 8);
					cline[sx * 3 + 1] = (unsigned char)((texel[1] * vg) >> 8);
					cline[sx * 3 + 2] = (unsigned char)((texel[2] * vr) >> 8);
				}
			}
			z += dz;
			r += dr; g += dg; b += db;
			fu += du;
			fv += dv;
		}

		uw0 += uw_dx * span_len;
		vw0 += vw_dx * span_len;
		qw0 += qw_dx * span_len;
		x = span_end;
	}
}

/*
 * Common perspective UV+Q gradient setup.
 * Reads comp_f[C_W] (float, NOT converted to fixed) and comp_x[C_U/C_V]
 * (16.16 fixed, already converted). Computes screen-space gradients for
 * u/w, v/w, and 1/w in float.
 */
typedef struct {
	float uw_current, vw_current, qw_current;
	float uw_grad_x, vw_grad_x, qw_grad_x;
	float uw_d_nocarry, vw_d_nocarry, qw_d_nocarry;
	float uw_d_carry, vw_d_carry, qw_d_carry;
} persp_params_t;

static void persp_setup(persp_params_t *p,
                        const brp_vertex *a, const brp_vertex *b, const brp_vertex *c,
                        br_int_32 g_divisor)
{
	float a_w = a->comp_f[C_W], b_w = b->comp_f[C_W], c_w = c->comp_f[C_W];
	float a_q = 1.0f / a_w, b_q = 1.0f / b_w, c_q = 1.0f / c_w;
	float a_u = a->comp_x[C_U] / 65536.0f, b_u = b->comp_x[C_U] / 65536.0f, c_u = c->comp_x[C_U] / 65536.0f;
	float a_v = a->comp_x[C_V] / 65536.0f, b_v = b->comp_x[C_V] / 65536.0f, c_v = c->comp_x[C_V] / 65536.0f;
	float a_uw = a_u * a_q, b_uw = b_u * b_q, c_uw = c_u * c_q;
	float a_vw = a_v * a_q, b_vw = b_v * b_q, c_vw = c_v * c_q;
	float g_inv = 1.0f / (float)g_divisor;
	float d1, d2, uw_grad_y, vw_grad_y, qw_grad_y;
	int grad_i;

	d1 = b_uw - a_uw; d2 = c_uw - a_uw;
	p->uw_grad_x = (d1 * work.main.y - d2 * work.top.y) * g_inv;
	uw_grad_y    = (d2 * work.top.x - d1 * work.main.x) * g_inv;

	d1 = b_vw - a_vw; d2 = c_vw - a_vw;
	p->vw_grad_x = (d1 * work.main.y - d2 * work.top.y) * g_inv;
	vw_grad_y    = (d2 * work.top.x - d1 * work.main.x) * g_inv;

	d1 = b_q - a_q; d2 = c_q - a_q;
	p->qw_grad_x = (d1 * work.main.y - d2 * work.top.y) * g_inv;
	qw_grad_y    = (d2 * work.top.x - d1 * work.main.x) * g_inv;

	p->uw_current = a_uw + p->uw_grad_x * 0.5f;
	p->vw_current = a_vw + p->vw_grad_x * 0.5f;
	p->qw_current = a_q  + p->qw_grad_x * 0.5f;

	grad_i = BrFixedToInt(work.main.grad);
	p->uw_d_nocarry = grad_i * p->uw_grad_x + uw_grad_y;
	p->vw_d_nocarry = grad_i * p->vw_grad_x + vw_grad_y;
	p->qw_d_nocarry = grad_i * p->qw_grad_x + qw_grad_y;
	p->uw_d_carry = p->uw_d_nocarry + p->uw_grad_x;
	p->vw_d_carry = p->vw_d_nocarry + p->vw_grad_x;
	p->qw_d_carry = p->qw_d_nocarry + p->qw_grad_x;
}

/*
 * Perspective-correct textured triangle: RGB_888 + Z16 (no per-vertex lighting)
 */
void BR_ASM_CALL TriangleRenderPerspTextured_RGB_888_ZB(brp_block *block, brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
	brp_vertex *a, *b, *c;
	br_int_32 g_divisor;
	char *colour_base, *depth_base, *tex_base;
	br_int_32 colour_stride, depth_stride, tex_stride;
	int tex_width, tex_height, y, scanline, main_x_i, main_x_f, x_left, x_right;
	br_int_32 z_current;
	persp_params_t pp;

	(void)block;
	if (triangle_setup(v0, v1, v2, &a, &b, &c, &g_divisor)) return;

	PARAM_SETUP_UNSIGNED(work.pz, comp_x[C_SZ]);
	persp_setup(&pp, a, b, c, g_divisor);

	colour_base = (char *)work.colour.base; depth_base = (char *)work.depth.base;
	colour_stride = work.colour.stride_b;   depth_stride = work.depth.stride_b;
	tex_base = (char *)work.texture.base;    tex_stride = work.texture.stride_b;
	tex_width = (int)work.texture.width_p;   tex_height = (int)work.texture.height;
	if (tex_width <= 0 || tex_height <= 0) return;

	main_x_i = a->comp_x[C_SX] >> 16; main_x_f = 0x80000000;
	z_current = work.pz.current;

#define PERSP_STEP_MAIN() \
	{ br_uint_32 nf = (br_uint_32)main_x_f + (br_uint_32)work.main.d_f; \
	  if (nf < (br_uint_32)main_x_f) { \
		main_x_i += work.main.d_i + 1; \
		z_current += work.pz.d_carry; \
		pp.uw_current += pp.uw_d_carry; pp.vw_current += pp.vw_d_carry; pp.qw_current += pp.qw_d_carry; \
	  } else { \
		main_x_i += work.main.d_i; \
		z_current += work.pz.d_nocarry; \
		pp.uw_current += pp.uw_d_nocarry; pp.vw_current += pp.vw_d_nocarry; pp.qw_current += pp.qw_d_nocarry; \
	  } main_x_f = nf; }

	/* Top half */
	{
		int minor_x_i = a->comp_x[C_SX] >> 16, minor_x_f = 0x80000000;
		for (scanline = 0; scanline < work.top.count; scanline++) {
			y = (a->comp_x[C_SY] >> 16) + scanline;
			if (y >= 0 && y < (int)work.colour.height) {
				int dx;
				x_left  = (g_divisor >= 0) ? main_x_i  : minor_x_i;
				x_right = (g_divisor >= 0) ? minor_x_i : main_x_i;
				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;
				dx = x_left - main_x_i;
				if (x_left < x_right)
					scanline_persp_textured_zb(y, x_left, x_right,
						z_current + work.pz.grad_x * dx, work.pz.grad_x,
						pp.uw_current + pp.uw_grad_x * dx,
						pp.vw_current + pp.vw_grad_x * dx,
						pp.qw_current + pp.qw_grad_x * dx,
						pp.uw_grad_x, pp.vw_grad_x, pp.qw_grad_x,
						colour_base, colour_stride, depth_base, depth_stride,
						tex_base, tex_stride, tex_width, tex_height);
			}
			PERSP_STEP_MAIN()
			{ br_uint_32 nf = (br_uint_32)minor_x_f + (br_uint_32)work.top.d_f;
			  minor_x_i += (nf < (br_uint_32)minor_x_f) ? work.top.d_i + 1 : work.top.d_i;
			  minor_x_f = nf; }
		}
	}
	/* Bottom half */
	{
		int minor_x_i = b->comp_x[C_SX] >> 16, minor_x_f = 0x80000000;
		for (scanline = 0; scanline < work.bot.count; scanline++) {
			y = (b->comp_x[C_SY] >> 16) + scanline;
			if (y >= 0 && y < (int)work.colour.height) {
				int dx;
				x_left  = (g_divisor >= 0) ? main_x_i  : minor_x_i;
				x_right = (g_divisor >= 0) ? minor_x_i : main_x_i;
				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;
				dx = x_left - main_x_i;
				if (x_left < x_right)
					scanline_persp_textured_zb(y, x_left, x_right,
						z_current + work.pz.grad_x * dx, work.pz.grad_x,
						pp.uw_current + pp.uw_grad_x * dx,
						pp.vw_current + pp.vw_grad_x * dx,
						pp.qw_current + pp.qw_grad_x * dx,
						pp.uw_grad_x, pp.vw_grad_x, pp.qw_grad_x,
						colour_base, colour_stride, depth_base, depth_stride,
						tex_base, tex_stride, tex_width, tex_height);
			}
			PERSP_STEP_MAIN()
			{ br_uint_32 nf = (br_uint_32)minor_x_f + (br_uint_32)work.bot.d_f;
			  minor_x_i += (nf < (br_uint_32)minor_x_f) ? work.bot.d_i + 1 : work.bot.d_i;
			  minor_x_f = nf; }
		}
	}
#undef PERSP_STEP_MAIN
}

/*
 * Perspective-correct textured + Gouraud-lit triangle: RGB_888 + Z16
 */
void BR_ASM_CALL TriangleRenderPerspTexturedSmooth_RGB_888_ZB(brp_block *block, brp_vertex *v0, brp_vertex *v1, brp_vertex *v2)
{
	brp_vertex *a, *b, *c;
	br_int_32 g_divisor;
	char *colour_base, *depth_base, *tex_base;
	br_int_32 colour_stride, depth_stride, tex_stride;
	int tex_width, tex_height, y, scanline, main_x_i, main_x_f, x_left, x_right;
	br_int_32 z_current, r_current, g_current, b_current;
	persp_params_t pp;

	(void)block;
	if (triangle_setup(v0, v1, v2, &a, &b, &c, &g_divisor)) return;

	PARAM_SETUP_UNSIGNED(work.pz, comp_x[C_SZ]);
	PARAM_SETUP(work.pr, comp_x[C_R]);
	PARAM_SETUP(work.pg, comp_x[C_G]);
	PARAM_SETUP(work.pb, comp_x[C_B]);
	persp_setup(&pp, a, b, c, g_divisor);

	colour_base = (char *)work.colour.base; depth_base = (char *)work.depth.base;
	colour_stride = work.colour.stride_b;   depth_stride = work.depth.stride_b;
	tex_base = (char *)work.texture.base;    tex_stride = work.texture.stride_b;
	tex_width = (int)work.texture.width_p;   tex_height = (int)work.texture.height;
	if (tex_width <= 0 || tex_height <= 0) return;

	main_x_i = a->comp_x[C_SX] >> 16; main_x_f = 0x80000000;
	z_current = work.pz.current;
	r_current = work.pr.current; g_current = work.pg.current; b_current = work.pb.current;

#define PERSP_LIT_STEP_MAIN() \
	{ br_uint_32 nf = (br_uint_32)main_x_f + (br_uint_32)work.main.d_f; \
	  if (nf < (br_uint_32)main_x_f) { \
		main_x_i += work.main.d_i + 1; \
		z_current += work.pz.d_carry; \
		r_current += work.pr.d_carry; g_current += work.pg.d_carry; b_current += work.pb.d_carry; \
		pp.uw_current += pp.uw_d_carry; pp.vw_current += pp.vw_d_carry; pp.qw_current += pp.qw_d_carry; \
	  } else { \
		main_x_i += work.main.d_i; \
		z_current += work.pz.d_nocarry; \
		r_current += work.pr.d_nocarry; g_current += work.pg.d_nocarry; b_current += work.pb.d_nocarry; \
		pp.uw_current += pp.uw_d_nocarry; pp.vw_current += pp.vw_d_nocarry; pp.qw_current += pp.qw_d_nocarry; \
	  } main_x_f = nf; }

	/* Top half */
	{
		int minor_x_i = a->comp_x[C_SX] >> 16, minor_x_f = 0x80000000;
		for (scanline = 0; scanline < work.top.count; scanline++) {
			y = (a->comp_x[C_SY] >> 16) + scanline;
			if (y >= 0 && y < (int)work.colour.height) {
				int dx;
				x_left  = (g_divisor >= 0) ? main_x_i  : minor_x_i;
				x_right = (g_divisor >= 0) ? minor_x_i : main_x_i;
				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;
				dx = x_left - main_x_i;
				if (x_left < x_right)
					scanline_persp_textured_smooth_zb(y, x_left, x_right,
						z_current + work.pz.grad_x * dx, work.pz.grad_x,
						pp.uw_current + pp.uw_grad_x * dx,
						pp.vw_current + pp.vw_grad_x * dx,
						pp.qw_current + pp.qw_grad_x * dx,
						pp.uw_grad_x, pp.vw_grad_x, pp.qw_grad_x,
						r_current + work.pr.grad_x * dx, work.pr.grad_x,
						g_current + work.pg.grad_x * dx, work.pg.grad_x,
						b_current + work.pb.grad_x * dx, work.pb.grad_x,
						colour_base, colour_stride, depth_base, depth_stride,
						tex_base, tex_stride, tex_width, tex_height);
			}
			PERSP_LIT_STEP_MAIN()
			{ br_uint_32 nf = (br_uint_32)minor_x_f + (br_uint_32)work.top.d_f;
			  minor_x_i += (nf < (br_uint_32)minor_x_f) ? work.top.d_i + 1 : work.top.d_i;
			  minor_x_f = nf; }
		}
	}
	/* Bottom half */
	{
		int minor_x_i = b->comp_x[C_SX] >> 16, minor_x_f = 0x80000000;
		for (scanline = 0; scanline < work.bot.count; scanline++) {
			y = (b->comp_x[C_SY] >> 16) + scanline;
			if (y >= 0 && y < (int)work.colour.height) {
				int dx;
				x_left  = (g_divisor >= 0) ? main_x_i  : minor_x_i;
				x_right = (g_divisor >= 0) ? minor_x_i : main_x_i;
				if (x_left < 0) x_left = 0;
				if (x_right > (int)work.colour.width_p) x_right = (int)work.colour.width_p;
				dx = x_left - main_x_i;
				if (x_left < x_right)
					scanline_persp_textured_smooth_zb(y, x_left, x_right,
						z_current + work.pz.grad_x * dx, work.pz.grad_x,
						pp.uw_current + pp.uw_grad_x * dx,
						pp.vw_current + pp.vw_grad_x * dx,
						pp.qw_current + pp.qw_grad_x * dx,
						pp.uw_grad_x, pp.vw_grad_x, pp.qw_grad_x,
						r_current + work.pr.grad_x * dx, work.pr.grad_x,
						g_current + work.pg.grad_x * dx, work.pg.grad_x,
						b_current + work.pb.grad_x * dx, work.pb.grad_x,
						colour_base, colour_stride, depth_base, depth_stride,
						tex_base, tex_stride, tex_width, tex_height);
			}
			PERSP_LIT_STEP_MAIN()
			{ br_uint_32 nf = (br_uint_32)minor_x_f + (br_uint_32)work.bot.d_f;
			  minor_x_i += (nf < (br_uint_32)minor_x_f) ? work.bot.d_i + 1 : work.bot.d_i;
			  minor_x_f = nf; }
		}
	}
#undef PERSP_LIT_STEP_MAIN
}
