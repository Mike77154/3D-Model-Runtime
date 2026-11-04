#include "mf3d_ext_boolean.h"
#include "mf3d_ext_xml.h"

extern int mf3d_parse_u32(const char *s, mf3d_u32 *out);

static int str_eq(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) { if (*a != *b) return 0; ++a; ++b; }
    return *a == 0 && *b == 0;
}

static mf3d_u32 parse_op(const char *s)
{
    if (str_eq(s, "difference")) return MF3D_BOOL_DIFFERENCE;
    if (str_eq(s, "intersection")) return MF3D_BOOL_INTERSECTION;
    return MF3D_BOOL_UNION;
}

static void copy_attr(char *dst, int cap, const char *src)
{
    int i;
    if (!dst || cap <= 0) return;
    dst[0] = 0;
    if (!src) return;
    for (i = 0; i < cap - 1 && src[i]; ++i) dst[i] = src[i];
    dst[i] = 0;
}

static int object_exists(const mf3d_model *m, mf3d_u32 id)
{
    mf3d_u32 i;
    if (!m) return 0;
    for (i = 0; i < m->object_count; ++i) if (m->objects[i].id == id) return 1;
    return 0;
}

void mf3d_booleans_init(mf3d_booleans *b)
{
    if (!b) return;
    b->shape_count = 0;
    b->op_count = 0;
}

int mf3d_booleans_parse_xml(char *xml, mf3d_booleans *out_bools)
{
    const char *p, *obj_end, *bs, *bs_end, *q;
    char tmp[192];
    mf3d_u32 owner;
    if (!xml || !out_bools) return MF3D_E_BADARG;
    mf3d_booleans_init(out_bools);
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "object")) != 0) {
        owner = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &owner);
        obj_end = mf3d_ext_find_close(p, "object");
        if (!obj_end) return MF3D_E_XML;
        bs = mf3d_ext_next_tag(p, "booleanshape");
        if (bs && bs < obj_end) {
            mf3d_boolean_shape *sh;
            if (out_bools->shape_count >= MF3D_EXT_MAX_BOOLEAN_SHAPES) return MF3D_E_NOSPACE;
            sh = &out_bools->shapes[out_bools->shape_count];
            sh->owner_objectid = owner;
            sh->base_objectid = 0;
            sh->base_operation = MF3D_BOOL_UNION;
            mf3d_ext_identity(sh->base_transform);
            sh->base_path[0] = 0;
            sh->first_op = out_bools->op_count;
            sh->op_count = 0;
            mf3d_ext_attr(bs, "objectid", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &sh->base_objectid);
            mf3d_ext_attr(bs, "operation", tmp, (int)sizeof(tmp)); sh->base_operation = parse_op(tmp);
            mf3d_ext_attr(bs, "transform", tmp, (int)sizeof(tmp)); mf3d_ext_parse_transform(tmp, sh->base_transform);
            mf3d_ext_attr(bs, "path", tmp, (int)sizeof(tmp)); copy_attr(sh->base_path, (int)sizeof(sh->base_path), tmp);
            bs_end = mf3d_ext_find_close(bs, "booleanshape");
            if (!bs_end) return MF3D_E_XML;
            q = bs;
            while ((q = mf3d_ext_next_tag(q, "boolean")) != 0 && q < bs_end) {
                mf3d_boolean_op *op;
                if (out_bools->op_count >= MF3D_EXT_MAX_BOOLEAN_OPS) return MF3D_E_NOSPACE;
                op = &out_bools->ops[out_bools->op_count];
                op->parent_objectid = owner;
                op->objectid = 0;
                op->operation = sh->base_operation;
                mf3d_ext_identity(op->transform);
                op->path[0] = 0;
                mf3d_ext_attr(q, "objectid", tmp, (int)sizeof(tmp)); if (tmp[0]) mf3d_parse_u32(tmp, &op->objectid);
                mf3d_ext_attr(q, "operation", tmp, (int)sizeof(tmp)); if (tmp[0]) op->operation = parse_op(tmp);
                mf3d_ext_attr(q, "transform", tmp, (int)sizeof(tmp)); mf3d_ext_parse_transform(tmp, op->transform);
                mf3d_ext_attr(q, "path", tmp, (int)sizeof(tmp)); copy_attr(op->path, (int)sizeof(op->path), tmp);
                out_bools->op_count++;
                sh->op_count++;
                q++;
            }
            out_bools->shape_count++;
        }
        p = obj_end + 1;
    }
    return MF3D_OK;
}

const mf3d_boolean_shape *mf3d_booleans_find_by_objectid(const mf3d_booleans *b, mf3d_u32 objectid)
{
    mf3d_u32 i;
    if (!b) return 0;
    for (i = 0; i < b->shape_count; ++i) if (b->shapes[i].owner_objectid == objectid) return &b->shapes[i];
    return 0;
}

int mf3d_booleans_validate_refs(const mf3d_booleans *b, const mf3d_model *m, mf3d_u32 *missing_refs)
{
    mf3d_u32 i, miss;
    if (!b || !m) return MF3D_E_BADARG;
    miss = 0;
    for (i = 0; i < b->shape_count; ++i) {
        if (b->shapes[i].base_objectid && b->shapes[i].base_path[0] == 0 && !object_exists(m, b->shapes[i].base_objectid)) miss++;
    }
    for (i = 0; i < b->op_count; ++i) {
        if (b->ops[i].objectid && b->ops[i].path[0] == 0 && !object_exists(m, b->ops[i].objectid)) miss++;
    }
    if (missing_refs) *missing_refs = miss;
    return miss ? MF3D_E_NOTFOUND : MF3D_OK;
}

const char *mf3d_booleans_operation_name(mf3d_u32 op)
{
    if (op == MF3D_BOOL_DIFFERENCE) return "difference";
    if (op == MF3D_BOOL_INTERSECTION) return "intersection";
    return "union";
}
