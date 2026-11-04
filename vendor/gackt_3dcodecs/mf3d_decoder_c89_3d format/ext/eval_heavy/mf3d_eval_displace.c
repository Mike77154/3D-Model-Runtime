#include "mf3d_eval_displace.h"

static void tri_normal(const mf3d_model *m, const mf3d_tri *t, mf3d_vec3 *n)
{
    mf3d_vec3 a, b;
    const mf3d_vec3 *p0;
    p0 = &m->vertices[t->v1];
    mf3d_eval_vec_sub(&m->vertices[t->v2], p0, &a);
    mf3d_eval_vec_sub(&m->vertices[t->v3], p0, &b);
    mf3d_eval_vec_cross(&a, &b, n);
    mf3d_eval_vec_normalize(n);
}

static void bary_point(const mf3d_vec3 *a, const mf3d_vec3 *b, const mf3d_vec3 *c, mf3d_fix u, mf3d_fix v, mf3d_vec3 *outp)
{
    mf3d_fix w;
    w = 65536L - u - v;
    outp->x = mf3d_eval_mul(a->x, w) + mf3d_eval_mul(b->x, u) + mf3d_eval_mul(c->x, v);
    outp->y = mf3d_eval_mul(a->y, w) + mf3d_eval_mul(b->y, u) + mf3d_eval_mul(c->y, v);
    outp->z = mf3d_eval_mul(a->z, w) + mf3d_eval_mul(b->z, u) + mf3d_eval_mul(c->z, v);
}

static mf3d_fix sample_height(const mf3d_eval_image_u8 *heightmap, mf3d_fix u, mf3d_fix v, mf3d_fix height, mf3d_fix offset)
{
    mf3d_fix g;
    if (!heightmap) return 0;
    g = mf3d_eval_image_sample_gray16(heightmap, u, v);
    return mf3d_eval_mul(g, height) + offset;
}

static int add_displaced_vertex(mf3d_eval_mesh *out_mesh, const mf3d_vec3 *p, const mf3d_vec3 *n, mf3d_fix h, mf3d_u32 *idx)
{
    mf3d_fix x, y, z;
    x = p->x + mf3d_eval_mul(n->x, h);
    y = p->y + mf3d_eval_mul(n->y, h);
    z = p->z + mf3d_eval_mul(n->z, h);
    return mf3d_eval_mesh_add_vertex(out_mesh, x, y, z, idx);
}

int mf3d_eval_displace_mesh(const mf3d_model *src, const mf3d_displacement *disp, const mf3d_eval_image_u8 *heightmap, mf3d_eval_mesh *out_mesh, const mf3d_eval_displace_options *opt)
{
    mf3d_u32 ti, sub, i, j;
    mf3d_fix step, height, offset;
    int r;
    (void)disp;
    if (!src || !out_mesh) return MF3D_E_BADARG;
    mf3d_eval_mesh_init(out_mesh);
    sub = opt ? opt->subdivisions : 1UL;
    if (sub == 0) sub = 1;
    if (sub > 16UL) sub = 16UL;
    height = opt && opt->height ? opt->height : 65536L;
    offset = opt ? opt->offset : 0;
    step = mf3d_eval_div(65536L, (mf3d_fix)sub);
    for (ti = 0; ti < src->triangle_count; ++ti) {
        const mf3d_tri *t;
        const mf3d_vec3 *a;
        const mf3d_vec3 *b;
        const mf3d_vec3 *c;
        mf3d_vec3 n;
        t = &src->triangles[ti];
        if (t->v1 >= src->vertex_count || t->v2 >= src->vertex_count || t->v3 >= src->vertex_count) continue;
        a = &src->vertices[t->v1]; b = &src->vertices[t->v2]; c = &src->vertices[t->v3];
        tri_normal(src, t, &n);
        for (j = 0; j < sub; ++j) {
            for (i = 0; i < sub - j; ++i) {
                mf3d_fix u0, v0, u1, v1, h0, h1, h2, h3;
                mf3d_vec3 p0, p1, p2, p3;
                mf3d_u32 i0, i1, i2, i3;
                u0 = mf3d_eval_mul((mf3d_fix)i, step);
                v0 = mf3d_eval_mul((mf3d_fix)j, step);
                u1 = mf3d_eval_mul((mf3d_fix)(i + 1UL), step);
                v1 = mf3d_eval_mul((mf3d_fix)(j + 1UL), step);
                bary_point(a, b, c, u0, v0, &p0);
                bary_point(a, b, c, u1, v0, &p1);
                bary_point(a, b, c, u0, v1, &p2);
                h0 = sample_height(heightmap, u0, v0, height, offset);
                h1 = sample_height(heightmap, u1, v0, height, offset);
                h2 = sample_height(heightmap, u0, v1, height, offset);
                r = add_displaced_vertex(out_mesh, &p0, &n, h0, &i0); if (r != MF3D_OK) return r;
                r = add_displaced_vertex(out_mesh, &p1, &n, h1, &i1); if (r != MF3D_OK) return r;
                r = add_displaced_vertex(out_mesh, &p2, &n, h2, &i2); if (r != MF3D_OK) return r;
                r = mf3d_eval_mesh_add_triangle(out_mesh, i0, i1, i2); if (r != MF3D_OK) return r;
                if (i + j + 1UL < sub) {
                    bary_point(a, b, c, u1, v1, &p3);
                    h3 = sample_height(heightmap, u1, v1, height, offset);
                    r = add_displaced_vertex(out_mesh, &p3, &n, h3, &i3); if (r != MF3D_OK) return r;
                    r = mf3d_eval_mesh_add_triangle(out_mesh, i1, i3, i2); if (r != MF3D_OK) return r;
                }
            }
        }
    }
    return MF3D_OK;
}
