#include "bravure_bvh_internal.h"

static int bbvh_is_position_channel(int ctype)
{
    return (ctype == BBVH_CH_XPOSITION || ctype == BBVH_CH_YPOSITION || ctype == BBVH_CH_ZPOSITION) ? BBVH_TRUE : BBVH_FALSE;
}

static int bbvh_is_rotation_channel(int ctype)
{
    return (ctype == BBVH_CH_XROTATION || ctype == BBVH_CH_YROTATION || ctype == BBVH_CH_ZROTATION) ? BBVH_TRUE : BBVH_FALSE;
}

int bbvh_estimate_rest_height_axis(const bbvh_document *doc, int up_axis,
                                   int include_end_sites, bbvh_pose *scratch_pose,
                                   bbvh_fixed *out_height)
{
    int rc;
    bbvh_fixed bmin[3];
    bbvh_fixed bmax[3];

    if (doc == (const bbvh_document *)0 || scratch_pose == (bbvh_pose *)0 ||
        out_height == (bbvh_fixed *)0) {
        return BBVH_ERR_NULL;
    }
    if (up_axis < 0 || up_axis > 2) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    rc = bbvh_build_rest_pose(doc, scratch_pose);
    if (rc != BBVH_OK) {
        return rc;
    }
    rc = bbvh_compute_pose_bounds(doc, scratch_pose, include_end_sites, bmin, bmax);
    if (rc != BBVH_OK) {
        return rc;
    }
    *out_height = bbvh_fixed_sub_sat(bmax[up_axis], bmin[up_axis]);
    if (*out_height < 0L) {
        *out_height = bbvh_fixed_abs_sat(*out_height);
    }
    return BBVH_OK;
}

int bbvh_retarget_motion_by_name_scaled(const bbvh_document *src_motion,
                                        const bbvh_document *target_skeleton,
                                        bbvh_document *dst,
                                        bbvh_fixed position_scale,
                                        int copy_root_position,
                                        int copy_rotations)
{
    int rc;
    int frame;
    int dst_node;
    int src_node;
    int dst_channel;
    int src_channel;
    int i;
    int ctype;
    bbvh_fixed value;

    if (src_motion == (const bbvh_document *)0 || target_skeleton == (const bbvh_document *)0 ||
        dst == (bbvh_document *)0) {
        return BBVH_ERR_NULL;
    }
    if (dst == src_motion || dst == target_skeleton) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    rc = bbvh_validate(src_motion);
    if (rc != BBVH_OK) {
        return rc;
    }
    rc = bbvh_validate(target_skeleton);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (src_motion->frame_count > BRAVURE_BVH_MAX_FRAMES) {
        return BBVH_ERR_TOO_MANY_FRAMES;
    }

    *dst = *target_skeleton;
    dst->frame_count = src_motion->frame_count;
    dst->frame_time = src_motion->frame_time;
    for (frame = 0; frame < dst->frame_count; ++frame) {
        for (dst_channel = 0; dst_channel < dst->channel_count; ++dst_channel) {
            dst->motion[frame][dst_channel] = 0L;
        }
    }

    for (frame = 0; frame < src_motion->frame_count; ++frame) {
        for (dst_node = 0; dst_node < dst->node_count; ++dst_node) {
            src_node = bbvh_find_node(src_motion, dst->nodes[dst_node].name);
            if (src_node < 0) {
                continue;
            }
            for (i = 0; i < dst->nodes[dst_node].channel_count; ++i) {
                ctype = (int)dst->nodes[dst_node].channel_types[i];
                if (bbvh_is_position_channel(ctype)) {
                    if (!copy_root_position || dst_node != dst->root_index) {
                        continue;
                    }
                } else if (bbvh_is_rotation_channel(ctype)) {
                    if (!copy_rotations) {
                        continue;
                    }
                } else {
                    continue;
                }
                src_channel = bbvh_node_find_channel(src_motion, src_node, ctype, 0);
                if (src_channel < 0) {
                    continue;
                }
                value = src_motion->motion[frame][src_channel];
                if (bbvh_is_position_channel(ctype)) {
                    value = bbvh_fixed_mul(value, position_scale);
                }
                dst_channel = dst->nodes[dst_node].channel_start + i;
                dst->motion[frame][dst_channel] = value;
            }
        }
    }
    return bbvh_validate(dst);
}

int bbvh_compute_node_velocity(const bbvh_document *doc, int frame_a, int frame_b,
                               int node_index, bbvh_pose *scratch_a,
                               bbvh_pose *scratch_b, bbvh_fixed out_velocity[3])
{
    int rc;
    int axis;
    bbvh_fixed dt;
    bbvh_fixed delta;

    if (doc == (const bbvh_document *)0 || scratch_a == (bbvh_pose *)0 ||
        scratch_b == (bbvh_pose *)0 || out_velocity == (bbvh_fixed *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (frame_a < 0 || frame_a >= doc->frame_count ||
        frame_b < 0 || frame_b >= doc->frame_count ||
        node_index < 0 || node_index >= doc->node_count ||
        frame_a == frame_b || doc->frame_time <= 0L) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    rc = bbvh_build_pose(doc, frame_a, scratch_a);
    if (rc != BBVH_OK) {
        return rc;
    }
    rc = bbvh_build_pose(doc, frame_b, scratch_b);
    if (rc != BBVH_OK) {
        return rc;
    }
    dt = bbvh_fixed_mul(bbvh_fixed_from_int((long)(frame_b - frame_a)), doc->frame_time);
    if (dt < 0L) {
        dt = bbvh_fixed_abs_sat(dt);
    }
    if (dt <= 0L) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    for (axis = 0; axis < 3; ++axis) {
        delta = bbvh_fixed_sub_sat(scratch_b->joint_position[node_index][axis],
                                   scratch_a->joint_position[node_index][axis]);
        out_velocity[axis] = bbvh_fixed_div(delta, dt);
    }
    return BBVH_OK;
}

int bbvh_detect_stationary_contact(const bbvh_document *doc, int frame_index,
                                   int node_index, int up_axis,
                                   bbvh_fixed max_height,
                                   bbvh_fixed max_speed,
                                   bbvh_pose *scratch_a,
                                   bbvh_pose *scratch_b,
                                   int *out_contact)
{
    int rc;
    int prev_frame;
    bbvh_fixed vel[3];
    bbvh_fixed speed;
    bbvh_fixed height;

    if (doc == (const bbvh_document *)0 || scratch_a == (bbvh_pose *)0 ||
        scratch_b == (bbvh_pose *)0 || out_contact == (int *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (frame_index < 0 || frame_index >= doc->frame_count ||
        node_index < 0 || node_index >= doc->node_count || up_axis < 0 || up_axis > 2) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    *out_contact = BBVH_FALSE;
    rc = bbvh_build_pose(doc, frame_index, scratch_a);
    if (rc != BBVH_OK) {
        return rc;
    }
    height = scratch_a->joint_position[node_index][up_axis];
    if (height < 0L) {
        height = bbvh_fixed_abs_sat(height);
    }
    if (height > max_height) {
        return BBVH_OK;
    }
    if (doc->frame_count <= 1) {
        *out_contact = BBVH_TRUE;
        return BBVH_OK;
    }
    prev_frame = (frame_index > 0) ? (frame_index - 1) : (frame_index + 1);
    rc = bbvh_compute_node_velocity(doc, prev_frame, frame_index, node_index,
                                    scratch_a, scratch_b, vel);
    if (rc != BBVH_OK) {
        return rc;
    }
    speed = bbvh_vec3_length(vel);
    if (speed <= max_speed) {
        *out_contact = BBVH_TRUE;
    }
    return BBVH_OK;
}
