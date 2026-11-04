#include "hlfp_internal.h"

#include <limits.h>

#define HLFP_PI_FIXED ((hlfp_fixed)205887)
#define HLFP_TWO_PI_FIXED ((hlfp_fixed)411775)

static int hlfp__blend_add(hlfp_fixed a, hlfp_fixed b, hlfp_fixed *out) {
    if (out == 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) {
        *out = (b > 0) ? INT_MAX : INT_MIN;
        return HLFP_ERR_OVERFLOW;
    }
    *out = a + b;
    return HLFP_OK;
}

static int hlfp__blend_sub(hlfp_fixed a, hlfp_fixed b, hlfp_fixed *out) {
    if (out == 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }
    if ((b > 0 && a < INT_MIN + b) || (b < 0 && a > INT_MAX + b)) {
        *out = (b > 0) ? INT_MIN : INT_MAX;
        return HLFP_ERR_OVERFLOW;
    }
    *out = a - b;
    return HLFP_OK;
}

static int hlfp__blend_init_output(int num_bones, hlfp_mdl_pose_set *out, hlfp_error *err) {
    int i;
    if (out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null pose blend output");
    }
    if (num_bones < 0 || num_bones > HLFP_MDL_MAX_BONES) {
        return hlfp__set_error(err, HLFP_ERR_CAPACITY, 0, 0, "pose blend bone count exceeds fixed capacity");
    }
    hlfp__zero(out, sizeof(*out));
    out->num_bones = num_bones;
    for (i = 0; i < num_bones; ++i) {
        out->poses[i].bone = i;
    }
    return HLFP_OK;
}

static int hlfp__blend_lerp_scalar(hlfp_fixed a, hlfp_fixed b, hlfp_fixed t, hlfp_fixed *out) {
    hlfp_fixed delta;
    hlfp_fixed scaled;
    int rc;

    rc = hlfp__blend_sub(b, a, &delta);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp_fixed_mul(delta, t, &scaled);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp__blend_add(a, scaled, out);
}

static hlfp_fixed hlfp__blend_wrap_shortest_angle(hlfp_fixed delta) {
    while (delta > HLFP_PI_FIXED) {
        delta -= HLFP_TWO_PI_FIXED;
    }
    while (delta < -HLFP_PI_FIXED) {
        delta += HLFP_TWO_PI_FIXED;
    }
    return delta;
}

static int hlfp__blend_lerp_angle(hlfp_fixed a, hlfp_fixed b, hlfp_fixed t, hlfp_fixed *out) {
    hlfp_fixed delta;
    hlfp_fixed scaled;
    int rc;

    rc = hlfp__blend_sub(b, a, &delta);
    if (rc != HLFP_OK) {
        return rc;
    }
    delta = hlfp__blend_wrap_shortest_angle(delta);
    rc = hlfp_fixed_mul(delta, t, &scaled);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp__blend_add(a, scaled, out);
}

static int hlfp__blend_normalize_frame_position(const hlfp_mdl_seqdesc *seq,
                                                hlfp_fixed frame_position,
                                                int *frame0,
                                                int *frame1,
                                                hlfp_fixed *t,
                                                hlfp_error *err) {
    hlfp_fixed pos;
    hlfp_fixed range;
    hlfp_fixed max_pos;
    int looping;

    if (seq == 0 || frame0 == 0 || frame1 == 0 || t == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null temporal decode input");
    }
    if (seq->numframes <= 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "sequence has no frames");
    }
    if (seq->numframes > INT_MAX / HLFP_FIXED_ONE) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence frame count exceeds 16.16 range");
    }
    if (seq->numframes == 1) {
        *frame0 = 0;
        *frame1 = 0;
        *t = 0;
        return HLFP_OK;
    }

    looping = (seq->flags & HLFP_STUDIO_LOOPING) != 0;
    pos = frame_position;

    if (looping) {
        /* GoldSrc loops over numframes - 1; the final frame is the
           seam/duplicate key, not one extra tick of playback.
           Signed modulo avoids potentially thousands of loop iterations. */
        range = (hlfp_fixed)((seq->numframes - 1) * HLFP_FIXED_ONE);
        pos %= range;
        if (pos < 0) pos += range;
        *frame0 = (int)(pos >> HLFP_FIXED_FRAC_BITS);
        *t = pos & 0xFFFF;
        if (*t == 0) {
            *frame1 = *frame0;
        } else {
            *frame1 = *frame0 + 1;
            /* frame numframes-1 is the key just before wrapping to 0 */
        }
        return HLFP_OK;
    }

    if (pos <= 0) {
        *frame0 = 0;
        *frame1 = 0;
        *t = 0;
        return HLFP_OK;
    }

    max_pos = (hlfp_fixed)((seq->numframes - 1) * HLFP_FIXED_ONE);
    if (pos >= max_pos) {
        *frame0 = seq->numframes - 1;
        *frame1 = *frame0;
        *t = 0;
        return HLFP_OK;
    }

    *frame0 = (int)(pos >> HLFP_FIXED_FRAC_BITS);
    *t = pos & 0xFFFF;
    *frame1 = *frame0 + 1;
    if (*frame1 >= seq->numframes) {
        *frame1 = *frame0;
        *t = 0;
    }
    return HLFP_OK;
}

int hlfp_mdl_interpolate_pose_sets(const hlfp_mdl_pose_set *a,
                                   const hlfp_mdl_pose_set *b,
                                   hlfp_fixed t,
                                   hlfp_mdl_pose_set *out,
                                   hlfp_error *err) {
    int rc;
    int bone;

    if (a == 0 || b == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null pose interpolation input");
    }
    hlfp_error_clear(err);
    if (t < 0 || t > HLFP_FIXED_ONE) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "interpolation factor out of range");
    }
    if (a->num_bones != b->num_bones) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "pose interpolation requires matching bone counts");
    }
    rc = hlfp__blend_init_output(a->num_bones, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    if (t == 0) {
        *out = *a;
        return HLFP_OK;
    }
    if (t == HLFP_FIXED_ONE) {
        *out = *b;
        return HLFP_OK;
    }

    for (bone = 0; bone < a->num_bones; ++bone) {
        if (a->poses[bone].bone != bone || b->poses[bone].bone != bone) {
            return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "pose interpolation expects bone-index ordered pose sets");
        }
        rc = hlfp__blend_lerp_scalar(a->poses[bone].pos.x, b->poses[bone].pos.x, t, &out->poses[bone].pos.x);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose interpolation overflow");
        rc = hlfp__blend_lerp_scalar(a->poses[bone].pos.y, b->poses[bone].pos.y, t, &out->poses[bone].pos.y);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose interpolation overflow");
        rc = hlfp__blend_lerp_scalar(a->poses[bone].pos.z, b->poses[bone].pos.z, t, &out->poses[bone].pos.z);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose interpolation overflow");
        rc = hlfp__blend_lerp_angle(a->poses[bone].rot.x, b->poses[bone].rot.x, t, &out->poses[bone].rot.x);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose interpolation overflow");
        rc = hlfp__blend_lerp_angle(a->poses[bone].rot.y, b->poses[bone].rot.y, t, &out->poses[bone].rot.y);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose interpolation overflow");
        rc = hlfp__blend_lerp_angle(a->poses[bone].rot.z, b->poses[bone].rot.z, t, &out->poses[bone].rot.z);
        if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose interpolation overflow");
    }

    return HLFP_OK;
}

int hlfp_mdl_decode_sequence_frame_fixed(const hlfp_mdl *mdl, int sequence_index,
                                         int blend_index, hlfp_fixed frame_position,
                                         hlfp_mdl_pose_set *out, hlfp_error *err) {
    const hlfp_mdl_seqdesc *seq;
    hlfp_mdl_pose_set a;
    hlfp_mdl_pose_set b;
    int frame0 = 0;
    int frame1 = 0;
    hlfp_fixed t = 0;
    int rc;

    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null temporal sequence decode output");
    }
    hlfp_error_clear(err);
    hlfp__zero(out, sizeof(*out));
    if (sequence_index < 0 || sequence_index >= mdl->num_seqdesc) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence index out of range");
    }
    seq = &mdl->seqdesc[sequence_index];
    rc = hlfp__blend_normalize_frame_position(seq, frame_position, &frame0, &frame1, &t, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    rc = hlfp_mdl_decode_sequence_frame(mdl, sequence_index, blend_index, frame0, &a, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    if (t == 0 || frame0 == frame1) {
        *out = a;
        return HLFP_OK;
    }
    rc = hlfp_mdl_decode_sequence_frame(mdl, sequence_index, blend_index, frame1, &b, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp_mdl_interpolate_pose_sets(&a, &b, t, out, err);
}

int hlfp_mdl_decode_sequence_time(const hlfp_mdl *mdl, int sequence_index,
                                  int blend_index, hlfp_fixed seconds,
                                  hlfp_mdl_pose_set *out, hlfp_error *err) {
    hlfp_fixed frame_position;
    int rc;

    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null time sequence decode output");
    }
    if (sequence_index < 0 || sequence_index >= mdl->num_seqdesc) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence index out of range");
    }
    rc = hlfp_fixed_mul(seconds, mdl->seqdesc[sequence_index].fps, &frame_position);
    if (rc != HLFP_OK) {
        return hlfp__set_error(err, rc, 0, 0, "time to frame conversion overflow");
    }
    return hlfp_mdl_decode_sequence_frame_fixed(mdl, sequence_index, blend_index, frame_position, out, err);
}

int hlfp_mdl_blend_pose_sets(const hlfp_mdl_pose_set *pose_sets, const hlfp_fixed *weights,
                             int count, hlfp_mdl_pose_set *out, hlfp_error *err) {
    hlfp_fixed accum[HLFP_MDL_MAX_BONES][6];
    hlfp_fixed total_weight;
    int i;
    int j;
    int bone;
    int rc;
    hlfp_fixed t;
    int num_bones;

    if (pose_sets == 0 || weights == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null pose blend input");
    }
    hlfp_error_clear(err);
    if (count <= 0 || count > HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "pose blend count out of range");
    }

    num_bones = pose_sets[0].num_bones;
    rc = hlfp__blend_init_output(num_bones, out, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    hlfp__zero(accum, sizeof(accum));
    total_weight = 0;

    for (i = 0; i < count; ++i) {
        if (pose_sets[i].num_bones != num_bones) {
            return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "all pose sets must have the same bone count");
        }
        rc = hlfp__blend_add(total_weight, weights[i], &total_weight);
        if (rc != HLFP_OK) {
            return hlfp__set_error(err, rc, 0, 0, "pose blend weight overflow");
        }
        for (bone = 0; bone < num_bones; ++bone) {
            if (pose_sets[i].poses[bone].bone != bone) {
                return hlfp__set_error(err, HLFP_ERR_BAD_FORMAT, 0, 0, "pose set must be bone-index ordered for blending");
            }
            rc = hlfp_fixed_mul(pose_sets[i].poses[bone].pos.x, weights[i], &t);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose blend overflow");
            rc = hlfp__blend_add(accum[bone][0], t, &accum[bone][0]);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose blend overflow");
            rc = hlfp_fixed_mul(pose_sets[i].poses[bone].pos.y, weights[i], &t);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose blend overflow");
            rc = hlfp__blend_add(accum[bone][1], t, &accum[bone][1]);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose blend overflow");
            rc = hlfp_fixed_mul(pose_sets[i].poses[bone].pos.z, weights[i], &t);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose blend overflow");
            rc = hlfp__blend_add(accum[bone][2], t, &accum[bone][2]);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose blend overflow");
            rc = hlfp_fixed_mul(pose_sets[i].poses[bone].rot.x, weights[i], &t);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose blend overflow");
            rc = hlfp__blend_add(accum[bone][3], t, &accum[bone][3]);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose blend overflow");
            rc = hlfp_fixed_mul(pose_sets[i].poses[bone].rot.y, weights[i], &t);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose blend overflow");
            rc = hlfp__blend_add(accum[bone][4], t, &accum[bone][4]);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose blend overflow");
            rc = hlfp_fixed_mul(pose_sets[i].poses[bone].rot.z, weights[i], &t);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose blend overflow");
            rc = hlfp__blend_add(accum[bone][5], t, &accum[bone][5]);
            if (rc != HLFP_OK) return hlfp__set_error(err, rc, 0, 0, "pose blend overflow");
        }
    }

    if (total_weight == 0) {
        return hlfp__set_error(err, HLFP_ERR_DIV_BY_ZERO, 0, 0, "pose blend total weight is zero");
    }

    for (bone = 0; bone < num_bones; ++bone) {
        for (j = 0; j < 6; ++j) {
            rc = hlfp_fixed_div(accum[bone][j], total_weight, &t);
            if (rc != HLFP_OK) {
                return hlfp__set_error(err, rc, 0, 0, "pose blend normalization failed");
            }
            if (j == 0) out->poses[bone].pos.x = t;
            else if (j == 1) out->poses[bone].pos.y = t;
            else if (j == 2) out->poses[bone].pos.z = t;
            else if (j == 3) out->poses[bone].rot.x = t;
            else if (j == 4) out->poses[bone].rot.y = t;
            else out->poses[bone].rot.z = t;
        }
    }

    return HLFP_OK;
}

int hlfp_mdl_blend_sequence_frames(const hlfp_mdl *mdl,
                                   const int *sequence_indices,
                                   const int *blend_indices,
                                   const int *frames,
                                   const hlfp_fixed *weights,
                                   int count,
                                   hlfp_mdl_pose_set *out,
                                   hlfp_error *err) {
    hlfp_mdl_pose_set decoded[HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS];
    int i;
    int rc;

    if (mdl == 0 || sequence_indices == 0 || blend_indices == 0 || frames == 0 || weights == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null sequence blend input");
    }
    hlfp_error_clear(err);
    if (count <= 0 || count > HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence blend count out of range");
    }

    for (i = 0; i < count; ++i) {
        rc = hlfp_mdl_decode_sequence_frame(mdl, sequence_indices[i], blend_indices[i], frames[i], &decoded[i], err);
        if (rc != HLFP_OK) {
            return rc;
        }
    }
    return hlfp_mdl_blend_pose_sets(decoded, weights, count, out, err);
}

int hlfp_mdl_blend_sequence_frame_positions(const hlfp_mdl *mdl,
                                            const int *sequence_indices,
                                            const int *blend_indices,
                                            const hlfp_fixed *frame_positions,
                                            const hlfp_fixed *weights,
                                            int count,
                                            hlfp_mdl_pose_set *out,
                                            hlfp_error *err) {
    hlfp_mdl_pose_set decoded[HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS];
    int i;
    int rc;

    if (mdl == 0 || sequence_indices == 0 || blend_indices == 0 || frame_positions == 0 || weights == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null temporal sequence blend input");
    }
    hlfp_error_clear(err);
    if (count <= 0 || count > HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence blend count out of range");
    }

    for (i = 0; i < count; ++i) {
        rc = hlfp_mdl_decode_sequence_frame_fixed(mdl, sequence_indices[i], blend_indices[i], frame_positions[i], &decoded[i], err);
        if (rc != HLFP_OK) {
            return rc;
        }
    }
    return hlfp_mdl_blend_pose_sets(decoded, weights, count, out, err);
}

int hlfp_mdl_blend_sequence_times(const hlfp_mdl *mdl,
                                  const int *sequence_indices,
                                  const int *blend_indices,
                                  const hlfp_fixed *seconds,
                                  const hlfp_fixed *weights,
                                  int count,
                                  hlfp_mdl_pose_set *out,
                                  hlfp_error *err) {
    hlfp_mdl_pose_set decoded[HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS];
    int i;
    int rc;

    if (mdl == 0 || sequence_indices == 0 || blend_indices == 0 || seconds == 0 || weights == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null time sequence blend input");
    }
    hlfp_error_clear(err);
    if (count <= 0 || count > HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS) {
        return hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, "sequence blend count out of range");
    }

    for (i = 0; i < count; ++i) {
        rc = hlfp_mdl_decode_sequence_time(mdl, sequence_indices[i], blend_indices[i], seconds[i], &decoded[i], err);
        if (rc != HLFP_OK) {
            return rc;
        }
    }
    return hlfp_mdl_blend_pose_sets(decoded, weights, count, out, err);
}

int hlfp_mdl_blend_sequence_world_matrices(const hlfp_mdl *mdl,
                                           const int *sequence_indices,
                                           const int *blend_indices,
                                           const int *frames,
                                           const hlfp_fixed *weights,
                                           int count,
                                           hlfp_mat3x4 *out,
                                           int out_cap,
                                           hlfp_error *err) {
    hlfp_mdl_pose_set poses;
    int rc;

    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null world blend output");
    }
    rc = hlfp_mdl_blend_sequence_frames(mdl, sequence_indices, blend_indices, frames, weights, count, &poses, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp_mdl_pose_to_world_matrices(mdl, &poses, out, out_cap, err);
}

int hlfp_mdl_blend_sequence_world_matrices_frame_positions(const hlfp_mdl *mdl,
                                                           const int *sequence_indices,
                                                           const int *blend_indices,
                                                           const hlfp_fixed *frame_positions,
                                                           const hlfp_fixed *weights,
                                                           int count,
                                                           hlfp_mat3x4 *out,
                                                           int out_cap,
                                                           hlfp_error *err) {
    hlfp_mdl_pose_set poses;
    int rc;

    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null temporal world blend output");
    }
    rc = hlfp_mdl_blend_sequence_frame_positions(mdl, sequence_indices, blend_indices, frame_positions, weights, count, &poses, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp_mdl_pose_to_world_matrices(mdl, &poses, out, out_cap, err);
}

int hlfp_mdl_blend_sequence_world_matrices_times(const hlfp_mdl *mdl,
                                                 const int *sequence_indices,
                                                 const int *blend_indices,
                                                 const hlfp_fixed *seconds,
                                                 const hlfp_fixed *weights,
                                                 int count,
                                                 hlfp_mat3x4 *out,
                                                 int out_cap,
                                                 hlfp_error *err) {
    hlfp_mdl_pose_set poses;
    int rc;

    if (mdl == 0 || out == 0) {
        return hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null time world blend output");
    }
    rc = hlfp_mdl_blend_sequence_times(mdl, sequence_indices, blend_indices, seconds, weights, count, &poses, err);
    if (rc != HLFP_OK) {
        return rc;
    }
    return hlfp_mdl_pose_to_world_matrices(mdl, &poses, out, out_cap, err);
}
