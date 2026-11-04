#include "iqm_math.h"

static iqm_fixed iqm_sat_i64_public(iqm_i64 v)
{
    if(v > (iqm_i64)IQM_FIXED_MAX) return IQM_FIXED_MAX;
    if(v < (iqm_i64)IQM_FIXED_MIN) return IQM_FIXED_MIN;
    return (iqm_fixed)v;
}

static iqm_u64 iqm_isqrt_u64(iqm_u64 x)
{
    iqm_u64 op;
    iqm_u64 res;
    iqm_u64 one;

    op = x;
    res = (iqm_u64)0;
    one = (iqm_u64)1 << 62;
    while(one > op) one >>= 2;
    while(one != (iqm_u64)0)
    {
        if(op >= res + one)
        {
            op -= res + one;
            res = (res >> 1) + one;
        }
        else
        {
            res >>= 1;
        }
        one >>= 2;
    }
    return res;
}

static iqm_fixed iqm_fixed_mul_weight_u8(iqm_fixed v, iqm_u8 weight)
{
    iqm_i64 r;
    r = (iqm_i64)v * (iqm_i64)weight;
    if(r >= (iqm_i64)0) r = (r + (iqm_i64)127) / (iqm_i64)255;
    else r = -(((-r) + (iqm_i64)127) / (iqm_i64)255);
    return iqm_sat_i64_public(r);
}

static void iqm_vec3_copy(iqm_fixed dst[3], const iqm_fixed src[3])
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static void iqm_vec3_zero(iqm_fixed dst[3])
{
    dst[0] = 0;
    dst[1] = 0;
    dst[2] = 0;
}

iqm_fixed iqm_fixed_abs(iqm_fixed v)
{
    if(v == IQM_FIXED_MIN) return IQM_FIXED_MAX;
    return v < 0 ? -v : v;
}

iqm_fixed iqm_fixed_div(iqm_fixed a, iqm_fixed b)
{
    iqm_i64 n;
    iqm_i64 q;

    if(b == 0) return a < 0 ? IQM_FIXED_MIN : IQM_FIXED_MAX;
    n = ((iqm_i64)a) << IQM_FP_SHIFT;
    q = n / (iqm_i64)b;
    return iqm_sat_i64_public(q);
}

iqm_fixed iqm_fixed_sqrt(iqm_fixed v)
{
    iqm_u64 n;
    iqm_u64 r;

    if(v <= 0) return 0;
    n = ((iqm_u64)((iqm_u32)v)) << IQM_FP_SHIFT;
    r = iqm_isqrt_u64(n);
    if(r > (iqm_u64)IQM_FIXED_MAX) return IQM_FIXED_MAX;
    return (iqm_fixed)r;
}

iqm_fixed iqm_fixed_lerp(iqm_fixed a, iqm_fixed b, iqm_fixed alpha)
{
    return a + iqm_fixed_mul((iqm_fixed)(b - a), alpha);
}

void iqm_transform_identity(iqm_transform *out_trs)
{
    if(out_trs == 0) return;
    out_trs->translate[0] = 0;
    out_trs->translate[1] = 0;
    out_trs->translate[2] = 0;
    iqm_quat_identity(out_trs->rotate);
    out_trs->scale[0] = IQM_FP_ONE;
    out_trs->scale[1] = IQM_FP_ONE;
    out_trs->scale[2] = IQM_FP_ONE;
}

void iqm_quat_identity(iqm_fixed q[4])
{
    if(q == 0) return;
    q[0] = 0;
    q[1] = 0;
    q[2] = 0;
    q[3] = IQM_FP_ONE;
}

void iqm_quat_normalize(iqm_fixed q[4])
{
    iqm_fixed len2;
    iqm_fixed len;
    iqm_u32 i;

    if(q == 0) return;
    len2 = iqm_fixed_mul(q[0], q[0]) + iqm_fixed_mul(q[1], q[1]) + iqm_fixed_mul(q[2], q[2]) + iqm_fixed_mul(q[3], q[3]);
    if(len2 <= 0)
    {
        iqm_quat_identity(q);
        return;
    }
    len = iqm_fixed_sqrt(len2);
    if(len <= 0)
    {
        iqm_quat_identity(q);
        return;
    }
    i = 0u;
    while(i < 4u)
    {
        q[i] = iqm_fixed_div(q[i], len);
        i++;
    }
}

void iqm_quat_nlerp(iqm_fixed out_q[4], const iqm_fixed a[4], const iqm_fixed b[4], iqm_fixed alpha)
{
    iqm_fixed dot;
    iqm_fixed bb[4];
    iqm_u32 i;

    if(out_q == 0 || a == 0 || b == 0) return;
    dot = iqm_fixed_mul(a[0], b[0]) + iqm_fixed_mul(a[1], b[1]) + iqm_fixed_mul(a[2], b[2]) + iqm_fixed_mul(a[3], b[3]);
    i = 0u;
    while(i < 4u)
    {
        bb[i] = dot < 0 ? (iqm_fixed)(-b[i]) : b[i];
        out_q[i] = iqm_fixed_lerp(a[i], bb[i], alpha);
        i++;
    }
    iqm_quat_normalize(out_q);
}

int iqm_get_base_joint_transform(const iqm_model *model, iqm_u32 joint_index, iqm_transform *out_trs)
{
    const iqm_joint *j;
    iqm_u32 i;

    if(model == 0 || out_trs == 0) return IQM_ERR_NULL;
    if(joint_index >= model->num_joints) return IQM_ERR_BOUNDS;
    j = &model->joints[joint_index];
    i = 0u;
    while(i < 3u)
    {
        out_trs->translate[i] = j->translate[i];
        out_trs->scale[i] = j->scale[i];
        i++;
    }
    i = 0u;
    while(i < 4u)
    {
        out_trs->rotate[i] = j->rotate[i];
        i++;
    }
    iqm_quat_normalize(out_trs->rotate);
    return IQM_OK;
}

int iqm_get_frame_pose_transform(const iqm_model *model, iqm_u32 frame_index, iqm_u32 pose_index, iqm_transform *out_trs)
{
    iqm_u32 i;
    int rc;

    if(model == 0 || out_trs == 0) return IQM_ERR_NULL;
    if(pose_index >= model->num_poses) return IQM_ERR_BOUNDS;
    if(frame_index >= model->num_frames) return IQM_ERR_BOUNDS;

    i = 0u;
    while(i < 3u)
    {
        rc = iqm_get_pose_channel(model, frame_index, pose_index, i, &out_trs->translate[i]);
        if(rc != IQM_OK) return rc;
        i++;
    }
    i = 0u;
    while(i < 4u)
    {
        rc = iqm_get_pose_channel(model, frame_index, pose_index, 3u + i, &out_trs->rotate[i]);
        if(rc != IQM_OK) return rc;
        i++;
    }
    i = 0u;
    while(i < 3u)
    {
        rc = iqm_get_pose_channel(model, frame_index, pose_index, 7u + i, &out_trs->scale[i]);
        if(rc != IQM_OK) return rc;
        i++;
    }
    iqm_quat_normalize(out_trs->rotate);
    return IQM_OK;
}

int iqm_get_frame_pose_transform_lerp(const iqm_model *model, iqm_u32 frame_a, iqm_u32 frame_b, iqm_fixed alpha, iqm_u32 pose_index, iqm_transform *out_trs)
{
    iqm_transform a;
    iqm_transform b;
    iqm_u32 i;
    int rc;

    if(out_trs == 0) return IQM_ERR_NULL;
    rc = iqm_get_frame_pose_transform(model, frame_a, pose_index, &a);
    if(rc != IQM_OK) return rc;
    rc = iqm_get_frame_pose_transform(model, frame_b, pose_index, &b);
    if(rc != IQM_OK) return rc;
    i = 0u;
    while(i < 3u)
    {
        out_trs->translate[i] = iqm_fixed_lerp(a.translate[i], b.translate[i], alpha);
        out_trs->scale[i] = iqm_fixed_lerp(a.scale[i], b.scale[i], alpha);
        i++;
    }
    iqm_quat_nlerp(out_trs->rotate, a.rotate, b.rotate, alpha);
    return IQM_OK;
}

void iqm_mat3x4_identity(iqm_mat3x4 *out_m)
{
    iqm_u32 i;
    if(out_m == 0) return;
    i = 0u;
    while(i < 12u)
    {
        out_m->m[i] = 0;
        i++;
    }
    out_m->m[0] = IQM_FP_ONE;
    out_m->m[5] = IQM_FP_ONE;
    out_m->m[10] = IQM_FP_ONE;
}

void iqm_mat3x4_from_transform(iqm_mat3x4 *out_m, const iqm_transform *trs)
{
    iqm_fixed q[4];
    iqm_fixed x;
    iqm_fixed y;
    iqm_fixed z;
    iqm_fixed w;
    iqm_fixed two;
    iqm_fixed xx;
    iqm_fixed yy;
    iqm_fixed zz;
    iqm_fixed xy;
    iqm_fixed xz;
    iqm_fixed yz;
    iqm_fixed wx;
    iqm_fixed wy;
    iqm_fixed wz;
    iqm_fixed r00;
    iqm_fixed r01;
    iqm_fixed r02;
    iqm_fixed r10;
    iqm_fixed r11;
    iqm_fixed r12;
    iqm_fixed r20;
    iqm_fixed r21;
    iqm_fixed r22;

    if(out_m == 0 || trs == 0) return;
    q[0] = trs->rotate[0];
    q[1] = trs->rotate[1];
    q[2] = trs->rotate[2];
    q[3] = trs->rotate[3];
    iqm_quat_normalize(q);

    x = q[0];
    y = q[1];
    z = q[2];
    w = q[3];
    two = (iqm_fixed)(IQM_FP_ONE * 2);

    xx = iqm_fixed_mul(x, x);
    yy = iqm_fixed_mul(y, y);
    zz = iqm_fixed_mul(z, z);
    xy = iqm_fixed_mul(x, y);
    xz = iqm_fixed_mul(x, z);
    yz = iqm_fixed_mul(y, z);
    wx = iqm_fixed_mul(w, x);
    wy = iqm_fixed_mul(w, y);
    wz = iqm_fixed_mul(w, z);

    r00 = IQM_FP_ONE - iqm_fixed_mul(two, (iqm_fixed)(yy + zz));
    r01 = iqm_fixed_mul(two, (iqm_fixed)(xy - wz));
    r02 = iqm_fixed_mul(two, (iqm_fixed)(xz + wy));
    r10 = iqm_fixed_mul(two, (iqm_fixed)(xy + wz));
    r11 = IQM_FP_ONE - iqm_fixed_mul(two, (iqm_fixed)(xx + zz));
    r12 = iqm_fixed_mul(two, (iqm_fixed)(yz - wx));
    r20 = iqm_fixed_mul(two, (iqm_fixed)(xz - wy));
    r21 = iqm_fixed_mul(two, (iqm_fixed)(yz + wx));
    r22 = IQM_FP_ONE - iqm_fixed_mul(two, (iqm_fixed)(xx + yy));

    out_m->m[0] = iqm_fixed_mul(r00, trs->scale[0]);
    out_m->m[1] = iqm_fixed_mul(r01, trs->scale[1]);
    out_m->m[2] = iqm_fixed_mul(r02, trs->scale[2]);
    out_m->m[3] = trs->translate[0];
    out_m->m[4] = iqm_fixed_mul(r10, trs->scale[0]);
    out_m->m[5] = iqm_fixed_mul(r11, trs->scale[1]);
    out_m->m[6] = iqm_fixed_mul(r12, trs->scale[2]);
    out_m->m[7] = trs->translate[1];
    out_m->m[8] = iqm_fixed_mul(r20, trs->scale[0]);
    out_m->m[9] = iqm_fixed_mul(r21, trs->scale[1]);
    out_m->m[10] = iqm_fixed_mul(r22, trs->scale[2]);
    out_m->m[11] = trs->translate[2];
}

void iqm_mat3x4_mul(iqm_mat3x4 *out_m, const iqm_mat3x4 *parent, const iqm_mat3x4 *local)
{
    iqm_mat3x4 r;
    iqm_u32 row;
    iqm_u32 col;
    iqm_u32 base;

    if(out_m == 0 || parent == 0 || local == 0) return;
    row = 0u;
    while(row < 3u)
    {
        base = row * 4u;
        col = 0u;
        while(col < 3u)
        {
            r.m[base + col] = iqm_fixed_mul(parent->m[base + 0u], local->m[col + 0u]) +
                              iqm_fixed_mul(parent->m[base + 1u], local->m[col + 4u]) +
                              iqm_fixed_mul(parent->m[base + 2u], local->m[col + 8u]);
            col++;
        }
        r.m[base + 3u] = iqm_fixed_mul(parent->m[base + 0u], local->m[3u]) +
                         iqm_fixed_mul(parent->m[base + 1u], local->m[7u]) +
                         iqm_fixed_mul(parent->m[base + 2u], local->m[11u]) +
                         parent->m[base + 3u];
        row++;
    }
    *out_m = r;
}

void iqm_mat3x4_transform_point(const iqm_mat3x4 *m, const iqm_fixed in_v[3], iqm_fixed out_v[3])
{
    iqm_fixed r[3];
    if(m == 0 || in_v == 0 || out_v == 0) return;
    r[0] = iqm_fixed_mul(m->m[0], in_v[0]) + iqm_fixed_mul(m->m[1], in_v[1]) + iqm_fixed_mul(m->m[2], in_v[2]) + m->m[3];
    r[1] = iqm_fixed_mul(m->m[4], in_v[0]) + iqm_fixed_mul(m->m[5], in_v[1]) + iqm_fixed_mul(m->m[6], in_v[2]) + m->m[7];
    r[2] = iqm_fixed_mul(m->m[8], in_v[0]) + iqm_fixed_mul(m->m[9], in_v[1]) + iqm_fixed_mul(m->m[10], in_v[2]) + m->m[11];
    out_v[0] = r[0];
    out_v[1] = r[1];
    out_v[2] = r[2];
}

void iqm_mat3x4_transform_vector(const iqm_mat3x4 *m, const iqm_fixed in_v[3], iqm_fixed out_v[3])
{
    iqm_fixed r[3];
    if(m == 0 || in_v == 0 || out_v == 0) return;
    r[0] = iqm_fixed_mul(m->m[0], in_v[0]) + iqm_fixed_mul(m->m[1], in_v[1]) + iqm_fixed_mul(m->m[2], in_v[2]);
    r[1] = iqm_fixed_mul(m->m[4], in_v[0]) + iqm_fixed_mul(m->m[5], in_v[1]) + iqm_fixed_mul(m->m[6], in_v[2]);
    r[2] = iqm_fixed_mul(m->m[8], in_v[0]) + iqm_fixed_mul(m->m[9], in_v[1]) + iqm_fixed_mul(m->m[10], in_v[2]);
    out_v[0] = r[0];
    out_v[1] = r[1];
    out_v[2] = r[2];
}

int iqm_build_base_matrices(const iqm_model *model, iqm_mat3x4 *out_mats, iqm_u32 max_mats)
{
    iqm_transform trs;
    iqm_mat3x4 local;
    iqm_u32 i;
    int rc;

    if(model == 0 || out_mats == 0) return IQM_ERR_NULL;
    if(model->num_joints > max_mats) return IQM_ERR_LIMIT;
    i = 0u;
    while(i < model->num_joints)
    {
        rc = iqm_get_base_joint_transform(model, i, &trs);
        if(rc != IQM_OK) return rc;
        iqm_mat3x4_from_transform(&local, &trs);
        if(model->joints[i].parent < 0)
        {
            out_mats[i] = local;
        }
        else
        {
            iqm_mat3x4_mul(&out_mats[i], &out_mats[(iqm_u32)model->joints[i].parent], &local);
        }
        i++;
    }
    return IQM_OK;
}

int iqm_build_frame_matrices(const iqm_model *model, iqm_u32 frame_index, iqm_mat3x4 *out_mats, iqm_u32 max_mats)
{
    iqm_transform trs;
    iqm_mat3x4 local;
    iqm_u32 i;
    int rc;

    if(model == 0 || out_mats == 0) return IQM_ERR_NULL;
    if(model->num_joints > max_mats) return IQM_ERR_LIMIT;
    if(model->num_joints == 0u) return IQM_OK;
    if(model->num_poses < model->num_joints) return iqm_build_base_matrices(model, out_mats, max_mats);
    if(frame_index >= model->num_frames) return IQM_ERR_BOUNDS;

    i = 0u;
    while(i < model->num_joints)
    {
        rc = iqm_get_frame_pose_transform(model, frame_index, i, &trs);
        if(rc != IQM_OK) return rc;
        iqm_mat3x4_from_transform(&local, &trs);
        if(model->poses[i].parent < 0)
        {
            out_mats[i] = local;
        }
        else
        {
            iqm_mat3x4_mul(&out_mats[i], &out_mats[(iqm_u32)model->poses[i].parent], &local);
        }
        i++;
    }
    return IQM_OK;
}

int iqm_build_frame_matrices_lerp(const iqm_model *model, iqm_u32 frame_a, iqm_u32 frame_b, iqm_fixed alpha, iqm_mat3x4 *out_mats, iqm_u32 max_mats)
{
    iqm_transform trs;
    iqm_mat3x4 local;
    iqm_u32 i;
    int rc;

    if(model == 0 || out_mats == 0) return IQM_ERR_NULL;
    if(model->num_joints > max_mats) return IQM_ERR_LIMIT;
    if(model->num_joints == 0u) return IQM_OK;
    if(model->num_poses < model->num_joints) return iqm_build_base_matrices(model, out_mats, max_mats);
    if(frame_a >= model->num_frames || frame_b >= model->num_frames) return IQM_ERR_BOUNDS;

    i = 0u;
    while(i < model->num_joints)
    {
        rc = iqm_get_frame_pose_transform_lerp(model, frame_a, frame_b, alpha, i, &trs);
        if(rc != IQM_OK) return rc;
        iqm_mat3x4_from_transform(&local, &trs);
        if(model->poses[i].parent < 0)
        {
            out_mats[i] = local;
        }
        else
        {
            iqm_mat3x4_mul(&out_mats[i], &out_mats[(iqm_u32)model->poses[i].parent], &local);
        }
        i++;
    }
    return IQM_OK;
}

int iqm_skin_vertex_position(const iqm_model *model, iqm_u32 vertex_index, const iqm_mat3x4 *joint_mats, iqm_u32 num_joint_mats, iqm_fixed out_pos[3])
{
    const iqm_vertex *v;
    iqm_fixed tmp[3];
    iqm_fixed acc[3];
    iqm_u32 i;
    iqm_u32 joint_index;
    iqm_u8 weight;
    iqm_u32 weight_sum;

    if(model == 0 || out_pos == 0) return IQM_ERR_NULL;
    if(vertex_index >= model->num_vertexes) return IQM_ERR_BOUNDS;
    v = &model->vertexes[vertex_index];
    if(model->has_blendindexes == 0u || model->has_blendweights == 0u || joint_mats == 0)
    {
        iqm_vec3_copy(out_pos, v->position);
        return IQM_OK;
    }
    iqm_vec3_zero(acc);
    weight_sum = 0u;
    i = 0u;
    while(i < 4u)
    {
        weight = v->blendweights[i];
        joint_index = (iqm_u32)v->blendindexes[i];
        if(weight != 0u)
        {
            if(joint_index >= num_joint_mats) return IQM_ERR_BOUNDS;
            iqm_mat3x4_transform_point(&joint_mats[joint_index], v->position, tmp);
            acc[0] += iqm_fixed_mul_weight_u8(tmp[0], weight);
            acc[1] += iqm_fixed_mul_weight_u8(tmp[1], weight);
            acc[2] += iqm_fixed_mul_weight_u8(tmp[2], weight);
            weight_sum += (iqm_u32)weight;
        }
        i++;
    }
    if(weight_sum == 0u)
    {
        iqm_vec3_copy(out_pos, v->position);
        return IQM_OK;
    }
    out_pos[0] = acc[0];
    out_pos[1] = acc[1];
    out_pos[2] = acc[2];
    return IQM_OK;
}

int iqm_skin_vertex_normal(const iqm_model *model, iqm_u32 vertex_index, const iqm_mat3x4 *joint_mats, iqm_u32 num_joint_mats, iqm_fixed out_normal[3])
{
    const iqm_vertex *v;
    iqm_fixed tmp[3];
    iqm_fixed acc[3];
    iqm_fixed len2;
    iqm_fixed len;
    iqm_u32 i;
    iqm_u32 joint_index;
    iqm_u8 weight;
    iqm_u32 weight_sum;

    if(model == 0 || out_normal == 0) return IQM_ERR_NULL;
    if(vertex_index >= model->num_vertexes) return IQM_ERR_BOUNDS;
    v = &model->vertexes[vertex_index];
    if(model->has_normal == 0u)
    {
        iqm_vec3_zero(out_normal);
        return IQM_OK;
    }
    if(model->has_blendindexes == 0u || model->has_blendweights == 0u || joint_mats == 0)
    {
        iqm_vec3_copy(out_normal, v->normal);
        return IQM_OK;
    }
    iqm_vec3_zero(acc);
    weight_sum = 0u;
    i = 0u;
    while(i < 4u)
    {
        weight = v->blendweights[i];
        joint_index = (iqm_u32)v->blendindexes[i];
        if(weight != 0u)
        {
            if(joint_index >= num_joint_mats) return IQM_ERR_BOUNDS;
            iqm_mat3x4_transform_vector(&joint_mats[joint_index], v->normal, tmp);
            acc[0] += iqm_fixed_mul_weight_u8(tmp[0], weight);
            acc[1] += iqm_fixed_mul_weight_u8(tmp[1], weight);
            acc[2] += iqm_fixed_mul_weight_u8(tmp[2], weight);
            weight_sum += (iqm_u32)weight;
        }
        i++;
    }
    if(weight_sum == 0u)
    {
        iqm_vec3_copy(out_normal, v->normal);
        return IQM_OK;
    }
    len2 = iqm_fixed_mul(acc[0], acc[0]) + iqm_fixed_mul(acc[1], acc[1]) + iqm_fixed_mul(acc[2], acc[2]);
    len = iqm_fixed_sqrt(len2);
    if(len > 0)
    {
        out_normal[0] = iqm_fixed_div(acc[0], len);
        out_normal[1] = iqm_fixed_div(acc[1], len);
        out_normal[2] = iqm_fixed_div(acc[2], len);
    }
    else
    {
        iqm_vec3_zero(out_normal);
    }
    return IQM_OK;
}
