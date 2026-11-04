#include "anima_biovisionbvh_internal.h"

static anima_bvh_fixed anima_bvh_neg_resample(anima_bvh_fixed v)
{
    if (v == ANIMA_BVH_FIXED_MIN) {
        return ANIMA_BVH_FIXED_MAX;
    }
    return -v;
}

static anima_bvh_fixed anima_bvh_add_resample(anima_bvh_fixed a, anima_bvh_fixed b)
{
    return anima_bvh_fixed_add_sat(a, b);
}

int anima_bvh_clone_skeleton_empty(const AnimaBVH *src, AnimaBVH *dst,
                                   int frame_count, anima_bvh_fixed frame_time_q16)
{
    int i;
    int c;
    int f;

    if (src == 0 || dst == 0 || frame_count < 0) {
        return ANIMA_BVH_FAIL;
    }
    anima_bvh_init(dst);
    dst->joint_count = src->joint_count;
    dst->channel_count = src->channel_count;
    dst->channels_declared = src->channels_declared;
    dst->root_joint = src->root_joint;
    dst->frames_declared = frame_count;
    if (frame_count > ANIMA_BVH_MAX_FRAMES) {
        dst->frames_stored = ANIMA_BVH_MAX_FRAMES;
        dst->truncation_flags |= ANIMA_BVH_TRUNC_FRAMES;
    } else {
        dst->frames_stored = frame_count;
    }
    dst->frame_time = frame_time_q16;
    dst->truncation_flags |= src->truncation_flags;

    i = 0;
    while (i < src->joint_count && i < ANIMA_BVH_MAX_JOINTS) {
        dst->joints[i] = src->joints[i];
        i++;
    }
    c = 0;
    while (c < src->channel_count && c < ANIMA_BVH_MAX_CHANNELS) {
        dst->channels[c] = src->channels[c];
        c++;
    }
    f = 0;
    while (f < dst->frames_stored) {
        c = 0;
        while (c < dst->channel_count) {
            dst->frame_values[f][c] = 0L;
            c++;
        }
        f++;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_copy_frame_range(const AnimaBVH *src, AnimaBVH *dst,
                               int first_frame, int frame_count)
{
    int f;
    int c;
    int stored;

    if (src == 0 || dst == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (first_frame < 0 || first_frame >= src->frames_stored) {
        return ANIMA_BVH_FAIL;
    }
    if (frame_count <= 0) {
        frame_count = src->frames_stored - first_frame;
    }
    if (frame_count < 0) {
        return ANIMA_BVH_FAIL;
    }
    if (first_frame + frame_count > src->frames_stored) {
        frame_count = src->frames_stored - first_frame;
    }
    if (!anima_bvh_clone_skeleton_empty(src, dst, frame_count, src->frame_time)) {
        return ANIMA_BVH_FAIL;
    }
    stored = dst->frames_stored;
    f = 0;
    while (f < stored) {
        c = 0;
        while (c < dst->channel_count) {
            dst->frame_values[f][c] = src->frame_values[first_frame + f][c];
            c++;
        }
        f++;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_resample_clip(const AnimaBVH *src, AnimaBVH *dst,
                            anima_bvh_fixed new_frame_time_q16,
                            int out_frames, int loop)
{
    AnimaBVHSample sample;
    int f;
    int c;
    anima_bvh_fixed t;

    if (src == 0 || dst == 0 || new_frame_time_q16 <= 0L || out_frames < 0) {
        return ANIMA_BVH_FAIL;
    }
    if (out_frames == 0) {
        out_frames = src->frames_stored;
    }
    if (!anima_bvh_clone_skeleton_empty(src, dst, out_frames, new_frame_time_q16)) {
        return ANIMA_BVH_FAIL;
    }
    t = 0L;
    f = 0;
    while (f < dst->frames_stored) {
        if (!anima_bvh_make_sample_time(src, t, loop, &sample, 0)) {
            return ANIMA_BVH_FAIL;
        }
        c = 0;
        while (c < dst->channel_count) {
            dst->frame_values[f][c] = sample.channel_values[c];
            c++;
        }
        t = anima_bvh_add_resample(t, new_frame_time_q16);
        f++;
    }
    return ANIMA_BVH_OK;
}

static int anima_bvh_get_position_triplet_resample(const AnimaBVH *bvh, int joint,
                                                   int *cx, int *cy, int *cz)
{
    if (cx != 0) {
        *cx = -1;
    }
    if (cy != 0) {
        *cy = -1;
    }
    if (cz != 0) {
        *cz = -1;
    }
    if (bvh == 0 || joint < 0 || joint >= bvh->joint_count || cx == 0 || cy == 0 || cz == 0) {
        return ANIMA_BVH_FAIL;
    }
    *cx = anima_bvh_find_channel(bvh, joint, ANIMA_BVH_CHAN_XPOSITION, 0);
    *cy = anima_bvh_find_channel(bvh, joint, ANIMA_BVH_CHAN_YPOSITION, 0);
    *cz = anima_bvh_find_channel(bvh, joint, ANIMA_BVH_CHAN_ZPOSITION, 0);
    if (*cx < 0 || *cy < 0 || *cz < 0) {
        return ANIMA_BVH_FAIL;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_copy_with_position_axis_map(const AnimaBVH *src, AnimaBVH *dst,
                                          const AnimaBVHAxisMap *map)
{
    int j;
    int f;
    int c;
    int cx;
    int cy;
    int cz;
    anima_bvh_fixed in_v[3];
    anima_bvh_fixed out_v[3];

    if (src == 0 || dst == 0 || map == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (!anima_bvh_clone_skeleton_empty(src, dst, src->frames_stored, src->frame_time)) {
        return ANIMA_BVH_FAIL;
    }
    f = 0;
    while (f < dst->frames_stored) {
        c = 0;
        while (c < dst->channel_count) {
            dst->frame_values[f][c] = src->frame_values[f][c];
            c++;
        }
        f++;
    }
    j = 0;
    while (j < dst->joint_count) {
        if (!anima_bvh_axis_map_apply_vec(map, src->joints[j].offset, dst->joints[j].offset)) {
            return ANIMA_BVH_FAIL;
        }
        if (anima_bvh_get_position_triplet_resample(src, j, &cx, &cy, &cz)) {
            f = 0;
            while (f < dst->frames_stored) {
                in_v[0] = src->frame_values[f][cx];
                in_v[1] = src->frame_values[f][cy];
                in_v[2] = src->frame_values[f][cz];
                if (!anima_bvh_axis_map_apply_vec(map, in_v, out_v)) {
                    return ANIMA_BVH_FAIL;
                }
                dst->frame_values[f][cx] = out_v[0];
                dst->frame_values[f][cy] = out_v[1];
                dst->frame_values[f][cz] = out_v[2];
                f++;
            }
        }
        j++;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_apply_root_translation_offset(AnimaBVH *bvh,
                                            const anima_bvh_fixed offset[3])
{
    int cx;
    int cy;
    int cz;
    int f;

    if (bvh == 0 || offset == 0 || bvh->root_joint < 0 || bvh->root_joint >= bvh->joint_count) {
        return ANIMA_BVH_FAIL;
    }
    cx = anima_bvh_find_channel(bvh, bvh->root_joint, ANIMA_BVH_CHAN_XPOSITION, 0);
    cy = anima_bvh_find_channel(bvh, bvh->root_joint, ANIMA_BVH_CHAN_YPOSITION, 0);
    cz = anima_bvh_find_channel(bvh, bvh->root_joint, ANIMA_BVH_CHAN_ZPOSITION, 0);
    f = 0;
    while (f < bvh->frames_stored) {
        if (cx >= 0) {
            bvh->frame_values[f][cx] = anima_bvh_add_resample(bvh->frame_values[f][cx], offset[0]);
        }
        if (cy >= 0) {
            bvh->frame_values[f][cy] = anima_bvh_add_resample(bvh->frame_values[f][cy], offset[1]);
        }
        if (cz >= 0) {
            bvh->frame_values[f][cz] = anima_bvh_add_resample(bvh->frame_values[f][cz], offset[2]);
        }
        f++;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_normalize_root_translation(AnimaBVH *bvh, int origin_frame,
                                         int keep_y_axis)
{
    int cx;
    int cy;
    int cz;
    anima_bvh_fixed offset[3];

    if (bvh == 0 || bvh->root_joint < 0 || bvh->root_joint >= bvh->joint_count) {
        return ANIMA_BVH_FAIL;
    }
    if (origin_frame < 0 || origin_frame >= bvh->frames_stored) {
        return ANIMA_BVH_FAIL;
    }
    offset[0] = 0L;
    offset[1] = 0L;
    offset[2] = 0L;
    cx = anima_bvh_find_channel(bvh, bvh->root_joint, ANIMA_BVH_CHAN_XPOSITION, 0);
    cy = anima_bvh_find_channel(bvh, bvh->root_joint, ANIMA_BVH_CHAN_YPOSITION, 0);
    cz = anima_bvh_find_channel(bvh, bvh->root_joint, ANIMA_BVH_CHAN_ZPOSITION, 0);
    if (cx >= 0) {
        offset[0] = anima_bvh_neg_resample(bvh->frame_values[origin_frame][cx]);
    }
    if (cy >= 0 && !keep_y_axis) {
        offset[1] = anima_bvh_neg_resample(bvh->frame_values[origin_frame][cy]);
    }
    if (cz >= 0) {
        offset[2] = anima_bvh_neg_resample(bvh->frame_values[origin_frame][cz]);
    }
    return anima_bvh_apply_root_translation_offset(bvh, offset);
}
