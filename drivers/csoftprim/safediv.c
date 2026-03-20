/*
 * Pure C replacements for safediv.asm and sar16.asm
 *
 * SafeFixedMac2Div: (a*b + c*d) / e with 64-bit intermediate
 * SafeFixedMac3Div: (a*b + c*d + e*f) / g with 64-bit intermediate
 * _sar16: arithmetic shift right by 16
 */
#include "brender.h"
#include <limits.h>
#include <stdint.h>

int BR_ASM_CALL SafeFixedMac2Div(int a, int b, int c, int d, int e)
{
	int64_t num, result;

	if (e == 0)
		return 0;

	num = (int64_t)a * b + (int64_t)c * d;
	result = num / e;

	if (result > INT32_MAX || result < INT32_MIN)
		return 0;

	return (int)result;
}

int BR_ASM_CALL SafeFixedMac3Div(int a, int b, int c, int d, int e, int f, int g)
{
	int64_t num, result;

	if (g == 0)
		return 0;

	num = (int64_t)a * b + (int64_t)c * d + (int64_t)e * f;
	result = num / g;

	if (result > INT32_MAX || result < INT32_MIN)
		return 0;

	return (int)result;
}

br_int_32 BR_ASM_CALL _sar16(br_int_32 a)
{
	return a >> 16;
}
