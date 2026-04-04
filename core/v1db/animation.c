/*
 * animation.c - Core keyframe animation evaluation for BRender
 *
 * Implements BrAnimationSetUpdate(): evaluates a br_animation_set at a
 * given time, interpolating keyframe channels and composing TRS into
 * br_matrix34 transforms on the target actors.
 *
 * Format-specific loading (glTF, etc.) is handled by core/fmt; this file
 * contains only the format-agnostic runtime.
 */
#include <math.h>
#include <brender.h>

/* ------------------------------------------------------------------ */
/* Math helpers                                                        */
/* ------------------------------------------------------------------ */

/*
 * Compose T(3) + R(4 quat xyzw) + S(3) into a BRender 3x4 matrix.
 * glTF spec 3.5.3: M = T * R * S
 */
static void anim_compose_trs(const float *t, const float *q, const float *s, br_matrix34 *out)
{
    float qx = q[0], qy = q[1], qz = q[2], qw = q[3];
    float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    float xy = qx * qy, xz = qx * qz, yz = qy * qz;
    float wx = qw * qx, wy = qw * qy, wz = qw * qz;

    out->m[0][0] = BR_SCALAR((1.0f - 2.0f * (yy + zz)) * s[0]);
    out->m[0][1] = BR_SCALAR((2.0f * (xy + wz)) * s[0]);
    out->m[0][2] = BR_SCALAR((2.0f * (xz - wy)) * s[0]);

    out->m[1][0] = BR_SCALAR((2.0f * (xy - wz)) * s[1]);
    out->m[1][1] = BR_SCALAR((1.0f - 2.0f * (xx + zz)) * s[1]);
    out->m[1][2] = BR_SCALAR((2.0f * (yz + wx)) * s[1]);

    out->m[2][0] = BR_SCALAR((2.0f * (xz + wy)) * s[2]);
    out->m[2][1] = BR_SCALAR((2.0f * (yz - wx)) * s[2]);
    out->m[2][2] = BR_SCALAR((1.0f - 2.0f * (xx + yy)) * s[2]);

    out->m[3][0] = BR_SCALAR(t[0]);
    out->m[3][1] = BR_SCALAR(t[1]);
    out->m[3][2] = BR_SCALAR(t[2]);
}

/*
 * Quaternion spherical linear interpolation (shortest path).
 *
 * Not using BrQuatSlerp(): it takes a br_int_16 spins parameter and operates
 * on br_quat structs. For keyframe animation we always want shortest-path
 * 0-spin SLERP on raw float[4] data — the impedance mismatch isn't worth it.
 */
static void anim_quat_slerp(float *out, const float *a, const float *b, float t)
{
    float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    float b2[4];
    int   i;

    if(dot < 0.0f) {
        dot = -dot;
        for(i = 0; i < 4; i++)
            b2[i] = -b[i];
    } else {
        for(i = 0; i < 4; i++)
            b2[i] = b[i];
    }

    if(dot > 0.9995f) {
        float len;
        for(i = 0; i < 4; i++)
            out[i] = a[i] + t * (b2[i] - a[i]);
        len = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2] + out[3] * out[3]);
        if(len > 1e-6f)
            for(i = 0; i < 4; i++)
                out[i] /= len;
    } else {
        float theta     = acosf(dot);
        float sin_theta = sinf(theta);
        float wa        = sinf((1.0f - t) * theta) / sin_theta;
        float wb        = sinf(t * theta) / sin_theta;
        for(i = 0; i < 4; i++)
            out[i] = wa * a[i] + wb * b2[i];
    }
}

/*
 * Evaluate a single animation channel at the given time.
 */
static void anim_eval_channel(const br_animation_channel *ch, float time, float *out)
{
    int components = (ch->path == BR_ANIM_PATH_ROTATION) ? 4 : 3;
    int lo, hi, mid;

    if(ch->nkeys == 0)
        return;

    if(ch->nkeys == 1 || time <= ch->times[0]) {
        BrMemCpy(out, ch->values, components * sizeof(float));
        return;
    }

    if(time >= ch->times[ch->nkeys - 1]) {
        BrMemCpy(out, &ch->values[(ch->nkeys - 1) * components], components * sizeof(float));
        return;
    }

    lo = 0;
    hi = ch->nkeys - 1;
    while(lo + 1 < hi) {
        mid = (lo + hi) / 2;
        if(ch->times[mid] <= time)
            lo = mid;
        else
            hi = mid;
    }

    if(ch->interpolation == BR_ANIM_INTERP_STEP) {
        BrMemCpy(out, &ch->values[lo * components], components * sizeof(float));
    } else {
        float t_range  = ch->times[hi] - ch->times[lo];
        float t_factor = (t_range > 0.0f) ? (time - ch->times[lo]) / t_range : 0.0f;

        if(ch->path == BR_ANIM_PATH_ROTATION) {
            anim_quat_slerp(out, &ch->values[lo * 4], &ch->values[hi * 4], t_factor);
        } else {
            float *va = &ch->values[lo * 3];
            float *vb = &ch->values[hi * 3];
            int    i;
            for(i = 0; i < 3; i++)
                out[i] = va[i] + t_factor * (vb[i] - va[i]);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void BR_PUBLIC_ENTRY BrAnimationSetUpdate(br_animation_set *set, float time)
{
    float    *trs;
    br_int_32 ai, ci, ni;

    if(set == NULL || set->nclips == 0)
        return;

    /*
     * Scratch-allocate a working TRS buffer (nactors * 10 floats) and a
     * dirty flag array (nactors ints).  No fixed MAX_NODES limit.
     */
    trs = BrScratchAllocate(set->nactors * 10 * sizeof(float) + set->nactors * sizeof(br_int_32));
    {
        br_int_32 *dirty = (br_int_32 *)(trs + set->nactors * 10);

        BrMemCpy(trs, set->rest_trs, set->nactors * 10 * sizeof(float));
        BrMemSet(dirty, 0, set->nactors * sizeof(br_int_32));

        for(ai = 0; ai < set->nclips; ++ai) {
            br_animation_clip *clip = &set->clips[ai];
            float              clip_time;

            if(set->active >= 0 && ai != set->active)
                continue;

            if(clip->nchannels == 0 || clip->duration <= 0.0f)
                continue;

            if(clip->loop)
                clip_time = fmodf(time, clip->duration);
            else
                clip_time = (time > clip->duration) ? clip->duration : time;

            for(ci = 0; ci < clip->nchannels; ++ci) {
                br_animation_channel *ch = &clip->channels[ci];
                float                 value[4];
                float                *actor_trs;

                if(ch->path < 0 || ch->target < 0 || ch->target >= set->nactors)
                    continue;

                actor_trs         = &trs[ch->target * 10];
                dirty[ch->target] = 1;

                anim_eval_channel(ch, clip_time, value);

                switch(ch->path) {
                    case BR_ANIM_PATH_TRANSLATION:
                        actor_trs[0] = value[0];
                        actor_trs[1] = value[1];
                        actor_trs[2] = value[2];
                        break;
                    case BR_ANIM_PATH_ROTATION:
                        actor_trs[3] = value[0];
                        actor_trs[4] = value[1];
                        actor_trs[5] = value[2];
                        actor_trs[6] = value[3];
                        break;
                    case BR_ANIM_PATH_SCALE:
                        actor_trs[7] = value[0];
                        actor_trs[8] = value[1];
                        actor_trs[9] = value[2];
                        break;
                    default:
                        break;
                }
            }
        }

        for(ni = 0; ni < set->nactors; ++ni) {
            float *n;
            if(!dirty[ni] || set->actors[ni] == NULL)
                continue;
            n                          = &trs[ni * 10];
            /* Force MATRIX34: keyframe animation needs full TRS, so lighter
             * types (QUAT, EULER, IDENTITY, TRANSLATION) are not appropriate. */
            set->actors[ni]->t.type    = BR_TRANSFORM_MATRIX34;
            anim_compose_trs(n, n + 3, n + 7, &set->actors[ni]->t.t.mat);
        }
    }

    BrScratchFree(trs);
}
