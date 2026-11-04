#include "mf3d_eval_implicit.h"

void mf3d_impl_init(mf3d_impl_program *p)
{
    if (!p) return;
    p->node_count = 0;
    p->root = -1;
}

static int add_node(mf3d_impl_program *p, const mf3d_impl_node *n, mf3d_i32 *idx)
{
    mf3d_u32 i;
    if (!p || !n) return MF3D_E_BADARG;
    if (p->node_count >= (mf3d_u32)MF3D_IMPL_MAX_NODES) return MF3D_E_NOSPACE;
    i = p->node_count++;
    p->nodes[i] = *n;
    p->root = (mf3d_i32)i;
    if (idx) *idx = (mf3d_i32)i;
    return MF3D_OK;
}

int mf3d_impl_add_sphere(mf3d_impl_program *p, mf3d_fix cx, mf3d_fix cy, mf3d_fix cz, mf3d_fix radius, mf3d_i32 *idx)
{
    mf3d_impl_node n;
    n.kind = MF3D_IMPL_SPHERE; n.a = -1; n.b = -1;
    n.p.x = cx; n.p.y = cy; n.p.z = cz; n.q.x = 0; n.q.y = 0; n.q.z = 0; n.r = radius;
    return add_node(p, &n, idx);
}

int mf3d_impl_add_box(mf3d_impl_program *p, mf3d_fix cx, mf3d_fix cy, mf3d_fix cz, mf3d_fix sx, mf3d_fix sy, mf3d_fix sz, mf3d_i32 *idx)
{
    mf3d_impl_node n;
    n.kind = MF3D_IMPL_BOX; n.a = -1; n.b = -1;
    n.p.x = cx; n.p.y = cy; n.p.z = cz; n.q.x = sx; n.q.y = sy; n.q.z = sz; n.r = 0;
    return add_node(p, &n, idx);
}

int mf3d_impl_add_plane(mf3d_impl_program *p, mf3d_fix nx, mf3d_fix ny, mf3d_fix nz, mf3d_fix d, mf3d_i32 *idx)
{
    mf3d_impl_node n;
    n.kind = MF3D_IMPL_PLANE; n.a = -1; n.b = -1;
    n.p.x = nx; n.p.y = ny; n.p.z = nz; mf3d_eval_vec_normalize(&n.p);
    n.q.x = 0; n.q.y = 0; n.q.z = 0; n.r = d;
    return add_node(p, &n, idx);
}

int mf3d_impl_add_op(mf3d_impl_program *p, mf3d_u32 kind, mf3d_i32 a, mf3d_i32 b, mf3d_i32 *idx)
{
    mf3d_impl_node n;
    if (kind != MF3D_IMPL_UNION && kind != MF3D_IMPL_INTERSECTION && kind != MF3D_IMPL_DIFFERENCE && kind != MF3D_IMPL_NEGATE) return MF3D_E_BADARG;
    n.kind = kind; n.a = a; n.b = b;
    n.p.x = n.p.y = n.p.z = 0; n.q.x = n.q.y = n.q.z = 0; n.r = 0;
    return add_node(p, &n, idx);
}

static mf3d_fix length3(mf3d_fix x, mf3d_fix y, mf3d_fix z)
{
    return mf3d_eval_sqrt(mf3d_eval_mul(x, x) + mf3d_eval_mul(y, y) + mf3d_eval_mul(z, z));
}

mf3d_fix mf3d_impl_eval(const mf3d_impl_program *p, mf3d_i32 node, const mf3d_vec3 *pt)
{
    const mf3d_impl_node *n;
    mf3d_fix da, db, dx, dy, dz, ax, ay, az, ox, oy, oz, outside, inside;
    if (!p || !pt || node < 0 || (mf3d_u32)node >= p->node_count) return 2147483647L;
    n = &p->nodes[node];
    if (n->kind == MF3D_IMPL_SPHERE) {
        dx = pt->x - n->p.x; dy = pt->y - n->p.y; dz = pt->z - n->p.z;
        return length3(dx, dy, dz) - n->r;
    }
    if (n->kind == MF3D_IMPL_BOX) {
        ax = mf3d_eval_abs(pt->x - n->p.x) - n->q.x;
        ay = mf3d_eval_abs(pt->y - n->p.y) - n->q.y;
        az = mf3d_eval_abs(pt->z - n->p.z) - n->q.z;
        ox = mf3d_eval_max(ax, 0); oy = mf3d_eval_max(ay, 0); oz = mf3d_eval_max(az, 0);
        outside = length3(ox, oy, oz);
        inside = mf3d_eval_min(mf3d_eval_max(ax, mf3d_eval_max(ay, az)), 0);
        return outside + inside;
    }
    if (n->kind == MF3D_IMPL_PLANE) {
        return mf3d_eval_mul(n->p.x, pt->x) + mf3d_eval_mul(n->p.y, pt->y) + mf3d_eval_mul(n->p.z, pt->z) + n->r;
    }
    da = mf3d_impl_eval(p, n->a, pt);
    if (n->kind == MF3D_IMPL_NEGATE) return -da;
    db = mf3d_impl_eval(p, n->b, pt);
    if (n->kind == MF3D_IMPL_UNION) return mf3d_eval_min(da, db);
    if (n->kind == MF3D_IMPL_INTERSECTION) return mf3d_eval_max(da, db);
    if (n->kind == MF3D_IMPL_DIFFERENCE) return mf3d_eval_max(da, -db);
    return 2147483647L;
}

static void interp_zero(const mf3d_vec3 *a, const mf3d_vec3 *b, mf3d_fix va, mf3d_fix vb, mf3d_vec3 *outp)
{
    mf3d_fix t, den;
    den = va - vb;
    if (den == 0) t = 32768L;
    else t = mf3d_eval_div(va, den);
    t = mf3d_eval_clamp(t, 0, 65536L);
    outp->x = mf3d_eval_lerp(a->x, b->x, t);
    outp->y = mf3d_eval_lerp(a->y, b->y, t);
    outp->z = mf3d_eval_lerp(a->z, b->z, t);
}

static int add_tri_pts(mf3d_eval_mesh *m, const mf3d_vec3 *a, const mf3d_vec3 *b, const mf3d_vec3 *c)
{
    mf3d_u32 ia, ib, ic;
    int r;
    r = mf3d_eval_mesh_add_vertex(m, a->x, a->y, a->z, &ia); if (r != MF3D_OK) return r;
    r = mf3d_eval_mesh_add_vertex(m, b->x, b->y, b->z, &ib); if (r != MF3D_OK) return r;
    r = mf3d_eval_mesh_add_vertex(m, c->x, c->y, c->z, &ic); if (r != MF3D_OK) return r;
    return mf3d_eval_mesh_add_triangle(m, ia, ib, ic);
}

static int polygonize_tet(mf3d_eval_mesh *m, const mf3d_vec3 p[4], const mf3d_fix val[4])
{
    int inside[4];
    int ids_in[4], ids_out[4];
    int ni, no, i;
    mf3d_vec3 q[4];
    ni = 0; no = 0;
    for (i = 0; i < 4; ++i) {
        inside[i] = (val[i] <= 0);
        if (inside[i]) ids_in[ni++] = i; else ids_out[no++] = i;
    }
    if (ni == 0 || ni == 4) return MF3D_OK;
    if (ni == 1) {
        interp_zero(&p[ids_in[0]], &p[ids_out[0]], val[ids_in[0]], val[ids_out[0]], &q[0]);
        interp_zero(&p[ids_in[0]], &p[ids_out[1]], val[ids_in[0]], val[ids_out[1]], &q[1]);
        interp_zero(&p[ids_in[0]], &p[ids_out[2]], val[ids_in[0]], val[ids_out[2]], &q[2]);
        return add_tri_pts(m, &q[0], &q[1], &q[2]);
    }
    if (ni == 3) {
        interp_zero(&p[ids_out[0]], &p[ids_in[0]], val[ids_out[0]], val[ids_in[0]], &q[0]);
        interp_zero(&p[ids_out[0]], &p[ids_in[1]], val[ids_out[0]], val[ids_in[1]], &q[1]);
        interp_zero(&p[ids_out[0]], &p[ids_in[2]], val[ids_out[0]], val[ids_in[2]], &q[2]);
        return add_tri_pts(m, &q[2], &q[1], &q[0]);
    }
    interp_zero(&p[ids_in[0]], &p[ids_out[0]], val[ids_in[0]], val[ids_out[0]], &q[0]);
    interp_zero(&p[ids_in[0]], &p[ids_out[1]], val[ids_in[0]], val[ids_out[1]], &q[1]);
    interp_zero(&p[ids_in[1]], &p[ids_out[0]], val[ids_in[1]], val[ids_out[0]], &q[2]);
    interp_zero(&p[ids_in[1]], &p[ids_out[1]], val[ids_in[1]], val[ids_out[1]], &q[3]);
    i = add_tri_pts(m, &q[0], &q[1], &q[2]); if (i != MF3D_OK) return i;
    return add_tri_pts(m, &q[1], &q[3], &q[2]);
}

int mf3d_impl_polygonize_tets(const mf3d_impl_program *p, mf3d_i32 root, const mf3d_impl_polygonize_options *opt, mf3d_eval_mesh *out_mesh)
{
    static const int tet[6][4] = {{0,5,1,6},{0,1,2,6},{0,2,3,6},{0,3,7,6},{0,7,4,6},{0,4,5,6}};
    mf3d_u32 ix, iy, iz, tx, cx, cy, cz;
    mf3d_fix sx, sy, sz;
    int r;
    if (!p || !opt || !out_mesh || !opt->bounds.valid) return MF3D_E_BADARG;
    mf3d_eval_mesh_init(out_mesh);
    cx = opt->cells_x; cy = opt->cells_y; cz = opt->cells_z;
    if (cx < 1) cx = 1;
    if (cy < 1) cy = 1;
    if (cz < 1) cz = 1;
    if (cx > 32) cx = 32;
    if (cy > 32) cy = 32;
    if (cz > 32) cz = 32;
    sx = mf3d_eval_div(opt->bounds.maxv.x - opt->bounds.minv.x, (mf3d_fix)cx);
    sy = mf3d_eval_div(opt->bounds.maxv.y - opt->bounds.minv.y, (mf3d_fix)cy);
    sz = mf3d_eval_div(opt->bounds.maxv.z - opt->bounds.minv.z, (mf3d_fix)cz);
    for (iz = 0; iz < cz; ++iz) for (iy = 0; iy < cy; ++iy) for (ix = 0; ix < cx; ++ix) {
        mf3d_vec3 v[8];
        mf3d_fix val[8];
        mf3d_fix x0, y0, z0, x1, y1, z1;
        int k;
        x0 = opt->bounds.minv.x + mf3d_eval_mul((mf3d_fix)ix, sx);
        y0 = opt->bounds.minv.y + mf3d_eval_mul((mf3d_fix)iy, sy);
        z0 = opt->bounds.minv.z + mf3d_eval_mul((mf3d_fix)iz, sz);
        x1 = x0 + sx; y1 = y0 + sy; z1 = z0 + sz;
        v[0].x=x0; v[0].y=y0; v[0].z=z0; v[1].x=x1; v[1].y=y0; v[1].z=z0;
        v[2].x=x1; v[2].y=y1; v[2].z=z0; v[3].x=x0; v[3].y=y1; v[3].z=z0;
        v[4].x=x0; v[4].y=y0; v[4].z=z1; v[5].x=x1; v[5].y=y0; v[5].z=z1;
        v[6].x=x1; v[6].y=y1; v[6].z=z1; v[7].x=x0; v[7].y=y1; v[7].z=z1;
        for (k = 0; k < 8; ++k) val[k] = mf3d_impl_eval(p, root, &v[k]);
        for (tx = 0; tx < 6; ++tx) {
            mf3d_vec3 tv[4]; mf3d_fix tval[4];
            for (k = 0; k < 4; ++k) { tv[k] = v[tet[tx][k]]; tval[k] = val[tet[tx][k]]; }
            r = polygonize_tet(out_mesh, tv, tval);
            if (r != MF3D_OK) return r;
        }
    }
    return MF3D_OK;
}
