#include <string.h>
#include "ask3d_post.h"

static ask3d_fixed ask3d_abs_fixed(ask3d_fixed v)
{
    return v < 0 ? -v : v;
}

static Ask3dVec3 ask3d_vec3_sub(const Ask3dVec3 *a, const Ask3dVec3 *b)
{
    Ask3dVec3 r;
    r.x = a->x - b->x;
    r.y = a->y - b->y;
    r.z = a->z - b->z;
    return r;
}

static Ask3dVec3 ask3d_vec3_cross(const Ask3dVec3 *a, const Ask3dVec3 *b)
{
    Ask3dVec3 r;
    r.x = ask3d_fixed_mul(a->y, b->z) - ask3d_fixed_mul(a->z, b->y);
    r.y = ask3d_fixed_mul(a->z, b->x) - ask3d_fixed_mul(a->x, b->z);
    r.z = ask3d_fixed_mul(a->x, b->y) - ask3d_fixed_mul(a->y, b->x);
    return r;
}

static void ask3d_sort3_desc(ask3d_fixed *a, ask3d_fixed *b, ask3d_fixed *c)
{
    ask3d_fixed t;
    if (*a < *b) { t = *a; *a = *b; *b = t; }
    if (*b < *c) { t = *b; *b = *c; *c = t; }
    if (*a < *b) { t = *a; *a = *b; *b = t; }
}

static ask3d_fixed ask3d_vec3_len_approx(const Ask3dVec3 *v)
{
    ask3d_fixed a;
    ask3d_fixed b;
    ask3d_fixed c;
    if (v == 0) {
        return 0;
    }
    a = ask3d_abs_fixed(v->x);
    b = ask3d_abs_fixed(v->y);
    c = ask3d_abs_fixed(v->z);
    ask3d_sort3_desc(&a, &b, &c);
    return a + (b >> 1) + (c >> 2);
}

static int ask3d_vec3_normalize_approx(Ask3dVec3 *v)
{
    ask3d_fixed len;
    if (v == 0) {
        return 0;
    }
    len = ask3d_vec3_len_approx(v);
    if (len == 0) {
        return 0;
    }
    v->x = ask3d_fixed_div(v->x, len);
    v->y = ask3d_fixed_div(v->y, len);
    v->z = ask3d_fixed_div(v->z, len);
    return 1;
}

void ask3d_post_report_clear(Ask3dPostReport *report)
{
    if (report != 0) {
        memset(report, 0, sizeof(Ask3dPostReport));
    }
}

void ask3d_topology_report_clear(Ask3dTopologyReport *report)
{
    if (report != 0) {
        memset(report, 0, sizeof(Ask3dTopologyReport));
    }
}

static int ask3d_make_face_normal(const Ask3dScene *scene, const Ask3dFace *face, Ask3dVec3 *out)
{
    Ask3dVec3 e1;
    Ask3dVec3 e2;
    Ask3dVec3 n;
    if (scene == 0 || face == 0 || out == 0) {
        return 0;
    }
    if (face->v[0] >= scene->vertex_count || face->v[1] >= scene->vertex_count || face->v[2] >= scene->vertex_count) {
        return 0;
    }
    e1 = ask3d_vec3_sub(&scene->vertices[face->v[1]], &scene->vertices[face->v[0]]);
    e2 = ask3d_vec3_sub(&scene->vertices[face->v[2]], &scene->vertices[face->v[0]]);
    n = ask3d_vec3_cross(&e1, &e2);
    if (!ask3d_vec3_normalize_approx(&n)) {
        return 0;
    }
    *out = n;
    return 1;
}

int ask3d_post_fill_missing_normals(Ask3dScene *scene, Ask3dPostReport *report)
{
    unsigned int i;
    unsigned int c;
    Ask3dFace *face;
    Ask3dVec3 n;
    if (report != 0) {
        ask3d_post_report_clear(report);
    }
    if (scene == 0) {
        return 0;
    }
    for (i = 0U; i < scene->face_count; i++) {
        face = &scene->faces[i];
        if (report != 0) {
            report->processed_faces++;
        }
        if (face->has_normal != 0U) {
            if (report != 0) {
                report->already_had_face_normals++;
            }
            n = face->normal;
        } else if (ask3d_make_face_normal(scene, face, &n)) {
            face->normal = n;
            face->has_normal = 1U;
            if (report != 0) {
                report->generated_face_normals++;
            }
        } else {
            if (report != 0) {
                report->degenerate_faces++;
            }
            continue;
        }
        for (c = 0U; c < 3U; c++) {
            if (face->has_vertex_normal[c] != 0U) {
                if (report != 0) {
                    report->already_had_vertex_normals++;
                }
            } else {
                face->vertex_normal[c] = n;
                face->has_vertex_normal[c] = 1U;
                if (report != 0) {
                    report->generated_vertex_normals++;
                }
            }
        }
    }
    return 1;
}

static void ask3d_edge_key(unsigned int a, unsigned int b, unsigned int *lo, unsigned int *hi)
{
    if (a <= b) {
        *lo = a;
        *hi = b;
    } else {
        *lo = b;
        *hi = a;
    }
}

static void ask3d_get_face_edge(const Ask3dFace *face, unsigned int e, unsigned int *lo, unsigned int *hi)
{
    unsigned int a;
    unsigned int b;
    if (e == 0U) {
        a = face->v[0]; b = face->v[1];
    } else if (e == 1U) {
        a = face->v[1]; b = face->v[2];
    } else {
        a = face->v[2]; b = face->v[0];
    }
    ask3d_edge_key(a, b, lo, hi);
}

static unsigned int ask3d_count_edge_uses(const Ask3dScene *scene, unsigned int lo, unsigned int hi)
{
    unsigned int i;
    unsigned int e;
    unsigned int a;
    unsigned int b;
    unsigned int count;
    count = 0U;
    for (i = 0U; i < scene->face_count; i++) {
        for (e = 0U; e < 3U; e++) {
            ask3d_get_face_edge(&scene->faces[i], e, &a, &b);
            if (a == lo && b == hi) {
                count++;
            }
        }
    }
    return count;
}

static int ask3d_edge_seen_before(const Ask3dScene *scene, unsigned int face_index, unsigned int edge_index, unsigned int lo, unsigned int hi)
{
    unsigned int i;
    unsigned int e;
    unsigned int a;
    unsigned int b;
    for (i = 0U; i <= face_index && i < scene->face_count; i++) {
        for (e = 0U; e < 3U; e++) {
            if (i == face_index && e >= edge_index) {
                return 0;
            }
            ask3d_get_face_edge(&scene->faces[i], e, &a, &b);
            if (a == lo && b == hi) {
                return 1;
            }
        }
    }
    return 0;
}

static int ask3d_face_degenerate(const Ask3dFace *face)
{
    if (face == 0) {
        return 1;
    }
    if (face->v[0] == face->v[1] || face->v[1] == face->v[2] || face->v[2] == face->v[0]) {
        return 1;
    }
    return 0;
}

int ask3d_topology_analyze(const Ask3dScene *scene, Ask3dTopologyReport *report)
{
    unsigned int i;
    unsigned int e;
    unsigned int lo;
    unsigned int hi;
    unsigned int uses;
    const Ask3dFace *face;
    if (report != 0) {
        ask3d_topology_report_clear(report);
    }
    if (scene == 0 || report == 0) {
        return 0;
    }
    for (i = 0U; i < scene->face_count; i++) {
        face = &scene->faces[i];
        report->processed_faces++;
        report->smoothing_group_bits |= face->smoothing;
        if (ask3d_face_degenerate(face) != 0) {
            report->degenerate_faces++;
        }
        for (e = 0U; e < 3U; e++) {
            if (face->edge_visible[e] != 0U) {
                report->visible_edges++;
            } else {
                report->hidden_edges++;
            }
            ask3d_get_face_edge(face, e, &lo, &hi);
            if (ask3d_edge_seen_before(scene, i, e, lo, hi) == 0) {
                report->unique_edges++;
                uses = ask3d_count_edge_uses(scene, lo, hi);
                if (uses == 1U) {
                    report->boundary_edges++;
                } else if (uses == 2U) {
                    report->shared_edges++;
                } else {
                    report->nonmanifold_edges++;
                }
            }
        }
    }
    report->hard_edges = report->boundary_edges + report->hidden_edges + report->nonmanifold_edges;
    return 1;
}
