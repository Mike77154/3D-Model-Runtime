#include "bravure_bvh_internal.h"

bbvh_fixed bbvh_get_last_frame_time(const bbvh_document *doc)
{
    int i;
    bbvh_fixed t;

    if (doc == (const bbvh_document *)0 || doc->frame_count <= 1) {
        return 0L;
    }
    t = 0L;
    for (i = 1; i < doc->frame_count; ++i) {
        t = bbvh_fixed_add_sat(t, doc->frame_time);
    }
    return t;
}

int bbvh_node_find_channel(const bbvh_document *doc, int node_index,
                           int channel_type, int occurrence)
{
    int i;
    int seen;
    const bbvh_node *node;

    if (doc == (const bbvh_document *)0) {
        return -1;
    }
    if (node_index < 0 || node_index >= doc->node_count) {
        return -1;
    }
    if (occurrence < 0) {
        occurrence = 0;
    }
    node = &doc->nodes[node_index];
    seen = 0;
    for (i = 0; i < node->channel_count; ++i) {
        if ((int)node->channel_types[i] == channel_type) {
            if (seen == occurrence) {
                return node->channel_start + i;
            }
            seen += 1;
        }
    }
    return -1;
}

int bbvh_get_frame_channel_value(const bbvh_document *doc, int frame_index,
                                 int channel_index, bbvh_fixed *out_value)
{
    int rc;

    if (doc == (const bbvh_document *)0 || out_value == (bbvh_fixed *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (frame_index < 0 || frame_index >= doc->frame_count ||
        channel_index < 0 || channel_index >= doc->channel_count) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    *out_value = doc->motion[frame_index][channel_index];
    return BBVH_OK;
}

int bbvh_set_frame_channel_value(bbvh_document *doc, int frame_index,
                                 int channel_index, bbvh_fixed value)
{
    int rc;

    if (doc == (bbvh_document *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (frame_index < 0 || frame_index >= doc->frame_count ||
        channel_index < 0 || channel_index >= doc->channel_count) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    doc->motion[frame_index][channel_index] = value;
    return BBVH_OK;
}

int bbvh_sample_channel_lerp_frames(const bbvh_document *doc, int frame_a,
                                    int frame_b, bbvh_fixed t_q16,
                                    int channel_index, bbvh_fixed *out_value)
{
    int rc;
    bbvh_fixed a;
    bbvh_fixed b;

    if (doc == (const bbvh_document *)0 || out_value == (bbvh_fixed *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (frame_a < 0 || frame_a >= doc->frame_count ||
        frame_b < 0 || frame_b >= doc->frame_count ||
        channel_index < 0 || channel_index >= doc->channel_count) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    if (t_q16 <= 0L || frame_a == frame_b) {
        *out_value = doc->motion[frame_a][channel_index];
        return BBVH_OK;
    }
    if (t_q16 >= BBVH_FIXED_ONE) {
        *out_value = doc->motion[frame_b][channel_index];
        return BBVH_OK;
    }
    a = doc->motion[frame_a][channel_index];
    b = doc->motion[frame_b][channel_index];
    *out_value = bbvh_fixed_lerp(a, b, t_q16);
    return BBVH_OK;
}

int bbvh_sample_channel_at_time(const bbvh_document *doc, bbvh_fixed time_seconds,
                                int loop, int channel_index, bbvh_fixed *out_value)
{
    int rc;
    int frame;
    bbvh_fixed rem;
    bbvh_fixed t;
    bbvh_fixed span;

    if (doc == (const bbvh_document *)0 || out_value == (bbvh_fixed *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (channel_index < 0 || channel_index >= doc->channel_count || doc->frame_count <= 0) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    if (time_seconds <= 0L || doc->frame_count == 1 || doc->frame_time <= 0L) {
        *out_value = doc->motion[0][channel_index];
        return BBVH_OK;
    }

    span = bbvh_get_last_frame_time(doc);
    if (loop && span > 0L) {
        while (time_seconds >= span) {
            time_seconds = bbvh_fixed_sub_sat(time_seconds, span);
        }
    } else if (time_seconds >= span) {
        *out_value = doc->motion[doc->frame_count - 1][channel_index];
        return BBVH_OK;
    }

    frame = 0;
    rem = time_seconds;
    while (frame + 1 < doc->frame_count && rem >= doc->frame_time) {
        rem = bbvh_fixed_sub_sat(rem, doc->frame_time);
        frame += 1;
    }
    if (frame >= doc->frame_count - 1) {
        *out_value = doc->motion[doc->frame_count - 1][channel_index];
        return BBVH_OK;
    }
    t = bbvh_fixed_ratio_clamped(rem, doc->frame_time);
    return bbvh_sample_channel_lerp_frames(doc, frame, frame + 1, t, channel_index, out_value);
}

static bbvh_fixed bbvh_motion_time_for_resample_index(int frame, int new_frame_count,
                                                      int old_frame_count)
{
    bbvh_fixed ratio;
    bbvh_fixed span;

    if (new_frame_count <= 1 || old_frame_count <= 1) {
        return 0L;
    }
    ratio = bbvh_fixed_ratio_clamped(bbvh_fixed_from_int((long)frame),
                                    bbvh_fixed_from_int((long)(new_frame_count - 1)));
    span = bbvh_fixed_from_int((long)(old_frame_count - 1));
    return bbvh_fixed_mul(ratio, span);
}

int bbvh_resample_uniform(const bbvh_document *src, bbvh_document *dst,
                          int new_frame_count)
{
    int rc;
    int frame;
    int channel;
    bbvh_fixed pos;
    int a;
    int b;
    bbvh_fixed frac;
    bbvh_fixed value;
    bbvh_fixed end_time;

    if (src == (const bbvh_document *)0 || dst == (bbvh_document *)0) {
        return BBVH_ERR_NULL;
    }
    if (src == (const bbvh_document *)dst) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    rc = bbvh_validate(src);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (new_frame_count < 0 || new_frame_count > BRAVURE_BVH_MAX_FRAMES) {
        return BBVH_ERR_TOO_MANY_FRAMES;
    }
    if (src->frame_count <= 0 && new_frame_count > 0) {
        return BBVH_ERR_BAD_ARGUMENT;
    }

    *dst = *src;
    dst->frame_count = new_frame_count;
    if (new_frame_count <= 1) {
        dst->frame_time = 0L;
    } else {
        end_time = bbvh_get_last_frame_time(src);
        dst->frame_time = bbvh_fixed_div_int(end_time, (long)(new_frame_count - 1));
    }

    for (frame = 0; frame < new_frame_count; ++frame) {
        pos = bbvh_motion_time_for_resample_index(frame, new_frame_count, src->frame_count);
        a = (int)bbvh_fixed_floor_int(pos);
        if (a < 0) {
            a = 0;
        }
        if (a >= src->frame_count - 1) {
            a = src->frame_count - 1;
            b = a;
            frac = 0L;
        } else {
            b = a + 1;
            frac = bbvh_fixed_sub_sat(pos, bbvh_fixed_from_int((long)a));
        }
        for (channel = 0; channel < src->channel_count; ++channel) {
            rc = bbvh_sample_channel_lerp_frames(src, a, b, frac, channel, &value);
            if (rc != BBVH_OK) {
                return rc;
            }
            dst->motion[frame][channel] = value;
        }
    }
    return bbvh_validate(dst);
}

int bbvh_compute_channel_range(const bbvh_document *doc, int channel_index,
                               bbvh_fixed *out_min, bbvh_fixed *out_max)
{
    int rc;
    int frame;
    bbvh_fixed v;

    if (doc == (const bbvh_document *)0 || out_min == (bbvh_fixed *)0 || out_max == (bbvh_fixed *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (channel_index < 0 || channel_index >= doc->channel_count || doc->frame_count <= 0) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    *out_min = doc->motion[0][channel_index];
    *out_max = doc->motion[0][channel_index];
    for (frame = 1; frame < doc->frame_count; ++frame) {
        v = doc->motion[frame][channel_index];
        if (v < *out_min) {
            *out_min = v;
        }
        if (v > *out_max) {
            *out_max = v;
        }
    }
    return BBVH_OK;
}

int bbvh_compute_all_channel_ranges(const bbvh_document *doc,
                                    bbvh_fixed out_min[BRAVURE_BVH_MAX_CHANNELS],
                                    bbvh_fixed out_max[BRAVURE_BVH_MAX_CHANNELS])
{
    int rc;
    int channel;

    if (doc == (const bbvh_document *)0 || out_min == (bbvh_fixed *)0 || out_max == (bbvh_fixed *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    for (channel = 0; channel < doc->channel_count; ++channel) {
        rc = bbvh_compute_channel_range(doc, channel, &out_min[channel], &out_max[channel]);
        if (rc != BBVH_OK) {
            return rc;
        }
    }
    return BBVH_OK;
}

static void bbvh_motion_add_point(bbvh_fixed p[3], int *have,
                                  bbvh_fixed out_min_xyz[3], bbvh_fixed out_max_xyz[3])
{
    int axis;

    if (!*have) {
        for (axis = 0; axis < 3; ++axis) {
            out_min_xyz[axis] = p[axis];
            out_max_xyz[axis] = p[axis];
        }
        *have = BBVH_TRUE;
        return;
    }
    for (axis = 0; axis < 3; ++axis) {
        if (p[axis] < out_min_xyz[axis]) {
            out_min_xyz[axis] = p[axis];
        }
        if (p[axis] > out_max_xyz[axis]) {
            out_max_xyz[axis] = p[axis];
        }
    }
}

int bbvh_compute_motion_bounds(const bbvh_document *doc, int include_end_sites,
                               bbvh_pose *scratch_pose, bbvh_fixed out_min_xyz[3],
                               bbvh_fixed out_max_xyz[3])
{
    int rc;
    int frame;
    int node;
    int have;
    bbvh_fixed p[3];

    if (doc == (const bbvh_document *)0 || scratch_pose == (bbvh_pose *)0 ||
        out_min_xyz == (bbvh_fixed *)0 || out_max_xyz == (bbvh_fixed *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (doc->frame_count <= 0) {
        return BBVH_ERR_BAD_ARGUMENT;
    }

    have = BBVH_FALSE;
    for (frame = 0; frame < doc->frame_count; ++frame) {
        rc = bbvh_build_pose(doc, frame, scratch_pose);
        if (rc != BBVH_OK) {
            return rc;
        }
        for (node = 0; node < doc->node_count; ++node) {
            p[0] = scratch_pose->joint_position[node][0];
            p[1] = scratch_pose->joint_position[node][1];
            p[2] = scratch_pose->joint_position[node][2];
            bbvh_motion_add_point(p, &have, out_min_xyz, out_max_xyz);
            if (include_end_sites && doc->nodes[node].has_end_site) {
                rc = bbvh_get_end_site_position(doc, scratch_pose, node, p);
                if (rc != BBVH_OK) {
                    return rc;
                }
                bbvh_motion_add_point(p, &have, out_min_xyz, out_max_xyz);
            }
        }
    }
    return have ? BBVH_OK : BBVH_ERR_BAD_ARGUMENT;
}
