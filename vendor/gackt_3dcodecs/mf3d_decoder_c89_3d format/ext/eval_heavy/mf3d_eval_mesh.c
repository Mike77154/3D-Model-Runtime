#include "mf3d_eval_mesh.h"

void mf3d_eval_mesh_init(mf3d_eval_mesh *m)
{
    if (!m) return;
    m->vertex_count = 0;
    m->triangle_count = 0;
}

int mf3d_eval_mesh_add_vertex(mf3d_eval_mesh *m, mf3d_fix x, mf3d_fix y, mf3d_fix z, mf3d_u32 *out_index)
{
    mf3d_u32 i;
    if (!m) return MF3D_E_BADARG;
    if (m->vertex_count >= (mf3d_u32)MF3D_EVAL_MAX_VERTICES) return MF3D_E_NOSPACE;
    i = m->vertex_count++;
    m->vertices[i].x = x; m->vertices[i].y = y; m->vertices[i].z = z;
    if (out_index) *out_index = i;
    return MF3D_OK;
}

int mf3d_eval_mesh_add_triangle(mf3d_eval_mesh *m, mf3d_u32 a, mf3d_u32 b, mf3d_u32 c)
{
    mf3d_u32 i;
    if (!m) return MF3D_E_BADARG;
    if (a == b || b == c || a == c) return MF3D_OK;
    if (m->triangle_count >= (mf3d_u32)MF3D_EVAL_MAX_TRIANGLES) return MF3D_E_NOSPACE;
    i = m->triangle_count++;
    m->triangles[i].v1 = a; m->triangles[i].v2 = b; m->triangles[i].v3 = c;
    m->triangles[i].pid = 0; m->triangles[i].p1 = 0; m->triangles[i].p2 = 0; m->triangles[i].p3 = 0;
    return MF3D_OK;
}

void mf3d_eval_aabb_init(mf3d_eval_aabb *b)
{
    if (!b) return;
    b->minv.x = b->minv.y = b->minv.z = 0;
    b->maxv.x = b->maxv.y = b->maxv.z = 0;
    b->valid = 0;
}

void mf3d_eval_aabb_add(mf3d_eval_aabb *b, const mf3d_vec3 *p)
{
    if (!b || !p) return;
    if (!b->valid) { b->minv = *p; b->maxv = *p; b->valid = 1; return; }
    if (p->x < b->minv.x) b->minv.x = p->x;
    if (p->y < b->minv.y) b->minv.y = p->y;
    if (p->z < b->minv.z) b->minv.z = p->z;
    if (p->x > b->maxv.x) b->maxv.x = p->x;
    if (p->y > b->maxv.y) b->maxv.y = p->y;
    if (p->z > b->maxv.z) b->maxv.z = p->z;
}

void mf3d_eval_model_aabb(const mf3d_model *m, mf3d_eval_aabb *b)
{
    mf3d_u32 i;
    mf3d_eval_aabb_init(b);
    if (!m || !b) return;
    for (i = 0; i < m->vertex_count; ++i) mf3d_eval_aabb_add(b, &m->vertices[i]);
}

mf3d_fix mf3d_eval_abs(mf3d_fix v) { return v < 0 ? -v : v; }
mf3d_fix mf3d_eval_min(mf3d_fix a, mf3d_fix b) { return a < b ? a : b; }
mf3d_fix mf3d_eval_max(mf3d_fix a, mf3d_fix b) { return a > b ? a : b; }
mf3d_fix mf3d_eval_clamp(mf3d_fix v, mf3d_fix lo, mf3d_fix hi) { if (v < lo) return lo; if (v > hi) return hi; return v; }

mf3d_fix mf3d_eval_mul(mf3d_fix a, mf3d_fix b)
{
    int neg;
    unsigned long ua, ub, ahi, alo, bhi, blo, r, term;
    neg = 0;
    if (a < 0) { ua = (unsigned long)(-a); neg = !neg; } else ua = (unsigned long)a;
    if (b < 0) { ub = (unsigned long)(-b); neg = !neg; } else ub = (unsigned long)b;
    ahi = ua >> 16; alo = ua & 65535UL;
    bhi = ub >> 16; blo = ub & 65535UL;
    if (ahi != 0 && bhi > 32767UL / ahi) return neg ? -2147483647L : 2147483647L;
    r = (ahi * bhi) << 16;
    term = ahi * blo; if (r > 2147483647UL - term) return neg ? -2147483647L : 2147483647L; r += term;
    term = bhi * alo; if (r > 2147483647UL - term) return neg ? -2147483647L : 2147483647L; r += term;
    term = (alo * blo) >> 16; if (r > 2147483647UL - term) return neg ? -2147483647L : 2147483647L; r += term;
    return neg ? -(mf3d_fix)r : (mf3d_fix)r;
}

static mf3d_fix div_abs_fixed(unsigned long ua, unsigned long ub)
{
    unsigned long q, rem, bit;
    int i;
    if (ub == 0) return 0;
    q = 0;
    rem = 0;
    for (i = 47; i >= 0; --i) {
        if (rem > 2147483647UL) return 2147483647L;
        rem <<= 1;
        if (i >= 16) bit = (ua >> (i - 16)) & 1UL;
        else bit = 0;
        rem |= bit;
        if (rem >= ub) {
            rem -= ub;
            if (i < 31) q |= (1UL << i);
            else return 2147483647L;
        }
    }
    if (q > 2147483647UL) return 2147483647L;
    return (mf3d_fix)q;
}

mf3d_fix mf3d_eval_div(mf3d_fix a, mf3d_fix b)
{
    int neg;
    unsigned long ua, ub;
    mf3d_fix r;
    if (b == 0) return 0;
    neg = 0;
    if (a < 0) { ua = (unsigned long)(-a); neg = !neg; } else ua = (unsigned long)a;
    if (b < 0) { ub = (unsigned long)(-b); neg = !neg; } else ub = (unsigned long)b;
    r = div_abs_fixed(ua, ub);
    return neg ? -r : r;
}

mf3d_fix mf3d_eval_lerp(mf3d_fix a, mf3d_fix b, mf3d_fix t)
{
    return a + mf3d_eval_mul((b - a), t);
}

mf3d_fix mf3d_eval_sqrt(mf3d_fix v)
{
    mf3d_u32 x, bit, res;
    if (v <= 0) return 0;
    x = (mf3d_u32)v;
    bit = 1UL << 30;
    res = 0;
    while (bit > x) bit >>= 2;
    while (bit != 0) {
        if (x >= res + bit) { x -= res + bit; res = (res >> 1) + bit; }
        else res >>= 1;
        bit >>= 2;
    }
    return (mf3d_fix)(res << 8); /* sqrt(16.16) rough: integer sqrt then rescale */
}

void mf3d_eval_vec_sub(const mf3d_vec3 *a, const mf3d_vec3 *b, mf3d_vec3 *r)
{
    r->x = a->x - b->x; r->y = a->y - b->y; r->z = a->z - b->z;
}

void mf3d_eval_vec_cross(const mf3d_vec3 *a, const mf3d_vec3 *b, mf3d_vec3 *r)
{
    r->x = mf3d_eval_mul(a->y, b->z) - mf3d_eval_mul(a->z, b->y);
    r->y = mf3d_eval_mul(a->z, b->x) - mf3d_eval_mul(a->x, b->z);
    r->z = mf3d_eval_mul(a->x, b->y) - mf3d_eval_mul(a->y, b->x);
}

void mf3d_eval_vec_normalize(mf3d_vec3 *v)
{
    mf3d_fix len2, len;
    if (!v) return;
    len2 = mf3d_eval_mul(v->x, v->x) + mf3d_eval_mul(v->y, v->y) + mf3d_eval_mul(v->z, v->z);
    len = mf3d_eval_sqrt(len2);
    if (len <= 0) { v->x = 0; v->y = 0; v->z = 65536L; return; }
    v->x = mf3d_eval_div(v->x, len);
    v->y = mf3d_eval_div(v->y, len);
    v->z = mf3d_eval_div(v->z, len);
}

void mf3d_eval_identity(mf3d_fix t[12])
{
    int i;
    if (!t) return;
    for (i = 0; i < 12; ++i) t[i] = 0;
    t[0] = 65536L; t[5] = 65536L; t[10] = 65536L;
}

void mf3d_eval_transform_point(const mf3d_fix t[12], const mf3d_vec3 *p, mf3d_vec3 *outp)
{
    if (!t || !p || !outp) return;
    outp->x = mf3d_eval_mul(t[0], p->x) + mf3d_eval_mul(t[1], p->y) + mf3d_eval_mul(t[2], p->z) + t[3];
    outp->y = mf3d_eval_mul(t[4], p->x) + mf3d_eval_mul(t[5], p->y) + mf3d_eval_mul(t[6], p->z) + t[7];
    outp->z = mf3d_eval_mul(t[8], p->x) + mf3d_eval_mul(t[9], p->y) + mf3d_eval_mul(t[10], p->z) + t[11];
}
