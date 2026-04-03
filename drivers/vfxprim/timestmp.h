/*
 * Driver-wide timestamp support
 */
#ifndef _TIMESTMP_H_
#define _TIMESTMP_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef br_uint_32 br_timestamp;

extern br_timestamp VFXPrimDriverTimestamp;

#define Timestamp() (VFXPrimDriverTimestamp += 2)

#define TIMESTAMP_START 1
#define INVALID_TIME    0

#ifdef __cplusplus
};
#endif
#endif
