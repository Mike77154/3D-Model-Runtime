#include "bravure_bvh_internal.h"

#define BBVH_POSE_MODE_REST 0
#define BBVH_POSE_MODE_FRAME 1
#define BBVH_POSE_MODE_LERP 2

void bbvh_pose_clear(bbvh_pose *pose)
{
    if (pose == (bbvh_pose *)0) {
        return;
    }
    bbvh_mem_zero((void *)pose, (long)sizeof(bbvh_pose));
}

static void bbvh_local_apply(bbvh_mat4 *local, const bbvh_mat4 *op)
{
    bbvh_mat4 tmp;

    bbvh_mat_mul(&tmp, local, op);
    *local = tmp;
}

static bbvh_fixed bbvh_sample_channel_value(const bbvh_document *doc,
                                            int mode, int frame_a, int frame_b,
                                            bbvh_fixed t_q16, int global_channel)
{
    bbvh_fixed a;
    bbvh_fixed b;

    if (mode == BBVH_POSE_MODE_REST) {
        return 0L;
    }
    if (mode == BBVH_POSE_MODE_FRAME) {
        return doc->motion[frame_a][global_channel];
    }
    a = doc->motion[frame_a][global_channel];
    b = doc->motion[frame_b][global_channel];
    return bbvh_fixed_lerp(a, b, t_q16);
}

static int bbvh_apply_channel_op(bbvh_mat4 *local, int channel_type, bbvh_fixed value)
{
    bbvh_mat4 op;

    switch (channel_type) {
    case BBVH_CH_XPOSITION:
        bbvh_mat_translation(&op, value, 0L, 0L);
        bbvh_local_apply(local, &op);
        break;
    case BBVH_CH_YPOSITION:
        bbvh_mat_translation(&op, 0L, value, 0L);
        bbvh_local_apply(local, &op);
        break;
    case BBVH_CH_ZPOSITION:
        bbvh_mat_translation(&op, 0L, 0L, value);
        bbvh_local_apply(local, &op);
        break;
    case BBVH_CH_XROTATION:
        bbvh_mat_rotation_x(&op, value);
        bbvh_local_apply(local, &op);
        break;
    case BBVH_CH_YROTATION:
        bbvh_mat_rotation_y(&op, value);
        bbvh_local_apply(local, &op);
        break;
    case BBVH_CH_ZROTATION:
        bbvh_mat_rotation_z(&op, value);
        bbvh_local_apply(local, &op);
        break;
    default:
        return BBVH_ERR_BAD_CHANNEL;
    }
    return BBVH_OK;
}

static int bbvh_build_local_for_node_ex(const bbvh_document *doc,
                                        int mode, int frame_a, int frame_b,
                                        bbvh_fixed t_q16,
                                        int node_index, bbvh_mat4 *local)
{
    const bbvh_node *node;
    bbvh_mat4 op;
    int i;
    int global_channel;
    int channel_type;
    bbvh_fixed value;
    int rc;

    if (doc == (const bbvh_document *)0 || local == (bbvh_mat4 *)0) {
        return BBVH_ERR_NULL;
    }
    if (node_index < 0 || node_index >= doc->node_count) {
        return BBVH_ERR_BAD_ARGUMENT;
    }

    node = &doc->nodes[node_index];
    bbvh_mat_identity(local);

    bbvh_mat_translation(&op, node->offset[0], node->offset[1], node->offset[2]);
    bbvh_local_apply(local, &op);

    for (i = 0; i < node->channel_count; ++i) {
        global_channel = node->channel_start + i;
        if (global_channel < 0 || global_channel >= doc->channel_count) {
            return BBVH_ERR_BAD_DOCUMENT;
        }
        channel_type = (int)node->channel_types[i];
        value = bbvh_sample_channel_value(doc, mode, frame_a, frame_b, t_q16, global_channel);
        rc = bbvh_apply_channel_op(local, channel_type, value);
        if (rc != BBVH_OK) {
            return rc;
        }
    }
    return BBVH_OK;
}

static int bbvh_build_pose_common(const bbvh_document *doc,
                                  int mode, int frame_a, int frame_b,
                                  bbvh_fixed t_q16, bbvh_pose *pose)
{
    int i;
    int rc;
    bbvh_mat4 local;
    bbvh_fixed origin[3];

    if (doc == (const bbvh_document *)0 || pose == (bbvh_pose *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (mode != BBVH_POSE_MODE_REST) {
        if (frame_a < 0 || frame_a >= doc->frame_count) {
            return BBVH_ERR_BAD_ARGUMENT;
        }
    }
    if (mode == BBVH_POSE_MODE_LERP) {
        if (frame_b < 0 || frame_b >= doc->frame_count) {
            return BBVH_ERR_BAD_ARGUMENT;
        }
    }

    bbvh_pose_clear(pose);

    for (i = 0; i < doc->node_count; ++i) {
        rc = bbvh_build_local_for_node_ex(doc, mode, frame_a, frame_b, t_q16, i, &local);
        if (rc != BBVH_OK) {
            return rc;
        }
        if (doc->nodes[i].parent >= 0) {
            bbvh_mat_mul(&pose->world[i], &pose->world[doc->nodes[i].parent], &local);
        } else {
            pose->world[i] = local;
        }
        origin[0] = 0L;
        origin[1] = 0L;
        origin[2] = 0L;
        bbvh_mat_apply_point(&pose->world[i], origin[0], origin[1], origin[2], pose->joint_position[i]);
    }
    return BBVH_OK;
}

int bbvh_build_rest_pose(const bbvh_document *doc, bbvh_pose *pose)
{
    return bbvh_build_pose_common(doc, BBVH_POSE_MODE_REST, 0, 0, 0L, pose);
}

int bbvh_build_pose(const bbvh_document *doc, int frame_index, bbvh_pose *pose)
{
    return bbvh_build_pose_common(doc, BBVH_POSE_MODE_FRAME, frame_index, frame_index, 0L, pose);
}

int bbvh_build_pose_lerp_frames(const bbvh_document *doc, int frame_a, int frame_b,
                                bbvh_fixed t_q16, bbvh_pose *pose)
{
    if (t_q16 <= 0L) {
        return bbvh_build_pose(doc, frame_a, pose);
    }
    if (t_q16 >= BBVH_FIXED_ONE) {
        return bbvh_build_pose(doc, frame_b, pose);
    }
    return bbvh_build_pose_common(doc, BBVH_POSE_MODE_LERP, frame_a, frame_b, t_q16, pose);
}

int bbvh_build_pose_at_time(const bbvh_document *doc, bbvh_fixed time_seconds,
                            bbvh_pose *pose)
{
    int rc;
    int frame;
    bbvh_fixed rem;
    bbvh_fixed t;

    if (doc == (const bbvh_document *)0 || pose == (bbvh_pose *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (doc->frame_count <= 0) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    if (time_seconds <= 0L || doc->frame_count == 1 || doc->frame_time <= 0L) {
        return bbvh_build_pose(doc, 0, pose);
    }

    frame = 0;
    rem = time_seconds;
    while (frame + 1 < doc->frame_count && rem >= doc->frame_time) {
        rem = bbvh_fixed_sub_sat(rem, doc->frame_time);
        frame += 1;
    }
    if (frame >= doc->frame_count - 1) {
        return bbvh_build_pose(doc, doc->frame_count - 1, pose);
    }
    t = bbvh_fixed_ratio_clamped(rem, doc->frame_time);
    return bbvh_build_pose_lerp_frames(doc, frame, frame + 1, t, pose);
}

int bbvh_get_node_position(const bbvh_document *doc, const bbvh_pose *pose,
                           int node_index, bbvh_fixed out_xyz[3])
{
    if (doc == (const bbvh_document *)0 || pose == (const bbvh_pose *)0 || out_xyz == (bbvh_fixed *)0) {
        return BBVH_ERR_NULL;
    }
    if (node_index < 0 || node_index >= doc->node_count) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    out_xyz[0] = pose->joint_position[node_index][0];
    out_xyz[1] = pose->joint_position[node_index][1];
    out_xyz[2] = pose->joint_position[node_index][2];
    return BBVH_OK;
}

int bbvh_get_end_site_position(const bbvh_document *doc, const bbvh_pose *pose,
                               int node_index, bbvh_fixed out_xyz[3])
{
    if (doc == (const bbvh_document *)0 || pose == (const bbvh_pose *)0 || out_xyz == (bbvh_fixed *)0) {
        return BBVH_ERR_NULL;
    }
    if (node_index < 0 || node_index >= doc->node_count) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    if (!doc->nodes[node_index].has_end_site) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    bbvh_mat_apply_point(&pose->world[node_index],
                         doc->nodes[node_index].end_offset[0],
                         doc->nodes[node_index].end_offset[1],
                         doc->nodes[node_index].end_offset[2],
                         out_xyz);
    return BBVH_OK;
}

static void bbvh_bounds_add_point(bbvh_fixed p[3], int *have,
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

int bbvh_compute_pose_bounds(const bbvh_document *doc, const bbvh_pose *pose,
                             int include_end_sites,
                             bbvh_fixed out_min_xyz[3], bbvh_fixed out_max_xyz[3])
{
    int i;
    int have;
    bbvh_fixed p[3];
    int rc;

    if (doc == (const bbvh_document *)0 || pose == (const bbvh_pose *)0 ||
        out_min_xyz == (bbvh_fixed *)0 || out_max_xyz == (bbvh_fixed *)0) {
        return BBVH_ERR_NULL;
    }
    if (doc->node_count <= 0) {
        return BBVH_ERR_BAD_ARGUMENT;
    }

    have = BBVH_FALSE;
    for (i = 0; i < doc->node_count; ++i) {
        p[0] = pose->joint_position[i][0];
        p[1] = pose->joint_position[i][1];
        p[2] = pose->joint_position[i][2];
        bbvh_bounds_add_point(p, &have, out_min_xyz, out_max_xyz);
        if (include_end_sites && doc->nodes[i].has_end_site) {
            rc = bbvh_get_end_site_position(doc, pose, i, p);
            if (rc != BBVH_OK) {
                return rc;
            }
            bbvh_bounds_add_point(p, &have, out_min_xyz, out_max_xyz);
        }
    }
    return have ? BBVH_OK : BBVH_ERR_BAD_ARGUMENT;
}

int bbvh_build_pose_at_time_loop(const bbvh_document *doc, bbvh_fixed time_seconds,
                                 int loop, bbvh_pose *pose)
{
    int rc;
    int frame;
    bbvh_fixed rem;
    bbvh_fixed t;
    bbvh_fixed span;

    if (doc == (const bbvh_document *)0 || pose == (bbvh_pose *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (doc->frame_count <= 0) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    if (time_seconds <= 0L || doc->frame_count == 1 || doc->frame_time <= 0L) {
        return bbvh_build_pose(doc, 0, pose);
    }

    span = bbvh_get_last_frame_time(doc);
    if (loop && span > 0L) {
        while (time_seconds >= span) {
            time_seconds = bbvh_fixed_sub_sat(time_seconds, span);
        }
    } else if (time_seconds >= span) {
        return bbvh_build_pose(doc, doc->frame_count - 1, pose);
    }

    frame = 0;
    rem = time_seconds;
    while (frame + 1 < doc->frame_count && rem >= doc->frame_time) {
        rem = bbvh_fixed_sub_sat(rem, doc->frame_time);
        frame += 1;
    }
    if (frame >= doc->frame_count - 1) {
        return bbvh_build_pose(doc, doc->frame_count - 1, pose);
    }
    t = bbvh_fixed_ratio_clamped(rem, doc->frame_time);
    return bbvh_build_pose_lerp_frames(doc, frame, frame + 1, t, pose);
}

static int bbvh_build_quat_local_for_node(const bbvh_document *doc,
                                          int frame_a, int frame_b,
                                          bbvh_fixed t_q16,
                                          int node_index, bbvh_mat4 *local)
{
    const bbvh_node *node;
    bbvh_mat4 op;
    bbvh_quat q;
    int i;
    int global_channel;
    int ctype;
    bbvh_fixed value;
    int rc;

    if (doc == (const bbvh_document *)0 || local == (bbvh_mat4 *)0) {
        return BBVH_ERR_NULL;
    }
    if (node_index < 0 || node_index >= doc->node_count) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    node = &doc->nodes[node_index];
    bbvh_mat_identity(local);

    bbvh_mat_translation(&op, node->offset[0], node->offset[1], node->offset[2]);
    bbvh_local_apply(local, &op);

    for (i = 0; i < node->channel_count; ++i) {
        global_channel = node->channel_start + i;
        ctype = (int)node->channel_types[i];
        if (ctype == BBVH_CH_XPOSITION || ctype == BBVH_CH_YPOSITION || ctype == BBVH_CH_ZPOSITION) {
            rc = bbvh_sample_channel_lerp_frames(doc, frame_a, frame_b, t_q16, global_channel, &value);
            if (rc != BBVH_OK) {
                return rc;
            }
            if (ctype == BBVH_CH_XPOSITION) {
                bbvh_mat_translation(&op, value, 0L, 0L);
            } else if (ctype == BBVH_CH_YPOSITION) {
                bbvh_mat_translation(&op, 0L, value, 0L);
            } else {
                bbvh_mat_translation(&op, 0L, 0L, value);
            }
            bbvh_local_apply(local, &op);
        }
    }

    rc = bbvh_get_node_rotation_quat_lerp_frames(doc, frame_a, frame_b, t_q16, node_index, &q);
    if (rc != BBVH_OK) {
        return rc;
    }
    bbvh_quat_to_mat4(&op, &q);
    bbvh_local_apply(local, &op);
    return BBVH_OK;
}

int bbvh_build_pose_quat_lerp_frames(const bbvh_document *doc, int frame_a,
                                     int frame_b, bbvh_fixed t_q16,
                                     bbvh_pose *pose)
{
    int rc;
    int i;
    bbvh_mat4 local;
    bbvh_fixed origin[3];

    if (doc == (const bbvh_document *)0 || pose == (bbvh_pose *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (frame_a < 0 || frame_a >= doc->frame_count ||
        frame_b < 0 || frame_b >= doc->frame_count) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    if (t_q16 <= 0L || frame_a == frame_b) {
        return bbvh_build_pose(doc, frame_a, pose);
    }
    if (t_q16 >= BBVH_FIXED_ONE) {
        return bbvh_build_pose(doc, frame_b, pose);
    }

    bbvh_pose_clear(pose);
    origin[0] = 0L;
    origin[1] = 0L;
    origin[2] = 0L;

    for (i = 0; i < doc->node_count; ++i) {
        rc = bbvh_build_quat_local_for_node(doc, frame_a, frame_b, t_q16, i, &local);
        if (rc != BBVH_OK) {
            return rc;
        }
        if (doc->nodes[i].parent >= 0) {
            bbvh_mat_mul(&pose->world[i], &pose->world[doc->nodes[i].parent], &local);
        } else {
            pose->world[i] = local;
        }
        bbvh_mat_apply_point(&pose->world[i], origin[0], origin[1], origin[2],
                             pose->joint_position[i]);
    }
    return BBVH_OK;
}

int bbvh_build_pose_quat_at_time(const bbvh_document *doc, bbvh_fixed time_seconds,
                                 int loop, bbvh_pose *pose)
{
    int rc;
    int frame;
    bbvh_fixed rem;
    bbvh_fixed t;
    bbvh_fixed span;

    if (doc == (const bbvh_document *)0 || pose == (bbvh_pose *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (doc->frame_count <= 0) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    if (time_seconds <= 0L || doc->frame_count == 1 || doc->frame_time <= 0L) {
        return bbvh_build_pose(doc, 0, pose);
    }

    span = bbvh_get_last_frame_time(doc);
    if (loop && span > 0L) {
        while (time_seconds >= span) {
            time_seconds = bbvh_fixed_sub_sat(time_seconds, span);
        }
    } else if (time_seconds >= span) {
        return bbvh_build_pose(doc, doc->frame_count - 1, pose);
    }

    frame = 0;
    rem = time_seconds;
    while (frame + 1 < doc->frame_count && rem >= doc->frame_time) {
        rem = bbvh_fixed_sub_sat(rem, doc->frame_time);
        frame += 1;
    }
    if (frame >= doc->frame_count - 1) {
        return bbvh_build_pose(doc, doc->frame_count - 1, pose);
    }
    t = bbvh_fixed_ratio_clamped(rem, doc->frame_time);
    return bbvh_build_pose_quat_lerp_frames(doc, frame, frame + 1, t, pose);
}
