#include "mf3d_final_beam_mesh.h"

static void add_offset_axis(const mf3d_vec3 *p, int ax1, int ax2, mf3d_fix s1, mf3d_fix s2, mf3d_vec3 *outp)
{
    *outp = *p;
    if (ax1 == 0) outp->x += s1; else if (ax1 == 1) outp->y += s1; else outp->z += s1;
    if (ax2 == 0) outp->x += s2; else if (ax2 == 1) outp->y += s2; else outp->z += s2;
}

static int add_beam_box(mf3d_eval_mesh *m, const mf3d_vec3 *a, const mf3d_vec3 *b, mf3d_fix r)
{
    int major, ax1, ax2;
    mf3d_fix dx, dy, dz, adx, ady, adz;
    mf3d_u32 v[8];
    mf3d_vec3 p;
    dx = b->x - a->x; dy = b->y - a->y; dz = b->z - a->z;
    adx = mf3d_eval_abs(dx); ady = mf3d_eval_abs(dy); adz = mf3d_eval_abs(dz);
    major = 0;
    if (ady > adx && ady >= adz) major = 1; else if (adz > adx && adz > ady) major = 2;
    if (major == 0) { ax1 = 1; ax2 = 2; }
    else if (major == 1) { ax1 = 0; ax2 = 2; }
    else { ax1 = 0; ax2 = 1; }
    add_offset_axis(a, ax1, ax2, -r, -r, &p); if (mf3d_eval_mesh_add_vertex(m, p.x, p.y, p.z, &v[0]) != MF3D_OK) return MF3D_E_NOSPACE;
    add_offset_axis(a, ax1, ax2,  r, -r, &p); if (mf3d_eval_mesh_add_vertex(m, p.x, p.y, p.z, &v[1]) != MF3D_OK) return MF3D_E_NOSPACE;
    add_offset_axis(a, ax1, ax2,  r,  r, &p); if (mf3d_eval_mesh_add_vertex(m, p.x, p.y, p.z, &v[2]) != MF3D_OK) return MF3D_E_NOSPACE;
    add_offset_axis(a, ax1, ax2, -r,  r, &p); if (mf3d_eval_mesh_add_vertex(m, p.x, p.y, p.z, &v[3]) != MF3D_OK) return MF3D_E_NOSPACE;
    add_offset_axis(b, ax1, ax2, -r, -r, &p); if (mf3d_eval_mesh_add_vertex(m, p.x, p.y, p.z, &v[4]) != MF3D_OK) return MF3D_E_NOSPACE;
    add_offset_axis(b, ax1, ax2,  r, -r, &p); if (mf3d_eval_mesh_add_vertex(m, p.x, p.y, p.z, &v[5]) != MF3D_OK) return MF3D_E_NOSPACE;
    add_offset_axis(b, ax1, ax2,  r,  r, &p); if (mf3d_eval_mesh_add_vertex(m, p.x, p.y, p.z, &v[6]) != MF3D_OK) return MF3D_E_NOSPACE;
    add_offset_axis(b, ax1, ax2, -r,  r, &p); if (mf3d_eval_mesh_add_vertex(m, p.x, p.y, p.z, &v[7]) != MF3D_OK) return MF3D_E_NOSPACE;
    if (mf3d_eval_mesh_add_triangle(m, v[0], v[1], v[2]) != MF3D_OK) return MF3D_E_NOSPACE;
    if (mf3d_eval_mesh_add_triangle(m, v[0], v[2], v[3]) != MF3D_OK) return MF3D_E_NOSPACE;
    if (mf3d_eval_mesh_add_triangle(m, v[4], v[6], v[5]) != MF3D_OK) return MF3D_E_NOSPACE;
    if (mf3d_eval_mesh_add_triangle(m, v[4], v[7], v[6]) != MF3D_OK) return MF3D_E_NOSPACE;
    if (mf3d_eval_mesh_add_triangle(m, v[0], v[4], v[5]) != MF3D_OK) return MF3D_E_NOSPACE;
    if (mf3d_eval_mesh_add_triangle(m, v[0], v[5], v[1]) != MF3D_OK) return MF3D_E_NOSPACE;
    if (mf3d_eval_mesh_add_triangle(m, v[1], v[5], v[6]) != MF3D_OK) return MF3D_E_NOSPACE;
    if (mf3d_eval_mesh_add_triangle(m, v[1], v[6], v[2]) != MF3D_OK) return MF3D_E_NOSPACE;
    if (mf3d_eval_mesh_add_triangle(m, v[2], v[6], v[7]) != MF3D_OK) return MF3D_E_NOSPACE;
    if (mf3d_eval_mesh_add_triangle(m, v[2], v[7], v[3]) != MF3D_OK) return MF3D_E_NOSPACE;
    if (mf3d_eval_mesh_add_triangle(m, v[3], v[7], v[4]) != MF3D_OK) return MF3D_E_NOSPACE;
    if (mf3d_eval_mesh_add_triangle(m, v[3], v[4], v[0]) != MF3D_OK) return MF3D_E_NOSPACE;
    return MF3D_OK;
}

int mf3d_eval_beam_lattice_to_mesh(const mf3d_beam_lattice *lat, mf3d_eval_mesh *out_mesh, const mf3d_beam_mesh_options *opt)
{
    mf3d_u32 oi, bi, gi;
    mf3d_beam b;
    mf3d_vec3 p0, p1;
    mf3d_fix r;
    mf3d_u32 object_filter;
    mf3d_fix default_r;
    if (!lat || !out_mesh) return MF3D_E_BADARG;
    mf3d_eval_mesh_init(out_mesh);
    object_filter = opt ? opt->objectid : 0;
    default_r = (opt && opt->default_radius) ? opt->default_radius : 3277L;
    for (oi = 0; oi < lat->object_count; ++oi) {
        if (object_filter != 0 && lat->objects[oi].objectid != object_filter) continue;
        for (bi = 0; bi < lat->objects[oi].beam_count; ++bi) {
            gi = lat->objects[oi].first_beam + bi;
            if (gi >= lat->beam_count) continue;
            b = lat->beams[gi];
            if (b.v1 >= lat->objects[oi].node_count || b.v2 >= lat->objects[oi].node_count) continue;
            if (lat->objects[oi].first_node + b.v1 >= lat->node_count || lat->objects[oi].first_node + b.v2 >= lat->node_count) continue;
            p0.x = lat->nodes[lat->objects[oi].first_node + b.v1].x; p0.y = lat->nodes[lat->objects[oi].first_node + b.v1].y; p0.z = lat->nodes[lat->objects[oi].first_node + b.v1].z;
            p1.x = lat->nodes[lat->objects[oi].first_node + b.v2].x; p1.y = lat->nodes[lat->objects[oi].first_node + b.v2].y; p1.z = lat->nodes[lat->objects[oi].first_node + b.v2].z;
            if (opt && opt->radius_override) r = opt->radius_override;
            else { r = (b.r1 + b.r2) / 2L; if (r <= 0) r = default_r; }
            if (add_beam_box(out_mesh, &p0, &p1, r) != MF3D_OK) return MF3D_E_NOSPACE;
        }
    }
    return MF3D_OK;
}
