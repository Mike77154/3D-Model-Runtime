#include "anima_biovisionbvh_internal.h"

static anima_bvh_fixed anima_bvh_sub_sat_extra(anima_bvh_fixed a, anima_bvh_fixed b)
{
    if (b == ANIMA_BVH_FIXED_MIN) {
        return anima_bvh_fixed_add_sat(a, ANIMA_BVH_FIXED_MAX);
    }
    return anima_bvh_fixed_add_sat(a, -b);
}

static anima_bvh_fixed anima_bvh_abs_extra(anima_bvh_fixed v)
{
    if (v < 0L) {
        if (v == ANIMA_BVH_FIXED_MIN) {
            return ANIMA_BVH_FIXED_MAX;
        }
        return -v;
    }
    return v;
}

static anima_bvh_fixed anima_bvh_mul_q16_extra(anima_bvh_fixed value, anima_bvh_fixed q16)
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

static anima_bvh_fixed anima_bvh_lerp_extra(anima_bvh_fixed a,
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
    d = anima_bvh_sub_sat_extra(b, a);
    s = anima_bvh_mul_q16_extra(d, alpha_q16);
    return anima_bvh_fixed_add_sat(a, s);
}

static anima_bvh_fixed anima_bvh_fraction_extra(anima_bvh_fixed num, anima_bvh_fixed den)
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

static void anima_bvh_append_char_extra(char *out, unsigned int cap, unsigned int *pos, char c)
{
    if (*pos + 1U < cap) {
        out[*pos] = c;
        *pos = *pos + 1U;
        out[*pos] = '\0';
    }
}

static void anima_bvh_append_text_extra(char *out, unsigned int cap, const char *text)
{
    unsigned int pos;
    unsigned int i;

    if (out == 0 || cap == 0U || text == 0) {
        return;
    }
    pos = 0U;
    while (out[pos] != '\0' && pos + 1U < cap) {
        pos++;
    }
    i = 0U;
    while (text[i] != '\0') {
        anima_bvh_append_char_extra(out, cap, &pos, text[i]);
        i++;
    }
}

static void anima_bvh_append_ulong_extra(char *out, unsigned int cap, unsigned long v)
{
    char tmp[32];
    unsigned int n;
    unsigned int pos;

    if (out == 0 || cap == 0U) {
        return;
    }
    pos = 0U;
    while (out[pos] != '\0' && pos + 1U < cap) {
        pos++;
    }
    n = 0U;
    if (v == 0UL) {
        anima_bvh_append_char_extra(out, cap, &pos, '0');
        return;
    }
    while (v > 0UL && n < (unsigned int)sizeof(tmp)) {
        tmp[n] = (char)('0' + (char)(v % 10UL));
        n++;
        v = v / 10UL;
    }
    while (n > 0U) {
        n--;
        anima_bvh_append_char_extra(out, cap, &pos, tmp[n]);
    }
}

static void anima_bvh_append_int_extra(char *out, unsigned int cap, int value)
{
    long v;

    if (value < 0) {
        anima_bvh_append_text_extra(out, cap, "-");
        v = -(long)value;
    } else {
        v = (long)value;
    }
    anima_bvh_append_ulong_extra(out, cap, (unsigned long)v);
}

static int anima_bvh_write_line_extra(AnimaBVHWriteFn write_fn, void *user, const char *text)
{
    if (!anima_bvh_write_raw(write_fn, user, text)) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_raw(write_fn, user, "\n")) {
        return ANIMA_BVH_FAIL;
    }
    return ANIMA_BVH_OK;
}

static int anima_bvh_write_char_extra(AnimaBVHWriteFn write_fn, void *user, char c)
{
    char tmp[2];

    tmp[0] = c;
    tmp[1] = '\0';
    return anima_bvh_write_raw(write_fn, user, tmp);
}

static int anima_bvh_write_csv_text_extra(AnimaBVHWriteFn write_fn, void *user, const char *text)
{
    unsigned int i;

    if (!anima_bvh_write_char_extra(write_fn, user, '"')) {
        return ANIMA_BVH_FAIL;
    }
    if (text != 0) {
        i = 0U;
        while (text[i] != '\0') {
            if (text[i] == '"') {
                if (!anima_bvh_write_raw(write_fn, user, "\"\"")) {
                    return ANIMA_BVH_FAIL;
                }
            } else {
                if (!anima_bvh_write_char_extra(write_fn, user, text[i])) {
                    return ANIMA_BVH_FAIL;
                }
            }
            i++;
        }
    }
    if (!anima_bvh_write_char_extra(write_fn, user, '"')) {
        return ANIMA_BVH_FAIL;
    }
    return ANIMA_BVH_OK;
}

static int anima_bvh_channel_occurrence_extra(const AnimaBVH *bvh, int channel_index)
{
    int first;
    int i;
    int occ;
    int joint;
    AnimaBVHChannelType type;

    if (bvh == 0 || channel_index < 0 || channel_index >= bvh->channel_count) {
        return -1;
    }
    joint = bvh->channels[channel_index].joint;
    type = bvh->channels[channel_index].type;
    if (joint < 0 || joint >= bvh->joint_count) {
        return -1;
    }
    first = bvh->joints[joint].first_channel;
    occ = 0;
    i = 0;
    while (i < bvh->joints[joint].channel_count) {
        if (first + i == channel_index) {
            return occ;
        }
        if (first + i >= 0 && first + i < bvh->channel_count && bvh->channels[first + i].type == type) {
            occ++;
        }
        i++;
    }
    return -1;
}

int anima_bvh_find_channel(const AnimaBVH *bvh, int joint_index,
                           AnimaBVHChannelType type, int occurrence)
{
    int i;
    int found;
    int c;

    if (bvh == 0 || joint_index < 0 || joint_index >= bvh->joint_count) {
        return -1;
    }
    if (occurrence < 0) {
        occurrence = 0;
    }
    if (bvh->joints[joint_index].first_channel < 0) {
        return -1;
    }
    found = 0;
    i = 0;
    while (i < bvh->joints[joint_index].channel_count) {
        c = bvh->joints[joint_index].first_channel + i;
        if (c >= 0 && c < bvh->channel_count && bvh->channels[c].type == type) {
            if (found == occurrence) {
                return c;
            }
            found++;
        }
        i++;
    }
    return -1;
}

int anima_bvh_get_frame_channel(const AnimaBVH *bvh, int frame_index, int joint_index,
                                AnimaBVHChannelType type, int occurrence,
                                anima_bvh_fixed *out_value)
{
    int c;

    if (out_value != 0) {
        *out_value = 0L;
    }
    if (bvh == 0 || out_value == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (frame_index < 0 || frame_index >= bvh->frames_stored) {
        return ANIMA_BVH_FAIL;
    }
    c = anima_bvh_find_channel(bvh, joint_index, type, occurrence);
    if (c < 0) {
        return ANIMA_BVH_FAIL;
    }
    *out_value = bvh->frame_values[frame_index][c];
    return ANIMA_BVH_OK;
}

int anima_bvh_set_frame_channel(AnimaBVH *bvh, int frame_index, int channel_index,
                                anima_bvh_fixed value)
{
    if (bvh == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (frame_index < 0 || frame_index >= bvh->frames_stored) {
        return ANIMA_BVH_FAIL;
    }
    if (channel_index < 0 || channel_index >= bvh->channel_count) {
        return ANIMA_BVH_FAIL;
    }
    bvh->frame_values[frame_index][channel_index] = value;
    return ANIMA_BVH_OK;
}

static anima_bvh_fixed anima_bvh_time_mul_frames_extra(anima_bvh_fixed frame_time, int frames)
{
    anima_bvh_fixed out;
    int i;

    out = 0L;
    i = 0;
    while (i < frames) {
        out = anima_bvh_fixed_add_sat(out, frame_time);
        i++;
    }
    return out;
}

int anima_bvh_time_to_cursor(const AnimaBVH *bvh, anima_bvh_fixed time_q16,
                             int loop, AnimaBVHTimeCursor *cursor)
{
    anima_bvh_fixed duration;
    anima_bvh_fixed last_time;
    anima_bvh_fixed rem;
    int frame;
    int did_wrap;

    if (cursor != 0) {
        cursor->frame_a = 0;
        cursor->frame_b = 0;
        cursor->alpha_q16 = 0L;
        cursor->wrapped = 0;
    }
    if (bvh == 0 || cursor == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (bvh->frames_stored <= 0 || bvh->frame_time <= 0L) {
        return ANIMA_BVH_FAIL;
    }
    if (bvh->frames_stored == 1) {
        return ANIMA_BVH_OK;
    }

    duration = anima_bvh_time_mul_frames_extra(bvh->frame_time, bvh->frames_stored);
    last_time = anima_bvh_time_mul_frames_extra(bvh->frame_time, bvh->frames_stored - 1);
    did_wrap = 0;

    if (loop) {
        while (time_q16 < 0L) {
            time_q16 = anima_bvh_fixed_add_sat(time_q16, duration);
            did_wrap = 1;
        }
        while (time_q16 >= duration) {
            time_q16 = anima_bvh_sub_sat_extra(time_q16, duration);
            did_wrap = 1;
        }
    } else {
        if (time_q16 < 0L) {
            time_q16 = 0L;
        }
        if (time_q16 >= last_time) {
            cursor->frame_a = bvh->frames_stored - 1;
            cursor->frame_b = bvh->frames_stored - 1;
            cursor->alpha_q16 = 0L;
            cursor->wrapped = 0;
            return ANIMA_BVH_OK;
        }
    }

    frame = 0;
    rem = time_q16;
    while (rem >= bvh->frame_time && frame + 1 < bvh->frames_stored) {
        rem = anima_bvh_sub_sat_extra(rem, bvh->frame_time);
        frame++;
    }

    cursor->frame_a = frame;
    if (frame + 1 < bvh->frames_stored) {
        cursor->frame_b = frame + 1;
        cursor->alpha_q16 = anima_bvh_fraction_extra(rem, bvh->frame_time);
        cursor->wrapped = did_wrap;
    } else if (loop) {
        cursor->frame_b = 0;
        cursor->alpha_q16 = anima_bvh_fraction_extra(rem, bvh->frame_time);
        cursor->wrapped = 1;
    } else {
        cursor->frame_b = frame;
        cursor->alpha_q16 = 0L;
        cursor->wrapped = did_wrap;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_make_sample_time(const AnimaBVH *bvh, anima_bvh_fixed time_q16,
                               int loop, AnimaBVHSample *sample,
                               AnimaBVHTimeCursor *cursor)
{
    AnimaBVHTimeCursor local_cursor;
    AnimaBVHTimeCursor *cur;

    if (bvh == 0 || sample == 0) {
        return ANIMA_BVH_FAIL;
    }
    cur = cursor;
    if (cur == 0) {
        cur = &local_cursor;
    }
    if (!anima_bvh_time_to_cursor(bvh, time_q16, loop, cur)) {
        return ANIMA_BVH_FAIL;
    }
    if (cur->frame_a == cur->frame_b) {
        return anima_bvh_make_sample_frame(bvh, cur->frame_a, sample);
    }
    return anima_bvh_make_sample_lerp(bvh, cur->frame_a, cur->frame_b, cur->alpha_q16, sample);
}

int anima_bvh_make_sample_blend(const AnimaBVH *bvh,
                                const AnimaBVHSample *sample_a,
                                const AnimaBVHSample *sample_b,
                                anima_bvh_fixed alpha_q16,
                                AnimaBVHSample *out_sample)
{
    int c;

    if (bvh == 0 || sample_a == 0 || sample_b == 0 || out_sample == 0) {
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
            out_sample->channel_values[c] = anima_bvh_lerp_extra(sample_a->channel_values[c],
                                                                 sample_b->channel_values[c],
                                                                 alpha_q16);
        } else {
            out_sample->channel_values[c] = 0L;
        }
        c++;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_make_sample_additive(const AnimaBVH *bvh,
                                   const AnimaBVHSample *base_sample,
                                   const AnimaBVHSample *add_sample,
                                   const AnimaBVHSample *ref_sample,
                                   anima_bvh_fixed weight_q16,
                                   AnimaBVHSample *out_sample)
{
    int c;
    anima_bvh_fixed d;
    anima_bvh_fixed w;

    if (bvh == 0 || base_sample == 0 || add_sample == 0 || ref_sample == 0 || out_sample == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (weight_q16 < 0L) {
        weight_q16 = 0L;
    }
    c = 0;
    while (c < ANIMA_BVH_MAX_CHANNELS) {
        if (c < bvh->channel_count) {
            d = anima_bvh_sub_sat_extra(add_sample->channel_values[c], ref_sample->channel_values[c]);
            w = anima_bvh_mul_q16_extra(d, weight_q16);
            out_sample->channel_values[c] = anima_bvh_fixed_add_sat(base_sample->channel_values[c], w);
        } else {
            out_sample->channel_values[c] = 0L;
        }
        c++;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_make_sample_root_relative(const AnimaBVH *bvh,
                                        const AnimaBVHSample *in_sample,
                                        const anima_bvh_fixed origin[3],
                                        AnimaBVHSample *out_sample)
{
    int c;
    int root;
    int cx;
    int cy;
    int cz;

    if (bvh == 0 || in_sample == 0 || origin == 0 || out_sample == 0) {
        return ANIMA_BVH_FAIL;
    }
    c = 0;
    while (c < ANIMA_BVH_MAX_CHANNELS) {
        if (c < bvh->channel_count) {
            out_sample->channel_values[c] = in_sample->channel_values[c];
        } else {
            out_sample->channel_values[c] = 0L;
        }
        c++;
    }
    root = bvh->root_joint;
    if (root < 0) {
        return ANIMA_BVH_FAIL;
    }
    cx = anima_bvh_find_channel(bvh, root, ANIMA_BVH_CHAN_XPOSITION, 0);
    cy = anima_bvh_find_channel(bvh, root, ANIMA_BVH_CHAN_YPOSITION, 0);
    cz = anima_bvh_find_channel(bvh, root, ANIMA_BVH_CHAN_ZPOSITION, 0);
    if (cx >= 0) {
        out_sample->channel_values[cx] = anima_bvh_sub_sat_extra(out_sample->channel_values[cx], origin[0]);
    }
    if (cy >= 0) {
        out_sample->channel_values[cy] = anima_bvh_sub_sat_extra(out_sample->channel_values[cy], origin[1]);
    }
    if (cz >= 0) {
        out_sample->channel_values[cz] = anima_bvh_sub_sat_extra(out_sample->channel_values[cz], origin[2]);
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_build_joint_map_by_name(const AnimaBVH *src, const AnimaBVH *dst,
                                      AnimaBVHJointMap *map)
{
    int i;
    int j;

    if (src == 0 || dst == 0 || map == 0) {
        return ANIMA_BVH_FAIL;
    }
    map->mapped_count = 0;
    map->missing_count = 0;
    i = 0;
    while (i < ANIMA_BVH_MAX_JOINTS) {
        map->dst_to_src_joint[i] = -1;
        i++;
    }
    i = 0;
    while (i < dst->joint_count) {
        j = anima_bvh_find_joint(src, dst->joints[i].name);
        map->dst_to_src_joint[i] = j;
        if (j >= 0) {
            map->mapped_count++;
        } else {
            map->missing_count++;
        }
        i++;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_build_retarget_map_by_name(const AnimaBVH *src, const AnimaBVH *dst,
                                         AnimaBVHRetargetMap *map)
{
    int i;
    int dst_joint;
    int src_joint;
    int occurrence;
    int src_channel;

    if (src == 0 || dst == 0 || map == 0) {
        return ANIMA_BVH_FAIL;
    }
    map->mapped_count = 0;
    map->missing_count = 0;
    i = 0;
    while (i < ANIMA_BVH_MAX_CHANNELS) {
        map->dst_to_src_channel[i] = -1;
        i++;
    }

    i = 0;
    while (i < dst->channel_count) {
        dst_joint = dst->channels[i].joint;
        src_joint = -1;
        if (dst_joint >= 0 && dst_joint < dst->joint_count) {
            src_joint = anima_bvh_find_joint(src, dst->joints[dst_joint].name);
        }
        if (src_joint >= 0) {
            occurrence = anima_bvh_channel_occurrence_extra(dst, i);
            src_channel = anima_bvh_find_channel(src, src_joint, dst->channels[i].type, occurrence);
            map->dst_to_src_channel[i] = src_channel;
            if (src_channel >= 0) {
                map->mapped_count++;
            } else {
                map->missing_count++;
            }
        } else {
            map->missing_count++;
        }
        i++;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_retarget_sample(const AnimaBVH *src, const AnimaBVH *dst,
                              const AnimaBVHRetargetMap *map,
                              const AnimaBVHSample *src_sample,
                              const AnimaBVHSample *dst_ref_sample,
                              AnimaBVHSample *dst_out_sample)
{
    int c;
    int sc;

    if (src == 0 || dst == 0 || map == 0 || src_sample == 0 || dst_out_sample == 0) {
        return ANIMA_BVH_FAIL;
    }
    c = 0;
    while (c < ANIMA_BVH_MAX_CHANNELS) {
        if (c < dst->channel_count) {
            if (dst_ref_sample != 0) {
                dst_out_sample->channel_values[c] = dst_ref_sample->channel_values[c];
            } else {
                dst_out_sample->channel_values[c] = 0L;
            }
        } else {
            dst_out_sample->channel_values[c] = 0L;
        }
        c++;
    }

    c = 0;
    while (c < dst->channel_count) {
        sc = map->dst_to_src_channel[c];
        if (sc >= 0 && sc < src->channel_count) {
            dst_out_sample->channel_values[c] = src_sample->channel_values[sc];
        }
        c++;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_compute_channel_range(const AnimaBVH *bvh, int channel_index,
                                    int first_frame, int frame_count,
                                    AnimaBVHChannelRange *range)
{
    int end_frame;
    int f;
    anima_bvh_fixed v;

    if (range != 0) {
        range->initialized = 0;
        range->min_v = 0L;
        range->max_v = 0L;
        range->min_frame = -1;
        range->max_frame = -1;
    }
    if (bvh == 0 || range == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (channel_index < 0 || channel_index >= bvh->channel_count) {
        return ANIMA_BVH_FAIL;
    }
    if (first_frame < 0) {
        first_frame = 0;
    }
    if (frame_count <= 0 || first_frame + frame_count > bvh->frames_stored) {
        end_frame = bvh->frames_stored;
    } else {
        end_frame = first_frame + frame_count;
    }
    if (first_frame >= end_frame) {
        return ANIMA_BVH_FAIL;
    }

    f = first_frame;
    while (f < end_frame) {
        v = bvh->frame_values[f][channel_index];
        if (!range->initialized) {
            range->initialized = 1;
            range->min_v = v;
            range->max_v = v;
            range->min_frame = f;
            range->max_frame = f;
        } else {
            if (v < range->min_v) {
                range->min_v = v;
                range->min_frame = f;
            }
            if (v > range->max_v) {
                range->max_v = v;
                range->max_frame = f;
            }
        }
        f++;
    }
    return ANIMA_BVH_OK;
}

static void anima_bvh_aabb_merge_extra(AnimaBVHAabb *dst, const AnimaBVHAabb *src)
{
    int a;

    if (dst == 0 || src == 0 || !src->initialized) {
        return;
    }
    if (!dst->initialized) {
        dst->initialized = 1;
        dst->min_v[0] = src->min_v[0];
        dst->min_v[1] = src->min_v[1];
        dst->min_v[2] = src->min_v[2];
        dst->max_v[0] = src->max_v[0];
        dst->max_v[1] = src->max_v[1];
        dst->max_v[2] = src->max_v[2];
        return;
    }
    a = 0;
    while (a < 3) {
        if (src->min_v[a] < dst->min_v[a]) {
            dst->min_v[a] = src->min_v[a];
        }
        if (src->max_v[a] > dst->max_v[a]) {
            dst->max_v[a] = src->max_v[a];
        }
        a++;
    }
}

int anima_bvh_compute_motion_aabb(const AnimaBVH *bvh, int first_frame,
                                  int frame_count, AnimaBVHPose *scratch_pose,
                                  AnimaBVHAabb *aabb)
{
    int end_frame;
    int f;
    AnimaBVHAabb one;

    if (aabb != 0) {
        aabb->initialized = 0;
        aabb->min_v[0] = 0L;
        aabb->min_v[1] = 0L;
        aabb->min_v[2] = 0L;
        aabb->max_v[0] = 0L;
        aabb->max_v[1] = 0L;
        aabb->max_v[2] = 0L;
    }
    if (bvh == 0 || scratch_pose == 0 || aabb == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (first_frame < 0) {
        first_frame = 0;
    }
    if (frame_count <= 0 || first_frame + frame_count > bvh->frames_stored) {
        end_frame = bvh->frames_stored;
    } else {
        end_frame = first_frame + frame_count;
    }
    if (first_frame >= end_frame) {
        return ANIMA_BVH_FAIL;
    }

    f = first_frame;
    while (f < end_frame) {
        if (!anima_bvh_eval_frame(bvh, f, scratch_pose)) {
            return ANIMA_BVH_FAIL;
        }
        if (!anima_bvh_compute_pose_aabb(bvh, scratch_pose, &one)) {
            return ANIMA_BVH_FAIL;
        }
        anima_bvh_aabb_merge_extra(aabb, &one);
        f++;
    }
    return ANIMA_BVH_OK;
}

anima_bvh_fixed anima_bvh_bone_length_estimate(const AnimaBVH *bvh, int joint_index)
{
    anima_bvh_fixed a;
    anima_bvh_fixed b;
    anima_bvh_fixed c;
    anima_bvh_fixed t;

    if (bvh == 0 || joint_index < 0 || joint_index >= bvh->joint_count) {
        return 0L;
    }
    a = anima_bvh_abs_extra(bvh->joints[joint_index].offset[0]);
    b = anima_bvh_abs_extra(bvh->joints[joint_index].offset[1]);
    c = anima_bvh_abs_extra(bvh->joints[joint_index].offset[2]);

    if (a < b) { t = a; a = b; b = t; }
    if (a < c) { t = a; a = c; c = t; }
    if (b < c) { t = b; b = c; c = t; }

    t = anima_bvh_fixed_add_sat(a, b >> 1);
    t = anima_bvh_fixed_add_sat(t, c >> 2);
    return t;
}

int anima_bvh_compute_skeleton_stats(const AnimaBVH *bvh,
                                     AnimaBVHSkeletonStats *stats)
{
    int i;
    int d;
    anima_bvh_fixed len;

    if (stats != 0) {
        memset(stats, 0, sizeof(AnimaBVHSkeletonStats));
        stats->longest_bone_joint = -1;
    }
    if (bvh == 0 || stats == 0) {
        return ANIMA_BVH_FAIL;
    }
    stats->joint_count = bvh->joint_count;
    stats->max_depth = 0;
    i = 0;
    while (i < bvh->joint_count) {
        if (bvh->joints[i].is_end_site) {
            stats->end_site_count++;
        }
        if (bvh->joints[i].channel_count > 0) {
            stats->animated_joint_count++;
        }
        d = anima_bvh_joint_depth(bvh, i);
        if (d > stats->max_depth) {
            stats->max_depth = d;
        }
        len = anima_bvh_bone_length_estimate(bvh, i);
        if (bvh->joints[i].parent >= 0) {
            if (len == 0L) {
                stats->zero_length_bone_count++;
            }
            stats->total_bone_length = anima_bvh_fixed_add_sat(stats->total_bone_length, len);
            if (len > stats->longest_bone_length) {
                stats->longest_bone_length = len;
                stats->longest_bone_joint = i;
            }
        }
        i++;
    }
    return ANIMA_BVH_OK;
}

static void anima_bvh_hash_byte_extra(unsigned long *h, unsigned int byte_value)
{
    *h = *h ^ (unsigned long)(byte_value & 255U);
    *h = *h * 16777619UL;
}

static void anima_bvh_hash_text_extra(unsigned long *h, const char *text)
{
    unsigned int i;

    if (text == 0) {
        return;
    }
    i = 0U;
    while (text[i] != '\0') {
        anima_bvh_hash_byte_extra(h, (unsigned int)((unsigned char)text[i]));
        i++;
    }
}

static void anima_bvh_hash_long_extra(unsigned long *h, long value)
{
    unsigned long v;
    int i;

    v = (unsigned long)value;
    i = 0;
    while (i < 4) {
        anima_bvh_hash_byte_extra(h, (unsigned int)(v & 255UL));
        v = v >> 8;
        i++;
    }
}

unsigned long anima_bvh_skeleton_hash(const AnimaBVH *bvh)
{
    unsigned long h;
    int i;
    int c;

    if (bvh == 0) {
        return 0UL;
    }
    h = 2166136261UL;
    anima_bvh_hash_long_extra(&h, (long)bvh->joint_count);
    anima_bvh_hash_long_extra(&h, (long)bvh->channel_count);
    i = 0;
    while (i < bvh->joint_count) {
        anima_bvh_hash_text_extra(&h, bvh->joints[i].name);
        anima_bvh_hash_long_extra(&h, (long)bvh->joints[i].parent);
        anima_bvh_hash_long_extra(&h, (long)bvh->joints[i].is_end_site);
        anima_bvh_hash_long_extra(&h, bvh->joints[i].offset[0]);
        anima_bvh_hash_long_extra(&h, bvh->joints[i].offset[1]);
        anima_bvh_hash_long_extra(&h, bvh->joints[i].offset[2]);
        c = 0;
        while (c < bvh->joints[i].channel_count) {
            if (bvh->joints[i].first_channel + c < bvh->channel_count) {
                anima_bvh_hash_long_extra(&h, (long)bvh->channels[bvh->joints[i].first_channel + c].type);
            }
            c++;
        }
        i++;
    }
    return h;
}

int anima_bvh_export_bone_csv(const AnimaBVH *bvh, int decimals,
                              AnimaBVHWriteFn write_fn, void *user)
{
    char line[192];
    char num[64];
    int i;
    int p;
    anima_bvh_fixed len;

    if (bvh == 0 || write_fn == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_line_extra(write_fn, user, "index,name,parent,parent_name,depth,end_site,length,offset_x,offset_y,offset_z")) {
        return ANIMA_BVH_FAIL;
    }
    i = 0;
    while (i < bvh->joint_count) {
        line[0] = '\0';
        anima_bvh_append_int_extra(line, sizeof(line), i);
        anima_bvh_append_text_extra(line, sizeof(line), ",");
        if (!anima_bvh_write_raw(write_fn, user, line)) {
            return ANIMA_BVH_FAIL;
        }
        if (!anima_bvh_write_csv_text_extra(write_fn, user, bvh->joints[i].name)) {
            return ANIMA_BVH_FAIL;
        }
        p = bvh->joints[i].parent;
        line[0] = '\0';
        anima_bvh_append_text_extra(line, sizeof(line), ",");
        anima_bvh_append_int_extra(line, sizeof(line), p);
        anima_bvh_append_text_extra(line, sizeof(line), ",");
        if (!anima_bvh_write_raw(write_fn, user, line)) {
            return ANIMA_BVH_FAIL;
        }
        if (p >= 0 && p < bvh->joint_count) {
            if (!anima_bvh_write_csv_text_extra(write_fn, user, bvh->joints[p].name)) {
                return ANIMA_BVH_FAIL;
            }
        } else {
            if (!anima_bvh_write_csv_text_extra(write_fn, user, "")) {
                return ANIMA_BVH_FAIL;
            }
        }
        line[0] = '\0';
        anima_bvh_append_text_extra(line, sizeof(line), ",");
        anima_bvh_append_int_extra(line, sizeof(line), anima_bvh_joint_depth(bvh, i));
        anima_bvh_append_text_extra(line, sizeof(line), ",");
        anima_bvh_append_int_extra(line, sizeof(line), bvh->joints[i].is_end_site);
        anima_bvh_append_text_extra(line, sizeof(line), ",");
        len = anima_bvh_bone_length_estimate(bvh, i);
        anima_bvh_fixed_to_cstr(len, num, sizeof(num), decimals);
        anima_bvh_append_text_extra(line, sizeof(line), num);
        anima_bvh_append_text_extra(line, sizeof(line), ",");
        anima_bvh_fixed_to_cstr(bvh->joints[i].offset[0], num, sizeof(num), decimals);
        anima_bvh_append_text_extra(line, sizeof(line), num);
        anima_bvh_append_text_extra(line, sizeof(line), ",");
        anima_bvh_fixed_to_cstr(bvh->joints[i].offset[1], num, sizeof(num), decimals);
        anima_bvh_append_text_extra(line, sizeof(line), num);
        anima_bvh_append_text_extra(line, sizeof(line), ",");
        anima_bvh_fixed_to_cstr(bvh->joints[i].offset[2], num, sizeof(num), decimals);
        anima_bvh_append_text_extra(line, sizeof(line), num);
        if (!anima_bvh_write_line_extra(write_fn, user, line)) {
            return ANIMA_BVH_FAIL;
        }
        i++;
    }
    return ANIMA_BVH_OK;
}

static anima_bvh_fixed anima_bvh_root_channel_or_offset_extra(const AnimaBVH *bvh, int frame, AnimaBVHChannelType type, int axis)
{
    int c;
    anima_bvh_fixed v;

    v = 0L;
    if (bvh == 0 || bvh->root_joint < 0 || bvh->root_joint >= bvh->joint_count) {
        return 0L;
    }
    v = bvh->joints[bvh->root_joint].offset[axis];
    c = anima_bvh_find_channel(bvh, bvh->root_joint, type, 0);
    if (c >= 0 && frame >= 0 && frame < bvh->frames_stored) {
        v = anima_bvh_fixed_add_sat(v, bvh->frame_values[frame][c]);
    }
    return v;
}

int anima_bvh_export_root_path_csv(const AnimaBVH *bvh, int decimals,
                                   AnimaBVHWriteFn write_fn, void *user)
{
    char line[256];
    char num[64];
    int f;
    anima_bvh_fixed x;
    anima_bvh_fixed y;
    anima_bvh_fixed z;
    anima_bvh_fixed px;
    anima_bvh_fixed py;
    anima_bvh_fixed pz;
    anima_bvh_fixed time;

    if (bvh == 0 || write_fn == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_write_line_extra(write_fn, user, "frame,time,root_x,root_y,root_z,delta_x,delta_y,delta_z")) {
        return ANIMA_BVH_FAIL;
    }
    px = 0L;
    py = 0L;
    pz = 0L;
    time = 0L;
    f = 0;
    while (f < bvh->frames_stored) {
        x = anima_bvh_root_channel_or_offset_extra(bvh, f, ANIMA_BVH_CHAN_XPOSITION, 0);
        y = anima_bvh_root_channel_or_offset_extra(bvh, f, ANIMA_BVH_CHAN_YPOSITION, 1);
        z = anima_bvh_root_channel_or_offset_extra(bvh, f, ANIMA_BVH_CHAN_ZPOSITION, 2);
        line[0] = '\0';
        anima_bvh_append_int_extra(line, sizeof(line), f);
        anima_bvh_append_text_extra(line, sizeof(line), ",");
        anima_bvh_fixed_to_cstr(time, num, sizeof(num), decimals);
        anima_bvh_append_text_extra(line, sizeof(line), num);
        anima_bvh_append_text_extra(line, sizeof(line), ",");
        anima_bvh_fixed_to_cstr(x, num, sizeof(num), decimals);
        anima_bvh_append_text_extra(line, sizeof(line), num);
        anima_bvh_append_text_extra(line, sizeof(line), ",");
        anima_bvh_fixed_to_cstr(y, num, sizeof(num), decimals);
        anima_bvh_append_text_extra(line, sizeof(line), num);
        anima_bvh_append_text_extra(line, sizeof(line), ",");
        anima_bvh_fixed_to_cstr(z, num, sizeof(num), decimals);
        anima_bvh_append_text_extra(line, sizeof(line), num);
        anima_bvh_append_text_extra(line, sizeof(line), ",");
        if (f == 0) {
            anima_bvh_fixed_to_cstr(0L, num, sizeof(num), decimals);
            anima_bvh_append_text_extra(line, sizeof(line), num);
            anima_bvh_append_text_extra(line, sizeof(line), ",");
            anima_bvh_append_text_extra(line, sizeof(line), num);
            anima_bvh_append_text_extra(line, sizeof(line), ",");
            anima_bvh_append_text_extra(line, sizeof(line), num);
        } else {
            anima_bvh_fixed_to_cstr(anima_bvh_sub_sat_extra(x, px), num, sizeof(num), decimals);
            anima_bvh_append_text_extra(line, sizeof(line), num);
            anima_bvh_append_text_extra(line, sizeof(line), ",");
            anima_bvh_fixed_to_cstr(anima_bvh_sub_sat_extra(y, py), num, sizeof(num), decimals);
            anima_bvh_append_text_extra(line, sizeof(line), num);
            anima_bvh_append_text_extra(line, sizeof(line), ",");
            anima_bvh_fixed_to_cstr(anima_bvh_sub_sat_extra(z, pz), num, sizeof(num), decimals);
            anima_bvh_append_text_extra(line, sizeof(line), num);
        }
        if (!anima_bvh_write_line_extra(write_fn, user, line)) {
            return ANIMA_BVH_FAIL;
        }
        px = x;
        py = y;
        pz = z;
        time = anima_bvh_fixed_add_sat(time, bvh->frame_time);
        f++;
    }
    return ANIMA_BVH_OK;
}

void anima_bvh_axis_map_identity(AnimaBVHAxisMap *map)
{
    if (map == 0) {
        return;
    }
    map->src_axis_for_dst[0] = 0;
    map->src_axis_for_dst[1] = 1;
    map->src_axis_for_dst[2] = 2;
    map->sign[0] = 1;
    map->sign[1] = 1;
    map->sign[2] = 1;
    map->scale_q16 = ANIMA_BVH_FIXED_ONE;
}

static int anima_bvh_axis_map_valid_extra(const AnimaBVHAxisMap *map)
{
    int used[3];
    int i;
    int a;

    if (map == 0) {
        return 0;
    }
    used[0] = 0;
    used[1] = 0;
    used[2] = 0;
    i = 0;
    while (i < 3) {
        a = map->src_axis_for_dst[i];
        if (a < 0 || a > 2) {
            return 0;
        }
        if (used[a]) {
            return 0;
        }
        used[a] = 1;
        if (!(map->sign[i] == 1 || map->sign[i] == -1)) {
            return 0;
        }
        i++;
    }
    return 1;
}

int anima_bvh_axis_map_apply_vec(const AnimaBVHAxisMap *map,
                                 const anima_bvh_fixed in_v[3],
                                 anima_bvh_fixed out_v[3])
{
    int i;
    int a;
    anima_bvh_fixed v;

    if (!anima_bvh_axis_map_valid_extra(map) || in_v == 0 || out_v == 0) {
        return ANIMA_BVH_FAIL;
    }
    i = 0;
    while (i < 3) {
        a = map->src_axis_for_dst[i];
        v = in_v[a];
        if (map->sign[i] < 0) {
            if (v == ANIMA_BVH_FIXED_MIN) {
                v = ANIMA_BVH_FIXED_MAX;
            } else {
                v = -v;
            }
        }
        if (map->scale_q16 != ANIMA_BVH_FIXED_ONE) {
            v = anima_bvh_mul_q16_extra(v, map->scale_q16);
        }
        out_v[i] = v;
        i++;
    }
    return ANIMA_BVH_OK;
}

static anima_bvh_q15 anima_bvh_axis_map_rot_value_extra(const AnimaBVHAxisMap *map,
                                                        const anima_bvh_q15 in_m[9],
                                                        int row, int col)
{
    int sr;
    int sc;
    long v;

    sr = map->src_axis_for_dst[row];
    sc = map->src_axis_for_dst[col];
    v = in_m[sr * 3 + sc];
    if (map->sign[row] * map->sign[col] < 0) {
        v = -v;
    }
    if (v > ANIMA_BVH_Q15_ONE) {
        v = ANIMA_BVH_Q15_ONE;
    }
    if (v < -ANIMA_BVH_Q15_ONE) {
        v = -ANIMA_BVH_Q15_ONE;
    }
    return v;
}

int anima_bvh_axis_map_apply_pose(const AnimaBVH *bvh,
                                  const AnimaBVHPose *in_pose,
                                  const AnimaBVHAxisMap *map,
                                  AnimaBVHPose *out_pose)
{
    int j;
    int r;
    int c;

    if (bvh == 0 || in_pose == 0 || out_pose == 0 || !anima_bvh_axis_map_valid_extra(map)) {
        return ANIMA_BVH_FAIL;
    }
    j = 0;
    while (j < bvh->joint_count) {
        if (!anima_bvh_axis_map_apply_vec(map, in_pose->world_pos[j], out_pose->world_pos[j])) {
            return ANIMA_BVH_FAIL;
        }
        r = 0;
        while (r < 3) {
            c = 0;
            while (c < 3) {
                out_pose->world_rot[j][r * 3 + c] = anima_bvh_axis_map_rot_value_extra(map, in_pose->world_rot[j], r, c);
                c++;
            }
            r++;
        }
        j++;
    }
    return ANIMA_BVH_OK;
}
