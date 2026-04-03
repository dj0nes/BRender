/*
 * vfx_compat.h - Portable type definitions for VFX polygon engine
 *
 * Include this BEFORE vfx_port.h when building outside Watcom/Win16.
 * Defines VFX_TYPES_DEFINED to suppress vfx_port.h's own typedefs,
 * then provides LP64-safe equivalents using <stdint.h>.
 */
#ifndef VFX_COMPAT_H
#define VFX_COMPAT_H

#include <stdint.h>

/*
 * Tell vfx_port.h to skip its type block
 */
#define VFX_TYPES_DEFINED

typedef uint8_t   UBYTE;
typedef uint16_t  UWORD;
typedef uint32_t  ULONG;
typedef int8_t    BYTE;
typedef int16_t   WORD;
typedef int32_t   LONG;
typedef int32_t   FIXED16;
typedef int32_t   FIXED30;

#endif /* VFX_COMPAT_H */
