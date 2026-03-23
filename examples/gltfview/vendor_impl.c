/*
 * vendor_impl.c - cgltf single-header library implementation
 *
 * BRender's core/fmt/ has its own cgltf implementation with modified
 * struct layouts (BRender extensions) and stubbed-out allocators.
 * We use the unmodified vendor copy here with standard malloc/free.
 *
 * The vendor cgltf.h has different struct layouts than BRender's modified
 * copy, so the linker should prefer our definitions for our translation units.
 * If duplicate symbol issues arise, we may need to prefix these.
 */
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
