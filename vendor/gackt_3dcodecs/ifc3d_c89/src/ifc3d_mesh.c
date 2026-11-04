#include "ifc3d_internal.h"

static ifc3d_fixed fx_mul(ifc3d_fixed a, ifc3d_fixed b)
{
    /* Q16.16 multiply using Q8.8 halves to avoid needing 64-bit arithmetic. */
    return (ifc3d_fixed)((a >> 8) * (b >> 8));
}

static ifc3d_vec3 vec3_add(ifc3d_vec3 a, ifc3d_vec3 b)
{
    ifc3d_vec3 r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    r.z = a.z + b.z;
    return r;
}

static ifc3d_vec3 vec3_scale(ifc3d_vec3 a, ifc3d_fixed s)
{
    ifc3d_vec3 r;
    r.x = fx_mul(a.x, s);
    r.y = fx_mul(a.y, s);
    r.z = fx_mul(a.z, s);
    return r;
}

static ifc3d_vec3 zero_vec3(void)
{
    ifc3d_vec3 r;
    r.x = r.y = r.z = 0;
    return r;
}

ifc3d_status ifc3d_mesh_add_triangle(ifc3d_mesh *mesh, ifc3d_vec3 a, ifc3d_vec3 b, ifc3d_vec3 c, ifc3d_diag *diag)
{
    int base;
    if (!mesh) return IFC3D_ERR_BAD_ARG;
    if (mesh->vertex_count + 3 > IFC3D_MAX_MESH_VERTS || mesh->tri_count + 1 > IFC3D_MAX_MESH_TRIS) {
        ifc3d_set_diag(diag, IFC3D_ERR_OVERFLOW, 0, "mesh storage overflow");
        return IFC3D_ERR_OVERFLOW;
    }
    base = mesh->vertex_count;
    mesh->vertices[base] = a;
    mesh->vertices[base + 1] = b;
    mesh->vertices[base + 2] = c;
    mesh->tris[mesh->tri_count].a = base;
    mesh->tris[mesh->tri_count].b = base + 1;
    mesh->tris[mesh->tri_count].c = base + 2;
    mesh->vertex_count += 3;
    mesh->tri_count += 1;
    return IFC3D_OK;
}

ifc3d_status ifc3d_mesh_add_quad(ifc3d_mesh *mesh, ifc3d_vec3 a, ifc3d_vec3 b, ifc3d_vec3 c, ifc3d_vec3 d, ifc3d_diag *diag)
{
    ifc3d_status st;
    st = ifc3d_mesh_add_triangle(mesh, a, b, c, diag);
    if (st != IFC3D_OK) return st;
    return ifc3d_mesh_add_triangle(mesh, a, c, d, diag);
}

static int get_point_by_ref(const ifc3d_model *m, long ref, ifc3d_vec3 *out)
{
    const ifc3d_entity *e;
    e = ifc3d_find_entity_const(m, ref);
    if (!e || e->type != IFC3D_T_IFCCARTESIANPOINT) return 0;
    *out = e->v;
    return 1;
}

static ifc3d_vec3 get_direction_by_ref(const ifc3d_model *m, long ref, ifc3d_vec3 fallback)
{
    const ifc3d_entity *e;
    e = ifc3d_find_entity_const(m, ref);
    if (!e || e->type != IFC3D_T_IFCDIRECTION) return fallback;
    return e->v;
}

static ifc3d_vec3 get_axis_translation(const ifc3d_model *m, long ref)
{
    const ifc3d_entity *e;
    ifc3d_vec3 r;
    r = zero_vec3();
    if (!ref) return r;
    e = ifc3d_find_entity_const(m, ref);
    if (!e) return r;
    if (e->type == IFC3D_T_IFCAXIS2PLACEMENT3D || e->type == IFC3D_T_IFCAXIS2PLACEMENT2D) {
        get_point_by_ref(m, e->ref0, &r);
    }
    return r;
}

static ifc3d_status add_loop_face(const ifc3d_model *m, const ifc3d_entity *loop, int reverse, ifc3d_mesh *mesh, ifc3d_diag *diag)
{
    ifc3d_vec3 pts[IFC3D_ENTITY_MAX_REFS];
    int n;
    int i;
    if (!loop || (loop->type != IFC3D_T_IFCPOLYLOOP && loop->type != IFC3D_T_IFCPOLYLINE)) return IFC3D_ERR_UNSUPPORTED;
    n = 0;
    for (i = 0; i < loop->ref_count && i < IFC3D_ENTITY_MAX_REFS; ++i) {
        if (get_point_by_ref(m, loop->refs[i], &pts[n])) ++n;
    }
    if (n < 3) return IFC3D_ERR_PARSE;
    if (!reverse) {
        for (i = 1; i < n - 1; ++i) {
            if (ifc3d_mesh_add_triangle(mesh, pts[0], pts[i], pts[i + 1], diag) != IFC3D_OK) return IFC3D_ERR_OVERFLOW;
        }
    } else {
        for (i = 1; i < n - 1; ++i) {
            if (ifc3d_mesh_add_triangle(mesh, pts[0], pts[i + 1], pts[i], diag) != IFC3D_OK) return IFC3D_ERR_OVERFLOW;
        }
    }
    return IFC3D_OK;
}

static ifc3d_status build_brep(const ifc3d_model *m, const ifc3d_entity *brep, ifc3d_mesh *mesh, ifc3d_diag *diag)
{
    const ifc3d_entity *shell;
    const ifc3d_entity *face;
    const ifc3d_entity *bound;
    const ifc3d_entity *loop;
    int i;
    int reverse;
    shell = ifc3d_find_entity_const(m, brep->ref0);
    if (!shell || shell->type != IFC3D_T_IFCCLOSEDSHELL) return IFC3D_ERR_UNSUPPORTED;
    for (i = 0; i < shell->ref_count; ++i) {
        face = ifc3d_find_entity_const(m, shell->refs[i]);
        if (!face || face->type != IFC3D_T_IFCFACE || face->ref_count <= 0) continue;
        bound = ifc3d_find_entity_const(m, face->refs[0]);
        if (!bound || (bound->type != IFC3D_T_IFCFACEOUTERBOUND && bound->type != IFC3D_T_IFCFACEBOUND)) continue;
        loop = ifc3d_find_entity_const(m, bound->ref0);
        reverse = (bound->flags == 0U);
        add_loop_face(m, loop, reverse, mesh, diag);
    }
    return IFC3D_OK;
}

static int profile_to_points(const ifc3d_model *m, const ifc3d_entity *profile, ifc3d_vec3 *pts, int max_pts)
{
    const ifc3d_entity *curve;
    int i;
    int n;
    if (!m || !profile || !pts) return 0;
    if (profile->type == IFC3D_T_IFCARBITRARYCLOSEDPROFILEDEF) {
        curve = ifc3d_find_entity_const(m, profile->ref0);
        if (!curve || curve->type != IFC3D_T_IFCPOLYLINE) return 0;
        n = 0;
        for (i = 0; i < curve->ref_count && n < max_pts; ++i) {
            if (get_point_by_ref(m, curve->refs[i], &pts[n])) ++n;
        }
        if (n > 1 && pts[0].x == pts[n - 1].x && pts[0].y == pts[n - 1].y && pts[0].z == pts[n - 1].z) --n;
        return n;
    }
    if (profile->type == IFC3D_T_IFCRECTANGLEPROFILEDEF) {
        if (max_pts < 4) return 0;
        pts[0].x = -profile->scalar0 / 2; pts[0].y = -profile->scalar1 / 2; pts[0].z = 0;
        pts[1].x =  profile->scalar0 / 2; pts[1].y = -profile->scalar1 / 2; pts[1].z = 0;
        pts[2].x =  profile->scalar0 / 2; pts[2].y =  profile->scalar1 / 2; pts[2].z = 0;
        pts[3].x = -profile->scalar0 / 2; pts[3].y =  profile->scalar1 / 2; pts[3].z = 0;
        return 4;
    }
    if (profile->type == IFC3D_T_IFCCIRCLEPROFILEDEF) {
        static const long c16[16][2] = {
            { 65536L, 0L}, { 60547L, 25080L}, { 46341L, 46341L}, { 25080L, 60547L},
            { 0L, 65536L}, {-25080L, 60547L}, {-46341L, 46341L}, {-60547L, 25080L},
            {-65536L, 0L}, {-60547L,-25080L}, {-46341L,-46341L}, {-25080L,-60547L},
            { 0L,-65536L}, { 25080L,-60547L}, { 46341L,-46341L}, { 60547L,-25080L}
        };
        if (max_pts < 16) return 0;
        for (i = 0; i < 16; ++i) {
            pts[i].x = fx_mul(profile->scalar0, c16[i][0]);
            pts[i].y = fx_mul(profile->scalar0, c16[i][1]);
            pts[i].z = 0;
        }
        return 16;
    }
    return 0;
}

static ifc3d_status build_extrusion(const ifc3d_model *m, const ifc3d_entity *ex, ifc3d_mesh *mesh, ifc3d_diag *diag)
{
    const ifc3d_entity *profile;
    ifc3d_vec3 bottom[IFC3D_ENTITY_MAX_REFS];
    ifc3d_vec3 top[IFC3D_ENTITY_MAX_REFS];
    ifc3d_vec3 dir;
    ifc3d_vec3 delta;
    ifc3d_vec3 trans;
    int n;
    int i;

    profile = ifc3d_find_entity_const(m, ex->ref0);
    if (!profile) return IFC3D_ERR_UNSUPPORTED;
    n = profile_to_points(m, profile, bottom, IFC3D_ENTITY_MAX_REFS);
    if (n < 3) return IFC3D_ERR_UNSUPPORTED;

    dir.x = 0; dir.y = 0; dir.z = IFC3D_FIXED_ONE;
    dir = get_direction_by_ref(m, ex->ref2, dir);
    delta = vec3_scale(dir, ex->scalar0);
    trans = get_axis_translation(m, ex->ref1);

    for (i = 0; i < n; ++i) {
        bottom[i] = vec3_add(bottom[i], trans);
        top[i] = vec3_add(bottom[i], delta);
    }

    for (i = 1; i < n - 1; ++i) {
        if (ifc3d_mesh_add_triangle(mesh, bottom[0], bottom[i + 1], bottom[i], diag) != IFC3D_OK) return IFC3D_ERR_OVERFLOW;
    }
    for (i = 1; i < n - 1; ++i) {
        if (ifc3d_mesh_add_triangle(mesh, top[0], top[i], top[i + 1], diag) != IFC3D_OK) return IFC3D_ERR_OVERFLOW;
    }
    for (i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        if (ifc3d_mesh_add_quad(mesh, bottom[i], bottom[j], top[j], top[i], diag) != IFC3D_OK) return IFC3D_ERR_OVERFLOW;
    }
    return IFC3D_OK;
}

static ifc3d_status build_triangulated_faceset(const ifc3d_model *m, const ifc3d_entity *fs, ifc3d_mesh *mesh, ifc3d_diag *diag)
{
    const ifc3d_entity *pl;
    int i;
    int ia, ib, ic;
    ifc3d_vec3 a, b, c;
    pl = ifc3d_find_entity_const(m, fs->ref0);
    if (!pl || pl->type != IFC3D_T_IFCCARTESIANPOINTLIST3D) return IFC3D_ERR_UNSUPPORTED;
    for (i = 0; i < fs->count2; ++i) {
        ia = m->tri_indices[fs->first2 + i].a - 1;
        ib = m->tri_indices[fs->first2 + i].b - 1;
        ic = m->tri_indices[fs->first2 + i].c - 1;
        if (ia < 0 || ib < 0 || ic < 0 || ia >= pl->count || ib >= pl->count || ic >= pl->count) continue;
        a = m->pointlist_points[pl->first + ia];
        b = m->pointlist_points[pl->first + ib];
        c = m->pointlist_points[pl->first + ic];
        if (ifc3d_mesh_add_triangle(mesh, a, b, c, diag) != IFC3D_OK) return IFC3D_ERR_OVERFLOW;
    }
    return IFC3D_OK;
}

ifc3d_status ifc3d_build_mesh(ifc3d_model *m, ifc3d_mesh *mesh, ifc3d_diag *diag)
{
    int i;
    ifc3d_status st;
    if (!m || !mesh) return IFC3D_ERR_BAD_ARG;
    ifc3d_mesh_init(mesh);
    for (i = 0; i < m->entity_count; ++i) {
        st = IFC3D_OK;
        if (m->entities[i].type == IFC3D_T_IFCFACETEDBREP) st = build_brep(m, &m->entities[i], mesh, diag);
        else if (m->entities[i].type == IFC3D_T_IFCEXTRUDEDAREASOLID) st = build_extrusion(m, &m->entities[i], mesh, diag);
        else if (m->entities[i].type == IFC3D_T_IFCTRIANGULATEDFACESET) st = build_triangulated_faceset(m, &m->entities[i], mesh, diag);
        if (st != IFC3D_OK && st != IFC3D_ERR_UNSUPPORTED) return st;
    }
    ifc3d_set_diag(diag, IFC3D_OK, 0, "ok");
    return IFC3D_OK;
}
