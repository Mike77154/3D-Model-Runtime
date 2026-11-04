#include "lws3d_lwo_subd.h"
#include <string.h>

#define SUBD_ONE ((LWS3D_Fixed)65536L)
#define SUBD_MAX_LONG ((long)2147483647L)
#define SUBD_MIN_LONG ((long)(-2147483647L - 1L))

typedef struct SubdEdge_tag {
    unsigned long a;
    unsigned long b;
    long f0;
    long f1;
    unsigned long ep;
} SubdEdge;

typedef struct SubdVertexAccum_tag {
    long n_faces;
    long n_edges;
    long boundary_neighbors;
    LWS3D_Fixed face_x;
    LWS3D_Fixed face_y;
    LWS3D_Fixed face_z;
    LWS3D_Fixed edge_x;
    LWS3D_Fixed edge_y;
    LWS3D_Fixed edge_z;
    LWS3D_Fixed boundary_x;
    LWS3D_Fixed boundary_y;
    LWS3D_Fixed boundary_z;
} SubdVertexAccum;

static long subd_div_fixed(LWS3D_Fixed v, long d)
{
    if (d == 0) return 0;
    return (long)(v / (LWS3D_Fixed)d);
}

static LWS3D_Fixed subd_sat_add(LWS3D_Fixed a, LWS3D_Fixed b)
{
    if (b > 0 && a > SUBD_MAX_LONG - b) return SUBD_MAX_LONG;
    if (b < 0 && a < SUBD_MIN_LONG - b) return SUBD_MIN_LONG;
    return a + b;
}

static LWS3D_Fixed subd_mid(LWS3D_Fixed a, LWS3D_Fixed b)
{
    return (LWS3D_Fixed)(a / 2L + b / 2L + ((a & 1L) && (b & 1L) ? 1L : 0L));
}

static void subd_stats_zero(LWS3D_LWO_SubdStats *stats)
{
    if (stats) memset(stats, 0, sizeof(*stats));
}

static void subd_stat_overflow(LWS3D_LWO_SubdMesh *mesh, LWS3D_LWO_SubdStats *stats)
{
    if (mesh) mesh->overflowed = 1;
    if (stats) ++stats->overflow_events;
}

static void subd_stat_bad_index(LWS3D_LWO_SubdMesh *mesh, LWS3D_LWO_SubdStats *stats)
{
    if (mesh) mesh->bad_indices = 1;
    if (stats) ++stats->bad_indices;
}

void lws3d_lwo_subd_mesh_init(LWS3D_LWO_SubdMesh *mesh)
{
    if (mesh) memset(mesh, 0, sizeof(*mesh));
}

static int subd_add_point(LWS3D_LWO_SubdMesh *mesh, LWS3D_Fixed x, LWS3D_Fixed y, LWS3D_Fixed z, unsigned long source)
{
    long i;
    if (!mesh) return -1;
    if (mesh->point_count >= LWS3D_LWO_SUBD_MAX_POINTS) {
        mesh->overflowed = 1;
        return -1;
    }
    i = mesh->point_count;
    ++mesh->point_count;
    mesh->points[i].x = x;
    mesh->points[i].y = y;
    mesh->points[i].z = z;
    mesh->points[i].source_point = source;
    return (int)i;
}

static int subd_add_face(LWS3D_LWO_SubdMesh *mesh, const unsigned long *verts, int n, int surf, unsigned long source_poly)
{
    long i;
    LWS3D_LWO_SubdFace *f;
    if (!mesh || !verts || n < 3) return -1;
    if (n > LWS3D_LWO_SUBD_MAX_FACE_VERTS) n = LWS3D_LWO_SUBD_MAX_FACE_VERTS;
    if (mesh->face_count >= LWS3D_LWO_SUBD_MAX_FACES) {
        mesh->overflowed = 1;
        return -1;
    }
    f = &mesh->faces[mesh->face_count];
    memset(f, 0, sizeof(*f));
    f->vertex_count = n;
    f->surface_index = surf;
    f->source_polygon = source_poly;
    for (i = 0; i < n; ++i) f->vertices[i] = verts[i];
    ++mesh->face_count;
    return 0;
}

int lws3d_lwo_subd_build_cage_from_ptch(const LWS3D_LWO_Object *obj, LWS3D_LWO_SubdMesh *mesh, LWS3D_LWO_SubdStats *stats)
{
    long i;
    if (!obj || !mesh) return LWS3D_LWO_SUBD_ERR_BAD_ARG;
    lws3d_lwo_subd_mesh_init(mesh);
    subd_stats_zero(stats);
    mesh->mode = LWS3D_LWO_SUBD_MODE_CATMARK;
    for (i = 0; i < obj->point_count; ++i) {
        if (subd_add_point(mesh, obj->points[i].x, obj->points[i].y, obj->points[i].z, (unsigned long)i) < 0) {
            subd_stat_overflow(mesh, stats);
            return LWS3D_LWO_SUBD_ERR_OVERFLOW;
        }
    }
    if (stats) stats->cage_points = mesh->point_count;
    for (i = 0; i < obj->polygon_count; ++i) {
        const LWS3D_LWO_Polygon *p;
        unsigned long v[LWS3D_LWO_SUBD_MAX_FACE_VERTS];
        int k;
        if (obj->polygons[i].type != LWS3D_LWO_POLY_PTCH) continue;
        p = &obj->polygons[i];
        if (p->stored_vertex_count < 3) continue;
        for (k = 0; k < p->stored_vertex_count && k < LWS3D_LWO_SUBD_MAX_FACE_VERTS; ++k) {
            if (p->vertices[k] >= (unsigned long)obj->point_count) {
                subd_stat_bad_index(mesh, stats);
                return LWS3D_LWO_SUBD_ERR_BAD_INDEX;
            }
            v[k] = p->vertices[k];
        }
        if (subd_add_face(mesh, v, p->stored_vertex_count, p->surface_index, (unsigned long)i) < 0) {
            subd_stat_overflow(mesh, stats);
            return LWS3D_LWO_SUBD_ERR_OVERFLOW;
        }
    }
    if (stats) stats->cage_faces = mesh->face_count;
    return mesh->overflowed ? LWS3D_LWO_SUBD_ERR_OVERFLOW : LWS3D_LWO_SUBD_OK;
}

static long subd_find_edge(SubdEdge *edges, long edge_count, unsigned long a, unsigned long b)
{
    long i;
    unsigned long x;
    unsigned long y;
    x = a < b ? a : b;
    y = a < b ? b : a;
    for (i = 0; i < edge_count; ++i) {
        if (edges[i].a == x && edges[i].b == y) return i;
    }
    return -1;
}

static int subd_add_or_update_edge(SubdEdge *edges, long *edge_count, unsigned long a, unsigned long b, long face, LWS3D_LWO_SubdStats *stats)
{
    long idx;
    unsigned long x;
    unsigned long y;
    x = a < b ? a : b;
    y = a < b ? b : a;
    idx = subd_find_edge(edges, *edge_count, x, y);
    if (idx >= 0) {
        if (edges[idx].f1 < 0) edges[idx].f1 = face;
        else if (stats) ++stats->nonmanifold_edges;
        return (int)idx;
    }
    if (*edge_count >= LWS3D_LWO_SUBD_MAX_EDGES) return -1;
    idx = *edge_count;
    ++(*edge_count);
    edges[idx].a = x;
    edges[idx].b = y;
    edges[idx].f0 = face;
    edges[idx].f1 = -1;
    edges[idx].ep = 0UL;
    return (int)idx;
}

static void subd_face_centroid(const LWS3D_LWO_SubdMesh *mesh, const LWS3D_LWO_SubdFace *face, LWS3D_LWO_SubdPoint *out)
{
    int i;
    LWS3D_Fixed x;
    LWS3D_Fixed y;
    LWS3D_Fixed z;
    x = 0; y = 0; z = 0;
    for (i = 0; i < face->vertex_count; ++i) {
        const LWS3D_LWO_SubdPoint *p;
        p = &mesh->points[face->vertices[i]];
        x = subd_sat_add(x, p->x / (LWS3D_Fixed)face->vertex_count);
        y = subd_sat_add(y, p->y / (LWS3D_Fixed)face->vertex_count);
        z = subd_sat_add(z, p->z / (LWS3D_Fixed)face->vertex_count);
    }
    out->x = x; out->y = y; out->z = z; out->source_point = 0UL;
}

int lws3d_lwo_subd_catmull_clark_step(const LWS3D_LWO_SubdMesh *src, LWS3D_LWO_SubdMesh *dst, LWS3D_LWO_SubdStats *stats)
{
    static SubdEdge edges[LWS3D_LWO_SUBD_MAX_EDGES];
    static SubdVertexAccum vacc[LWS3D_LWO_SUBD_MAX_POINTS];
    static unsigned long face_point_index[LWS3D_LWO_SUBD_MAX_FACES];
    long edge_count;
    long i;
    if (!src || !dst) return LWS3D_LWO_SUBD_ERR_BAD_ARG;
    if (src->point_count > LWS3D_LWO_SUBD_MAX_POINTS || src->face_count > LWS3D_LWO_SUBD_MAX_FACES) return LWS3D_LWO_SUBD_ERR_OVERFLOW;
    lws3d_lwo_subd_mesh_init(dst);
    dst->mode = LWS3D_LWO_SUBD_MODE_CATMARK;
    memset(edges, 0, sizeof(edges));
    memset(vacc, 0, sizeof(vacc));
    memset(face_point_index, 0, sizeof(face_point_index));
    edge_count = 0;

    for (i = 0; i < src->face_count; ++i) {
        const LWS3D_LWO_SubdFace *f;
        int k;
        f = &src->faces[i];
        if (f->vertex_count < 3) continue;
        for (k = 0; k < f->vertex_count; ++k) {
            unsigned long a;
            unsigned long b;
            int ei;
            a = f->vertices[k];
            b = f->vertices[(k + 1) % f->vertex_count];
            if (a >= (unsigned long)src->point_count || b >= (unsigned long)src->point_count) {
                subd_stat_bad_index(dst, stats);
                return LWS3D_LWO_SUBD_ERR_BAD_INDEX;
            }
            ei = subd_add_or_update_edge(edges, &edge_count, a, b, i, stats);
            if (ei < 0) {
                subd_stat_overflow(dst, stats);
                return LWS3D_LWO_SUBD_ERR_OVERFLOW;
            }
        }
    }
    if (stats) stats->edges_seen += edge_count;

    for (i = 0; i < src->face_count; ++i) {
        LWS3D_LWO_SubdPoint fp;
        int pi;
        subd_face_centroid(src, &src->faces[i], &fp);
        pi = subd_add_point(dst, fp.x, fp.y, fp.z, (unsigned long)i);
        if (pi < 0) {
            subd_stat_overflow(dst, stats);
            return LWS3D_LWO_SUBD_ERR_OVERFLOW;
        }
        face_point_index[i] = (unsigned long)pi;
    }

    for (i = 0; i < edge_count; ++i) {
        const LWS3D_LWO_SubdPoint *pa;
        const LWS3D_LWO_SubdPoint *pb;
        LWS3D_Fixed x;
        LWS3D_Fixed y;
        LWS3D_Fixed z;
        int pi;
        pa = &src->points[edges[i].a];
        pb = &src->points[edges[i].b];
        if (edges[i].f0 >= 0 && edges[i].f1 >= 0) {
            const LWS3D_LWO_SubdPoint *f0;
            const LWS3D_LWO_SubdPoint *f1;
            f0 = &dst->points[face_point_index[edges[i].f0]];
            f1 = &dst->points[face_point_index[edges[i].f1]];
            x = (LWS3D_Fixed)(pa->x / 4L + pb->x / 4L + f0->x / 4L + f1->x / 4L);
            y = (LWS3D_Fixed)(pa->y / 4L + pb->y / 4L + f0->y / 4L + f1->y / 4L);
            z = (LWS3D_Fixed)(pa->z / 4L + pb->z / 4L + f0->z / 4L + f1->z / 4L);
        } else {
            x = subd_mid(pa->x, pb->x);
            y = subd_mid(pa->y, pb->y);
            z = subd_mid(pa->z, pb->z);
            if (stats) ++stats->boundary_edges;
        }
        pi = subd_add_point(dst, x, y, z, 0UL);
        if (pi < 0) {
            subd_stat_overflow(dst, stats);
            return LWS3D_LWO_SUBD_ERR_OVERFLOW;
        }
        edges[i].ep = (unsigned long)pi;
    }

    for (i = 0; i < src->face_count; ++i) {
        const LWS3D_LWO_SubdFace *f;
        LWS3D_LWO_SubdPoint fp;
        int k;
        f = &src->faces[i];
        subd_face_centroid(src, f, &fp);
        for (k = 0; k < f->vertex_count; ++k) {
            unsigned long v;
            SubdVertexAccum *a;
            v = f->vertices[k];
            a = &vacc[v];
            ++a->n_faces;
            a->face_x = subd_sat_add(a->face_x, fp.x);
            a->face_y = subd_sat_add(a->face_y, fp.y);
            a->face_z = subd_sat_add(a->face_z, fp.z);
        }
    }
    for (i = 0; i < edge_count; ++i) {
        const LWS3D_LWO_SubdPoint *pa;
        const LWS3D_LWO_SubdPoint *pb;
        LWS3D_Fixed mx;
        LWS3D_Fixed my;
        LWS3D_Fixed mz;
        pa = &src->points[edges[i].a];
        pb = &src->points[edges[i].b];
        mx = subd_mid(pa->x, pb->x);
        my = subd_mid(pa->y, pb->y);
        mz = subd_mid(pa->z, pb->z);
        vacc[edges[i].a].edge_x = subd_sat_add(vacc[edges[i].a].edge_x, mx);
        vacc[edges[i].a].edge_y = subd_sat_add(vacc[edges[i].a].edge_y, my);
        vacc[edges[i].a].edge_z = subd_sat_add(vacc[edges[i].a].edge_z, mz);
        ++vacc[edges[i].a].n_edges;
        vacc[edges[i].b].edge_x = subd_sat_add(vacc[edges[i].b].edge_x, mx);
        vacc[edges[i].b].edge_y = subd_sat_add(vacc[edges[i].b].edge_y, my);
        vacc[edges[i].b].edge_z = subd_sat_add(vacc[edges[i].b].edge_z, mz);
        ++vacc[edges[i].b].n_edges;
        if (edges[i].f1 < 0) {
            vacc[edges[i].a].boundary_x = subd_sat_add(vacc[edges[i].a].boundary_x, pb->x);
            vacc[edges[i].a].boundary_y = subd_sat_add(vacc[edges[i].a].boundary_y, pb->y);
            vacc[edges[i].a].boundary_z = subd_sat_add(vacc[edges[i].a].boundary_z, pb->z);
            ++vacc[edges[i].a].boundary_neighbors;
            vacc[edges[i].b].boundary_x = subd_sat_add(vacc[edges[i].b].boundary_x, pa->x);
            vacc[edges[i].b].boundary_y = subd_sat_add(vacc[edges[i].b].boundary_y, pa->y);
            vacc[edges[i].b].boundary_z = subd_sat_add(vacc[edges[i].b].boundary_z, pa->z);
            ++vacc[edges[i].b].boundary_neighbors;
        }
    }

    for (i = 0; i < src->point_count; ++i) {
        const LWS3D_LWO_SubdPoint *p;
        LWS3D_Fixed x;
        LWS3D_Fixed y;
        LWS3D_Fixed z;
        long n;
        int pi;
        p = &src->points[i];
        n = vacc[i].n_faces;
        if (vacc[i].boundary_neighbors >= 2) {
            LWS3D_Fixed bx;
            LWS3D_Fixed by;
            LWS3D_Fixed bz;
            bx = subd_div_fixed(vacc[i].boundary_x, vacc[i].boundary_neighbors);
            by = subd_div_fixed(vacc[i].boundary_y, vacc[i].boundary_neighbors);
            bz = subd_div_fixed(vacc[i].boundary_z, vacc[i].boundary_neighbors);
            x = (LWS3D_Fixed)((p->x * 3L) / 4L + bx / 4L);
            y = (LWS3D_Fixed)((p->y * 3L) / 4L + by / 4L);
            z = (LWS3D_Fixed)((p->z * 3L) / 4L + bz / 4L);
        } else if (n > 0 && vacc[i].n_edges > 0) {
            LWS3D_Fixed F_x;
            LWS3D_Fixed F_y;
            LWS3D_Fixed F_z;
            LWS3D_Fixed R_x;
            LWS3D_Fixed R_y;
            LWS3D_Fixed R_z;
            F_x = subd_div_fixed(vacc[i].face_x, n);
            F_y = subd_div_fixed(vacc[i].face_y, n);
            F_z = subd_div_fixed(vacc[i].face_z, n);
            R_x = subd_div_fixed(vacc[i].edge_x, vacc[i].n_edges);
            R_y = subd_div_fixed(vacc[i].edge_y, vacc[i].n_edges);
            R_z = subd_div_fixed(vacc[i].edge_z, vacc[i].n_edges);
            x = (LWS3D_Fixed)((F_x + 2L * R_x + (n - 3L) * p->x) / n);
            y = (LWS3D_Fixed)((F_y + 2L * R_y + (n - 3L) * p->y) / n);
            z = (LWS3D_Fixed)((F_z + 2L * R_z + (n - 3L) * p->z) / n);
        } else {
            x = p->x; y = p->y; z = p->z;
        }
        pi = subd_add_point(dst, x, y, z, p->source_point);
        if (pi < 0) {
            subd_stat_overflow(dst, stats);
            return LWS3D_LWO_SUBD_ERR_OVERFLOW;
        }
    }

    for (i = 0; i < src->face_count; ++i) {
        const LWS3D_LWO_SubdFace *f;
        int k;
        f = &src->faces[i];
        for (k = 0; k < f->vertex_count; ++k) {
            unsigned long old_v;
            unsigned long fp;
            unsigned long e_prev;
            unsigned long e_next;
            long ep;
            long en;
            unsigned long q[4];
            old_v = f->vertices[k];
            fp = face_point_index[i];
            ep = subd_find_edge(edges, edge_count, f->vertices[(k + f->vertex_count - 1) % f->vertex_count], old_v);
            en = subd_find_edge(edges, edge_count, old_v, f->vertices[(k + 1) % f->vertex_count]);
            if (ep < 0 || en < 0) {
                subd_stat_bad_index(dst, stats);
                return LWS3D_LWO_SUBD_ERR_BAD_INDEX;
            }
            e_prev = edges[ep].ep;
            e_next = edges[en].ep;
            q[0] = (unsigned long)(src->face_count + edge_count + old_v);
            q[1] = e_next;
            q[2] = fp;
            q[3] = e_prev;
            if (subd_add_face(dst, q, 4, f->surface_index, f->source_polygon) < 0) {
                subd_stat_overflow(dst, stats);
                return LWS3D_LWO_SUBD_ERR_OVERFLOW;
            }
        }
    }
    if (stats) ++stats->subdivision_steps;
    return dst->overflowed ? LWS3D_LWO_SUBD_ERR_OVERFLOW : LWS3D_LWO_SUBD_OK;
}

int lws3d_lwo_subd_build_triangles(LWS3D_LWO_SubdMesh *mesh, LWS3D_LWO_SubdStats *stats)
{
    long i;
    if (!mesh) return LWS3D_LWO_SUBD_ERR_BAD_ARG;
    mesh->triangle_count = 0;
    for (i = 0; i < mesh->face_count; ++i) {
        LWS3D_LWO_SubdFace *f;
        int k;
        f = &mesh->faces[i];
        if (f->vertex_count < 3) continue;
        for (k = 1; k + 1 < f->vertex_count; ++k) {
            LWS3D_LWO_SubdTriangle *t;
            if (mesh->triangle_count >= LWS3D_LWO_SUBD_MAX_TRIANGLES) {
                subd_stat_overflow(mesh, stats);
                return LWS3D_LWO_SUBD_ERR_OVERFLOW;
            }
            t = &mesh->triangles[mesh->triangle_count];
            t->v0 = f->vertices[0];
            t->v1 = f->vertices[k];
            t->v2 = f->vertices[k + 1];
            t->surface_index = f->surface_index;
            t->source_polygon = f->source_polygon;
            ++mesh->triangle_count;
        }
    }
    return mesh->overflowed ? LWS3D_LWO_SUBD_ERR_OVERFLOW : LWS3D_LWO_SUBD_OK;
}

int lws3d_lwo_subd_eval_ptch(const LWS3D_LWO_Object *obj, int levels, LWS3D_LWO_SubdMesh *work_a, LWS3D_LWO_SubdMesh *work_b, LWS3D_LWO_SubdMesh **out_mesh, LWS3D_LWO_SubdStats *stats)
{
    int err;
    int i;
    LWS3D_LWO_SubdMesh *src;
    LWS3D_LWO_SubdMesh *dst;
    if (!obj || !work_a || !work_b || !out_mesh) return LWS3D_LWO_SUBD_ERR_BAD_ARG;
    if (levels < 0) levels = 0;
    if (levels > 4) levels = 4;
    err = lws3d_lwo_subd_build_cage_from_ptch(obj, work_a, stats);
    if (err != LWS3D_LWO_SUBD_OK) return err;
    src = work_a;
    dst = work_b;
    for (i = 0; i < levels; ++i) {
        err = lws3d_lwo_subd_catmull_clark_step(src, dst, stats);
        if (err != LWS3D_LWO_SUBD_OK) return err;
        src = (src == work_a) ? work_b : work_a;
        dst = (dst == work_a) ? work_b : work_a;
    }
    err = lws3d_lwo_subd_build_triangles(src, stats);
    if (err != LWS3D_LWO_SUBD_OK) return err;
    *out_mesh = src;
    return LWS3D_LWO_SUBD_OK;
}

const char *lws3d_lwo_subd_error_name(int err)
{
    switch (err) {
    case LWS3D_LWO_SUBD_OK: return "OK";
    case LWS3D_LWO_SUBD_ERR_BAD_ARG: return "BAD_ARG";
    case LWS3D_LWO_SUBD_ERR_OVERFLOW: return "OVERFLOW";
    case LWS3D_LWO_SUBD_ERR_BAD_INDEX: return "BAD_INDEX";
    default: return "UNKNOWN";
    }
}
