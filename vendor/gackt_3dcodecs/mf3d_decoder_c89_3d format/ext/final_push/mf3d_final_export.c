#include "mf3d_final_export.h"
#include "mf3d_final_repair.h"
#include <stdio.h>

static int append_str(char *dst, mf3d_u32 cap, mf3d_u32 *pos, const char *s)
{
    while (*s) {
        if (*pos + 1UL >= cap) return MF3D_E_NOSPACE;
        dst[*pos] = *s;
        (*pos)++;
        s++;
    }
    dst[*pos] = 0;
    return MF3D_OK;
}

static int append_num(char *dst, mf3d_u32 cap, mf3d_u32 *pos, mf3d_fix f)
{
    char buf[48];
    long ip, fp;
    int neg;
    neg = 0;
    if (f < 0) { neg = 1; f = -f; }
    ip = f >> 16;
    fp = ((f & 65535L) * 10000L) >> 16;
    if (neg) sprintf(buf, "-%ld.%04ld", ip, fp); else sprintf(buf, "%ld.%04ld", ip, fp);
    return append_str(dst, cap, pos, buf);
}

static int append_vertex(char *dst, mf3d_u32 cap, mf3d_u32 *pos, const char *prefix, const mf3d_vec3 *v)
{
    int r;
    r = append_str(dst, cap, pos, prefix); if (r != MF3D_OK) return r;
    r = append_num(dst, cap, pos, v->x); if (r != MF3D_OK) return r;
    r = append_str(dst, cap, pos, " "); if (r != MF3D_OK) return r;
    r = append_num(dst, cap, pos, v->y); if (r != MF3D_OK) return r;
    r = append_str(dst, cap, pos, " "); if (r != MF3D_OK) return r;
    r = append_num(dst, cap, pos, v->z); if (r != MF3D_OK) return r;
    return append_str(dst, cap, pos, "\n");
}

int mf3d_eval_mesh_write_ascii_stl(const mf3d_eval_mesh *mesh, char *dst, mf3d_u32 dst_cap, mf3d_u32 *out_len, const char *name)
{
    mf3d_u32 i, pos;
    mf3d_vec3 ab, ac, n;
    const mf3d_tri *t;
    int r;
    if (!mesh || !dst || dst_cap == 0) return MF3D_E_BADARG;
    pos = 0;
    r = append_str(dst, dst_cap, &pos, "solid "); if (r != MF3D_OK) return r;
    r = append_str(dst, dst_cap, &pos, name ? name : "mf3d"); if (r != MF3D_OK) return r;
    r = append_str(dst, dst_cap, &pos, "\n"); if (r != MF3D_OK) return r;
    for (i = 0; i < mesh->triangle_count; ++i) {
        t = &mesh->triangles[i];
        if (t->v1 >= mesh->vertex_count || t->v2 >= mesh->vertex_count || t->v3 >= mesh->vertex_count) continue;
        mf3d_eval_vec_sub(&mesh->vertices[t->v2], &mesh->vertices[t->v1], &ab);
        mf3d_eval_vec_sub(&mesh->vertices[t->v3], &mesh->vertices[t->v1], &ac);
        mf3d_eval_vec_cross(&ab, &ac, &n); mf3d_eval_vec_normalize(&n);
        r = append_str(dst, dst_cap, &pos, " facet normal "); if (r != MF3D_OK) return r;
        r = append_num(dst, dst_cap, &pos, n.x); if (r != MF3D_OK) return r;
        r = append_str(dst, dst_cap, &pos, " "); if (r != MF3D_OK) return r;
        r = append_num(dst, dst_cap, &pos, n.y); if (r != MF3D_OK) return r;
        r = append_str(dst, dst_cap, &pos, " "); if (r != MF3D_OK) return r;
        r = append_num(dst, dst_cap, &pos, n.z); if (r != MF3D_OK) return r;
        r = append_str(dst, dst_cap, &pos, "\n  outer loop\n"); if (r != MF3D_OK) return r;
        r = append_vertex(dst, dst_cap, &pos, "   vertex ", &mesh->vertices[t->v1]); if (r != MF3D_OK) return r;
        r = append_vertex(dst, dst_cap, &pos, "   vertex ", &mesh->vertices[t->v2]); if (r != MF3D_OK) return r;
        r = append_vertex(dst, dst_cap, &pos, "   vertex ", &mesh->vertices[t->v3]); if (r != MF3D_OK) return r;
        r = append_str(dst, dst_cap, &pos, "  endloop\n endfacet\n"); if (r != MF3D_OK) return r;
    }
    r = append_str(dst, dst_cap, &pos, "endsolid "); if (r != MF3D_OK) return r;
    r = append_str(dst, dst_cap, &pos, name ? name : "mf3d"); if (r != MF3D_OK) return r;
    r = append_str(dst, dst_cap, &pos, "\n"); if (r != MF3D_OK) return r;
    if (out_len) *out_len = pos;
    return MF3D_OK;
}

int mf3d_eval_model_write_ascii_stl(const mf3d_model *model, char *dst, mf3d_u32 dst_cap, mf3d_u32 *out_len, const char *name)
{
    static mf3d_eval_mesh mesh;
    int r;
    if (!model) return MF3D_E_BADARG;
    r = mf3d_eval_mesh_from_model(model, &mesh);
    if (r != MF3D_OK) return r;
    return mf3d_eval_mesh_write_ascii_stl(&mesh, dst, dst_cap, out_len, name);
}
