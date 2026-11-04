#include "bravure_bvh_internal.h"

void bbvh_axis_map_identity(bbvh_axis_map *map)
{
    if (map == (bbvh_axis_map *)0) {
        return;
    }
    map->src_axis_for_dst[0] = 0;
    map->src_axis_for_dst[1] = 1;
    map->src_axis_for_dst[2] = 2;
    map->sign_for_dst[0] = 1;
    map->sign_for_dst[1] = 1;
    map->sign_for_dst[2] = 1;
}

int bbvh_axis_map_set(bbvh_axis_map *map, int dst_axis, int src_axis, int sign)
{
    if (map == (bbvh_axis_map *)0) {
        return BBVH_ERR_NULL;
    }
    if (dst_axis < 0 || dst_axis > 2 || src_axis < 0 || src_axis > 2) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    map->src_axis_for_dst[dst_axis] = (signed char)src_axis;
    map->sign_for_dst[dst_axis] = (sign < 0) ? (signed char)-1 : (signed char)1;
    return BBVH_OK;
}

int bbvh_axis_map_apply_vec3(const bbvh_axis_map *map, const bbvh_fixed in_xyz[3],
                             bbvh_fixed out_xyz[3])
{
    int axis;
    int src_axis;
    bbvh_fixed tmp[3];

    if (map == (const bbvh_axis_map *)0 || in_xyz == (const bbvh_fixed *)0 ||
        out_xyz == (bbvh_fixed *)0) {
        return BBVH_ERR_NULL;
    }
    for (axis = 0; axis < 3; ++axis) {
        src_axis = (int)map->src_axis_for_dst[axis];
        if (src_axis < 0 || src_axis > 2) {
            return BBVH_ERR_BAD_ARGUMENT;
        }
        tmp[axis] = in_xyz[src_axis];
        if (map->sign_for_dst[axis] < 0) {
            tmp[axis] = bbvh_fixed_sub_sat(0L, tmp[axis]);
        }
    }
    out_xyz[0] = tmp[0];
    out_xyz[1] = tmp[1];
    out_xyz[2] = tmp[2];
    return BBVH_OK;
}

int bbvh_axis_map_apply_pose_positions(const bbvh_axis_map *map, bbvh_pose *pose,
                                       int node_count)
{
    int i;
    int rc;
    bbvh_fixed tmp[3];

    if (map == (const bbvh_axis_map *)0 || pose == (bbvh_pose *)0) {
        return BBVH_ERR_NULL;
    }
    if (node_count < 0 || node_count > BRAVURE_BVH_MAX_NODES) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    for (i = 0; i < node_count; ++i) {
        rc = bbvh_axis_map_apply_vec3(map, pose->joint_position[i], tmp);
        if (rc != BBVH_OK) {
            return rc;
        }
        pose->joint_position[i][0] = tmp[0];
        pose->joint_position[i][1] = tmp[1];
        pose->joint_position[i][2] = tmp[2];
    }
    return BBVH_OK;
}
