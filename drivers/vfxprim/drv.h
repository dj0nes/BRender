/*
 * Master include for VFX primitive library driver
 */
#ifndef _DRV_H_
#define _DRV_H_

#define BR_OBJECT_PRIVATE
#define BR_DEVICE_PRIVATE
#define BR_PRIMITIVE_LIBRARY_PRIVATE
#define BR_PRIMITIVE_STATE_PRIVATE
#define BR_BUFFER_STORED_PRIVATE

#ifndef _BRDDI_H_
#include "brddi.h"
#endif

#ifndef _PRIMINFO_H_
#include "priminfo.h"
#endif

#include "vfx_compat.h"
#include "vfx_port.h"

#ifndef _WORK_H_
#include "work.h"
#endif

#ifndef _OBJECT_H_
#include "object.h"
#endif

#ifndef _TEMPLATE_H_
#include "template.h"
#endif

#ifndef _DEVICE_H_
#include "device.h"
#endif

#ifndef _PLIB_H_
#include "plib.h"
#endif

#ifndef _PSTATE_H_
#include "pstate.h"
#endif

#ifndef _SBUFFER_H_
#include "sbuffer.h"
#endif

#ifndef _MATCH_H_
#include "match.h"
#endif

#ifndef _TIMESTMP_H_
#include "timestmp.h"
#endif

#define BRT(t)  BRT_##t,0
#define DEV(t)  0,#t

#ifndef _NO_PROTOTYPES
#ifndef _DRV_IP_H_
#include "drv_ip.h"
#endif
#endif

#ifdef __cplusplus
};
#endif
#endif
