#include "chiqui_amf.h"
#include <string.h>

size_t amf_find_object_by_id(const amf_document *doc, amf_u32 id)
{
    size_t i;
    if (doc == 0) return AMF_ALL_OBJECTS;
    i = 0u;
    while (i < doc->object_count) {
        if (doc->objects[i].id == id) return i;
        i++;
    }
    return AMF_ALL_OBJECTS;
}

size_t amf_find_material_by_id(const amf_document *doc, amf_u32 id)
{
    size_t i;
    if (doc == 0) return AMF_ALL_OBJECTS;
    i = 0u;
    while (i < doc->material_count) {
        if (doc->materials[i].id == id) return i;
        i++;
    }
    return AMF_ALL_OBJECTS;
}

size_t amf_find_texture_by_id(const amf_document *doc, amf_u32 id)
{
    size_t i;
    if (doc == 0) return AMF_ALL_OBJECTS;
    i = 0u;
    while (i < doc->texture_count) {
        if (doc->textures[i].id == id) return i;
        i++;
    }
    return AMF_ALL_OBJECTS;
}

size_t amf_find_constellation_by_id(const amf_document *doc, amf_u32 id)
{
    size_t i;
    if (doc == 0) return AMF_ALL_OBJECTS;
    i = 0u;
    while (i < doc->constellation_count) {
        if (doc->constellations[i].id == id) return i;
        i++;
    }
    return AMF_ALL_OBJECTS;
}

amf_fx amf_fx_abs(amf_fx v)
{
    if (v < 0) return (amf_fx)(-v);
    return v;
}

amf_fx amf_fx_sqrt(amf_fx v)
{
    amf_fx x;
    amf_fx last;
    int i;
    if (v <= 0) return 0;
    if (v <= AMF_FX_ONE) x = AMF_FX_ONE;
    else x = v;
    last = 0;
    i = 0;
    while (i < 24 && x != last) {
        last = x;
        x = (amf_fx)((x + amf_fx_div(v, x)) >> 1);
        if (x <= 0) return 0;
        i++;
    }
    return x;
}

amf_vec3 amf_vec3_add(amf_vec3 a, amf_vec3 b)
{
    amf_vec3 r;
    r.x = (amf_fx)(a.x + b.x);
    r.y = (amf_fx)(a.y + b.y);
    r.z = (amf_fx)(a.z + b.z);
    return r;
}

amf_vec3 amf_vec3_sub(amf_vec3 a, amf_vec3 b)
{
    amf_vec3 r;
    r.x = (amf_fx)(a.x - b.x);
    r.y = (amf_fx)(a.y - b.y);
    r.z = (amf_fx)(a.z - b.z);
    return r;
}

amf_vec3 amf_vec3_scale(amf_vec3 v, amf_fx s)
{
    amf_vec3 r;
    r.x = amf_fx_mul(v.x, s);
    r.y = amf_fx_mul(v.y, s);
    r.z = amf_fx_mul(v.z, s);
    return r;
}

amf_fx amf_vec3_dot(amf_vec3 a, amf_vec3 b)
{
    return (amf_fx)(amf_fx_mul(a.x, b.x) + amf_fx_mul(a.y, b.y) + amf_fx_mul(a.z, b.z));
}

amf_vec3 amf_vec3_cross(amf_vec3 a, amf_vec3 b)
{
    amf_vec3 r;
    r.x = (amf_fx)(amf_fx_mul(a.y, b.z) - amf_fx_mul(a.z, b.y));
    r.y = (amf_fx)(amf_fx_mul(a.z, b.x) - amf_fx_mul(a.x, b.z));
    r.z = (amf_fx)(amf_fx_mul(a.x, b.y) - amf_fx_mul(a.y, b.x));
    return r;
}

amf_vec3 amf_vec3_normalize(amf_vec3 v)
{
    amf_fx len2;
    amf_fx len;
    amf_vec3 zero;
    zero.x = 0;
    zero.y = 0;
    zero.z = 0;
    len2 = amf_vec3_dot(v, v);
    len = amf_fx_sqrt(len2);
    if (len <= 0) return zero;
    return amf_vec3_scale(v, amf_fx_div(AMF_FX_ONE, len));
}

static void amf_stats_zero(amf_stats *s)
{
    memset(s, 0, sizeof(*s));
}

int amf_recompute_bboxes(amf_document *doc)
{
    size_t oi;
    if (doc == 0) return AMF_ERR_BAD_ARG;
    oi = 0u;
    while (oi < doc->object_count) {
        amf_object *o;
        size_t vi;
        o = doc->objects + oi;
        if (o->vertex_count > 0u) {
            amf_vertex *v0;
            v0 = doc->vertices + o->vertex_first;
            o->bbox_min = v0->p;
            o->bbox_max = v0->p;
            vi = 1u;
            while (vi < o->vertex_count) {
                amf_vertex *v;
                v = doc->vertices + o->vertex_first + vi;
                if (v->p.x < o->bbox_min.x) o->bbox_min.x = v->p.x;
                if (v->p.y < o->bbox_min.y) o->bbox_min.y = v->p.y;
                if (v->p.z < o->bbox_min.z) o->bbox_min.z = v->p.z;
                if (v->p.x > o->bbox_max.x) o->bbox_max.x = v->p.x;
                if (v->p.y > o->bbox_max.y) o->bbox_max.y = v->p.y;
                if (v->p.z > o->bbox_max.z) o->bbox_max.z = v->p.z;
                vi++;
            }
        }
        oi++;
    }
    return AMF_OK;
}

static int amf_triangle_points(const amf_document *doc, size_t ti, amf_vec3 *a, amf_vec3 *b, amf_vec3 *c)
{
    const amf_triangle *t;
    const amf_object *o;
    if (doc == 0 || ti >= doc->triangle_count || a == 0 || b == 0 || c == 0) return AMF_ERR_BAD_ARG;
    t = doc->triangles + ti;
    if (t->object_index >= doc->object_count) return AMF_ERR_RANGE;
    o = doc->objects + t->object_index;
    if (t->v1 >= o->vertex_count || t->v2 >= o->vertex_count || t->v3 >= o->vertex_count) return AMF_ERR_RANGE;
    *a = doc->vertices[o->vertex_first + (size_t)t->v1].p;
    *b = doc->vertices[o->vertex_first + (size_t)t->v2].p;
    *c = doc->vertices[o->vertex_first + (size_t)t->v3].p;
    return AMF_OK;
}

static amf_fx amf_triangle_area2_from_points(amf_vec3 a, amf_vec3 b, amf_vec3 c)
{
    amf_vec3 ab;
    amf_vec3 ac;
    amf_vec3 cr;
    amf_fx len2;
    ab = amf_vec3_sub(b, a);
    ac = amf_vec3_sub(c, a);
    cr = amf_vec3_cross(ab, ac);
    len2 = amf_vec3_dot(cr, cr);
    return amf_fx_sqrt(len2);
}

static amf_fx amf_triangle_signed_volume6(amf_vec3 a, amf_vec3 b, amf_vec3 c)
{
    amf_vec3 cr;
    cr = amf_vec3_cross(b, c);
    return amf_vec3_dot(a, cr);
}

int amf_compute_stats(const amf_document *doc, size_t object_index, amf_stats *out_stats)
{
    size_t oi;
    int any;
    amf_stats st;
    if (doc == 0 || out_stats == 0) return AMF_ERR_BAD_ARG;
    amf_stats_zero(&st);
    any = 0;
    oi = 0u;
    while (oi < doc->object_count) {
        const amf_object *o;
        size_t ti;
        if (object_index != AMF_ALL_OBJECTS && object_index != oi) {
            oi++;
            continue;
        }
        o = doc->objects + oi;
        if (!any && o->vertex_count > 0u) {
            st.bbox_min = o->bbox_min;
            st.bbox_max = o->bbox_max;
            any = 1;
        } else if (o->vertex_count > 0u) {
            if (o->bbox_min.x < st.bbox_min.x) st.bbox_min.x = o->bbox_min.x;
            if (o->bbox_min.y < st.bbox_min.y) st.bbox_min.y = o->bbox_min.y;
            if (o->bbox_min.z < st.bbox_min.z) st.bbox_min.z = o->bbox_min.z;
            if (o->bbox_max.x > st.bbox_max.x) st.bbox_max.x = o->bbox_max.x;
            if (o->bbox_max.y > st.bbox_max.y) st.bbox_max.y = o->bbox_max.y;
            if (o->bbox_max.z > st.bbox_max.z) st.bbox_max.z = o->bbox_max.z;
        }
        st.object_count++;
        st.vertex_count += o->vertex_count;
        ti = o->triangle_first;
        while (ti < o->triangle_first + o->triangle_count) {
            amf_vec3 a;
            amf_vec3 b;
            amf_vec3 c;
            amf_fx area2;
            amf_fx vol6;
            if (amf_triangle_points(doc, ti, &a, &b, &c) == AMF_OK) {
                area2 = amf_triangle_area2_from_points(a, b, c);
                if (area2 == 0) st.degenerate_triangle_count++;
                st.surface_area = (amf_fx)(st.surface_area + amf_fx_div(area2, amf_fx_from_i32(2)));
                vol6 = amf_triangle_signed_volume6(a, b, c);
                st.signed_volume = (amf_fx)(st.signed_volume + amf_fx_div(vol6, amf_fx_from_i32(6)));
                st.triangle_count++;
            }
            ti++;
        }
        oi++;
    }
    if (object_index != AMF_ALL_OBJECTS && object_index >= doc->object_count) return AMF_ERR_RANGE;
    *out_stats = st;
    return AMF_OK;
}
