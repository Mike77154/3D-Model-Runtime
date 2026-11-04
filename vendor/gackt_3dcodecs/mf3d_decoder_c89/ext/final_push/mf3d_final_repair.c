#include "mf3d_final_repair.h"

void mf3d_repair_stats_init(mf3d_repair_stats *s)
{
    if (!s) return;
    s->input_vertices = 0; s->input_triangles = 0;
    s->output_vertices = 0; s->output_triangles = 0;
    s->degenerate_triangles = 0; s->duplicate_vertices = 0;
    s->boundary_edges = 0; s->nonmanifold_edges = 0;
    s->reversed_outward_triangles = 0;
    mf3d_eval_aabb_init(&s->bounds);
}

int mf3d_eval_mesh_from_model(const mf3d_model *src, mf3d_eval_mesh *out_mesh)
{
    mf3d_u32 i, idx;
    if (!src || !out_mesh) return MF3D_E_BADARG;
    mf3d_eval_mesh_init(out_mesh);
    for (i = 0; i < src->vertex_count; ++i) {
        if (mf3d_eval_mesh_add_vertex(out_mesh, src->vertices[i].x, src->vertices[i].y, src->vertices[i].z, &idx) != MF3D_OK) return MF3D_E_NOSPACE;
    }
    for (i = 0; i < src->triangle_count; ++i) {
        if (src->triangles[i].v1 < src->vertex_count && src->triangles[i].v2 < src->vertex_count && src->triangles[i].v3 < src->vertex_count) {
            if (mf3d_eval_mesh_add_triangle(out_mesh, src->triangles[i].v1, src->triangles[i].v2, src->triangles[i].v3) != MF3D_OK) return MF3D_E_NOSPACE;
            out_mesh->triangles[out_mesh->triangle_count - 1UL] = src->triangles[i];
        }
    }
    return MF3D_OK;
}

static int close_fix(mf3d_fix a, mf3d_fix b, mf3d_fix tol)
{
    mf3d_fix d;
    d = a - b;
    if (d < 0) d = -d;
    return d <= tol;
}

static int same_vertex_tol(const mf3d_vec3 *a, const mf3d_vec3 *b, mf3d_fix tol)
{
    return close_fix(a->x, b->x, tol) && close_fix(a->y, b->y, tol) && close_fix(a->z, b->z, tol);
}

static int tri_degenerate(const mf3d_eval_mesh *m, const mf3d_tri *t)
{
    mf3d_vec3 ab, ac, cr;
    if (t->v1 >= m->vertex_count || t->v2 >= m->vertex_count || t->v3 >= m->vertex_count) return 1;
    if (t->v1 == t->v2 || t->v2 == t->v3 || t->v1 == t->v3) return 1;
    mf3d_eval_vec_sub(&m->vertices[t->v2], &m->vertices[t->v1], &ab);
    mf3d_eval_vec_sub(&m->vertices[t->v3], &m->vertices[t->v1], &ac);
    mf3d_eval_vec_cross(&ab, &ac, &cr);
    if (mf3d_eval_abs(cr.x) + mf3d_eval_abs(cr.y) + mf3d_eval_abs(cr.z) < 16L) return 1;
    return 0;
}

int mf3d_eval_mesh_weld_clean(const mf3d_eval_mesh *src, mf3d_eval_mesh *out_mesh, mf3d_fix tolerance, mf3d_repair_stats *stats)
{
    mf3d_u32 i, j, mapped, idx;
    mf3d_u32 map[MF3D_EVAL_MAX_VERTICES];
    mf3d_tri nt;
    if (!src || !out_mesh) return MF3D_E_BADARG;
    if (stats) { mf3d_repair_stats_init(stats); stats->input_vertices = src->vertex_count; stats->input_triangles = src->triangle_count; }
    mf3d_eval_mesh_init(out_mesh);
    for (i = 0; i < src->vertex_count; ++i) {
        mapped = 0;
        for (j = 0; j < out_mesh->vertex_count; ++j) {
            if (same_vertex_tol(&src->vertices[i], &out_mesh->vertices[j], tolerance)) { mapped = 1; map[i] = j; break; }
        }
        if (!mapped) {
            if (mf3d_eval_mesh_add_vertex(out_mesh, src->vertices[i].x, src->vertices[i].y, src->vertices[i].z, &idx) != MF3D_OK) return MF3D_E_NOSPACE;
            map[i] = idx;
        } else if (stats) stats->duplicate_vertices++;
    }
    for (i = 0; i < src->triangle_count; ++i) {
        if (src->triangles[i].v1 >= src->vertex_count || src->triangles[i].v2 >= src->vertex_count || src->triangles[i].v3 >= src->vertex_count) { if (stats) stats->degenerate_triangles++; continue; }
        nt = src->triangles[i];
        nt.v1 = map[src->triangles[i].v1]; nt.v2 = map[src->triangles[i].v2]; nt.v3 = map[src->triangles[i].v3];
        if (nt.v1 == nt.v2 || nt.v2 == nt.v3 || nt.v1 == nt.v3) { if (stats) stats->degenerate_triangles++; continue; }
        if (tri_degenerate(out_mesh, &nt)) { if (stats) stats->degenerate_triangles++; continue; }
        if (out_mesh->triangle_count >= (mf3d_u32)MF3D_EVAL_MAX_TRIANGLES) return MF3D_E_NOSPACE;
        out_mesh->triangles[out_mesh->triangle_count++] = nt;
    }
    if (stats) {
        stats->output_vertices = out_mesh->vertex_count;
        stats->output_triangles = out_mesh->triangle_count;
        for (i = 0; i < out_mesh->vertex_count; ++i) mf3d_eval_aabb_add(&stats->bounds, &out_mesh->vertices[i]);
        mf3d_eval_mesh_edge_report(out_mesh, stats);
    }
    return MF3D_OK;
}

static void sorted_edge(const mf3d_tri *t, int e, mf3d_u32 *a, mf3d_u32 *b)
{
    mf3d_u32 x, y;
    if (e == 0) { x = t->v1; y = t->v2; }
    else if (e == 1) { x = t->v2; y = t->v3; }
    else { x = t->v3; y = t->v1; }
    if (x < y) { *a = x; *b = y; } else { *a = y; *b = x; }
}

void mf3d_eval_mesh_edge_report(const mf3d_eval_mesh *mesh, mf3d_repair_stats *stats)
{
    mf3d_u32 ti, tj, a, b, c, d, cnt;
    int ei, ej;
    if (!mesh || !stats) return;
    stats->boundary_edges = 0;
    stats->nonmanifold_edges = 0;
    for (ti = 0; ti < mesh->triangle_count; ++ti) {
        for (ei = 0; ei < 3; ++ei) {
            sorted_edge(&mesh->triangles[ti], ei, &a, &b);
            cnt = 0;
            for (tj = 0; tj < mesh->triangle_count; ++tj) {
                for (ej = 0; ej < 3; ++ej) {
                    sorted_edge(&mesh->triangles[tj], ej, &c, &d);
                    if (a == c && b == d) cnt++;
                }
            }
            if (cnt == 1) stats->boundary_edges++;
            else if (cnt > 2) stats->nonmanifold_edges++;
        }
    }
}

int mf3d_eval_mesh_orient_outward_aabb(mf3d_eval_mesh *mesh, mf3d_repair_stats *stats)
{
    mf3d_eval_aabb b;
    mf3d_vec3 center, ab, ac, n, tc, toface;
    mf3d_u32 i, tmp;
    mf3d_fix dot;
    if (!mesh) return MF3D_E_BADARG;
    mf3d_eval_aabb_init(&b);
    for (i = 0; i < mesh->vertex_count; ++i) mf3d_eval_aabb_add(&b, &mesh->vertices[i]);
    if (!b.valid) return MF3D_OK;
    center.x = b.minv.x + ((b.maxv.x - b.minv.x) / 2L);
    center.y = b.minv.y + ((b.maxv.y - b.minv.y) / 2L);
    center.z = b.minv.z + ((b.maxv.z - b.minv.z) / 2L);
    for (i = 0; i < mesh->triangle_count; ++i) {
        if (mesh->triangles[i].v1 >= mesh->vertex_count || mesh->triangles[i].v2 >= mesh->vertex_count || mesh->triangles[i].v3 >= mesh->vertex_count) continue;
        mf3d_eval_vec_sub(&mesh->vertices[mesh->triangles[i].v2], &mesh->vertices[mesh->triangles[i].v1], &ab);
        mf3d_eval_vec_sub(&mesh->vertices[mesh->triangles[i].v3], &mesh->vertices[mesh->triangles[i].v1], &ac);
        mf3d_eval_vec_cross(&ab, &ac, &n);
        tc.x = (mesh->vertices[mesh->triangles[i].v1].x + mesh->vertices[mesh->triangles[i].v2].x + mesh->vertices[mesh->triangles[i].v3].x) / 3L;
        tc.y = (mesh->vertices[mesh->triangles[i].v1].y + mesh->vertices[mesh->triangles[i].v2].y + mesh->vertices[mesh->triangles[i].v3].y) / 3L;
        tc.z = (mesh->vertices[mesh->triangles[i].v1].z + mesh->vertices[mesh->triangles[i].v2].z + mesh->vertices[mesh->triangles[i].v3].z) / 3L;
        mf3d_eval_vec_sub(&tc, &center, &toface);
        dot = mf3d_eval_mul(n.x, toface.x) + mf3d_eval_mul(n.y, toface.y) + mf3d_eval_mul(n.z, toface.z);
        if (dot < 0) { tmp = mesh->triangles[i].v2; mesh->triangles[i].v2 = mesh->triangles[i].v3; mesh->triangles[i].v3 = tmp; if (stats) stats->reversed_outward_triangles++; }
    }
    return MF3D_OK;
}

int mf3d_eval_mesh_vertex_normals(const mf3d_eval_mesh *mesh, mf3d_vec3 *normals, mf3d_u32 normal_cap)
{
    mf3d_u32 i;
    mf3d_vec3 ab, ac, n;
    if (!mesh || !normals) return MF3D_E_BADARG;
    if (normal_cap < mesh->vertex_count) return MF3D_E_NOSPACE;
    for (i = 0; i < mesh->vertex_count; ++i) { normals[i].x = 0; normals[i].y = 0; normals[i].z = 0; }
    for (i = 0; i < mesh->triangle_count; ++i) {
        if (mesh->triangles[i].v1 >= mesh->vertex_count || mesh->triangles[i].v2 >= mesh->vertex_count || mesh->triangles[i].v3 >= mesh->vertex_count) continue;
        mf3d_eval_vec_sub(&mesh->vertices[mesh->triangles[i].v2], &mesh->vertices[mesh->triangles[i].v1], &ab);
        mf3d_eval_vec_sub(&mesh->vertices[mesh->triangles[i].v3], &mesh->vertices[mesh->triangles[i].v1], &ac);
        mf3d_eval_vec_cross(&ab, &ac, &n);
        mf3d_eval_vec_normalize(&n);
        normals[mesh->triangles[i].v1].x += n.x; normals[mesh->triangles[i].v1].y += n.y; normals[mesh->triangles[i].v1].z += n.z;
        normals[mesh->triangles[i].v2].x += n.x; normals[mesh->triangles[i].v2].y += n.y; normals[mesh->triangles[i].v2].z += n.z;
        normals[mesh->triangles[i].v3].x += n.x; normals[mesh->triangles[i].v3].y += n.y; normals[mesh->triangles[i].v3].z += n.z;
    }
    for (i = 0; i < mesh->vertex_count; ++i) mf3d_eval_vec_normalize(&normals[i]);
    return MF3D_OK;
}
