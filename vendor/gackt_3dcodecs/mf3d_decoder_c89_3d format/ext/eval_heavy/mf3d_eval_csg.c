#include "mf3d_eval_csg.h"

static const mf3d_object *find_obj(const mf3d_model *m, mf3d_u32 id)
{
    mf3d_u32 i;
    if (!m) return 0;
    for (i = 0; i < m->object_count; ++i) if (m->objects[i].id == id) return &m->objects[i];
    return 0;
}

int mf3d_eval_csg_object_aabb(const mf3d_model *src, mf3d_u32 objectid, mf3d_eval_aabb *out_box)
{
    const mf3d_object *o;
    mf3d_u32 i;
    if (!src || !out_box) return MF3D_E_BADARG;
    mf3d_eval_aabb_init(out_box);
    o = find_obj(src, objectid);
    if (!o) return MF3D_E_NOTFOUND;
    for (i = 0; i < o->vertex_count; ++i) {
        mf3d_u32 vi = o->vertex_first + i;
        if (vi < src->vertex_count) mf3d_eval_aabb_add(out_box, &src->vertices[vi]);
    }
    return out_box->valid ? MF3D_OK : MF3D_E_NOTFOUND;
}

static int add_aabb_box(const mf3d_model *src, mf3d_u32 objectid, mf3d_impl_program *prog, mf3d_i32 *idx)
{
    mf3d_eval_aabb b;
    mf3d_fix cx, cy, cz, sx, sy, sz;
    int r;
    r = mf3d_eval_csg_object_aabb(src, objectid, &b);
    if (r != MF3D_OK) return r;
    cx = (b.minv.x + b.maxv.x) / 2; cy = (b.minv.y + b.maxv.y) / 2; cz = (b.minv.z + b.maxv.z) / 2;
    sx = mf3d_eval_abs(b.maxv.x - b.minv.x) / 2; sy = mf3d_eval_abs(b.maxv.y - b.minv.y) / 2; sz = mf3d_eval_abs(b.maxv.z - b.minv.z) / 2;
    if (sx <= 0) sx = 32768L;
    if (sy <= 0) sy = 32768L;
    if (sz <= 0) sz = 32768L;
    return mf3d_impl_add_box(prog, cx, cy, cz, sx, sy, sz, idx);
}

static mf3d_u32 op_to_impl(mf3d_u32 op)
{
    if (op == MF3D_BOOL_INTERSECTION) return MF3D_IMPL_INTERSECTION;
    if (op == MF3D_BOOL_DIFFERENCE) return MF3D_IMPL_DIFFERENCE;
    return MF3D_IMPL_UNION;
}

int mf3d_eval_csg_boolean_shape_to_implicit(const mf3d_model *src, const mf3d_booleans *bo, const mf3d_boolean_shape *shape, mf3d_impl_program *prog, mf3d_i32 *root)
{
    mf3d_i32 acc, rhs, next;
    mf3d_u32 i;
    int r;
    if (!src || !bo || !shape || !prog) return MF3D_E_BADARG;
    mf3d_impl_init(prog);
    r = add_aabb_box(src, shape->base_objectid, prog, &acc);
    if (r != MF3D_OK) return r;
    for (i = 0; i < shape->op_count; ++i) {
        const mf3d_boolean_op *op = &bo->ops[shape->first_op + i];
        r = add_aabb_box(src, op->objectid, prog, &rhs);
        if (r != MF3D_OK) return r;
        r = mf3d_impl_add_op(prog, op_to_impl(op->operation), acc, rhs, &next);
        if (r != MF3D_OK) return r;
        acc = next;
    }
    prog->root = acc;
    if (root) *root = acc;
    return MF3D_OK;
}

int mf3d_eval_csg_polygonize_shape(const mf3d_model *src, const mf3d_booleans *bo, const mf3d_boolean_shape *shape, const mf3d_eval_csg_options *opt, mf3d_eval_mesh *out_mesh)
{
    mf3d_impl_program prog;
    mf3d_i32 root;
    mf3d_impl_polygonize_options po;
    mf3d_eval_aabb b, ob;
    mf3d_u32 i;
    mf3d_fix pad;
    int r;
    if (!src || !bo || !shape || !out_mesh) return MF3D_E_BADARG;
    r = mf3d_eval_csg_boolean_shape_to_implicit(src, bo, shape, &prog, &root);
    if (r != MF3D_OK) return r;
    r = mf3d_eval_csg_object_aabb(src, shape->base_objectid, &b);
    if (r != MF3D_OK) return r;
    for (i = 0; i < shape->op_count; ++i) {
        r = mf3d_eval_csg_object_aabb(src, bo->ops[shape->first_op + i].objectid, &ob);
        if (r == MF3D_OK) { mf3d_eval_aabb_add(&b, &ob.minv); mf3d_eval_aabb_add(&b, &ob.maxv); }
    }
    pad = opt ? opt->padding : 65536L;
    b.minv.x -= pad; b.minv.y -= pad; b.minv.z -= pad; b.maxv.x += pad; b.maxv.y += pad; b.maxv.z += pad;
    po.bounds = b;
    po.cells_x = opt ? opt->cells_x : 12UL; po.cells_y = opt ? opt->cells_y : 12UL; po.cells_z = opt ? opt->cells_z : 12UL;
    return mf3d_impl_polygonize_tets(&prog, root, &po, out_mesh);
}
