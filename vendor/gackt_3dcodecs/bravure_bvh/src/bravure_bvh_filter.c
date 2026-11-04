#include "bravure_bvh_internal.h"

static int bbvh_axis_mask_contains(int axes_mask, int axis)
{
    return ((axes_mask & (1 << axis)) != 0) ? BBVH_TRUE : BBVH_FALSE;
}

int bbvh_trim_motion(const bbvh_document *src, bbvh_document *dst,
                     int first_frame, int frame_count)
{
    int rc;
    int frame;
    int channel;

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
    if (first_frame < 0 || frame_count < 0 ||
        first_frame + frame_count > src->frame_count ||
        frame_count > BRAVURE_BVH_MAX_FRAMES) {
        return BBVH_ERR_BAD_ARGUMENT;
    }

    *dst = *src;
    dst->frame_count = frame_count;
    for (frame = 0; frame < frame_count; ++frame) {
        for (channel = 0; channel < src->channel_count; ++channel) {
            dst->motion[frame][channel] = src->motion[first_frame + frame][channel];
        }
    }
    return bbvh_validate(dst);
}

int bbvh_reverse_motion(const bbvh_document *src, bbvh_document *dst)
{
    int rc;
    int frame;
    int channel;
    int src_frame;

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

    *dst = *src;
    for (frame = 0; frame < src->frame_count; ++frame) {
        src_frame = src->frame_count - 1 - frame;
        for (channel = 0; channel < src->channel_count; ++channel) {
            dst->motion[frame][channel] = src->motion[src_frame][channel];
        }
    }
    return bbvh_validate(dst);
}

int bbvh_smooth_motion_box(const bbvh_document *src, bbvh_document *dst,
                           int radius, int preserve_first_last)
{
    int rc;
    int frame;
    int channel;
    int k;
    int lo;
    int hi;
    int count;
    bbvh_fixed sum;

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
    if (radius < 0) {
        return BBVH_ERR_BAD_ARGUMENT;
    }

    *dst = *src;
    if (radius == 0 || src->frame_count <= 2) {
        return bbvh_validate(dst);
    }

    for (frame = 0; frame < src->frame_count; ++frame) {
        for (channel = 0; channel < src->channel_count; ++channel) {
            if (preserve_first_last && (frame == 0 || frame == src->frame_count - 1)) {
                dst->motion[frame][channel] = src->motion[frame][channel];
                continue;
            }
            lo = frame - radius;
            hi = frame + radius;
            if (lo < 0) {
                lo = 0;
            }
            if (hi >= src->frame_count) {
                hi = src->frame_count - 1;
            }
            sum = 0L;
            count = 0;
            for (k = lo; k <= hi; ++k) {
                sum = bbvh_fixed_add_sat(sum, src->motion[k][channel]);
                count += 1;
            }
            dst->motion[frame][channel] = bbvh_fixed_div_int(sum, (long)count);
        }
    }
    return bbvh_validate(dst);
}

int bbvh_scale_document(const bbvh_document *src, bbvh_document *dst,
                        bbvh_fixed scale_q16, int scale_offsets,
                        int scale_position_channels)
{
    int rc;
    int node;
    int axis;
    int frame;
    int channel;
    int ctype;

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

    *dst = *src;
    if (scale_offsets) {
        for (node = 0; node < dst->node_count; ++node) {
            for (axis = 0; axis < 3; ++axis) {
                dst->nodes[node].offset[axis] = bbvh_fixed_mul(dst->nodes[node].offset[axis], scale_q16);
                dst->nodes[node].end_offset[axis] = bbvh_fixed_mul(dst->nodes[node].end_offset[axis], scale_q16);
            }
        }
    }
    if (scale_position_channels) {
        for (frame = 0; frame < dst->frame_count; ++frame) {
            for (channel = 0; channel < dst->channel_count; ++channel) {
                ctype = (int)dst->channel_types[channel];
                if (ctype == BBVH_CH_XPOSITION || ctype == BBVH_CH_YPOSITION || ctype == BBVH_CH_ZPOSITION) {
                    dst->motion[frame][channel] = bbvh_fixed_mul(dst->motion[frame][channel], scale_q16);
                }
            }
        }
    }
    return bbvh_validate(dst);
}

int bbvh_recenter_root_translation(bbvh_document *doc, int base_frame,
                                   int axes_mask)
{
    int rc;
    int axis;
    int channel;
    int ctype;
    int frame;
    bbvh_fixed base[3];

    if (doc == (bbvh_document *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (base_frame < 0 || base_frame >= doc->frame_count) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    if (axes_mask == 0) {
        return BBVH_OK;
    }

    base[0] = 0L;
    base[1] = 0L;
    base[2] = 0L;
    for (axis = 0; axis < 3; ++axis) {
        if (!bbvh_axis_mask_contains(axes_mask, axis)) {
            continue;
        }
        ctype = (axis == 0) ? BBVH_CH_XPOSITION : ((axis == 1) ? BBVH_CH_YPOSITION : BBVH_CH_ZPOSITION);
        channel = bbvh_node_find_channel(doc, doc->root_index, ctype, 0);
        if (channel >= 0) {
            base[axis] = doc->motion[base_frame][channel];
        }
    }

    for (frame = 0; frame < doc->frame_count; ++frame) {
        for (axis = 0; axis < 3; ++axis) {
            if (!bbvh_axis_mask_contains(axes_mask, axis)) {
                continue;
            }
            ctype = (axis == 0) ? BBVH_CH_XPOSITION : ((axis == 1) ? BBVH_CH_YPOSITION : BBVH_CH_ZPOSITION);
            channel = bbvh_node_find_channel(doc, doc->root_index, ctype, 0);
            if (channel >= 0) {
                doc->motion[frame][channel] = bbvh_fixed_sub_sat(doc->motion[frame][channel], base[axis]);
            }
        }
    }
    return bbvh_validate(doc);
}

int bbvh_close_loop_tail_blend(const bbvh_document *src, bbvh_document *dst,
                               int blend_frame_count)
{
    int rc;
    int frame;
    int channel;
    int tail_start;
    int i;
    bbvh_fixed t;
    bbvh_fixed value;

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
    if (blend_frame_count < 0 || blend_frame_count > src->frame_count) {
        return BBVH_ERR_BAD_ARGUMENT;
    }

    *dst = *src;
    if (blend_frame_count == 0 || src->frame_count <= 1) {
        return bbvh_validate(dst);
    }

    tail_start = src->frame_count - blend_frame_count;
    for (i = 0; i < blend_frame_count; ++i) {
        frame = tail_start + i;
        t = bbvh_fixed_ratio_clamped(bbvh_fixed_from_int((long)(i + 1)),
                                    bbvh_fixed_from_int((long)blend_frame_count));
        for (channel = 0; channel < src->channel_count; ++channel) {
            value = bbvh_fixed_lerp(src->motion[frame][channel], src->motion[0][channel], t);
            dst->motion[frame][channel] = value;
        }
    }
    return bbvh_validate(dst);
}
