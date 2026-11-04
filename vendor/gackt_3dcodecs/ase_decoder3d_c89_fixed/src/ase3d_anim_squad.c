#include "ase3d_anim_squad.h"

#define ASE3D_SQUAD_CLOSE 256L
#define ASE3D_SQUAD_NEG_QUARTER (-16384L)

static ase3d_fixed ase3d_squad_dot(ase3d_quat a, ase3d_quat b)
{
    return ase3d_fixed_mul(a.x, b.x) + ase3d_fixed_mul(a.y, b.y) + ase3d_fixed_mul(a.z, b.z) + ase3d_fixed_mul(a.w, b.w);
}

static ase3d_vec3 ase3d_squad_vec_add(ase3d_vec3 a, ase3d_vec3 b)
{
    ase3d_vec3 r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    r.z = a.z + b.z;
    return r;
}

static ase3d_vec3 ase3d_squad_vec_scale(ase3d_vec3 a, ase3d_fixed s)
{
    ase3d_vec3 r;
    r.x = ase3d_fixed_mul(a.x, s);
    r.y = ase3d_fixed_mul(a.y, s);
    r.z = ase3d_fixed_mul(a.z, s);
    return r;
}

static ase3d_fixed ase3d_squad_vec_len(ase3d_vec3 v)
{
    ase3d_fixed d;
    d = ase3d_fixed_mul(v.x, v.x) + ase3d_fixed_mul(v.y, v.y) + ase3d_fixed_mul(v.z, v.z);
    return ase3d_fixed_sqrt_public(d);
}

static ase3d_quat ase3d_squad_neg_if_needed(ase3d_quat ref, ase3d_quat q)
{
    if (ase3d_squad_dot(ref, q) < 0L) {
        q.x = -q.x;
        q.y = -q.y;
        q.z = -q.z;
        q.w = -q.w;
    }
    return q;
}

static ase3d_quat ase3d_squad_key_quat(const ase3d_rot_key *k)
{
    if (!k) return ase3d_quat_identity();
    if (k->has_quat) return ase3d_quat_normalize(k->quat);
    return ase3d_quat_from_axis_angle(k->axis, k->angle);
}

ase3d_quat ase3d_quat_conjugate(ase3d_quat q)
{
    q.x = -q.x;
    q.y = -q.y;
    q.z = -q.z;
    return q;
}

ase3d_quat ase3d_quat_inverse_unit(ase3d_quat q)
{
    return ase3d_quat_conjugate(ase3d_quat_normalize(q));
}

ase3d_vec3 ase3d_quat_log_vec(ase3d_quat q)
{
    ase3d_vec3 v;
    ase3d_fixed theta;
    ase3d_fixed sin_theta;
    ase3d_fixed scale;
    q = ase3d_quat_normalize(q);
    if (q.w > ASE3D_FIXED_ONE) q.w = ASE3D_FIXED_ONE;
    if (q.w < -ASE3D_FIXED_ONE) q.w = -ASE3D_FIXED_ONE;
    theta = ase3d_fixed_acos(q.w);
    sin_theta = ase3d_fixed_sin_rad(theta);
    if (sin_theta < 0L) sin_theta = -sin_theta;
    if (sin_theta <= ASE3D_SQUAD_CLOSE) {
        v.x = q.x;
        v.y = q.y;
        v.z = q.z;
        return v;
    }
    scale = ase3d_fixed_div(theta, sin_theta);
    v.x = ase3d_fixed_mul(q.x, scale);
    v.y = ase3d_fixed_mul(q.y, scale);
    v.z = ase3d_fixed_mul(q.z, scale);
    return v;
}

ase3d_quat ase3d_quat_exp_vec(ase3d_vec3 v)
{
    ase3d_quat q;
    ase3d_fixed theta;
    ase3d_fixed sin_theta;
    ase3d_fixed scale;
    theta = ase3d_squad_vec_len(v);
    if (theta <= ASE3D_SQUAD_CLOSE) {
        q.x = v.x;
        q.y = v.y;
        q.z = v.z;
        q.w = ASE3D_FIXED_ONE;
        return ase3d_quat_normalize(q);
    }
    sin_theta = ase3d_fixed_sin_rad(theta);
    scale = ase3d_fixed_div(sin_theta, theta);
    q.x = ase3d_fixed_mul(v.x, scale);
    q.y = ase3d_fixed_mul(v.y, scale);
    q.z = ase3d_fixed_mul(v.z, scale);
    q.w = ase3d_fixed_cos_rad(theta);
    return ase3d_quat_normalize(q);
}

ase3d_quat ase3d_quat_squad_control(ase3d_quat q_prev, ase3d_quat q_cur, ase3d_quat q_next)
{
    ase3d_quat inv;
    ase3d_quat a;
    ase3d_quat b;
    ase3d_vec3 la;
    ase3d_vec3 lb;
    ase3d_vec3 sum;
    ase3d_quat e;
    q_cur = ase3d_quat_normalize(q_cur);
    q_prev = ase3d_squad_neg_if_needed(q_cur, ase3d_quat_normalize(q_prev));
    q_next = ase3d_squad_neg_if_needed(q_cur, ase3d_quat_normalize(q_next));
    inv = ase3d_quat_inverse_unit(q_cur);
    a = ase3d_quat_mul(inv, q_prev);
    b = ase3d_quat_mul(inv, q_next);
    la = ase3d_quat_log_vec(a);
    lb = ase3d_quat_log_vec(b);
    sum = ase3d_squad_vec_add(la, lb);
    sum = ase3d_squad_vec_scale(sum, ASE3D_SQUAD_NEG_QUARTER);
    e = ase3d_quat_exp_vec(sum);
    return ase3d_quat_mul(q_cur, e);
}

ase3d_quat ase3d_quat_squad(ase3d_quat q0, ase3d_quat q1, ase3d_quat s0, ase3d_quat s1, ase3d_fixed t)
{
    ase3d_quat a;
    ase3d_quat b;
    ase3d_fixed one_minus_t;
    ase3d_fixed k;
    q0 = ase3d_quat_normalize(q0);
    q1 = ase3d_squad_neg_if_needed(q0, ase3d_quat_normalize(q1));
    s0 = ase3d_squad_neg_if_needed(q0, ase3d_quat_normalize(s0));
    s1 = ase3d_squad_neg_if_needed(q1, ase3d_quat_normalize(s1));
    a = ase3d_quat_slerp(q0, q1, t);
    b = ase3d_quat_slerp(s0, s1, t);
    one_minus_t = ASE3D_FIXED_ONE - t;
    k = ase3d_fixed_mul((2L << ASE3D_FIXED_SHIFT), ase3d_fixed_mul(t, one_minus_t));
    return ase3d_quat_slerp(a, b, k);
}

static int ase3d_squad_find_prev_next(const ase3d_model *m, int object_id, long tick, int *prev_i, int *next_i)
{
    unsigned int i;
    int prev;
    int next;
    prev = -1;
    next = -1;
    if (!m || !m->rot_keys) return 0;
    for (i = 0U; i < m->rot_key_count; ++i) {
        if (m->rot_keys[i].object_id != object_id) continue;
        if (m->rot_keys[i].tick <= tick) {
            if (prev < 0 || m->rot_keys[i].tick > m->rot_keys[prev].tick) prev = (int)i;
        }
        if (m->rot_keys[i].tick >= tick) {
            if (next < 0 || m->rot_keys[i].tick < m->rot_keys[next].tick) next = (int)i;
        }
    }
    if (prev < 0 && next < 0) return 0;
    if (prev < 0) prev = next;
    if (next < 0) next = prev;
    *prev_i = prev;
    *next_i = next;
    return 1;
}

static int ase3d_squad_find_before(const ase3d_model *m, int object_id, long tick)
{
    unsigned int i;
    int best;
    best = -1;
    for (i = 0U; i < m->rot_key_count; ++i) {
        if (m->rot_keys[i].object_id != object_id) continue;
        if (m->rot_keys[i].tick < tick) {
            if (best < 0 || m->rot_keys[i].tick > m->rot_keys[best].tick) best = (int)i;
        }
    }
    return best;
}

static int ase3d_squad_find_after(const ase3d_model *m, int object_id, long tick)
{
    unsigned int i;
    int best;
    best = -1;
    for (i = 0U; i < m->rot_key_count; ++i) {
        if (m->rot_keys[i].object_id != object_id) continue;
        if (m->rot_keys[i].tick > tick) {
            if (best < 0 || m->rot_keys[i].tick < m->rot_keys[best].tick) best = (int)i;
        }
    }
    return best;
}

int ase3d_build_squad_basis_for_tick(const ase3d_model *m, int object_id, long tick, int endpoint_mode, ase3d_squad_basis *out)
{
    int i0;
    int i1;
    int im;
    int ip;
    ase3d_quat qm;
    ase3d_quat q0;
    ase3d_quat q1;
    ase3d_quat qp;
    (void)endpoint_mode;
    if (!out) return 0;
    out->valid = 0;
    if (!m || !m->rot_keys) return 0;
    if (!ase3d_squad_find_prev_next(m, object_id, tick, &i0, &i1)) return 0;
    im = ase3d_squad_find_before(m, object_id, m->rot_keys[i0].tick);
    ip = ase3d_squad_find_after(m, object_id, m->rot_keys[i1].tick);
    q0 = ase3d_squad_key_quat(&m->rot_keys[i0]);
    q1 = ase3d_squad_key_quat(&m->rot_keys[i1]);
    if (im >= 0) qm = ase3d_squad_key_quat(&m->rot_keys[im]);
    else qm = q0;
    if (ip >= 0) qp = ase3d_squad_key_quat(&m->rot_keys[ip]);
    else qp = q1;
    q1 = ase3d_squad_neg_if_needed(q0, q1);
    qm = ase3d_squad_neg_if_needed(q0, qm);
    qp = ase3d_squad_neg_if_needed(q1, qp);
    out->q0 = q0;
    out->q1 = q1;
    out->s0 = ase3d_quat_squad_control(qm, q0, q1);
    out->s1 = ase3d_quat_squad_control(q0, q1, qp);
    out->tick0 = m->rot_keys[i0].tick;
    out->tick1 = m->rot_keys[i1].tick;
    out->valid = 1;
    return 1;
}

int ase3d_sample_rotation_squad(const ase3d_model *m, int object_id, long tick, ase3d_quat *out)
{
    ase3d_squad_basis b;
    ase3d_fixed t;
    if (!out) return 0;
    if (!ase3d_build_squad_basis_for_tick(m, object_id, tick, ASE3D_SQUAD_ENDPOINT_CLAMP, &b)) return 0;
    if (b.tick0 == b.tick1) {
        *out = b.q0;
        return 1;
    }
    t = ase3d_fixed_div((ase3d_fixed)(tick - b.tick0), (ase3d_fixed)(b.tick1 - b.tick0));
    if (t < 0L) t = 0L;
    if (t > ASE3D_FIXED_ONE) t = ASE3D_FIXED_ONE;
    *out = ase3d_quat_squad(b.q0, b.q1, b.s0, b.s1, t);
    return 1;
}

static ase3d_vec3 ase3d_squad_transform_point(ase3d_mat43 m, ase3d_vec3 v)
{
    return ase3d_transform_point(m, v);
}

static ase3d_mat43 ase3d_squad_mat43_mul(ase3d_mat43 a, ase3d_mat43 b)
{
    ase3d_mat43 r;
    r.row0.x = ase3d_fixed_mul(a.row0.x, b.row0.x) + ase3d_fixed_mul(a.row0.y, b.row1.x) + ase3d_fixed_mul(a.row0.z, b.row2.x);
    r.row0.y = ase3d_fixed_mul(a.row0.x, b.row0.y) + ase3d_fixed_mul(a.row0.y, b.row1.y) + ase3d_fixed_mul(a.row0.z, b.row2.y);
    r.row0.z = ase3d_fixed_mul(a.row0.x, b.row0.z) + ase3d_fixed_mul(a.row0.y, b.row1.z) + ase3d_fixed_mul(a.row0.z, b.row2.z);
    r.row1.x = ase3d_fixed_mul(a.row1.x, b.row0.x) + ase3d_fixed_mul(a.row1.y, b.row1.x) + ase3d_fixed_mul(a.row1.z, b.row2.x);
    r.row1.y = ase3d_fixed_mul(a.row1.x, b.row0.y) + ase3d_fixed_mul(a.row1.y, b.row1.y) + ase3d_fixed_mul(a.row1.z, b.row2.y);
    r.row1.z = ase3d_fixed_mul(a.row1.x, b.row0.z) + ase3d_fixed_mul(a.row1.y, b.row1.z) + ase3d_fixed_mul(a.row1.z, b.row2.z);
    r.row2.x = ase3d_fixed_mul(a.row2.x, b.row0.x) + ase3d_fixed_mul(a.row2.y, b.row1.x) + ase3d_fixed_mul(a.row2.z, b.row2.x);
    r.row2.y = ase3d_fixed_mul(a.row2.x, b.row0.y) + ase3d_fixed_mul(a.row2.y, b.row1.y) + ase3d_fixed_mul(a.row2.z, b.row2.y);
    r.row2.z = ase3d_fixed_mul(a.row2.x, b.row0.z) + ase3d_fixed_mul(a.row2.y, b.row1.z) + ase3d_fixed_mul(a.row2.z, b.row2.z);
    r.row3.x = ase3d_fixed_mul(a.row3.x, b.row0.x) + ase3d_fixed_mul(a.row3.y, b.row1.x) + ase3d_fixed_mul(a.row3.z, b.row2.x) + b.row3.x;
    r.row3.y = ase3d_fixed_mul(a.row3.x, b.row0.y) + ase3d_fixed_mul(a.row3.y, b.row1.y) + ase3d_fixed_mul(a.row3.z, b.row2.y) + b.row3.y;
    r.row3.z = ase3d_fixed_mul(a.row3.x, b.row0.z) + ase3d_fixed_mul(a.row3.y, b.row1.z) + ase3d_fixed_mul(a.row3.z, b.row2.z) + b.row3.z;
    return r;
}

static ase3d_mat43 ase3d_squad_mat43_identity(void)
{
    ase3d_mat43 m;
    m.row0.x = ASE3D_FIXED_ONE; m.row0.y = 0L; m.row0.z = 0L;
    m.row1.x = 0L; m.row1.y = ASE3D_FIXED_ONE; m.row1.z = 0L;
    m.row2.x = 0L; m.row2.y = 0L; m.row2.z = ASE3D_FIXED_ONE;
    m.row3.x = 0L; m.row3.y = 0L; m.row3.z = 0L;
    return m;
}

static int ase3d_squad_sample_local_matrix(const ase3d_model *m, int object_id, long tick, ase3d_mat43 *out)
{
    ase3d_vec3 pos;
    ase3d_vec3 scl;
    ase3d_quat q;
    ase3d_mat43 mat;
    const ase3d_object *obj;
    if (!m || !out || !m->objects || object_id < 0 || (unsigned int)object_id >= m->object_count) return 0;
    obj = &m->objects[object_id];
    pos = obj->tm.has_pos ? obj->tm.pos : obj->tm.row3;
    scl = obj->tm.scale;
    if (!ase3d_sample_position(m, object_id, tick, &pos)) {
        if (obj->tm.has_row3) pos = obj->tm.row3;
    }
    if (!ase3d_sample_scale(m, object_id, tick, &scl)) {
        if (!obj->tm.has_scale) {
            scl.x = ASE3D_FIXED_ONE;
            scl.y = ASE3D_FIXED_ONE;
            scl.z = ASE3D_FIXED_ONE;
        }
    }
    if (!ase3d_sample_rotation_squad(m, object_id, tick, &q)) {
        if (!ase3d_sample_rotation_quat(m, object_id, tick, &q)) {
            q = obj->tm.has_rot ? ase3d_quat_from_axis_angle(obj->tm.rot_axis, obj->tm.rot_angle) : ase3d_quat_identity();
        }
    }
    mat = ase3d_quat_to_mat43(q);
    mat.row0.x = ase3d_fixed_mul(mat.row0.x, scl.x); mat.row0.y = ase3d_fixed_mul(mat.row0.y, scl.x); mat.row0.z = ase3d_fixed_mul(mat.row0.z, scl.x);
    mat.row1.x = ase3d_fixed_mul(mat.row1.x, scl.y); mat.row1.y = ase3d_fixed_mul(mat.row1.y, scl.y); mat.row1.z = ase3d_fixed_mul(mat.row1.z, scl.y);
    mat.row2.x = ase3d_fixed_mul(mat.row2.x, scl.z); mat.row2.y = ase3d_fixed_mul(mat.row2.y, scl.z); mat.row2.z = ase3d_fixed_mul(mat.row2.z, scl.z);
    mat.row3 = pos;
    *out = mat;
    return 1;
}

int ase3d_sample_world_matrix_squad(const ase3d_model *m, int object_id, long tick, ase3d_mat43 *out)
{
    int chain[ASE3D_STACK_MAX];
    int count;
    int cur;
    int i;
    ase3d_mat43 local;
    ase3d_mat43 acc;
    (void)ase3d_squad_transform_point;
    if (!m || !out || object_id < 0) return 0;
    count = 0;
    cur = object_id;
    while (cur >= 0 && count < ASE3D_STACK_MAX) {
        chain[count++] = cur;
        cur = ase3d_find_parent_index(m, cur);
    }
    if (count <= 0) return 0;
    acc = ase3d_squad_mat43_identity();
    for (i = count - 1; i >= 0; --i) {
        if (!ase3d_squad_sample_local_matrix(m, chain[i], tick, &local)) local = ase3d_squad_mat43_identity();
        acc = ase3d_squad_mat43_mul(acc, local);
    }
    *out = acc;
    return 1;
}
