#include "anima_biovisionbvh_internal.h"

static anima_bvh_fixed anima_bvh_fixed_sub_sat_tools(anima_bvh_fixed a, anima_bvh_fixed b)
{
    if (b == ANIMA_BVH_FIXED_MIN) {
        return anima_bvh_fixed_add_sat(a, ANIMA_BVH_FIXED_MAX);
    }
    return anima_bvh_fixed_add_sat(a, -b);
}

anima_bvh_fixed anima_bvh_duration_frames(const AnimaBVH *bvh)
{
    int i;
    anima_bvh_fixed out;

    if (bvh == 0 || bvh->frame_time <= 0L || bvh->frames_stored <= 0) {
        return 0L;
    }

    out = 0L;
    i = 0;
    while (i < bvh->frames_stored) {
        out = anima_bvh_fixed_add_sat(out, bvh->frame_time);
        i++;
    }
    return out;
}

int anima_bvh_count_end_sites(const AnimaBVH *bvh)
{
    int i;
    int n;

    if (bvh == 0) {
        return 0;
    }
    n = 0;
    i = 0;
    while (i < bvh->joint_count) {
        if (bvh->joints[i].is_end_site) {
            n++;
        }
        i++;
    }
    return n;
}

int anima_bvh_joint_depth(const AnimaBVH *bvh, int joint_index)
{
    int depth;
    int p;
    int guard;

    if (bvh == 0 || joint_index < 0 || joint_index >= bvh->joint_count) {
        return -1;
    }

    depth = 0;
    p = bvh->joints[joint_index].parent;
    guard = 0;
    while (p >= 0 && guard < ANIMA_BVH_MAX_JOINTS) {
        depth++;
        if (p >= bvh->joint_count) {
            return -1;
        }
        p = bvh->joints[p].parent;
        guard++;
    }
    if (guard >= ANIMA_BVH_MAX_JOINTS) {
        return -1;
    }
    return depth;
}

static int anima_bvh_is_position_channel_tools(AnimaBVHChannelType t)
{
    return t == ANIMA_BVH_CHAN_XPOSITION ||
           t == ANIMA_BVH_CHAN_YPOSITION ||
           t == ANIMA_BVH_CHAN_ZPOSITION;
}

int anima_bvh_validate(const AnimaBVH *bvh, AnimaBVHValidation *report)
{
    int i;
    int j;
    int c;
    int depth;
    int first;
    int idx;

    if (report != 0) {
        memset(report, 0, sizeof(AnimaBVHValidation));
    }
    if (bvh == 0 || report == 0) {
        return ANIMA_BVH_FAIL;
    }

    if (bvh->root_joint < 0 || bvh->root_joint >= bvh->joint_count) {
        report->issue_flags |= ANIMA_BVH_VALIDATE_NO_ROOT;
    }
    if (bvh->frames_stored <= 0) {
        report->issue_flags |= ANIMA_BVH_VALIDATE_NO_FRAMES;
    }

    i = 0;
    while (i < bvh->joint_count) {
        if (bvh->joints[i].is_end_site) {
            report->end_site_count++;
        }
        if (bvh->joints[i].parent >= i || bvh->joints[i].parent < -1) {
            report->bad_parent_count++;
            report->issue_flags |= ANIMA_BVH_VALIDATE_BAD_PARENT_ORDER;
        }
        if (i == bvh->root_joint && bvh->joints[i].parent != -1) {
            report->bad_parent_count++;
            report->issue_flags |= ANIMA_BVH_VALIDATE_BAD_PARENT_ORDER;
        }
        if (bvh->joints[i].parent >= 0 &&
            bvh->joints[i].offset[0] == 0L &&
            bvh->joints[i].offset[1] == 0L &&
            bvh->joints[i].offset[2] == 0L) {
            report->zero_length_bone_count++;
            report->issue_flags |= ANIMA_BVH_VALIDATE_ZERO_LENGTH_BONES;
        }

        depth = anima_bvh_joint_depth(bvh, i);
        if (depth > report->max_depth) {
            report->max_depth = depth;
        }

        first = bvh->joints[i].first_channel;
        c = 0;
        while (c < bvh->joints[i].channel_count) {
            idx = first + c;
            if (first < 0 || idx < 0 || idx >= bvh->channel_count || bvh->channels[idx].joint != i) {
                report->channel_mismatch_count++;
                report->issue_flags |= ANIMA_BVH_VALIDATE_CHANNEL_MISMATCH;
            } else if (i != bvh->root_joint && anima_bvh_is_position_channel_tools(bvh->channels[idx].type)) {
                report->non_root_position_channel_count++;
                report->issue_flags |= ANIMA_BVH_VALIDATE_NON_ROOT_POSITION;
            }
            c++;
        }
        i++;
    }

    i = 0;
    while (i < bvh->joint_count) {
        j = i + 1;
        while (j < bvh->joint_count) {
            if (strcmp(bvh->joints[i].name, bvh->joints[j].name) == 0) {
                report->duplicate_name_count++;
                report->issue_flags |= ANIMA_BVH_VALIDATE_DUPLICATE_NAMES;
            }
            j++;
        }
        i++;
    }

    return ANIMA_BVH_OK;
}

static void anima_bvh_aabb_include(AnimaBVHAabb *aabb, const anima_bvh_fixed v[3])
{
    int k;

    if (aabb->initialized == 0) {
        aabb->initialized = 1;
        k = 0;
        while (k < 3) {
            aabb->min_v[k] = v[k];
            aabb->max_v[k] = v[k];
            k++;
        }
        return;
    }

    k = 0;
    while (k < 3) {
        if (v[k] < aabb->min_v[k]) {
            aabb->min_v[k] = v[k];
        }
        if (v[k] > aabb->max_v[k]) {
            aabb->max_v[k] = v[k];
        }
        k++;
    }
}

int anima_bvh_compute_pose_aabb(const AnimaBVH *bvh, const AnimaBVHPose *pose, AnimaBVHAabb *aabb)
{
    int i;

    if (bvh == 0 || pose == 0 || aabb == 0) {
        return ANIMA_BVH_FAIL;
    }

    memset(aabb, 0, sizeof(AnimaBVHAabb));
    i = 0;
    while (i < bvh->joint_count) {
        anima_bvh_aabb_include(aabb, pose->world_pos[i]);
        i++;
    }
    return ANIMA_BVH_OK;
}

int anima_bvh_extract_root_delta(const AnimaBVH *bvh, int frame_a, int frame_b,
                                 anima_bvh_fixed out_delta[3])
{
    int c;
    int idx;
    int first;
    AnimaBVHChannelType t;
    anima_bvh_fixed va;
    anima_bvh_fixed vb;

    if (out_delta != 0) {
        out_delta[0] = 0L;
        out_delta[1] = 0L;
        out_delta[2] = 0L;
    }
    if (bvh == 0 || out_delta == 0) {
        return ANIMA_BVH_FAIL;
    }
    if (bvh->root_joint < 0 || bvh->root_joint >= bvh->joint_count) {
        return ANIMA_BVH_FAIL;
    }
    if (frame_a < 0 || frame_a >= bvh->frames_stored || frame_b < 0 || frame_b >= bvh->frames_stored) {
        return ANIMA_BVH_FAIL;
    }

    first = bvh->joints[bvh->root_joint].first_channel;
    c = 0;
    while (c < bvh->joints[bvh->root_joint].channel_count) {
        idx = first + c;
        if (idx >= 0 && idx < bvh->channel_count) {
            t = bvh->channels[idx].type;
            va = bvh->frame_values[frame_a][idx];
            vb = bvh->frame_values[frame_b][idx];
            if (t == ANIMA_BVH_CHAN_XPOSITION) {
                out_delta[0] = anima_bvh_fixed_sub_sat_tools(vb, va);
            } else if (t == ANIMA_BVH_CHAN_YPOSITION) {
                out_delta[1] = anima_bvh_fixed_sub_sat_tools(vb, va);
            } else if (t == ANIMA_BVH_CHAN_ZPOSITION) {
                out_delta[2] = anima_bvh_fixed_sub_sat_tools(vb, va);
            }
        }
        c++;
    }

    return ANIMA_BVH_OK;
}
