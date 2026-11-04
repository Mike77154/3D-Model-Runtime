#include "chiqui_amf.h"
#include <string.h>

typedef struct amf_edge_tmp_s {
    size_t object_index;
    amf_u32 a;
    amf_u32 b;
    size_t uses;
} amf_edge_tmp;

static void amf_report_zero(amf_topology_report *r)
{
    memset(r, 0, sizeof(*r));
}

static int amf_id_dup_objects(const amf_document *doc, size_t *out_count)
{
    size_t i;
    size_t j;
    size_t c;
    c = 0u;
    i = 0u;
    while (i < doc->object_count) {
        j = i + 1u;
        while (j < doc->object_count) {
            if (doc->objects[i].id == doc->objects[j].id) c++;
            j++;
        }
        i++;
    }
    *out_count += c;
    return c != 0u;
}

static int amf_id_dup_materials(const amf_document *doc, size_t *out_count)
{
    size_t i;
    size_t j;
    size_t c;
    c = 0u;
    i = 0u;
    while (i < doc->material_count) {
        j = i + 1u;
        while (j < doc->material_count) {
            if (doc->materials[i].id == doc->materials[j].id) c++;
            j++;
        }
        i++;
    }
    *out_count += c;
    return c != 0u;
}

static int amf_id_dup_textures(const amf_document *doc, size_t *out_count)
{
    size_t i;
    size_t j;
    size_t c;
    c = 0u;
    i = 0u;
    while (i < doc->texture_count) {
        j = i + 1u;
        while (j < doc->texture_count) {
            if (doc->textures[i].id == doc->textures[j].id) c++;
            j++;
        }
        i++;
    }
    *out_count += c;
    return c != 0u;
}

static int amf_id_dup_constellations(const amf_document *doc, size_t *out_count)
{
    size_t i;
    size_t j;
    size_t c;
    c = 0u;
    i = 0u;
    while (i < doc->constellation_count) {
        j = i + 1u;
        while (j < doc->constellation_count) {
            if (doc->constellations[i].id == doc->constellations[j].id) c++;
            j++;
        }
        i++;
    }
    *out_count += c;
    return c != 0u;
}

static int amf_material_exists(const amf_document *doc, amf_u32 id)
{
    if (id == 0u) return 1;
    return amf_find_material_by_id(doc, id) != AMF_ALL_OBJECTS;
}

static int amf_object_exists(const amf_document *doc, amf_u32 id)
{
    if (id == 0u) return 1;
    return amf_find_object_by_id(doc, id) != AMF_ALL_OBJECTS;
}

static int amf_constellation_exists(const amf_document *doc, amf_u32 id)
{
    if (id == 0u) return 1;
    return amf_find_constellation_by_id(doc, id) != AMF_ALL_OBJECTS;
}

static int amf_triangle_degenerate(const amf_triangle *t)
{
    if (t->v1 == t->v2) return 1;
    if (t->v1 == t->v3) return 1;
    if (t->v2 == t->v3) return 1;
    return 0;
}

static void amf_tri_sort3(amf_u32 a, amf_u32 b, amf_u32 c, amf_u32 *o0, amf_u32 *o1, amf_u32 *o2)
{
    amf_u32 x;
    amf_u32 y;
    amf_u32 z;
    amf_u32 tmp;
    x = a;
    y = b;
    z = c;
    if (x > y) { tmp = x; x = y; y = tmp; }
    if (y > z) { tmp = y; y = z; z = tmp; }
    if (x > y) { tmp = x; x = y; y = tmp; }
    *o0 = x;
    *o1 = y;
    *o2 = z;
}

static int amf_same_triangle_unordered(const amf_triangle *a, const amf_triangle *b)
{
    amf_u32 a0;
    amf_u32 a1;
    amf_u32 a2;
    amf_u32 b0;
    amf_u32 b1;
    amf_u32 b2;
    if (a->object_index != b->object_index) return 0;
    amf_tri_sort3(a->v1, a->v2, a->v3, &a0, &a1, &a2);
    amf_tri_sort3(b->v1, b->v2, b->v3, &b0, &b1, &b2);
    return a0 == b0 && a1 == b1 && a2 == b2;
}

static int amf_add_edge(amf_edge_tmp *edges, size_t edge_cap, size_t *edge_count, size_t object_index, amf_u32 a, amf_u32 b)
{
    size_t i;
    amf_u32 lo;
    amf_u32 hi;
    if (a < b) { lo = a; hi = b; } else { lo = b; hi = a; }
    i = 0u;
    while (i < *edge_count) {
        if (edges[i].object_index == object_index && edges[i].a == lo && edges[i].b == hi) {
            edges[i].uses++;
            return AMF_OK;
        }
        i++;
    }
    if (*edge_count >= edge_cap) return AMF_ERR_ARENA_FULL;
    edges[*edge_count].object_index = object_index;
    edges[*edge_count].a = lo;
    edges[*edge_count].b = hi;
    edges[*edge_count].uses = 1u;
    *edge_count = *edge_count + 1u;
    return AMF_OK;
}

int amf_validate_deep(const amf_document *doc, amf_arena *temp_arena, amf_u32 flags, amf_topology_report *out_report, amf_diag *diag)
{
    amf_topology_report report;
    int r;
    size_t oi;
    if (doc == 0) {
        amf_diag_set(diag, AMF_ERR_BAD_ARG, 1u, 1u, "bad argument");
        return AMF_ERR_BAD_ARG;
    }
    if (flags == 0u) flags = AMF_VALIDATE_ALL;
    amf_report_zero(&report);
    if ((flags & AMF_VALIDATE_BASIC) != 0u) {
        r = amf_validate(doc, diag);
        if (r != AMF_OK) {
            if (out_report != 0) *out_report = report;
            return r;
        }
    }
    if ((flags & AMF_VALIDATE_DUPLICATES) != 0u) {
        amf_id_dup_objects(doc, &report.duplicate_id_count);
        amf_id_dup_materials(doc, &report.duplicate_id_count);
        amf_id_dup_textures(doc, &report.duplicate_id_count);
        amf_id_dup_constellations(doc, &report.duplicate_id_count);
        if (report.duplicate_id_count != 0u) report.flags |= AMF_REPORT_DUPLICATE_ID;
    }
    if ((flags & AMF_VALIDATE_REFERENCES) != 0u) {
        size_t i;
        i = 0u;
        while (i < doc->object_count) {
            if ((doc->objects[i].flags & AMF_FLAG_HAS_MATERIAL) != 0u && !amf_material_exists(doc, doc->objects[i].material_id)) {
                report.missing_material_ref_count++;
            }
            i++;
        }
        i = 0u;
        while (i < doc->volume_count) {
            if ((doc->volumes[i].flags & AMF_FLAG_HAS_MATERIAL) != 0u && !amf_material_exists(doc, doc->volumes[i].material_id)) {
                report.missing_material_ref_count++;
            }
            i++;
        }
        i = 0u;
        while (i < doc->composite_count) {
            if (doc->composites[i].material_id != 0u && !amf_material_exists(doc, doc->composites[i].material_id)) {
                report.missing_material_ref_count++;
            }
            i++;
        }
        i = 0u;
        while (i < doc->instance_count) {
            if (doc->instances[i].object_id != 0u && !amf_object_exists(doc, doc->instances[i].object_id)) {
                report.missing_object_ref_count++;
            }
            if (doc->instances[i].constellation_id != 0u && !amf_constellation_exists(doc, doc->instances[i].constellation_id)) {
                report.missing_object_ref_count++;
            }
            i++;
        }
        if (report.missing_material_ref_count != 0u) report.flags |= AMF_REPORT_MISSING_MATERIAL;
        if (report.missing_object_ref_count != 0u) report.flags |= AMF_REPORT_MISSING_OBJECT;
    }
    oi = 0u;
    while (oi < doc->object_count) {
        const amf_object *o;
        size_t ti;
        report.checked_object_count++;
        o = doc->objects + oi;
        ti = o->triangle_first;
        while (ti < o->triangle_first + o->triangle_count) {
            const amf_triangle *t;
            t = doc->triangles + ti;
            report.checked_triangle_count++;
            if ((flags & AMF_VALIDATE_DEGENERATE) != 0u) {
                if (amf_triangle_degenerate(t)) {
                    report.degenerate_triangle_count++;
                    report.flags |= AMF_REPORT_DEGENERATE_TRI;
                }
            }
            if ((flags & AMF_VALIDATE_DUPLICATES) != 0u) {
                size_t tj;
                tj = ti + 1u;
                while (tj < o->triangle_first + o->triangle_count) {
                    if (amf_same_triangle_unordered(t, doc->triangles + tj)) {
                        report.duplicate_triangle_count++;
                        report.flags |= AMF_REPORT_DUPLICATE_TRI;
                    }
                    tj++;
                }
            }
            ti++;
        }
        oi++;
    }
    if ((flags & AMF_VALIDATE_MANIFOLD) != 0u) {
        amf_edge_tmp *edges;
        size_t edge_cap;
        size_t edge_count;
        size_t ti;
        size_t ei;
        if (temp_arena == 0) {
            amf_diag_set(diag, AMF_ERR_BAD_ARG, 1u, 1u, "deep manifold validation needs temp arena");
            if (out_report != 0) *out_report = report;
            return AMF_ERR_BAD_ARG;
        }
        edge_cap = doc->triangle_count * 3u;
        edges = (amf_edge_tmp *)amf_arena_calloc(temp_arena, edge_cap, sizeof(amf_edge_tmp), sizeof(void *));
        if (edges == 0 && edge_cap != 0u) {
            amf_diag_set(diag, AMF_ERR_ARENA_FULL, 1u, 1u, "temp arena too small for edge table");
            if (out_report != 0) *out_report = report;
            return AMF_ERR_ARENA_FULL;
        }
        edge_count = 0u;
        ti = 0u;
        while (ti < doc->triangle_count) {
            const amf_triangle *t2;
            t2 = doc->triangles + ti;
            r = amf_add_edge(edges, edge_cap, &edge_count, t2->object_index, t2->v1, t2->v2);
            if (r != AMF_OK) return r;
            r = amf_add_edge(edges, edge_cap, &edge_count, t2->object_index, t2->v2, t2->v3);
            if (r != AMF_OK) return r;
            r = amf_add_edge(edges, edge_cap, &edge_count, t2->object_index, t2->v3, t2->v1);
            if (r != AMF_OK) return r;
            ti++;
        }
        report.edge_count = edge_count;
        ei = 0u;
        while (ei < edge_count) {
            if (edges[ei].uses == 1u) {
                report.boundary_edge_count++;
                report.flags |= AMF_REPORT_BOUNDARY_EDGE;
            } else if (edges[ei].uses > 2u) {
                report.nonmanifold_edge_count++;
                report.flags |= AMF_REPORT_NONMANIFOLD_EDGE;
            }
            ei++;
        }
    }
    if (out_report != 0) *out_report = report;
    if (report.flags != 0u) {
        amf_diag_set(diag, AMF_ERR_VALIDATE, 1u, 1u, "deep AMF validation found warnings/errors; inspect topology report");
        return AMF_ERR_VALIDATE;
    }
    amf_diag_clear(diag);
    return AMF_OK;
}
