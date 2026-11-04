#include "anima_biovisionbvh_internal.h"

static anima_bvh_q15 anima_bvh_q15_mul(anima_bvh_q15 a, anima_bvh_q15 b)
{
    long p;

    p = a * b;
    if (ANIMA_BVH_Q15_ONE != 0L) {
        p = p / ANIMA_BVH_Q15_ONE;
    }
    if (p > ANIMA_BVH_Q15_ONE) {
        p = ANIMA_BVH_Q15_ONE;
    }
    if (p < -ANIMA_BVH_Q15_ONE) {
        p = -ANIMA_BVH_Q15_ONE;
    }
    return p;
}

static anima_bvh_q15 anima_bvh_q15_clamp(long v)
{
    if (v > ANIMA_BVH_Q15_ONE) {
        return ANIMA_BVH_Q15_ONE;
    }
    if (v < -ANIMA_BVH_Q15_ONE) {
        return -ANIMA_BVH_Q15_ONE;
    }
    return v;
}

static anima_bvh_fixed anima_bvh_fixed_sub_sat_local(anima_bvh_fixed a, anima_bvh_fixed b)
{
    if (b == ANIMA_BVH_FIXED_MIN) {
        return anima_bvh_fixed_add_sat(a, ANIMA_BVH_FIXED_MAX);
    }
    return anima_bvh_fixed_add_sat(a, -b);
}

static anima_bvh_fixed anima_bvh_fixed_mul_q16_sat_local(anima_bvh_fixed value, anima_bvh_fixed q16)
{
    long v;
    long q;
    long whole;
    long rem;
    long result;
    long part;
    int neg;

    if (value == 0L || q16 == 0L) {
        return 0L;
    }

    neg = 0;
    v = value;
    q = q16;
    if (v < 0L) {
        neg = !neg;
        if (v == ANIMA_BVH_FIXED_MIN) {
            v = ANIMA_BVH_FIXED_MAX;
        } else {
            v = -v;
        }
    }
    if (q < 0L) {
        neg = !neg;
        if (q == ANIMA_BVH_FIXED_MIN) {
            q = ANIMA_BVH_FIXED_MAX;
        } else {
            q = -q;
        }
    }

    whole = v / ANIMA_BVH_FIXED_ONE;
    rem = v & 65535L;

    if (q != 0L && whole > ANIMA_BVH_FIXED_MAX / q) {
        result = ANIMA_BVH_FIXED_MAX;
    } else {
        result = whole * q;
        part = (rem * q) >> ANIMA_BVH_FIXED_SHIFT;
        if (part > ANIMA_BVH_FIXED_MAX - result) {
            result = ANIMA_BVH_FIXED_MAX;
        } else {
            result = result + part;
        }
    }

    if (neg) {
        if (result == ANIMA_BVH_FIXED_MAX) {
            return ANIMA_BVH_FIXED_MIN + 1L;
        }
        result = -result;
    }
    return result;
}

static anima_bvh_fixed anima_bvh_fixed_lerp_local(anima_bvh_fixed a,
                                                  anima_bvh_fixed b,
                                                  anima_bvh_fixed alpha_q16)
{
    anima_bvh_fixed d;
    anima_bvh_fixed s;

    if (alpha_q16 <= 0L) {
        return a;
    }
    if (alpha_q16 >= ANIMA_BVH_FIXED_ONE) {
        return b;
    }
    d = anima_bvh_fixed_sub_sat_local(b, a);
    s = anima_bvh_fixed_mul_q16_sat_local(d, alpha_q16);
    return anima_bvh_fixed_add_sat(a, s);
}

static anima_bvh_fixed anima_bvh_fraction_q16_local(anima_bvh_fixed num, anima_bvh_fixed den)
{
    long q;
    long r;
    int bit;

    if (num <= 0L || den <= 0L) {
        return 0L;
    }
    if (num >= den) {
        return ANIMA_BVH_FIXED_ONE;
    }

    q = 0L;
    r = num;
    bit = 0;
    while (bit < ANIMA_BVH_FIXED_SHIFT) {
        q = q << 1;
        if (r > ANIMA_BVH_FIXED_MAX / 2L) {
            r = den;
        } else {
            r = r << 1;
        }
        if (r >= den) {
            r = r - den;
            q = q + 1L;
        }
        bit++;
    }
    return q;
}

void anima_bvh_mat_identity(anima_bvh_q15 m[9])
{
    int i;

    i = 0;
    while (i < 9) {
        m[i] = 0L;
        i++;
    }
    m[0] = ANIMA_BVH_Q15_ONE;
    m[4] = ANIMA_BVH_Q15_ONE;
    m[8] = ANIMA_BVH_Q15_ONE;
}

void anima_bvh_mat_mul(const anima_bvh_q15 a[9], const anima_bvh_q15 b[9], anima_bvh_q15 out[9])
{
    anima_bvh_q15 tmp[9];
    int r;
    int c;
    long sum;

    r = 0;
    while (r < 3) {
        c = 0;
        while (c < 3) {
            sum = 0L;
            sum += anima_bvh_q15_mul(a[r * 3 + 0], b[0 * 3 + c]);
            sum += anima_bvh_q15_mul(a[r * 3 + 1], b[1 * 3 + c]);
            sum += anima_bvh_q15_mul(a[r * 3 + 2], b[2 * 3 + c]);
            tmp[r * 3 + c] = anima_bvh_q15_clamp(sum);
            c++;
        }
        r++;
    }

    r = 0;
    while (r < 9) {
        out[r] = tmp[r];
        r++;
    }
}

void anima_bvh_mat_axis_rotation(AnimaBVHChannelType axis, anima_bvh_fixed degrees, anima_bvh_q15 out[9])
{
    anima_bvh_q15 s;
    anima_bvh_q15 c;

    anima_bvh_mat_identity(out);
    s = anima_bvh_sin_deg_q15(degrees);
    c = anima_bvh_cos_deg_q15(degrees);

    if (axis == ANIMA_BVH_CHAN_XROTATION) {
        out[4] = c;
        out[5] = -s;
        out[7] = s;
        out[8] = c;
    } else if (axis == ANIMA_BVH_CHAN_YROTATION) {
        out[0] = c;
        out[2] = s;
        out[6] = -s;
        out[8] = c;
    } else if (axis == ANIMA_BVH_CHAN_ZROTATION) {
        out[0] = c;
        out[1] = -s;
        out[3] = s;
        out[4] = c;
    }
}

void anima_bvh_mat_transform_point(const anima_bvh_q15 m[9], const anima_bvh_fixed in_v[3], anima_bvh_fixed out_v[3])
{
    anima_bvh_fixed x;
    anima_bvh_fixed y;
    anima_bvh_fixed z;
    anima_bvh_fixed a;
    anima_bvh_fixed b;
    anima_bvh_fixed c;

    x = in_v[0];
    y = in_v[1];
    z = in_v[2];

    a = anima_bvh_fixed_mul_q15(x, m[0]);
    b = anima_bvh_fixed_mul_q15(y, m[1]);
    c = anima_bvh_fixed_mul_q15(z, m[2]);
    out_v[0] = anima_bvh_fixed_add_sat(anima_bvh_fixed_add_sat(a, b), c);

    a = anima_bvh_fixed_mul_q15(x, m[3]);
    b = anima_bvh_fixed_mul_q15(y, m[4]);
    c = anima_bvh_fixed_mul_q15(z, m[5]);
    out_v[1] = anima_bvh_fixed_add_sat(anima_bvh_fixed_add_sat(a, b), c);

    a = anima_bvh_fixed_mul_q15(x, m[6]);
    b = anima_bvh_fixed_mul_q15(y, m[7]);
    c = anima_bvh_fixed_mul_q15(z, m[8]);
    out_v[2] = anima_bvh_fixed_add_sat(anima_bvh_fixed_add_sat(a, b), c);
}

static int anima_bvh_is_rotation_channel(AnimaBVHChannelType type)
{
    return type == ANIMA_BVH_CHAN_XROTATION || type == ANIMA_BVH_CHAN_YROTATION || type == ANIMA_BVH_CHAN_ZROTATION;
}

static void anima_bvh_build_local_transform_values(const AnimaBVH *bvh,
                                                   const anima_bvh_fixed *values,
                                                   int joint_index,
                                                   anima_bvh_fixed local_pos[3],
                                                   anima_bvh_q15 local_rot[9])
{
    int i;
    int channel_index;
    int k;
    AnimaBVHChannelType type;
    anima_bvh_fixed value;
    anima_bvh_q15 axis_rot[9];
    anima_bvh_q15 tmp[9];

    local_pos[0] = bvh->joints[joint_index].offset[0];
    local_pos[1] = bvh->joints[joint_index].offset[1];
    local_pos[2] = bvh->joints[joint_index].offset[2];
    anima_bvh_mat_identity(local_rot);

    if (values == 0 || bvh->joints[joint_index].first_channel < 0) {
        return;
    }

    i = 0;
    while (i < bvh->joints[joint_index].channel_count) {
        channel_index = bvh->joints[joint_index].first_channel + i;
        if (channel_index >= 0 && channel_index < bvh->channel_count) {
            type = bvh->channels[channel_index].type;
            value = values[channel_index];
            if (type == ANIMA_BVH_CHAN_XPOSITION) {
                local_pos[0] = anima_bvh_fixed_add_sat(local_pos[0], value);
            } else if (type == ANIMA_BVH_CHAN_YPOSITION) {
                local_pos[1] = anima_bvh_fixed_add_sat(local_pos[1], value);
            } else if (type == ANIMA_BVH_CHAN_ZPOSITION) {
                local_pos[2] = anima_bvh_fixed_add_sat(local_pos[2], value);
            } else if (anima_bvh_is_rotation_channel(type)) {
                anima_bvh_mat_axis_rotation(type, value, axis_rot);
                anima_bvh_mat_mul(local_rot, axis_rot, tmp);
                k = 0;
                while (k < 9) {
                    local_rot[k] = tmp[k];
                    k++;
                }
            }
        }
        i++;
    }
}

static int anima_bvh_eval_values_scaled(const AnimaBVH *bvh,
                                        const anima_bvh_fixed *values,
                                        anima_bvh_fixed scale_q16,
                                        AnimaBVHPose *pose)
{
    int j;
    int p;
    int k;
    anima_bvh_fixed local_pos[3];
    anima_bvh_fixed rotated_pos[3];
    anima_bvh_q15 local_rot[9];
    anima_bvh_q15 composed_rot[9];

    if (bvh == 0 || pose == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (bvh->root_joint < 0 || bvh->joint_count <= 0) {
        return ANIMA_BVH_FAIL;
    }

    j = 0;
    while (j < bvh->joint_count) {
        anima_bvh_build_local_transform_values(bvh, values, j, local_pos, local_rot);
        if (scale_q16 != ANIMA_BVH_FIXED_ONE) {
            local_pos[0] = anima_bvh_fixed_mul_q16_sat_local(local_pos[0], scale_q16);
            local_pos[1] = anima_bvh_fixed_mul_q16_sat_local(local_pos[1], scale_q16);
            local_pos[2] = anima_bvh_fixed_mul_q16_sat_local(local_pos[2], scale_q16);
        }
        p = bvh->joints[j].parent;
        if (p < 0) {
            pose->world_pos[j][0] = local_pos[0];
            pose->world_pos[j][1] = local_pos[1];
            pose->world_pos[j][2] = local_pos[2];
            k = 0;
            while (k < 9) {
                pose->world_rot[j][k] = local_rot[k];
                k++;
            }
        } else if (p < j) {
            anima_bvh_mat_transform_point(pose->world_rot[p], local_pos, rotated_pos);
            pose->world_pos[j][0] = anima_bvh_fixed_add_sat(pose->world_pos[p][0], rotated_pos[0]);
            pose->world_pos[j][1] = anima_bvh_fixed_add_sat(pose->world_pos[p][1], rotated_pos[1]);
            pose->world_pos[j][2] = anima_bvh_fixed_add_sat(pose->world_pos[p][2], rotated_pos[2]);
            anima_bvh_mat_mul(pose->world_rot[p], local_rot, composed_rot);
            k = 0;
            while (k < 9) {
                pose->world_rot[j][k] = composed_rot[k];
                k++;
            }
        } else {
            return ANIMA_BVH_FAIL;
        }
        j++;
    }

    return ANIMA_BVH_OK;
}

int anima_bvh_make_sample_frame(const AnimaBVH *bvh, int frame_index, AnimaBVHSample *sample)
{
    int c;

    if (bvh == 0 || sample == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (frame_index < 0 || frame_index >= bvh->frames_stored) {
        return ANIMA_BVH_FAIL;
    }

    c = 0;
    while (c < ANIMA_BVH_MAX_CHANNELS) {
        if (c < bvh->channel_count) {
            sample->channel_values[c] = bvh->frame_values[frame_index][c];
        } else {
            sample->channel_values[c] = 0L;
        }
        c++;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_make_sample_lerp(const AnimaBVH *bvh, int frame_a, int frame_b,
                               anima_bvh_fixed alpha_q16, AnimaBVHSample *sample)
{
    int c;

    if (bvh == 0 || sample == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (frame_a < 0 || frame_a >= bvh->frames_stored) {
        return ANIMA_BVH_FAIL;
    }
    if (frame_b < 0 || frame_b >= bvh->frames_stored) {
        return ANIMA_BVH_FAIL;
    }
    if (alpha_q16 < 0L) {
        alpha_q16 = 0L;
    }
    if (alpha_q16 > ANIMA_BVH_FIXED_ONE) {
        alpha_q16 = ANIMA_BVH_FIXED_ONE;
    }

    c = 0;
    while (c < ANIMA_BVH_MAX_CHANNELS) {
        if (c < bvh->channel_count) {
            sample->channel_values[c] = anima_bvh_fixed_lerp_local(bvh->frame_values[frame_a][c],
                                                                   bvh->frame_values[frame_b][c],
                                                                   alpha_q16);
        } else {
            sample->channel_values[c] = 0L;
        }
        c++;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_eval_sample(const AnimaBVH *bvh, const AnimaBVHSample *sample, AnimaBVHPose *pose)
{
    if (sample == 0) {
        return ANIMA_BVH_FAIL;
    }
    return anima_bvh_eval_values_scaled(bvh, sample->channel_values, ANIMA_BVH_FIXED_ONE, pose);
}

int anima_bvh_eval_frame(const AnimaBVH *bvh, int frame_index, AnimaBVHPose *pose)
{
    if (bvh == 0 || pose == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (frame_index < 0 || frame_index >= bvh->frames_stored) {
        return ANIMA_BVH_FAIL;
    }
    return anima_bvh_eval_values_scaled(bvh, bvh->frame_values[frame_index], ANIMA_BVH_FIXED_ONE, pose);
}

int anima_bvh_eval_frame_scaled(const AnimaBVH *bvh, int frame_index,
                                anima_bvh_fixed scale_q16, AnimaBVHPose *pose)
{
    if (bvh == 0 || pose == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (frame_index < 0 || frame_index >= bvh->frames_stored) {
        return ANIMA_BVH_FAIL;
    }
    return anima_bvh_eval_values_scaled(bvh, bvh->frame_values[frame_index], scale_q16, pose);
}

int anima_bvh_eval_time_loop(const AnimaBVH *bvh, anima_bvh_fixed time_q16,
                             int loop, AnimaBVHSample *sample, AnimaBVHPose *pose)
{
    anima_bvh_fixed duration;
    anima_bvh_fixed rem;
    anima_bvh_fixed alpha;
    int frame;

    if (bvh == 0 || sample == 0 || pose == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (bvh->frames_stored <= 0 || bvh->frame_time <= 0L) {
        return ANIMA_BVH_FAIL;
    }
    if (bvh->frames_stored == 1) {
        if (!anima_bvh_make_sample_frame(bvh, 0, sample)) {
            return ANIMA_BVH_FAIL;
        }
        return anima_bvh_eval_sample(bvh, sample, pose);
    }

    duration = anima_bvh_duration_frames(bvh);
    if (duration <= 0L) {
        return ANIMA_BVH_FAIL;
    }

    if (loop) {
        while (time_q16 < 0L) {
            time_q16 = anima_bvh_fixed_add_sat(time_q16, duration);
        }
        while (time_q16 >= duration) {
            time_q16 = anima_bvh_fixed_sub_sat_local(time_q16, duration);
        }
    } else {
        if (time_q16 < 0L) {
            time_q16 = 0L;
        }
        if (time_q16 >= duration) {
            return anima_bvh_eval_frame(bvh, bvh->frames_stored - 1, pose);
        }
    }

    frame = 0;
    rem = time_q16;
    while (rem >= bvh->frame_time && frame + 1 < bvh->frames_stored) {
        rem = anima_bvh_fixed_sub_sat_local(rem, bvh->frame_time);
        frame++;
    }

    if (frame + 1 >= bvh->frames_stored) {
        if (!anima_bvh_make_sample_frame(bvh, bvh->frames_stored - 1, sample)) {
            return ANIMA_BVH_FAIL;
        }
    } else {
        alpha = anima_bvh_fraction_q16_local(rem, bvh->frame_time);
        if (!anima_bvh_make_sample_lerp(bvh, frame, frame + 1, alpha, sample)) {
            return ANIMA_BVH_FAIL;
        }
    }

    return anima_bvh_eval_sample(bvh, sample, pose);
}
