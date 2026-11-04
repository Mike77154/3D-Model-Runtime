#include "bravure_bvh_internal.h"

void bbvh_quat_identity(bbvh_quat *q)
{
    if (q == (bbvh_quat *)0) {
        return;
    }
    q->x = 0L;
    q->y = 0L;
    q->z = 0L;
    q->w = BBVH_FIXED_ONE;
}

bbvh_fixed bbvh_quat_dot(const bbvh_quat *a, const bbvh_quat *b)
{
    bbvh_fixed sum;

    if (a == (const bbvh_quat *)0 || b == (const bbvh_quat *)0) {
        return 0L;
    }
    sum = bbvh_fixed_mul(a->x, b->x);
    sum = bbvh_fixed_add_sat(sum, bbvh_fixed_mul(a->y, b->y));
    sum = bbvh_fixed_add_sat(sum, bbvh_fixed_mul(a->z, b->z));
    sum = bbvh_fixed_add_sat(sum, bbvh_fixed_mul(a->w, b->w));
    return sum;
}

void bbvh_quat_normalize(bbvh_quat *q)
{
    bbvh_fixed len2;
    bbvh_fixed len;

    if (q == (bbvh_quat *)0) {
        return;
    }
    len2 = bbvh_quat_dot(q, q);
    len = bbvh_fixed_sqrt(len2);
    if (len <= 0L) {
        bbvh_quat_identity(q);
        return;
    }
    q->x = bbvh_fixed_div(q->x, len);
    q->y = bbvh_fixed_div(q->y, len);
    q->z = bbvh_fixed_div(q->z, len);
    q->w = bbvh_fixed_div(q->w, len);
}

void bbvh_quat_mul(bbvh_quat *out_q, const bbvh_quat *a, const bbvh_quat *b)
{
    bbvh_quat r;
    bbvh_fixed t0;
    bbvh_fixed t1;
    bbvh_fixed t2;
    bbvh_fixed t3;

    if (out_q == (bbvh_quat *)0 || a == (const bbvh_quat *)0 || b == (const bbvh_quat *)0) {
        return;
    }

    t0 = bbvh_fixed_mul(a->w, b->x);
    t1 = bbvh_fixed_mul(a->x, b->w);
    t2 = bbvh_fixed_mul(a->y, b->z);
    t3 = bbvh_fixed_mul(a->z, b->y);
    r.x = bbvh_fixed_add_sat(bbvh_fixed_add_sat(t0, t1), bbvh_fixed_sub_sat(t2, t3));

    t0 = bbvh_fixed_mul(a->w, b->y);
    t1 = bbvh_fixed_mul(a->x, b->z);
    t2 = bbvh_fixed_mul(a->y, b->w);
    t3 = bbvh_fixed_mul(a->z, b->x);
    r.y = bbvh_fixed_add_sat(bbvh_fixed_sub_sat(t0, t1), bbvh_fixed_add_sat(t2, t3));

    t0 = bbvh_fixed_mul(a->w, b->z);
    t1 = bbvh_fixed_mul(a->x, b->y);
    t2 = bbvh_fixed_mul(a->y, b->x);
    t3 = bbvh_fixed_mul(a->z, b->w);
    r.z = bbvh_fixed_add_sat(bbvh_fixed_sub_sat(bbvh_fixed_add_sat(t0, t1), t2), -0L);
    r.z = bbvh_fixed_add_sat(r.z, t3);

    t0 = bbvh_fixed_mul(a->w, b->w);
    t1 = bbvh_fixed_mul(a->x, b->x);
    t2 = bbvh_fixed_mul(a->y, b->y);
    t3 = bbvh_fixed_mul(a->z, b->z);
    r.w = bbvh_fixed_sub_sat(bbvh_fixed_sub_sat(bbvh_fixed_sub_sat(t0, t1), t2), t3);

    *out_q = r;
}

void bbvh_quat_from_axis_deg(bbvh_quat *q, int axis, bbvh_fixed degrees_q16)
{
    bbvh_fixed half_angle;
    bbvh_fixed s;
    bbvh_fixed c;

    if (q == (bbvh_quat *)0) {
        return;
    }
    half_angle = bbvh_fixed_div_int(degrees_q16, 2L);
    s = bbvh_fixed_sin_deg(half_angle);
    c = bbvh_fixed_cos_deg(half_angle);
    q->x = 0L;
    q->y = 0L;
    q->z = 0L;
    q->w = c;
    if (axis == 0) {
        q->x = s;
    } else if (axis == 1) {
        q->y = s;
    } else if (axis == 2) {
        q->z = s;
    }
}

void bbvh_quat_lerp_shortest(bbvh_quat *out_q, const bbvh_quat *a,
                             const bbvh_quat *b, bbvh_fixed t_q16)
{
    bbvh_quat bb;

    if (out_q == (bbvh_quat *)0 || a == (const bbvh_quat *)0 || b == (const bbvh_quat *)0) {
        return;
    }
    bb = *b;
    if (bbvh_quat_dot(a, b) < 0L) {
        bb.x = -bb.x;
        bb.y = -bb.y;
        bb.z = -bb.z;
        bb.w = -bb.w;
    }
    out_q->x = bbvh_fixed_lerp(a->x, bb.x, t_q16);
    out_q->y = bbvh_fixed_lerp(a->y, bb.y, t_q16);
    out_q->z = bbvh_fixed_lerp(a->z, bb.z, t_q16);
    out_q->w = bbvh_fixed_lerp(a->w, bb.w, t_q16);
    bbvh_quat_normalize(out_q);
}

void bbvh_quat_to_mat4(bbvh_mat4 *out_m, const bbvh_quat *q)
{
    bbvh_fixed two;
    bbvh_fixed xx;
    bbvh_fixed yy;
    bbvh_fixed zz;
    bbvh_fixed xy;
    bbvh_fixed xz;
    bbvh_fixed yz;
    bbvh_fixed wx;
    bbvh_fixed wy;
    bbvh_fixed wz;

    if (out_m == (bbvh_mat4 *)0 || q == (const bbvh_quat *)0) {
        return;
    }
    two = bbvh_fixed_from_int(2L);
    xx = bbvh_fixed_mul(q->x, q->x);
    yy = bbvh_fixed_mul(q->y, q->y);
    zz = bbvh_fixed_mul(q->z, q->z);
    xy = bbvh_fixed_mul(q->x, q->y);
    xz = bbvh_fixed_mul(q->x, q->z);
    yz = bbvh_fixed_mul(q->y, q->z);
    wx = bbvh_fixed_mul(q->w, q->x);
    wy = bbvh_fixed_mul(q->w, q->y);
    wz = bbvh_fixed_mul(q->w, q->z);

    bbvh_mat_identity(out_m);
    out_m->m[0] = bbvh_fixed_sub_sat(BBVH_FIXED_ONE, bbvh_fixed_mul(two, bbvh_fixed_add_sat(yy, zz)));
    out_m->m[1] = bbvh_fixed_mul(two, bbvh_fixed_sub_sat(xy, wz));
    out_m->m[2] = bbvh_fixed_mul(two, bbvh_fixed_add_sat(xz, wy));

    out_m->m[4] = bbvh_fixed_mul(two, bbvh_fixed_add_sat(xy, wz));
    out_m->m[5] = bbvh_fixed_sub_sat(BBVH_FIXED_ONE, bbvh_fixed_mul(two, bbvh_fixed_add_sat(xx, zz)));
    out_m->m[6] = bbvh_fixed_mul(two, bbvh_fixed_sub_sat(yz, wx));

    out_m->m[8] = bbvh_fixed_mul(two, bbvh_fixed_sub_sat(xz, wy));
    out_m->m[9] = bbvh_fixed_mul(two, bbvh_fixed_add_sat(yz, wx));
    out_m->m[10] = bbvh_fixed_sub_sat(BBVH_FIXED_ONE, bbvh_fixed_mul(two, bbvh_fixed_add_sat(xx, yy)));
}

static bbvh_fixed bbvh_quat_sample_rotation_channel(const bbvh_document *doc,
                                                    int frame_a, int frame_b,
                                                    bbvh_fixed t_q16,
                                                    int channel_index)
{
    bbvh_fixed a;
    bbvh_fixed b;

    if (frame_a < 0) {
        return 0L;
    }
    if (frame_b < 0 || frame_a == frame_b) {
        return doc->motion[frame_a][channel_index];
    }
    a = doc->motion[frame_a][channel_index];
    b = doc->motion[frame_b][channel_index];
    return bbvh_fixed_lerp(a, b, t_q16);
}

static int bbvh_quat_for_node_common(const bbvh_document *doc, int frame_a, int frame_b,
                                     bbvh_fixed t_q16, int node_index, bbvh_quat *out_q)
{
    int rc;
    const bbvh_node *node;
    int i;
    int ctype;
    int channel;
    bbvh_fixed value;
    bbvh_quat op;
    bbvh_quat tmp;

    if (doc == (const bbvh_document *)0 || out_q == (bbvh_quat *)0) {
        return BBVH_ERR_NULL;
    }
    rc = bbvh_validate(doc);
    if (rc != BBVH_OK) {
        return rc;
    }
    if (node_index < 0 || node_index >= doc->node_count) {
        return BBVH_ERR_BAD_ARGUMENT;
    }
    if (frame_a >= doc->frame_count || frame_b >= doc->frame_count) {
        return BBVH_ERR_BAD_ARGUMENT;
    }

    node = &doc->nodes[node_index];
    bbvh_quat_identity(out_q);
    for (i = 0; i < node->channel_count; ++i) {
        ctype = (int)node->channel_types[i];
        channel = node->channel_start + i;
        if (ctype == BBVH_CH_XROTATION || ctype == BBVH_CH_YROTATION || ctype == BBVH_CH_ZROTATION) {
            value = bbvh_quat_sample_rotation_channel(doc, frame_a, frame_b, t_q16, channel);
            if (ctype == BBVH_CH_XROTATION) {
                bbvh_quat_from_axis_deg(&op, 0, value);
            } else if (ctype == BBVH_CH_YROTATION) {
                bbvh_quat_from_axis_deg(&op, 1, value);
            } else {
                bbvh_quat_from_axis_deg(&op, 2, value);
            }
            bbvh_quat_mul(&tmp, out_q, &op);
            *out_q = tmp;
        }
    }
    bbvh_quat_normalize(out_q);
    return BBVH_OK;
}

int bbvh_get_node_rotation_quat(const bbvh_document *doc, int frame_index,
                                int node_index, bbvh_quat *out_q)
{
    if (frame_index < 0) {
        return bbvh_quat_for_node_common(doc, -1, -1, 0L, node_index, out_q);
    }
    return bbvh_quat_for_node_common(doc, frame_index, frame_index, 0L, node_index, out_q);
}

int bbvh_get_node_rotation_quat_lerp_frames(const bbvh_document *doc,
                                            int frame_a, int frame_b,
                                            bbvh_fixed t_q16, int node_index,
                                            bbvh_quat *out_q)
{
    if (t_q16 <= 0L) {
        return bbvh_get_node_rotation_quat(doc, frame_a, node_index, out_q);
    }
    if (t_q16 >= BBVH_FIXED_ONE) {
        return bbvh_get_node_rotation_quat(doc, frame_b, node_index, out_q);
    }
    return bbvh_quat_for_node_common(doc, frame_a, frame_b, t_q16, node_index, out_q);
}
