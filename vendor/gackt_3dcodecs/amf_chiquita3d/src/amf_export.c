#include "chiqui_amf.h"
#include <string.h>

typedef struct amf_out_s {
    char *dst;
    size_t cap;
    size_t pos;
} amf_out;

static int ao_ch(amf_out *o, char c)
{
    if (o->pos + 1u >= o->cap) return AMF_ERR_WRITE_FULL;
    o->dst[o->pos] = c;
    o->pos++;
    o->dst[o->pos] = '\0';
    return AMF_OK;
}

static int ao_s(amf_out *o, const char *s)
{
    size_t n;
    n = 0u;
    while (s[n] != '\0') n++;
    if (o->pos + n >= o->cap) return AMF_ERR_WRITE_FULL;
    memcpy(o->dst + o->pos, s, n);
    o->pos += n;
    o->dst[o->pos] = '\0';
    return AMF_OK;
}

static int ao_u(amf_out *o, size_t v)
{
    char tmp[32];
    size_t n;
    n = 0u;
    if (v == 0u) return ao_ch(o, '0');
    while (v > 0u && n < sizeof(tmp)) {
        tmp[n] = (char)('0' + (v % 10u));
        v /= 10u;
        n++;
    }
    while (n > 0u) {
        int r;
        n--;
        r = ao_ch(o, tmp[n]);
        if (r != AMF_OK) return r;
    }
    return AMF_OK;
}

static int ao_fx(amf_out *o, amf_fx v)
{
    char tmp[48];
    size_t n;
    int r;
    r = amf_fx_to_text(v, tmp, sizeof(tmp), &n);
    if (r != AMF_OK) return r;
    return ao_s(o, tmp);
}

static int ao_vec3(amf_out *o, amf_vec3 v)
{
    int r;
    r = ao_fx(o, v.x); if (r != AMF_OK) return r;
    r = ao_ch(o, ' '); if (r != AMF_OK) return r;
    r = ao_fx(o, v.y); if (r != AMF_OK) return r;
    r = ao_ch(o, ' '); if (r != AMF_OK) return r;
    r = ao_fx(o, v.z); if (r != AMF_OK) return r;
    return AMF_OK;
}

int amf_write_obj(const amf_document *doc, char *dst, size_t cap, size_t *out_len)
{
    amf_out o;
    size_t i;
    int r;
    if (doc == 0 || dst == 0 || cap == 0u) return AMF_ERR_BAD_ARG;
    o.dst = dst;
    o.cap = cap;
    o.pos = 0u;
    dst[0] = '\0';
    r = ao_s(&o, "# OBJ exported by chiqui_amf; material/color data remains in AMF\n");
    if (r != AMF_OK) return r;
    i = 0u;
    while (i < doc->vertex_count) {
        r = ao_s(&o, "v "); if (r != AMF_OK) return r;
        r = ao_vec3(&o, doc->vertices[i].p); if (r != AMF_OK) return r;
        r = ao_ch(&o, '\n'); if (r != AMF_OK) return r;
        i++;
    }
    i = 0u;
    while (i < doc->vertex_count) {
        if ((doc->vertices[i].flags & AMF_FLAG_HAS_NORMAL) != 0u) {
            r = ao_s(&o, "vn "); if (r != AMF_OK) return r;
            r = ao_vec3(&o, doc->vertices[i].n); if (r != AMF_OK) return r;
            r = ao_ch(&o, '\n'); if (r != AMF_OK) return r;
        }
        i++;
    }
    i = 0u;
    while (i < doc->object_count) {
        const amf_object *obj;
        size_t ti;
        obj = doc->objects + i;
        r = ao_s(&o, "o object_"); if (r != AMF_OK) return r;
        r = ao_u(&o, (size_t)obj->id); if (r != AMF_OK) return r;
        r = ao_ch(&o, '\n'); if (r != AMF_OK) return r;
        ti = obj->triangle_first;
        while (ti < obj->triangle_first + obj->triangle_count) {
            const amf_triangle *t;
            t = doc->triangles + ti;
            r = ao_s(&o, "f "); if (r != AMF_OK) return r;
            r = ao_u(&o, obj->vertex_first + (size_t)t->v1 + 1u); if (r != AMF_OK) return r;
            r = ao_ch(&o, ' '); if (r != AMF_OK) return r;
            r = ao_u(&o, obj->vertex_first + (size_t)t->v2 + 1u); if (r != AMF_OK) return r;
            r = ao_ch(&o, ' '); if (r != AMF_OK) return r;
            r = ao_u(&o, obj->vertex_first + (size_t)t->v3 + 1u); if (r != AMF_OK) return r;
            r = ao_ch(&o, '\n'); if (r != AMF_OK) return r;
            ti++;
        }
        i++;
    }
    if (out_len != 0) *out_len = o.pos;
    return AMF_OK;
}

static int amf_tri_normal(const amf_document *doc, const amf_triangle *t, amf_vec3 *normal)
{
    const amf_object *o;
    amf_vec3 a;
    amf_vec3 b;
    amf_vec3 c;
    amf_vec3 ab;
    amf_vec3 ac;
    if (doc == 0 || t == 0 || normal == 0) return AMF_ERR_BAD_ARG;
    if (t->object_index >= doc->object_count) return AMF_ERR_RANGE;
    o = doc->objects + t->object_index;
    if (t->v1 >= o->vertex_count || t->v2 >= o->vertex_count || t->v3 >= o->vertex_count) return AMF_ERR_RANGE;
    a = doc->vertices[o->vertex_first + (size_t)t->v1].p;
    b = doc->vertices[o->vertex_first + (size_t)t->v2].p;
    c = doc->vertices[o->vertex_first + (size_t)t->v3].p;
    ab = amf_vec3_sub(b, a);
    ac = amf_vec3_sub(c, a);
    *normal = amf_vec3_normalize(amf_vec3_cross(ab, ac));
    return AMF_OK;
}

static int amf_tri_vertex(const amf_document *doc, const amf_triangle *t, amf_u32 idx, amf_vec3 *out)
{
    const amf_object *o;
    if (t->object_index >= doc->object_count) return AMF_ERR_RANGE;
    o = doc->objects + t->object_index;
    if (idx >= o->vertex_count) return AMF_ERR_RANGE;
    *out = doc->vertices[o->vertex_first + (size_t)idx].p;
    return AMF_OK;
}

int amf_write_stl_ascii(const amf_document *doc, const char *solid_name, char *dst, size_t cap, size_t *out_len)
{
    amf_out o;
    size_t ti;
    int r;
    if (doc == 0 || dst == 0 || cap == 0u) return AMF_ERR_BAD_ARG;
    if (solid_name == 0) solid_name = "chiqui_amf";
    o.dst = dst;
    o.cap = cap;
    o.pos = 0u;
    dst[0] = '\0';
    r = ao_s(&o, "solid "); if (r != AMF_OK) return r;
    r = ao_s(&o, solid_name); if (r != AMF_OK) return r;
    r = ao_ch(&o, '\n'); if (r != AMF_OK) return r;
    ti = 0u;
    while (ti < doc->triangle_count) {
        const amf_triangle *t;
        amf_vec3 n;
        amf_vec3 v;
        t = doc->triangles + ti;
        r = amf_tri_normal(doc, t, &n); if (r != AMF_OK) return r;
        r = ao_s(&o, "  facet normal "); if (r != AMF_OK) return r;
        r = ao_vec3(&o, n); if (r != AMF_OK) return r;
        r = ao_s(&o, "\n    outer loop\n"); if (r != AMF_OK) return r;
        r = amf_tri_vertex(doc, t, t->v1, &v); if (r != AMF_OK) return r;
        r = ao_s(&o, "      vertex "); if (r != AMF_OK) return r;
        r = ao_vec3(&o, v); if (r != AMF_OK) return r;
        r = ao_ch(&o, '\n'); if (r != AMF_OK) return r;
        r = amf_tri_vertex(doc, t, t->v2, &v); if (r != AMF_OK) return r;
        r = ao_s(&o, "      vertex "); if (r != AMF_OK) return r;
        r = ao_vec3(&o, v); if (r != AMF_OK) return r;
        r = ao_ch(&o, '\n'); if (r != AMF_OK) return r;
        r = amf_tri_vertex(doc, t, t->v3, &v); if (r != AMF_OK) return r;
        r = ao_s(&o, "      vertex "); if (r != AMF_OK) return r;
        r = ao_vec3(&o, v); if (r != AMF_OK) return r;
        r = ao_s(&o, "\n    endloop\n  endfacet\n"); if (r != AMF_OK) return r;
        ti++;
    }
    r = ao_s(&o, "endsolid "); if (r != AMF_OK) return r;
    r = ao_s(&o, solid_name); if (r != AMF_OK) return r;
    r = ao_ch(&o, '\n'); if (r != AMF_OK) return r;
    if (out_len != 0) *out_len = o.pos;
    return AMF_OK;
}
