#include "chiqui_amf.h"

typedef struct amf_curve_vertex_s {
    amf_vec3 p;
    amf_vec3 n;
    amf_u32 has_n;
} amf_curve_vertex;

size_t amf_flatten_triangle_count(unsigned int level)
{
    size_t n;
    unsigned int i;
    if (level > AMF_MAX_SUBDIV_LEVEL) return 0u;
    n = 1u;
    i = 0u;
    while (i < level) {
        if (n > ((size_t)-1) / 4u) return 0u;
        n *= 4u;
        i++;
    }
    return n;
}

static int amf_get_curve_vertex(const amf_document *doc, const amf_triangle *t, amf_u32 idx, amf_curve_vertex *out)
{
    const amf_object *o;
    const amf_vertex *v;
    if (doc == 0 || t == 0 || out == 0) return AMF_ERR_BAD_ARG;
    if (t->object_index >= doc->object_count) return AMF_ERR_RANGE;
    o = doc->objects + t->object_index;
    if (idx >= o->vertex_count) return AMF_ERR_RANGE;
    v = doc->vertices + o->vertex_first + (size_t)idx;
    out->p = v->p;
    out->n = v->n;
    out->has_n = (v->flags & AMF_FLAG_HAS_NORMAL) != 0u ? 1u : 0u;
    return AMF_OK;
}

static amf_vec3 amf_mid_linear(amf_vec3 a, amf_vec3 b)
{
    amf_vec3 r;
    r.x = (amf_fx)((a.x + b.x) >> 1);
    r.y = (amf_fx)((a.y + b.y) >> 1);
    r.z = (amf_fx)((a.z + b.z) >> 1);
    return r;
}

static amf_vec3 amf_project_tangent(amf_vec3 d, amf_vec3 n)
{
    amf_fx nd;
    amf_vec3 remove;
    nd = amf_vec3_dot(n, d);
    remove = amf_vec3_scale(n, nd);
    return amf_vec3_sub(d, remove);
}

static amf_curve_vertex amf_mid_curved(amf_curve_vertex a, amf_curve_vertex b)
{
    amf_curve_vertex r;
    if (a.has_n != 0u && b.has_n != 0u) {
        amf_vec3 d;
        amf_vec3 t0;
        amf_vec3 t1;
        amf_vec3 base;
        amf_vec3 warp;
        d = amf_vec3_sub(b.p, a.p);
        t0 = amf_project_tangent(d, amf_vec3_normalize(a.n));
        t1 = amf_project_tangent(d, amf_vec3_normalize(b.n));
        base = amf_mid_linear(a.p, b.p);
        warp = amf_vec3_scale(amf_vec3_sub(t0, t1), amf_fx_div(AMF_FX_ONE, amf_fx_from_i32(8)));
        r.p = amf_vec3_add(base, warp);
        r.n = amf_vec3_normalize(amf_vec3_add(a.n, b.n));
        r.has_n = 1u;
    } else {
        r.p = amf_mid_linear(a.p, b.p);
        r.n = amf_vec3_add(a.n, b.n);
        r.has_n = 0u;
    }
    return r;
}

static int amf_emit_flat(amf_curve_vertex a, amf_curve_vertex b, amf_curve_vertex c, unsigned int level, int curved, amf_flat_triangle *out, size_t cap, size_t *count)
{
    amf_curve_vertex ab;
    amf_curve_vertex bc;
    amf_curve_vertex ca;
    int r;
    if (level == 0u) {
        if (*count >= cap) return AMF_ERR_WRITE_FULL;
        out[*count].p1 = a.p;
        out[*count].p2 = b.p;
        out[*count].p3 = c.p;
        *count = *count + 1u;
        return AMF_OK;
    }
    if (curved) {
        ab = amf_mid_curved(a, b);
        bc = amf_mid_curved(b, c);
        ca = amf_mid_curved(c, a);
    } else {
        ab.p = amf_mid_linear(a.p, b.p); ab.has_n = 0u;
        bc.p = amf_mid_linear(b.p, c.p); bc.has_n = 0u;
        ca.p = amf_mid_linear(c.p, a.p); ca.has_n = 0u;
    }
    r = amf_emit_flat(a, ab, ca, level - 1u, curved, out, cap, count);
    if (r != AMF_OK) return r;
    r = amf_emit_flat(ab, b, bc, level - 1u, curved, out, cap, count);
    if (r != AMF_OK) return r;
    r = amf_emit_flat(ca, bc, c, level - 1u, curved, out, cap, count);
    if (r != AMF_OK) return r;
    r = amf_emit_flat(ab, bc, ca, level - 1u, curved, out, cap, count);
    if (r != AMF_OK) return r;
    return AMF_OK;
}

static int amf_triangle_subdivide_common(const amf_document *doc, size_t triangle_index, unsigned int level, int curved, amf_flat_triangle *out_tris, size_t out_cap, size_t *out_count)
{
    const amf_triangle *t;
    amf_curve_vertex a;
    amf_curve_vertex b;
    amf_curve_vertex c;
    size_t needed;
    int r;
    if (doc == 0 || out_tris == 0 || out_count == 0) return AMF_ERR_BAD_ARG;
    if (triangle_index >= doc->triangle_count) return AMF_ERR_RANGE;
    if (level > AMF_MAX_SUBDIV_LEVEL) return AMF_ERR_RANGE;
    needed = amf_flatten_triangle_count(level);
    if (needed == 0u || out_cap < needed) return AMF_ERR_WRITE_FULL;
    t = doc->triangles + triangle_index;
    r = amf_get_curve_vertex(doc, t, t->v1, &a);
    if (r != AMF_OK) return r;
    r = amf_get_curve_vertex(doc, t, t->v2, &b);
    if (r != AMF_OK) return r;
    r = amf_get_curve_vertex(doc, t, t->v3, &c);
    if (r != AMF_OK) return r;
    *out_count = 0u;
    return amf_emit_flat(a, b, c, level, curved, out_tris, out_cap, out_count);
}

int amf_triangle_subdivide_linear(const amf_document *doc, size_t triangle_index, unsigned int level, amf_flat_triangle *out_tris, size_t out_cap, size_t *out_count)
{
    return amf_triangle_subdivide_common(doc, triangle_index, level, 0, out_tris, out_cap, out_count);
}

int amf_triangle_subdivide_curved(const amf_document *doc, size_t triangle_index, unsigned int level, amf_flat_triangle *out_tris, size_t out_cap, size_t *out_count)
{
    return amf_triangle_subdivide_common(doc, triangle_index, level, 1, out_tris, out_cap, out_count);
}
