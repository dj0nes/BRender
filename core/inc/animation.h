/*
 * Copyright (c) 1993-1995 by Argonaut Technologies Limited. All rights reserved.
 *
 * $Id: animation.h 1.1 2026/04/04 00:00:00 dj Exp $
 * $Locker: $
 *
 * Core animation types: format-agnostic keyframe animation for actors.
 */
#ifndef _ANIMATION_H_
#define _ANIMATION_H_

/*
 * Forward declaration — br_actor is fully defined in actor.h (via brv1db.h),
 * included later in brender.h. Only pointer use here, so forward decl suffices.
 */
#ifndef _ACTOR_H_
typedef struct br_actor br_actor;
#endif

/*
 * Path types for animation channels
 */
enum {
    BR_ANIM_PATH_TRANSLATION = 0,
    BR_ANIM_PATH_ROTATION    = 1,
    BR_ANIM_PATH_SCALE       = 2,
};

/*
 * Interpolation modes
 */
enum {
    BR_ANIM_INTERP_STEP   = 0,
    BR_ANIM_INTERP_LINEAR = 1,
};

/*
 * A single keyframe track: one TRS path on one target actor.
 * values are packed floats: 3 components for T/S, 4 (xyzw) for R.
 */
typedef struct br_animation_channel {
    br_int_32  target;        /* index into owning br_animation_set.actors[] */
    br_int_32  path;          /* BR_ANIM_PATH_* or -1 to skip */
    br_int_32  interpolation; /* BR_ANIM_INTERP_* */
    br_int_32  nkeys;
    float     *times;         /* [nkeys] */
    float     *values;        /* [nkeys * (path==ROTATION ? 4 : 3)] */
} br_animation_channel;

/*
 * A named animation clip: a set of channels sharing a timeline.
 */
typedef struct br_animation_clip {
    char                  identifier[64];
    float                 duration;
    br_boolean            loop;
    br_int_32             nchannels;
    br_animation_channel *channels;
} br_animation_clip;

/*
 * A scene animation set: actor bindings + rest poses + array of clips.
 * All sub-allocations are children of this resource; BrResFree() cleans up.
 */
typedef struct br_animation_set {
    br_actor          **actors;   /* weak refs [nactors] */
    br_int_32           nactors;
    float              *rest_trs; /* nactors * 10 floats: T(3) R_quat(4) S(3) */
    br_animation_clip  *clips;    /* [nclips] */
    br_int_32           nclips;
    br_int_32           active;   /* clip index to play, or -1 to layer all clips (last clip wins per channel) */
} br_animation_set;

#endif
