#include "mf3d_final_slice.h"
#include "mf3d_final_repair.h"

void mf3d_slice_segments_init(mf3d_slice_segments *s, mf3d_fix z)
{
    if (!s) return;
    s->segment_count = 0;
    s->z = z;
}

static int interp_on_edge(const mf3d_vec3 *a, const mf3d_vec3 *b, mf3d_fix z, mf3d_vec3 *outp)
{
    mf3d_fix dz, t;
    dz = b->z - a->z;
    if (dz == 0) return 0;
    if ((z < a->z && z < b->z) || (z > a->z && z > b->z)) return 0;
    t = mf3d_eval_div(z - a->z, dz);
    if (t < 0 || t > 65536L) return 0;
    outp->x = mf3d_eval_lerp(a->x, b->x, t);
    outp->y = mf3d_eval_lerp(a->y, b->y, t);
    outp->z = z;
    return 1;
}

static int same_point(const mf3d_vec3 *a, const mf3d_vec3 *b)
{
    return mf3d_eval_abs(a->x - b->x) < 8L && mf3d_eval_abs(a->y - b->y) < 8L && mf3d_eval_abs(a->z - b->z) < 8L;
}

int mf3d_eval_slice_mesh_z(const mf3d_eval_mesh *mesh, mf3d_fix z, mf3d_slice_segments *out_segments)
{
    mf3d_u32 i;
    mf3d_vec3 pts[3];
    mf3d_u32 cnt;
    const mf3d_tri *t;
    if (!mesh || !out_segments) return MF3D_E_BADARG;
    mf3d_slice_segments_init(out_segments, z);
    for (i = 0; i < mesh->triangle_count; ++i) {
        t = &mesh->triangles[i];
        if (t->v1 >= mesh->vertex_count || t->v2 >= mesh->vertex_count || t->v3 >= mesh->vertex_count) continue;
        cnt = 0;
        if (interp_on_edge(&mesh->vertices[t->v1], &mesh->vertices[t->v2], z, &pts[cnt])) cnt++;
        if (interp_on_edge(&mesh->vertices[t->v2], &mesh->vertices[t->v3], z, &pts[cnt])) { if (cnt == 0 || !same_point(&pts[0], &pts[cnt])) cnt++; }
        if (interp_on_edge(&mesh->vertices[t->v3], &mesh->vertices[t->v1], z, &pts[cnt])) { if (cnt == 0 || !same_point(&pts[0], &pts[cnt])) cnt++; }
        if (cnt >= 2) {
            if (out_segments->segment_count >= (mf3d_u32)MF3D_FINAL_MAX_SEGMENTS) return MF3D_E_NOSPACE;
            out_segments->segments[out_segments->segment_count].a = pts[0];
            out_segments->segments[out_segments->segment_count].b = pts[1];
            out_segments->segment_count++;
        }
    }
    return MF3D_OK;
}

int mf3d_eval_slice_model_z(const mf3d_model *model, mf3d_fix z, mf3d_slice_segments *out_segments)
{
    static mf3d_eval_mesh mesh;
    int r;
    if (!model || !out_segments) return MF3D_E_BADARG;
    r = mf3d_eval_mesh_from_model(model, &mesh);
    if (r != MF3D_OK) return r;
    return mf3d_eval_slice_mesh_z(&mesh, z, out_segments);
}
