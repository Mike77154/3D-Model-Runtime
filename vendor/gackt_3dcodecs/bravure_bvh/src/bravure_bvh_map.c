#include "bravure_bvh_internal.h"

int bbvh_extract_pose_segments(const bbvh_document *doc, const bbvh_pose *pose,
                               int include_end_sites, bbvh_segment *out_segments,
                               int max_segments, int *out_count)
{
    int rc;
    int i;
    int count;
    bbvh_segment *seg;

    if (doc == (const bbvh_document *)0 || pose == (const bbvh_pose *)0 || out_count == (int *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (max_segments < 0) {
        return BBVH_ERR_BAD_ARGUMENT;
    }

    count = 0;
    for (i = 0; i < doc->node_count; ++i) {
        if (doc->nodes[i].parent >= 0) {
            if (out_segments != (bbvh_segment *)0 && count < max_segments) {
                seg = &out_segments[count];
                seg->parent_node = doc->nodes[i].parent;
                seg->child_node = i;
                seg->is_end_site = BBVH_FALSE;
                seg->a[0] = pose->joint_position[doc->nodes[i].parent][0];
                seg->a[1] = pose->joint_position[doc->nodes[i].parent][1];
                seg->a[2] = pose->joint_position[doc->nodes[i].parent][2];
                seg->b[0] = pose->joint_position[i][0];
                seg->b[1] = pose->joint_position[i][1];
                seg->b[2] = pose->joint_position[i][2];
            }
            count += 1;
        }
        if (include_end_sites && doc->nodes[i].has_end_site) {
            if (out_segments != (bbvh_segment *)0 && count < max_segments) {
                seg = &out_segments[count];
                seg->parent_node = i;
                seg->child_node = -1;
                seg->is_end_site = BBVH_TRUE;
                seg->a[0] = pose->joint_position[i][0];
                seg->a[1] = pose->joint_position[i][1];
                seg->a[2] = pose->joint_position[i][2];
                rc = bbvh_get_end_site_position(doc, pose, i, seg->b);
                if (rc != BBVH_OK) {
                    return rc;
                }
            }
            count += 1;
        }
    }
    *out_count = count;
    if (out_segments != (bbvh_segment *)0 && count > max_segments) {
        return BBVH_ERR_OUTPUT_FULL;
    }
    return BBVH_OK;
}

int bbvh_build_name_map(const bbvh_document *src, const bbvh_document *dst,
                        int out_src_to_dst[BRAVURE_BVH_MAX_NODES])
{
    int rc;
    int i;
    int found;

    if (src == (const bbvh_document *)0 || dst == (const bbvh_document *)0 ||
        out_src_to_dst == (int *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(src);
    if (rc != BBVH_OK) {
        return rc;
    }
    rc = bbvh_validate(dst);
    if (rc != BBVH_OK) {
        return rc;
    }

    for (i = 0; i < BRAVURE_BVH_MAX_NODES; ++i) {
        out_src_to_dst[i] = -1;
    }
    for (i = 0; i < src->node_count; ++i) {
        found = bbvh_find_node(dst, src->nodes[i].name);
        out_src_to_dst[i] = found;
    }
    return BBVH_OK;
}

static int bbvh_transfer_channel_allowed(int ctype, int copy_position_channels,
                                         int copy_rotation_channels)
{
    if (ctype == BBVH_CH_XPOSITION || ctype == BBVH_CH_YPOSITION || ctype == BBVH_CH_ZPOSITION) {
        return copy_position_channels ? BBVH_TRUE : BBVH_FALSE;
    }
    if (ctype == BBVH_CH_XROTATION || ctype == BBVH_CH_YROTATION || ctype == BBVH_CH_ZROTATION) {
        return copy_rotation_channels ? BBVH_TRUE : BBVH_FALSE;
    }
    return BBVH_FALSE;
}

int bbvh_transfer_frame_by_name(const bbvh_document *src, int src_frame,
                                bbvh_document *dst, int dst_frame,
                                int copy_position_channels,
                                int copy_rotation_channels)
{
    int rc;
    int dst_node;
    int src_node;
    int i;
    int dst_channel;
    int src_channel;
    int ctype;

    if (src == (const bbvh_document *)0 || dst == (bbvh_document *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(src);
    if (rc != BBVH_OK) {
        return rc;
    }
    rc = bbvh_validate(dst);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (src_frame < 0 || src_frame >= src->frame_count || dst_frame < 0 || dst_frame >= dst->frame_count) {
        return BBVH_ERR_BAD_ARGUMENT;
    }

    for (dst_node = 0; dst_node < dst->node_count; ++dst_node) {
        src_node = bbvh_find_node(src, dst->nodes[dst_node].name);
        if (src_node < 0) {
            continue;
        }
        for (i = 0; i < dst->nodes[dst_node].channel_count; ++i) {
            ctype = (int)dst->nodes[dst_node].channel_types[i];
            if (!bbvh_transfer_channel_allowed(ctype, copy_position_channels, copy_rotation_channels)) {
                continue;
            }
            dst_channel = dst->nodes[dst_node].channel_start + i;
            src_channel = bbvh_node_find_channel(src, src_node, ctype, 0);
            if (src_channel >= 0) {
                dst->motion[dst_frame][dst_channel] = src->motion[src_frame][src_channel];
            }
        }
    }
    return BBVH_OK;
}
