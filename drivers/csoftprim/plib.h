/*
 * Copyright (c) 1993-1995 Argonaut Technologies Limited. All rights reserved.
 *
 * Private primitive library structure
 */
#ifndef _PRIMLIB_H_
#define _PRIMLIB_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct br_primitive_library {
	/*
	 * Dispatch table
	 */
	const struct br_primitive_library_dispatch *dispatch;

	/*
	 * Standard object identifier
	 */
	const char *identifier;

    /*
     * Pointer to owning device
     */
   struct br_device *device;

   /*
    * Pointer to the colour buffer being rendered into.
    */

   br_device_pixelmap *colour_buffer ;

	/*
	 * List of objects associated with this renderer
	 */
	void *object_list;

	/*
	 * Flag to indicate what sort of processor we have
	 */
	br_token processor_type;

	/*
	 * Flag indicating whether MMX renderers should be used
	 * (always BR_FALSE for csoftprim)
	 */
	br_boolean use_mmx;

} br_primitive_library;

#ifdef __cplusplus
};
#endif
#endif
