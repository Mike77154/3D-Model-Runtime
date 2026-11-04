#include "mf3d_ext_validate.h"
#include "mf3d_ext_xml.h"

static int v_streq(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == 0 && *b == 0;
}

static void v_copy_str(char *dst, int cap, const char *src)
{
    int i;
    if (!dst || cap <= 0) return;
    if (!src) { dst[0] = 0; return; }
    for (i = 0; i < cap - 1 && src[i]; ++i) dst[i] = src[i];
    dst[i] = 0;
}

static const mf3d_object *v_find_obj(const mf3d_model *m, mf3d_u32 id)
{
    mf3d_u32 i;
    if (!m) return 0;
    for (i = 0; i < m->object_count; ++i) if (m->objects[i].id == id) return &m->objects[i];
    return 0;
}

int mf3d_ext_unit_microns_per_unit(const char *unit, mf3d_u32 *microns_per_unit)
{
    if (!microns_per_unit) return MF3D_E_BADARG;
    if (!unit || unit[0] == 0 || v_streq(unit, "millimeter")) { *microns_per_unit = 1000UL; return MF3D_OK; }
    if (v_streq(unit, "micron")) { *microns_per_unit = 1UL; return MF3D_OK; }
    if (v_streq(unit, "centimeter")) { *microns_per_unit = 10000UL; return MF3D_OK; }
    if (v_streq(unit, "inch")) { *microns_per_unit = 25400UL; return MF3D_OK; }
    if (v_streq(unit, "foot")) { *microns_per_unit = 304800UL; return MF3D_OK; }
    if (v_streq(unit, "meter")) { *microns_per_unit = 1000000UL; return MF3D_OK; }
    *microns_per_unit = 1000UL;
    return MF3D_E_UNSUPPORTED;
}

int mf3d_ext_compute_aabb(const mf3d_model *m, mf3d_vec3 *out_min, mf3d_vec3 *out_max)
{
    mf3d_u32 i;
    mf3d_vec3 mn, mx;
    if (!m || !out_min || !out_max) return MF3D_E_BADARG;
    if (m->vertex_count == 0) return MF3D_E_NOTFOUND;
    mn = m->vertices[0];
    mx = m->vertices[0];
    for (i = 1; i < m->vertex_count; ++i) {
        const mf3d_vec3 *v;
        v = &m->vertices[i];
        if (v->x < mn.x) mn.x = v->x;
        if (v->y < mn.y) mn.y = v->y;
        if (v->z < mn.z) mn.z = v->z;
        if (v->x > mx.x) mx.x = v->x;
        if (v->y > mx.y) mx.y = v->y;
        if (v->z > mx.z) mx.z = v->z;
    }
    *out_min = mn; *out_max = mx;
    return MF3D_OK;
}

int mf3d_ext_validate_model(const mf3d_model *m, mf3d_validate_report *report)
{
    mf3d_u32 i;
    if (!m || !report) return MF3D_E_BADARG;
    report->invalid_triangle_indices = 0;
    report->degenerate_triangles = 0;
    report->missing_build_objects = 0;
    report->empty_objects = 0;
    report->aabb_valid = 0;
    if (mf3d_ext_compute_aabb(m, &report->aabb_min, &report->aabb_max) == MF3D_OK) report->aabb_valid = 1;
    for (i = 0; i < m->triangle_count; ++i) {
        const mf3d_tri *t;
        t = &m->triangles[i];
        if (t->v1 >= m->vertex_count || t->v2 >= m->vertex_count || t->v3 >= m->vertex_count) report->invalid_triangle_indices++;
        if (t->v1 == t->v2 || t->v2 == t->v3 || t->v1 == t->v3) report->degenerate_triangles++;
    }
    for (i = 0; i < m->object_count; ++i) {
        if (m->objects[i].vertex_count == 0 && m->objects[i].tri_count == 0) report->empty_objects++;
    }
    for (i = 0; i < m->builditem_count; ++i) {
        if (!v_find_obj(m, m->builditems[i].objectid)) report->missing_build_objects++;
    }
    return MF3D_OK;
}

static int v_copy_object_transformed(const mf3d_model *src, const mf3d_object *obj, const mf3d_fix *t, mf3d_model *dst, mf3d_object *dobj)
{
    mf3d_u32 i, fv, ft;
    if (dst->vertex_count + obj->vertex_count > MF3D_MAX_VERTICES) return MF3D_E_NOSPACE;
    if (dst->triangle_count + obj->tri_count > MF3D_MAX_TRIANGLES) return MF3D_E_NOSPACE;
    fv = dst->vertex_count;
    ft = dst->triangle_count;
    for (i = 0; i < obj->vertex_count; ++i) {
        mf3d_ext_apply_transform(t, &src->vertices[obj->vertex_first + i], &dst->vertices[dst->vertex_count]);
        dst->vertex_count++;
    }
    for (i = 0; i < obj->tri_count; ++i) {
        mf3d_tri tr;
        tr = src->triangles[obj->tri_first + i];
        tr.v1 = fv + (tr.v1 - obj->vertex_first);
        tr.v2 = fv + (tr.v2 - obj->vertex_first);
        tr.v3 = fv + (tr.v3 - obj->vertex_first);
        dst->triangles[dst->triangle_count++] = tr;
    }
    dobj->vertex_first = fv;
    dobj->vertex_count = obj->vertex_count;
    dobj->tri_first = ft;
    dobj->tri_count = obj->tri_count;
    dobj->id = obj->id;
    v_copy_str(dobj->type, 24, obj->type);
    v_copy_str(dobj->name, 64, obj->name);
    return MF3D_OK;
}

int mf3d_ext_flatten_build(const mf3d_model *src, mf3d_model *dst)
{
    mf3d_u32 i;
    int r;
    if (!src || !dst) return MF3D_E_BADARG;
    mf3d_model_init(dst);
    v_copy_str(dst->unit, 24, src->unit);
    v_copy_str(dst->lang, 32, src->lang);
    if (src->builditem_count == 0) {
        mf3d_fix id[12];
        mf3d_ext_identity(id);
        for (i = 0; i < src->object_count; ++i) {
            if (dst->object_count >= MF3D_MAX_OBJECTS) return MF3D_E_NOSPACE;
            r = v_copy_object_transformed(src, &src->objects[i], id, dst, &dst->objects[dst->object_count]);
            if (r != MF3D_OK) return r;
            dst->object_count++;
        }
        return MF3D_OK;
    }
    for (i = 0; i < src->builditem_count; ++i) {
        const mf3d_object *obj;
        obj = v_find_obj(src, src->builditems[i].objectid);
        if (!obj) return MF3D_E_NOTFOUND;
        if (dst->object_count >= MF3D_MAX_OBJECTS) return MF3D_E_NOSPACE;
        r = v_copy_object_transformed(src, obj, src->builditems[i].transform, dst, &dst->objects[dst->object_count]);
        if (r != MF3D_OK) return r;
        dst->object_count++;
    }
    return MF3D_OK;
}
