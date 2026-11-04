#include "mf3d_ext_components.h"
#include "mf3d_ext_xml.h"

extern int mf3d_parse_u32(const char *s, mf3d_u32 *out);

static const mf3d_object *find_obj(const mf3d_model *m, mf3d_u32 id)
{
    mf3d_u32 i;
    if (!m) return 0;
    for (i = 0; i < m->object_count; ++i) if (m->objects[i].id == id) return &m->objects[i];
    return 0;
}

void mf3d_components_init(mf3d_components *c)
{
    if (!c) return;
    c->count = 0;
}

int mf3d_components_parse_xml(char *xml, mf3d_components *out_components)
{
    const char *p, *end, *q;
    char tmp[160];
    mf3d_u32 parent_id;
    if (!xml || !out_components) return MF3D_E_BADARG;
    mf3d_components_init(out_components);
    p = xml;
    while ((p = mf3d_ext_next_tag(p, "object")) != 0) {
        parent_id = 0;
        mf3d_ext_attr(p, "id", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &parent_id);
        end = mf3d_ext_find_close(p, "object");
        if (!end) return MF3D_E_XML;
        q = p;
        while ((q = mf3d_ext_next_tag(q, "component")) != 0 && q < end) {
            mf3d_component *c;
            if (out_components->count >= MF3D_EXT_MAX_COMPONENTS) return MF3D_E_NOSPACE;
            c = &out_components->components[out_components->count];
            c->parent_objectid = parent_id;
            c->objectid = 0;
            mf3d_ext_identity(c->transform);
            mf3d_ext_attr(q, "objectid", tmp, (int)sizeof(tmp)); mf3d_parse_u32(tmp, &c->objectid);
            mf3d_ext_attr(q, "transform", tmp, (int)sizeof(tmp)); mf3d_ext_parse_transform(tmp, c->transform);
            out_components->count++;
            q++;
        }
        p = end + 1;
    }
    return MF3D_OK;
}

static int copy_object_mesh_transformed(const mf3d_model *src, const mf3d_object *obj, const mf3d_fix *t, mf3d_model *dst, mf3d_object *dst_obj)
{
    mf3d_u32 i, first_v, first_t;
    if (!src || !obj || !t || !dst || !dst_obj) return MF3D_E_BADARG;
    if (dst->vertex_count + obj->vertex_count > MF3D_MAX_VERTICES) return MF3D_E_NOSPACE;
    if (dst->triangle_count + obj->tri_count > MF3D_MAX_TRIANGLES) return MF3D_E_NOSPACE;
    first_v = dst->vertex_count;
    first_t = dst->triangle_count;
    for (i = 0; i < obj->vertex_count; ++i) {
        mf3d_ext_apply_transform(t, &src->vertices[obj->vertex_first + i], &dst->vertices[dst->vertex_count]);
        dst->vertex_count++;
    }
    for (i = 0; i < obj->tri_count; ++i) {
        mf3d_tri tr;
        tr = src->triangles[obj->tri_first + i];
        tr.v1 = first_v + (tr.v1 - obj->vertex_first);
        tr.v2 = first_v + (tr.v2 - obj->vertex_first);
        tr.v3 = first_v + (tr.v3 - obj->vertex_first);
        dst->triangles[dst->triangle_count++] = tr;
    }
    dst_obj->vertex_count += obj->vertex_count;
    dst_obj->tri_count += obj->tri_count;
    if (dst_obj->vertex_count == obj->vertex_count) dst_obj->vertex_first = first_v;
    if (dst_obj->tri_count == obj->tri_count) dst_obj->tri_first = first_t;
    return MF3D_OK;
}

int mf3d_components_flatten(const mf3d_model *src_model, const mf3d_components *components, mf3d_model *out_model)
{
    mf3d_u32 oi, ci;
    int has_comp, r;
    if (!src_model || !components || !out_model) return MF3D_E_BADARG;
    mf3d_model_init(out_model);
    out_model->unit[0] = 0;
    out_model->lang[0] = 0;
    {
        int i;
        for (i = 0; i < 23 && src_model->unit[i]; ++i) out_model->unit[i] = src_model->unit[i];
        out_model->unit[i] = 0;
        for (i = 0; i < 31 && src_model->lang[i]; ++i) out_model->lang[i] = src_model->lang[i];
        out_model->lang[i] = 0;
    }
    for (oi = 0; oi < src_model->object_count; ++oi) {
        const mf3d_object *src_obj;
        mf3d_object *dst_obj;
        if (out_model->object_count >= MF3D_MAX_OBJECTS) return MF3D_E_NOSPACE;
        src_obj = &src_model->objects[oi];
        dst_obj = &out_model->objects[out_model->object_count];
        *dst_obj = *src_obj;
        dst_obj->vertex_first = out_model->vertex_count;
        dst_obj->tri_first = out_model->triangle_count;
        dst_obj->vertex_count = 0;
        dst_obj->tri_count = 0;
        has_comp = 0;
        for (ci = 0; ci < components->count; ++ci) {
            if (components->components[ci].parent_objectid == src_obj->id) {
                const mf3d_object *child;
                child = find_obj(src_model, components->components[ci].objectid);
                if (!child) return MF3D_E_NOTFOUND;
                r = copy_object_mesh_transformed(src_model, child, components->components[ci].transform, out_model, dst_obj);
                if (r != MF3D_OK) return r;
                has_comp = 1;
            }
        }
        if (!has_comp) {
            mf3d_fix id[12];
            mf3d_ext_identity(id);
            r = copy_object_mesh_transformed(src_model, src_obj, id, out_model, dst_obj);
            if (r != MF3D_OK) return r;
        }
        out_model->object_count++;
    }
    return MF3D_OK;
}
