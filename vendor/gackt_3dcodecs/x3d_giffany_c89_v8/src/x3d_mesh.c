#include "x3d_codec.h"

static x3d_fx xm_abs(x3d_fx v)
{
    return v < 0 ? -v : v;
}

static int xm_resolve_use(const x3d_scene *s, int node)
{
    int d;
    if (!s || node < 0 || node >= s->node_count) return node;
    if (s->nodes[node].use_name[0]) {
        d = x3d_find_def(s, s->nodes[node].use_name);
        if (d != X3D_NONE) return d;
    }
    return node;
}

static x3d_vec2 xm_project(x3d_vec3 v, int axis)
{
    x3d_vec2 r;
    if (axis == 0) {
        r.x = v.y;
        r.y = v.z;
    } else if (axis == 1) {
        r.x = v.x;
        r.y = v.z;
    } else {
        r.x = v.x;
        r.y = v.y;
    }
    return r;
}

static x3d_fx xm_cross2(x3d_vec2 a, x3d_vec2 b, x3d_vec2 c)
{
    x3d_fx abx;
    x3d_fx aby;
    x3d_fx acx;
    x3d_fx acy;
    abx = b.x - a.x;
    aby = b.y - a.y;
    acx = c.x - a.x;
    acy = c.y - a.y;
    return x3d_fx_mul(abx, acy) - x3d_fx_mul(aby, acx);
}

static int xm_choose_axis(const x3d_scene *s, int coord_start, int *face, int face_n)
{
    x3d_vec3 a;
    x3d_vec3 b;
    x3d_vec3 c;
    x3d_vec3 ab;
    x3d_vec3 ac;
    x3d_vec3 cr;
    int i;
    if (face_n < 3) return 2;
    a = s->vec3_pool[coord_start + face[0]];
    b = s->vec3_pool[coord_start + face[1]];
    c = s->vec3_pool[coord_start + face[2]];
    for (i = 2; i < face_n; ++i) {
        c = s->vec3_pool[coord_start + face[i]];
        ab.x = b.x - a.x;
        ab.y = b.y - a.y;
        ab.z = b.z - a.z;
        ac.x = c.x - a.x;
        ac.y = c.y - a.y;
        ac.z = c.z - a.z;
        cr.x = x3d_fx_mul(ab.y, ac.z) - x3d_fx_mul(ab.z, ac.y);
        cr.y = x3d_fx_mul(ab.z, ac.x) - x3d_fx_mul(ab.x, ac.z);
        cr.z = x3d_fx_mul(ab.x, ac.y) - x3d_fx_mul(ab.y, ac.x);
        if (xm_abs(cr.x) >= xm_abs(cr.y) && xm_abs(cr.x) >= xm_abs(cr.z) && cr.x != 0) return 0;
        if (xm_abs(cr.y) >= xm_abs(cr.z) && cr.y != 0) return 1;
        if (cr.z != 0) return 2;
    }
    return 2;
}

static x3d_fx xm_area2(const x3d_scene *s, int coord_start, int *idx, int n, int axis)
{
    int i;
    int j;
    x3d_vec2 a;
    x3d_vec2 b;
    x3d_fx sum;
    sum = 0;
    for (i = 0; i < n; ++i) {
        j = i + 1;
        if (j >= n) j = 0;
        a = xm_project(s->vec3_pool[coord_start + idx[i]], axis);
        b = xm_project(s->vec3_pool[coord_start + idx[j]], axis);
        sum = sum + x3d_fx_mul(a.x, b.y) - x3d_fx_mul(b.x, a.y);
    }
    return sum;
}

static int xm_point_in_tri(x3d_vec2 p, x3d_vec2 a, x3d_vec2 b, x3d_vec2 c, int orient)
{
    x3d_fx c1;
    x3d_fx c2;
    x3d_fx c3;
    c1 = xm_cross2(a, b, p);
    c2 = xm_cross2(b, c, p);
    c3 = xm_cross2(c, a, p);
    if (orient >= 0) {
        return c1 >= 0 && c2 >= 0 && c3 >= 0;
    }
    return c1 <= 0 && c2 <= 0 && c3 <= 0;
}

static int xm_emit_tri(x3d_tri_mesh *mesh, int a, int b, int c, int ccw)
{
    if (mesh->tri_count >= X3D_TRI_MAX) return X3D_STATUS_CAPACITY;
    if (ccw) {
        mesh->tris[mesh->tri_count].a = a;
        mesh->tris[mesh->tri_count].b = b;
        mesh->tris[mesh->tri_count].c = c;
    } else {
        mesh->tris[mesh->tri_count].a = a;
        mesh->tris[mesh->tri_count].b = c;
        mesh->tris[mesh->tri_count].c = b;
    }
    mesh->tri_count = mesh->tri_count + 1;
    return X3D_STATUS_OK;
}

static int xm_fallback_fan(x3d_tri_mesh *mesh, int *face, int n, int ccw)
{
    int i;
    int rc;
    for (i = 1; i + 1 < n; ++i) {
        rc = xm_emit_tri(mesh, face[0], face[i], face[i + 1], ccw);
        if (rc != X3D_STATUS_OK) return rc;
    }
    return X3D_STATUS_OK;
}

static int xm_emit_face(const x3d_scene *s, int coord_start, int *face, int n, int ccw, x3d_tri_mesh *mesh)
{
    int active[X3D_MAX_FACE_VERTS];
    int active_n;
    int i;
    int j;
    int prev;
    int next;
    int axis;
    int orient;
    int found;
    int guard;
    int k;
    int idx;
    x3d_vec2 a;
    x3d_vec2 b;
    x3d_vec2 c;
    x3d_vec2 p;
    x3d_fx cr;
    x3d_fx area;
    int inside;
    int rc;

    if (n < 3) return X3D_STATUS_OK;
    if (n == 3) return xm_emit_tri(mesh, face[0], face[1], face[2], ccw);
    if (n > X3D_MAX_FACE_VERTS) return xm_fallback_fan(mesh, face, n, ccw);
    axis = xm_choose_axis(s, coord_start, face, n);
    for (i = 0; i < n; ++i) active[i] = face[i];
    active_n = n;
    area = xm_area2(s, coord_start, active, active_n, axis);
    orient = area >= 0 ? 1 : -1;
    guard = 0;
    while (active_n > 3 && guard < X3D_MAX_FACE_VERTS * X3D_MAX_FACE_VERTS) {
        found = 0;
        for (i = 0; i < active_n; ++i) {
            prev = i - 1;
            if (prev < 0) prev = active_n - 1;
            next = i + 1;
            if (next >= active_n) next = 0;
            a = xm_project(s->vec3_pool[coord_start + active[prev]], axis);
            b = xm_project(s->vec3_pool[coord_start + active[i]], axis);
            c = xm_project(s->vec3_pool[coord_start + active[next]], axis);
            cr = xm_cross2(a, b, c);
            if ((orient >= 0 && cr <= 0) || (orient < 0 && cr >= 0)) {
                continue;
            }
            inside = 0;
            for (j = 0; j < active_n; ++j) {
                if (j == prev || j == i || j == next) continue;
                p = xm_project(s->vec3_pool[coord_start + active[j]], axis);
                if (xm_point_in_tri(p, a, b, c, orient)) {
                    inside = 1;
                    break;
                }
            }
            if (!inside) {
                rc = xm_emit_tri(mesh, active[prev], active[i], active[next], ccw);
                if (rc != X3D_STATUS_OK) return rc;
                for (k = i; k + 1 < active_n; ++k) {
                    active[k] = active[k + 1];
                }
                active_n = active_n - 1;
                found = 1;
                break;
            }
        }
        if (!found) {
            idx = active[0];
            (void)idx;
            return xm_fallback_fan(mesh, active, active_n, ccw);
        }
        guard = guard + 1;
    }
    if (active_n == 3) {
        return xm_emit_tri(mesh, active[0], active[1], active[2], ccw);
    }
    return X3D_STATUS_OK;
}

int x3d_triangulate_ifs(const x3d_scene *scene, int ifs_node, x3d_tri_mesh *mesh, x3d_diag *diag)
{
    const x3d_ifs *ifs;
    int coord_node;
    int coord_start;
    int coord_count;
    int face[X3D_MAX_FACE_VERTS];
    int face_n;
    int i;
    int idx;
    int rc;

    if (!scene || !mesh || ifs_node < 0 || ifs_node >= scene->node_count) {
        if (diag) diag->code = X3D_STATUS_BAD_ARG;
        return X3D_STATUS_BAD_ARG;
    }
    mesh->coord_start = X3D_NONE;
    mesh->coord_count = 0;
    mesh->tri_count = 0;
    if (scene->nodes[ifs_node].type != X3D_NODE_INDEXED_FACE_SET) {
        if (diag) diag->code = X3D_STATUS_BAD_ARG;
        return X3D_STATUS_BAD_ARG;
    }
    ifs = &scene->ifs_nodes[ifs_node];
    coord_node = xm_resolve_use(scene, ifs->coord_node);
    if (coord_node == X3D_NONE) {
        coord_node = x3d_find_first_child_type(scene, ifs_node, X3D_NODE_COORDINATE);
        coord_node = xm_resolve_use(scene, coord_node);
    }
    if (coord_node == X3D_NONE) {
        if (diag) {
            diag->code = X3D_STATUS_BAD_ARG;
            diag->node_index = ifs_node;
        }
        return X3D_STATUS_BAD_ARG;
    }
    coord_start = scene->coord_refs[coord_node].start;
    coord_count = scene->coord_refs[coord_node].count;
    mesh->coord_start = coord_start;
    mesh->coord_count = coord_count;
    if (coord_start < 0 || coord_count <= 0 || ifs->coord_index_start < 0 || ifs->coord_index_count <= 0) {
        if (diag) {
            diag->code = X3D_STATUS_BAD_ARG;
            diag->node_index = ifs_node;
        }
        return X3D_STATUS_BAD_ARG;
    }
    face_n = 0;
    for (i = 0; i < ifs->coord_index_count; ++i) {
        idx = scene->int_pool[ifs->coord_index_start + i];
        if (idx == -1) {
            rc = xm_emit_face(scene, coord_start, face, face_n, ifs->ccw, mesh);
            if (rc != X3D_STATUS_OK) return rc;
            face_n = 0;
        } else if (idx >= 0 && idx < coord_count) {
            if (face_n < X3D_MAX_FACE_VERTS) {
                face[face_n] = idx;
                face_n = face_n + 1;
            } else {
                if (diag) {
                    diag->code = X3D_STATUS_CAPACITY;
                    diag->node_index = ifs_node;
                }
                return X3D_STATUS_CAPACITY;
            }
        }
    }
    if (face_n > 0) {
        rc = xm_emit_face(scene, coord_start, face, face_n, ifs->ccw, mesh);
        if (rc != X3D_STATUS_OK) return rc;
    }
    if (diag) {
        diag->code = X3D_STATUS_OK;
        diag->node_index = ifs_node;
    }
    return X3D_STATUS_OK;
}

static int xm_is_triangulatable_rendering(int type)
{
    return type == X3D_NODE_INDEXED_TRIANGLE_SET ||
        type == X3D_NODE_INDEXED_TRIANGLE_STRIP_SET ||
        type == X3D_NODE_INDEXED_TRIANGLE_FAN_SET ||
        type == X3D_NODE_TRIANGLE_SET ||
        type == X3D_NODE_TRIANGLE_STRIP_SET ||
        type == X3D_NODE_TRIANGLE_FAN_SET;
}

static int xm_get_render_coord(const x3d_scene *scene, int node, int *coord_start, int *coord_count)
{
    int coord_node;
    if (!scene || !coord_start || !coord_count) return X3D_STATUS_BAD_ARG;
    coord_node = scene->indexed_geoms[node].coord_node;
    coord_node = xm_resolve_use(scene, coord_node);
    if (coord_node == X3D_NONE) {
        coord_node = x3d_find_first_child_type(scene, node, X3D_NODE_COORDINATE);
        coord_node = xm_resolve_use(scene, coord_node);
    }
    if (coord_node == X3D_NONE) return X3D_STATUS_BAD_ARG;
    *coord_start = scene->coord_refs[coord_node].start;
    *coord_count = scene->coord_refs[coord_node].count;
    if (*coord_start < 0 || *coord_count <= 0) return X3D_STATUS_BAD_ARG;
    return X3D_STATUS_OK;
}

static int xm_valid_idx(int idx, int coord_count)
{
    return idx >= 0 && idx < coord_count;
}

static int xm_emit_index_tri(const x3d_scene *scene, int coord_count, x3d_tri_mesh *mesh, int a, int b, int c, int ccw)
{
    (void)scene;
    if (!xm_valid_idx(a, coord_count) || !xm_valid_idx(b, coord_count) || !xm_valid_idx(c, coord_count)) {
        return X3D_STATUS_BAD_ARG;
    }
    return xm_emit_tri(mesh, a, b, c, ccw);
}

static int xm_emit_strip_indices(const x3d_scene *scene, int coord_count, x3d_tri_mesh *mesh, int *idx, int count, int ccw)
{
    int i;
    int rc;
    if (count < 3) return X3D_STATUS_OK;
    for (i = 0; i + 2 < count; ++i) {
        if ((i & 1) == 0) {
            rc = xm_emit_index_tri(scene, coord_count, mesh, idx[i], idx[i + 1], idx[i + 2], ccw);
        } else {
            rc = xm_emit_index_tri(scene, coord_count, mesh, idx[i + 1], idx[i], idx[i + 2], ccw);
        }
        if (rc != X3D_STATUS_OK) return rc;
    }
    return X3D_STATUS_OK;
}

static int xm_emit_fan_indices(const x3d_scene *scene, int coord_count, x3d_tri_mesh *mesh, int *idx, int count, int ccw)
{
    int i;
    int rc;
    if (count < 3) return X3D_STATUS_OK;
    for (i = 1; i + 1 < count; ++i) {
        rc = xm_emit_index_tri(scene, coord_count, mesh, idx[0], idx[i], idx[i + 1], ccw);
        if (rc != X3D_STATUS_OK) return rc;
    }
    return X3D_STATUS_OK;
}

static int xm_triangulate_indexed_triangles(const x3d_scene *scene, int node, x3d_tri_mesh *mesh)
{
    const x3d_indexed_geom *g;
    int i;
    int tri[3];
    int tri_n;
    int v;
    int rc;
    g = &scene->indexed_geoms[node];
    if (g->index_start < 0 || g->index_count <= 0) return X3D_STATUS_BAD_ARG;
    tri_n = 0;
    for (i = 0; i < g->index_count; ++i) {
        v = scene->int_pool[g->index_start + i];
        if (v < 0) continue;
        tri[tri_n] = v;
        tri_n = tri_n + 1;
        if (tri_n == 3) {
            rc = xm_emit_index_tri(scene, mesh->coord_count, mesh, tri[0], tri[1], tri[2], g->ccw);
            if (rc != X3D_STATUS_OK) return rc;
            tri_n = 0;
        }
    }
    return X3D_STATUS_OK;
}

static int xm_triangulate_indexed_strip_or_fan(const x3d_scene *scene, int node, x3d_tri_mesh *mesh, int fan)
{
    const x3d_indexed_geom *g;
    int seg[X3D_MAX_FACE_VERTS];
    int seg_n;
    int i;
    int v;
    int rc;
    g = &scene->indexed_geoms[node];
    if (g->index_start < 0 || g->index_count <= 0) return X3D_STATUS_BAD_ARG;
    seg_n = 0;
    for (i = 0; i < g->index_count; ++i) {
        v = scene->int_pool[g->index_start + i];
        if (v < 0) {
            if (fan) rc = xm_emit_fan_indices(scene, mesh->coord_count, mesh, seg, seg_n, g->ccw);
            else rc = xm_emit_strip_indices(scene, mesh->coord_count, mesh, seg, seg_n, g->ccw);
            if (rc != X3D_STATUS_OK) return rc;
            seg_n = 0;
        } else {
            if (seg_n >= X3D_MAX_FACE_VERTS) return X3D_STATUS_CAPACITY;
            seg[seg_n] = v;
            seg_n = seg_n + 1;
        }
    }
    if (seg_n > 0) {
        if (fan) rc = xm_emit_fan_indices(scene, mesh->coord_count, mesh, seg, seg_n, g->ccw);
        else rc = xm_emit_strip_indices(scene, mesh->coord_count, mesh, seg, seg_n, g->ccw);
        if (rc != X3D_STATUS_OK) return rc;
    }
    return X3D_STATUS_OK;
}

static int xm_triangulate_triangle_set(const x3d_scene *scene, int node, x3d_tri_mesh *mesh)
{
    const x3d_indexed_geom *g;
    int i;
    int rc;
    g = &scene->indexed_geoms[node];
    for (i = 0; i + 2 < mesh->coord_count; i += 3) {
        rc = xm_emit_index_tri(scene, mesh->coord_count, mesh, i, i + 1, i + 2, g->ccw);
        if (rc != X3D_STATUS_OK) return rc;
    }
    return X3D_STATUS_OK;
}

static int xm_triangulate_counted_sets(const x3d_scene *scene, int node, x3d_tri_mesh *mesh, int fan)
{
    const x3d_indexed_geom *g;
    int idx[X3D_MAX_FACE_VERTS];
    int count_start;
    int count_count;
    int pos;
    int ci;
    int n;
    int i;
    int rc;
    g = &scene->indexed_geoms[node];
    if (fan) {
        count_start = g->fan_count_start;
        count_count = g->fan_count_count;
    } else {
        count_start = g->strip_count_start;
        count_count = g->strip_count_count;
    }
    if (count_start < 0 || count_count <= 0) {
        if (mesh->coord_count > X3D_MAX_FACE_VERTS) return X3D_STATUS_CAPACITY;
        for (i = 0; i < mesh->coord_count; ++i) idx[i] = i;
        if (fan) return xm_emit_fan_indices(scene, mesh->coord_count, mesh, idx, mesh->coord_count, g->ccw);
        return xm_emit_strip_indices(scene, mesh->coord_count, mesh, idx, mesh->coord_count, g->ccw);
    }
    pos = 0;
    for (ci = 0; ci < count_count; ++ci) {
        n = scene->int_pool[count_start + ci];
        if (n < 0) return X3D_STATUS_BAD_ARG;
        if (n > X3D_MAX_FACE_VERTS) return X3D_STATUS_CAPACITY;
        if (pos + n > mesh->coord_count) return X3D_STATUS_BAD_ARG;
        for (i = 0; i < n; ++i) idx[i] = pos + i;
        if (fan) rc = xm_emit_fan_indices(scene, mesh->coord_count, mesh, idx, n, g->ccw);
        else rc = xm_emit_strip_indices(scene, mesh->coord_count, mesh, idx, n, g->ccw);
        if (rc != X3D_STATUS_OK) return rc;
        pos = pos + n;
    }
    return X3D_STATUS_OK;
}

int x3d_triangulate_node(const x3d_scene *scene, int geom_node, x3d_tri_mesh *mesh, x3d_diag *diag)
{
    int type;
    int rc;
    int coord_start;
    int coord_count;
    if (!scene || !mesh || geom_node < 0 || geom_node >= scene->node_count) {
        if (diag) diag->code = X3D_STATUS_BAD_ARG;
        return X3D_STATUS_BAD_ARG;
    }
    type = scene->nodes[geom_node].type;
    if (type == X3D_NODE_INDEXED_FACE_SET) {
        return x3d_triangulate_ifs(scene, geom_node, mesh, diag);
    }
    mesh->coord_start = X3D_NONE;
    mesh->coord_count = 0;
    mesh->tri_count = 0;
    if (!xm_is_triangulatable_rendering(type)) {
        if (diag) diag->code = X3D_STATUS_BAD_ARG;
        return X3D_STATUS_BAD_ARG;
    }
    rc = xm_get_render_coord(scene, geom_node, &coord_start, &coord_count);
    if (rc != X3D_STATUS_OK) {
        if (diag) {
            diag->code = rc;
            diag->node_index = geom_node;
        }
        return rc;
    }
    mesh->coord_start = coord_start;
    mesh->coord_count = coord_count;
    if (type == X3D_NODE_INDEXED_TRIANGLE_SET) rc = xm_triangulate_indexed_triangles(scene, geom_node, mesh);
    else if (type == X3D_NODE_INDEXED_TRIANGLE_STRIP_SET) rc = xm_triangulate_indexed_strip_or_fan(scene, geom_node, mesh, 0);
    else if (type == X3D_NODE_INDEXED_TRIANGLE_FAN_SET) rc = xm_triangulate_indexed_strip_or_fan(scene, geom_node, mesh, 1);
    else if (type == X3D_NODE_TRIANGLE_SET) rc = xm_triangulate_triangle_set(scene, geom_node, mesh);
    else if (type == X3D_NODE_TRIANGLE_STRIP_SET) rc = xm_triangulate_counted_sets(scene, geom_node, mesh, 0);
    else if (type == X3D_NODE_TRIANGLE_FAN_SET) rc = xm_triangulate_counted_sets(scene, geom_node, mesh, 1);
    else rc = X3D_STATUS_BAD_ARG;
    if (diag) {
        diag->code = rc;
        diag->node_index = geom_node;
    }
    return rc;
}
