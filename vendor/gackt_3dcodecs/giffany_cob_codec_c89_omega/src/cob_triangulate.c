#include "cob_private.h"

/*
   cob_triangulate.c
   Ear-clipping triangulator for COB PolH polygons.
   - Handles concave simple polygons.
   - Preserves TrueSpace-style Hole faces by bridging consecutive hole rings
     to the preceding outer Face before clipping.
   - C89, fixed-point, no heap.
*/

#ifndef COB_TRI_WORK_MAX
#define COB_TRI_WORK_MAX 1024UL
#endif

#ifndef COB_EXPORT_TRI_MAX
#define COB_EXPORT_TRI_MAX 32768UL
#endif

typedef struct cob_t2_s {
    cob_fixed x;
    cob_fixed y;
    cob_vertex_ref ref;
} cob_t2;

static cob_t2 g_poly[COB_TRI_WORK_MAX];
static cob_t2 g_tmp_poly[COB_TRI_WORK_MAX];
static cob_t2 g_hole[COB_TRI_WORK_MAX];
static unsigned char g_alive[COB_TRI_WORK_MAX];
static cob_triangle g_export_tris[COB_EXPORT_TRI_MAX];

static unsigned long tri_abs_fixed(cob_fixed v)
{
    if (v == COB_FIXED_MIN) {
        return 2147483648UL;
    }
    if (v < 0) {
        return (unsigned long)(-v);
    }
    return (unsigned long)v;
}

static cob_fixed tri_from_mag_sign(unsigned long mag, int sign)
{
    if (sign < 0) {
        if (mag >= 2147483648UL) {
            return COB_FIXED_MIN;
        }
        return -(cob_fixed)mag;
    }
    if (mag > (unsigned long)COB_FIXED_MAX) {
        return COB_FIXED_MAX;
    }
    return (cob_fixed)mag;
}

static cob_fixed tri_mul(cob_fixed a, cob_fixed b)
{
    unsigned long am;
    unsigned long bm;
    unsigned long aw;
    unsigned long af;
    unsigned long bw;
    unsigned long bf;
    unsigned long out;
    int sign;

    sign = 1;
    if (a < 0) sign = -sign;
    if (b < 0) sign = -sign;
    am = tri_abs_fixed(a);
    bm = tri_abs_fixed(b);
    aw = am >> 16;
    af = am & 65535UL;
    bw = bm >> 16;
    bf = bm & 65535UL;
    if (aw != 0UL && bw > 32767UL / aw) {
        return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    }
    out = (aw * bw) << 16;
    if (aw != 0UL && bf > ((unsigned long)COB_FIXED_MAX - out) / aw) {
        return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    }
    out += aw * bf;
    if (bw != 0UL && af > ((unsigned long)COB_FIXED_MAX - out) / bw) {
        return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    }
    out += bw * af;
    out += (af * bf) >> 16;
    return tri_from_mag_sign(out, sign);
}

static cob_fixed tri_sat_add(cob_fixed a, cob_fixed b)
{
    if (b > 0 && a > COB_FIXED_MAX - b) return COB_FIXED_MAX;
    if (b < 0 && a < COB_FIXED_MIN - b) return COB_FIXED_MIN;
    return a + b;
}

static cob_fixed tri_orient2(const cob_t2 *a, const cob_t2 *b, const cob_t2 *c)
{
    cob_fixed abx;
    cob_fixed aby;
    cob_fixed acx;
    cob_fixed acy;
    abx = b->x - a->x;
    aby = b->y - a->y;
    acx = c->x - a->x;
    acy = c->y - a->y;
    return tri_sat_add(tri_mul(abx, acy), -tri_mul(aby, acx));
}

static cob_fixed tri_area2(const cob_t2 *p, unsigned long n)
{
    unsigned long i;
    unsigned long j;
    cob_fixed area;
    area = 0;
    if (p == 0 || n < 3UL) return 0;
    for (i = 0; i < n; ++i) {
        j = (i + 1UL == n) ? 0UL : i + 1UL;
        area = tri_sat_add(area, tri_sat_add(tri_mul(p[i].x, p[j].y), -tri_mul(p[j].x, p[i].y)));
    }
    return area;
}

static void tri_reverse(cob_t2 *p, unsigned long n)
{
    unsigned long i;
    cob_t2 t;
    for (i = 0; i < n / 2UL; ++i) {
        t = p[i];
        p[i] = p[n - 1UL - i];
        p[n - 1UL - i] = t;
    }
}

static int tri_same_xy(const cob_t2 *a, const cob_t2 *b)
{
    return a->x == b->x && a->y == b->y;
}

static int tri_same_point(const cob_t2 *a, const cob_t2 *b)
{
    return tri_same_xy(a, b) && a->ref.pos_index == b->ref.pos_index;
}

static unsigned long tri_compact(cob_t2 *p, unsigned long n)
{
    unsigned long i;
    unsigned long o;
    if (n == 0UL) return 0UL;
    o = 0UL;
    for (i = 0; i < n; ++i) {
        if (o == 0UL || !tri_same_point(&p[i], &p[o - 1UL])) {
            p[o++] = p[i];
        }
    }
    if (o > 1UL && tri_same_point(&p[0], &p[o - 1UL])) {
        --o;
    }
    return o;
}

static int tri_segments_intersect(const cob_t2 *a, const cob_t2 *b, const cob_t2 *c, const cob_t2 *d)
{
    cob_fixed o1;
    cob_fixed o2;
    cob_fixed o3;
    cob_fixed o4;

    if (tri_same_point(a, c) || tri_same_point(a, d) || tri_same_point(b, c) || tri_same_point(b, d)) {
        return 0;
    }
    o1 = tri_orient2(a, b, c);
    o2 = tri_orient2(a, b, d);
    o3 = tri_orient2(c, d, a);
    o4 = tri_orient2(c, d, b);
    if (((o1 > 0 && o2 < 0) || (o1 < 0 && o2 > 0)) &&
        ((o3 > 0 && o4 < 0) || (o3 < 0 && o4 > 0))) {
        return 1;
    }
    return 0;
}

static int tri_visible_bridge(const cob_t2 *poly, unsigned long n, const cob_t2 *a, const cob_t2 *b)
{
    unsigned long i;
    unsigned long j;
    for (i = 0; i < n; ++i) {
        j = (i + 1UL == n) ? 0UL : i + 1UL;
        if (tri_segments_intersect(a, b, &poly[i], &poly[j])) {
            return 0;
        }
    }
    return 1;
}

static unsigned long tri_dist_key(const cob_t2 *a, const cob_t2 *b)
{
    unsigned long dx;
    unsigned long dy;
    dx = tri_abs_fixed(a->x - b->x) >> 8;
    dy = tri_abs_fixed(a->y - b->y) >> 8;
    if (dx > 65535UL) dx = 65535UL;
    if (dy > 65535UL) dy = 65535UL;
    return dx * dx + dy * dy;
}

static int tri_merge_hole(cob_t2 *poly, unsigned long *poly_n, cob_t2 *hole, unsigned long hole_n, cob_triangulate_stats *stats)
{
    unsigned long n;
    unsigned long hv;
    unsigned long i;
    unsigned long best;
    unsigned long best_key;
    unsigned long key;
    unsigned long o;
    unsigned long k;
    int found;

    if (poly == 0 || poly_n == 0 || hole == 0 || *poly_n < 3UL || hole_n < 3UL) {
        return COB_ERR_ARGUMENT;
    }
    n = *poly_n;
    if (n + hole_n + 2UL > COB_TRI_WORK_MAX) {
        return COB_ERR_CAPACITY;
    }
    if (tri_area2(hole, hole_n) > 0) {
        tri_reverse(hole, hole_n);
    }
    hv = 0UL;
    for (i = 1UL; i < hole_n; ++i) {
        if (hole[i].x > hole[hv].x || (hole[i].x == hole[hv].x && hole[i].y < hole[hv].y)) {
            hv = i;
        }
    }
    best = 0UL;
    best_key = 4294967295UL;
    found = 0;
    for (i = 0; i < n; ++i) {
        if (tri_visible_bridge(poly, n, &hole[hv], &poly[i])) {
            key = tri_dist_key(&hole[hv], &poly[i]);
            if (!found || key < best_key) {
                found = 1;
                best = i;
                best_key = key;
            }
        }
    }
    if (!found) {
        for (i = 0; i < n; ++i) {
            key = tri_dist_key(&hole[hv], &poly[i]);
            if (i == 0UL || key < best_key) {
                best = i;
                best_key = key;
            }
        }
        if (stats != 0) {
            ++stats->skipped_holes;
        }
    }
    o = 0UL;
    for (i = 0; i <= best; ++i) {
        g_tmp_poly[o++] = poly[i];
    }
    for (k = 0; k < hole_n; ++k) {
        g_tmp_poly[o++] = hole[(hv + k) % hole_n];
    }
    g_tmp_poly[o++] = hole[hv];
    g_tmp_poly[o++] = poly[best];
    for (i = best + 1UL; i < n; ++i) {
        g_tmp_poly[o++] = poly[i];
    }
    for (i = 0; i < o; ++i) {
        poly[i] = g_tmp_poly[i];
    }
    *poly_n = tri_compact(poly, o);
    if (stats != 0) {
        ++stats->hole_groups;
        ++stats->bridge_count;
    }
    return COB_OK;
}

static int tri_select_axis(const cob_scene *scene, const cob_mesh *m, const cob_face *f)
{
    cob_vec3q a;
    cob_vec3q b;
    cob_vec3q c;
    cob_vec3q ab;
    cob_vec3q ac;
    cob_vec3q n;
    const cob_vertex_ref *r0;
    const cob_vertex_ref *r1;
    const cob_vertex_ref *r2;
    unsigned long ax;
    unsigned long ay;
    unsigned long az;

    if (f->index_count < 3U || f->first_index + 2UL >= scene->index_count) {
        return 2;
    }
    r0 = &scene->indices[f->first_index];
    r1 = &scene->indices[f->first_index + 1UL];
    r2 = &scene->indices[f->first_index + 2UL];
    if (r0->pos_index < 0 || r1->pos_index < 0 || r2->pos_index < 0) {
        return 2;
    }
    if ((unsigned long)r0->pos_index >= m->vertex_count || (unsigned long)r1->pos_index >= m->vertex_count || (unsigned long)r2->pos_index >= m->vertex_count) {
        return 2;
    }
    a = scene->vertices[m->first_vertex + (unsigned long)r0->pos_index];
    b = scene->vertices[m->first_vertex + (unsigned long)r1->pos_index];
    c = scene->vertices[m->first_vertex + (unsigned long)r2->pos_index];
    ab.x = b.x - a.x; ab.y = b.y - a.y; ab.z = b.z - a.z;
    ac.x = c.x - a.x; ac.y = c.y - a.y; ac.z = c.z - a.z;
    n.x = tri_sat_add(tri_mul(ab.y, ac.z), -tri_mul(ab.z, ac.y));
    n.y = tri_sat_add(tri_mul(ab.z, ac.x), -tri_mul(ab.x, ac.z));
    n.z = tri_sat_add(tri_mul(ab.x, ac.y), -tri_mul(ab.y, ac.x));
    ax = tri_abs_fixed(n.x);
    ay = tri_abs_fixed(n.y);
    az = tri_abs_fixed(n.z);
    if (ax >= ay && ax >= az) return 0;
    if (ay >= ax && ay >= az) return 1;
    return 2;
}

static void tri_project(cob_t2 *out, const cob_vec3q *p, int drop_axis)
{
    if (drop_axis == 0) {
        out->x = p->y;
        out->y = p->z;
    } else if (drop_axis == 1) {
        out->x = p->x;
        out->y = p->z;
    } else {
        out->x = p->x;
        out->y = p->y;
    }
}

static int tri_load_ring(const cob_scene *scene, const cob_mesh *m, const cob_face *f, int axis, cob_t2 *out, unsigned long *out_n)
{
    unsigned long i;
    const cob_vertex_ref *r;
    const cob_vec3q *p;

    if (scene == 0 || m == 0 || f == 0 || out == 0 || out_n == 0) {
        return COB_ERR_ARGUMENT;
    }
    if (f->index_count < 3U) {
        *out_n = 0UL;
        return COB_OK;
    }
    if (f->index_count > COB_TRI_WORK_MAX) {
        return COB_ERR_CAPACITY;
    }
    if (f->first_index + f->index_count > scene->index_count) {
        return COB_ERR_PARSE;
    }
    for (i = 0; i < (unsigned long)f->index_count; ++i) {
        r = &scene->indices[f->first_index + i];
        if (r->pos_index < 0 || (unsigned long)r->pos_index >= m->vertex_count) {
            return COB_ERR_PARSE;
        }
        p = &scene->vertices[m->first_vertex + (unsigned long)r->pos_index];
        memset(&out[i], 0, sizeof(out[i]));
        out[i].ref = *r;
        tri_project(&out[i], p, axis);
    }
    *out_n = tri_compact(out, (unsigned long)f->index_count);
    return COB_OK;
}

static int tri_point_inside_ccw(const cob_t2 *a, const cob_t2 *b, const cob_t2 *c, const cob_t2 *p)
{
    cob_fixed o1;
    cob_fixed o2;
    cob_fixed o3;
    o1 = tri_orient2(a, b, p);
    o2 = tri_orient2(b, c, p);
    o3 = tri_orient2(c, a, p);
    return o1 >= 0 && o2 >= 0 && o3 >= 0;
}

static unsigned long tri_prev_alive(unsigned long idx, unsigned long n)
{
    unsigned long p;
    p = idx == 0UL ? n - 1UL : idx - 1UL;
    while (!g_alive[p]) {
        p = p == 0UL ? n - 1UL : p - 1UL;
    }
    return p;
}

static unsigned long tri_next_alive(unsigned long idx, unsigned long n)
{
    unsigned long p;
    p = idx + 1UL == n ? 0UL : idx + 1UL;
    while (!g_alive[p]) {
        p = p + 1UL == n ? 0UL : p + 1UL;
    }
    return p;
}

static int tri_emit_triangle(const cob_scene *scene, const cob_mesh *m, const cob_face *f, unsigned long mesh_index, unsigned long face_index, const cob_t2 *a, const cob_t2 *b, const cob_t2 *c, cob_triangle *out)
{
    const cob_vertex_ref *ra;
    const cob_vertex_ref *rb;
    const cob_vertex_ref *rc;

    if (out == 0) {
        return COB_OK;
    }
    ra = &a->ref;
    rb = &b->ref;
    rc = &c->ref;
    if (ra->pos_index < 0 || rb->pos_index < 0 || rc->pos_index < 0) {
        return COB_ERR_PARSE;
    }
    if ((unsigned long)ra->pos_index >= m->vertex_count || (unsigned long)rb->pos_index >= m->vertex_count || (unsigned long)rc->pos_index >= m->vertex_count) {
        return COB_ERR_PARSE;
    }
    memset(out, 0, sizeof(*out));
    out->mesh_index = (cob_u32)mesh_index;
    out->face_index = (cob_u32)face_index;
    out->material = f->material;
    out->flags = f->flags;
    out->refs[0] = *ra;
    out->refs[1] = *rb;
    out->refs[2] = *rc;
    out->p[0] = scene->vertices[m->first_vertex + (unsigned long)ra->pos_index];
    out->p[1] = scene->vertices[m->first_vertex + (unsigned long)rb->pos_index];
    out->p[2] = scene->vertices[m->first_vertex + (unsigned long)rc->pos_index];
    if (m->uv_count != 0UL) {
        if (ra->uv_index >= 0 && (unsigned long)ra->uv_index < m->uv_count) out->uv[0] = scene->uvs[m->first_uv + (unsigned long)ra->uv_index];
        if (rb->uv_index >= 0 && (unsigned long)rb->uv_index < m->uv_count) out->uv[1] = scene->uvs[m->first_uv + (unsigned long)rb->uv_index];
        if (rc->uv_index >= 0 && (unsigned long)rc->uv_index < m->uv_count) out->uv[2] = scene->uvs[m->first_uv + (unsigned long)rc->uv_index];
    }
    return COB_OK;
}

static int tri_ear_clip(const cob_scene *scene, const cob_mesh *m, const cob_face *f, unsigned long mesh_index, unsigned long face_index, cob_t2 *poly, unsigned long n, cob_triangle *out_triangles, unsigned long triangle_capacity, unsigned long *out_n, cob_triangulate_stats *stats)
{
    unsigned long i;
    unsigned long prev;
    unsigned long next;
    unsigned long k;
    unsigned long alive_count;
    unsigned long guard;
    int is_ear;
    int st;
    cob_fixed area;

    if (n < 3UL) {
        if (stats != 0) ++stats->degenerate_faces;
        return COB_OK;
    }
    n = tri_compact(poly, n);
    if (n < 3UL) {
        if (stats != 0) ++stats->degenerate_faces;
        return COB_OK;
    }
    area = tri_area2(poly, n);
    if (area < 0) {
        tri_reverse(poly, n);
        area = -area;
    }
    if (area == 0) {
        if (stats != 0) ++stats->degenerate_faces;
        return COB_OK;
    }
    for (i = 0; i < n; ++i) {
        g_alive[i] = 1U;
    }
    alive_count = n;
    guard = 0UL;
    while (alive_count > 3UL && guard < n * n + 16UL) {
        int clipped;
        clipped = 0;
        for (i = 0; i < n; ++i) {
            if (!g_alive[i]) continue;
            prev = tri_prev_alive(i, n);
            next = tri_next_alive(i, n);
            if (tri_orient2(&poly[prev], &poly[i], &poly[next]) <= 0) {
                continue;
            }
            is_ear = 1;
            for (k = 0; k < n; ++k) {
                if (!g_alive[k] || k == prev || k == i || k == next) continue;
                if (tri_same_xy(&poly[k], &poly[prev]) || tri_same_xy(&poly[k], &poly[i]) || tri_same_xy(&poly[k], &poly[next])) continue;
                if (tri_point_inside_ccw(&poly[prev], &poly[i], &poly[next], &poly[k])) {
                    is_ear = 0;
                    break;
                }
            }
            if (is_ear) {
                if (*out_n >= triangle_capacity) return COB_ERR_CAPACITY;
                st = tri_emit_triangle(scene, m, f, mesh_index, face_index, &poly[prev], &poly[i], &poly[next], out_triangles == 0 ? 0 : &out_triangles[*out_n]);
                if (st != COB_OK) return st;
                ++(*out_n);
                if (stats != 0) ++stats->triangles;
                g_alive[i] = 0U;
                --alive_count;
                clipped = 1;
                break;
            }
        }
        if (!clipped) {
            break;
        }
        ++guard;
    }
    if (alive_count == 3UL) {
        unsigned long ids[3];
        unsigned long o;
        o = 0UL;
        for (i = 0; i < n; ++i) {
            if (g_alive[i] && o < 3UL) ids[o++] = i;
        }
        if (o == 3UL) {
            if (*out_n >= triangle_capacity) return COB_ERR_CAPACITY;
            st = tri_emit_triangle(scene, m, f, mesh_index, face_index, &poly[ids[0]], &poly[ids[1]], &poly[ids[2]], out_triangles == 0 ? 0 : &out_triangles[*out_n]);
            if (st != COB_OK) return st;
            ++(*out_n);
            if (stats != 0) ++stats->triangles;
        }
        return COB_OK;
    }

    /* Recovery path: some COB exporters generate duplicate bridge vertices or
       almost-collinear chains.  Fall back to conservative fan output rather
       than dropping the face completely. */
    if (stats != 0) ++stats->failed_faces;
    for (i = 1UL; i + 1UL < n; ++i) {
        if (tri_orient2(&poly[0], &poly[i], &poly[i + 1UL]) == 0) {
            continue;
        }
        if (*out_n >= triangle_capacity) return COB_ERR_CAPACITY;
        st = tri_emit_triangle(scene, m, f, mesh_index, face_index, &poly[0], &poly[i], &poly[i + 1UL], out_triangles == 0 ? 0 : &out_triangles[*out_n]);
        if (st != COB_OK) return st;
        ++(*out_n);
        if (stats != 0) ++stats->triangles;
    }
    return COB_OK;
}

int cob_scene_build_triangles_ex(const cob_scene *scene, cob_triangle *out_triangles, unsigned long triangle_capacity, unsigned long *out_count, cob_triangulate_stats *stats)
{
    unsigned long mi;
    unsigned long fi;
    unsigned long next_fi;
    unsigned long poly_n;
    unsigned long hole_n;
    unsigned long out_n;
    int axis;
    int st;
    const cob_mesh *m;
    const cob_face *f;
    const cob_face *hf;

    if (out_count != 0) *out_count = 0UL;
    if (stats != 0) memset(stats, 0, sizeof(*stats));
    if (scene == 0) return COB_ERR_ARGUMENT;
    if (out_triangles == 0 && triangle_capacity != 0UL) return COB_ERR_ARGUMENT;
    out_n = 0UL;
    for (mi = 0; mi < scene->mesh_count; ++mi) {
        m = &scene->meshes[mi];
        if (m->first_face + m->face_count > scene->face_count) return COB_ERR_PARSE;
        fi = 0UL;
        while (fi < m->face_count) {
            f = &scene->faces[m->first_face + fi];
            if ((f->flags & COB_FACE_HOLE) != 0U || f->index_count < 3U) {
                if (stats != 0 && f->index_count < 3U) ++stats->degenerate_faces;
                ++fi;
                continue;
            }
            axis = tri_select_axis(scene, m, f);
            st = tri_load_ring(scene, m, f, axis, g_poly, &poly_n);
            if (st != COB_OK) return st;
            if (tri_area2(g_poly, poly_n) < 0) tri_reverse(g_poly, poly_n);
            if (poly_n > 3UL) {
                unsigned long ii;
                int concave;
                concave = 0;
                for (ii = 0; ii < poly_n; ++ii) {
                    unsigned long pp = ii == 0UL ? poly_n - 1UL : ii - 1UL;
                    unsigned long nn = ii + 1UL == poly_n ? 0UL : ii + 1UL;
                    if (tri_orient2(&g_poly[pp], &g_poly[ii], &g_poly[nn]) < 0) {
                        concave = 1;
                        break;
                    }
                }
                if (concave && stats != 0) ++stats->concave_faces;
            }
            next_fi = fi + 1UL;
            while (next_fi < m->face_count) {
                hf = &scene->faces[m->first_face + next_fi];
                if ((hf->flags & COB_FACE_HOLE) == 0U) break;
                st = tri_load_ring(scene, m, hf, axis, g_hole, &hole_n);
                if (st != COB_OK) return st;
                if (hole_n >= 3UL) {
                    st = tri_merge_hole(g_poly, &poly_n, g_hole, hole_n, stats);
                    if (st != COB_OK) return st;
                }
                ++next_fi;
            }
            st = tri_ear_clip(scene, m, f, mi, fi, g_poly, poly_n, out_triangles, triangle_capacity, &out_n, stats);
            if (st != COB_OK) {
                if (out_count != 0) *out_count = out_n;
                return st;
            }
            fi = next_fi;
        }
    }
    if (out_count != 0) *out_count = out_n;
    return COB_OK;
}

static long tri_find_material_number(const cob_scene *scene, cob_i32 mesh_id, cob_i16 face_mat)
{
    unsigned long i;
    const cob_material *mat;
    for (i = 0; i < scene->material_count; ++i) {
        mat = &scene->materials[i];
        if ((mat->parent_id == mesh_id || mesh_id == 0L) && mat->matnum == face_mat) {
            return (long)mat->matnum;
        }
    }
    return (long)face_mat;
}

static int tri_mtl_name(cob_write_fn fn, void *user, long matnum)
{
    int st;
    st = cob_emit(fn, user, "mat_");
    if (st != COB_OK) return st;
    return cob_emit_i(fn, user, matnum);
}

int cob_export_tri_obj_mtl(const cob_scene *scene, cob_write_fn write_fn, void *user, const char *mtl_name)
{
    unsigned long tri_count;
    unsigned long mi;
    unsigned long i;
    unsigned long vertex_base[COB_MAX_MESHES];
    unsigned long uv_base[COB_MAX_MESHES];
    unsigned long vb;
    unsigned long ub;
    long current_mat;
    long mat_to_use;
    const cob_mesh *m;
    const cob_vec3q *v;
    const cob_vec2q *uv;
    const cob_triangle *t;
    int st;

    if (scene == 0 || write_fn == 0) return COB_ERR_ARGUMENT;
    tri_count = 0UL;
    st = cob_scene_build_triangles_ex(scene, g_export_tris, COB_EXPORT_TRI_MAX, &tri_count, 0);
    if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "# triangulated OBJ exported by cob_chiqui3d\n");
    if (st != COB_OK) return st;
    if (mtl_name != 0 && mtl_name[0] != '\0') {
        st = cob_emit(write_fn, user, "mtllib ");
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, mtl_name);
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\n");
        if (st != COB_OK) return st;
    }
    vb = 1UL;
    ub = 1UL;
    for (mi = 0; mi < scene->mesh_count; ++mi) {
        vertex_base[mi] = vb;
        uv_base[mi] = ub;
        m = &scene->meshes[mi];
        st = cob_emit(write_fn, user, "o ");
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, m->name[0] ? m->name : "Mesh");
        if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\n");
        if (st != COB_OK) return st;
        for (i = 0; i < m->vertex_count; ++i) {
            v = &scene->vertices[m->first_vertex + i];
            st = cob_emit(write_fn, user, "v ");
            if (st != COB_OK) return st;
            st = cob_emit_fixed(write_fn, user, v->x, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, " ");
            if (st != COB_OK) return st;
            st = cob_emit_fixed(write_fn, user, v->y, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, " ");
            if (st != COB_OK) return st;
            st = cob_emit_fixed(write_fn, user, v->z, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, "\n");
            if (st != COB_OK) return st;
        }
        for (i = 0; i < m->uv_count; ++i) {
            uv = &scene->uvs[m->first_uv + i];
            st = cob_emit(write_fn, user, "vt ");
            if (st != COB_OK) return st;
            st = cob_emit_fixed(write_fn, user, uv->u, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, " ");
            if (st != COB_OK) return st;
            st = cob_emit_fixed(write_fn, user, uv->v, 6);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, "\n");
            if (st != COB_OK) return st;
        }
        vb += m->vertex_count;
        ub += m->uv_count;
    }
    current_mat = -2147483647L;
    for (i = 0; i < tri_count; ++i) {
        t = &g_export_tris[i];
        if ((unsigned long)t->mesh_index >= scene->mesh_count) return COB_ERR_PARSE;
        m = &scene->meshes[t->mesh_index];
        mat_to_use = tri_find_material_number(scene, m->id, t->material);
        if (mat_to_use != current_mat) {
            current_mat = mat_to_use;
            st = cob_emit(write_fn, user, "usemtl ");
            if (st != COB_OK) return st;
            st = tri_mtl_name(write_fn, user, mat_to_use);
            if (st != COB_OK) return st;
            st = cob_emit(write_fn, user, "\n");
            if (st != COB_OK) return st;
        }
        st = cob_emit(write_fn, user, "f");
        if (st != COB_OK) return st;
        {
            int k;
            for (k = 0; k < 3; ++k) {
                st = cob_emit(write_fn, user, " ");
                if (st != COB_OK) return st;
                st = cob_emit_u(write_fn, user, vertex_base[t->mesh_index] + (unsigned long)t->refs[k].pos_index);
                if (st != COB_OK) return st;
                if (m->uv_count != 0UL && t->refs[k].uv_index >= 0) {
                    st = cob_emit(write_fn, user, "/");
                    if (st != COB_OK) return st;
                    st = cob_emit_u(write_fn, user, uv_base[t->mesh_index] + (unsigned long)t->refs[k].uv_index);
                    if (st != COB_OK) return st;
                }
            }
        }
        st = cob_emit(write_fn, user, "\n");
        if (st != COB_OK) return st;
    }
    return COB_OK;
}

int cob_export_tri_obj(const cob_scene *scene, cob_write_fn write_fn, void *user)
{
    return cob_export_tri_obj_mtl(scene, write_fn, user, 0);
}
