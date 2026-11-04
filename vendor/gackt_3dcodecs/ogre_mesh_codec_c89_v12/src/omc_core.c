#include "ogre_mesh_codec.h"
#include <stdio.h>
#include <string.h>

static void omc_zero_mem(void *p, unsigned long n)
{
    unsigned char *b;
    b = (unsigned char *)p;
    while (n > 0UL) {
        *b++ = 0;
        n--;
    }
}

static void omc_copy_str(char *dst, unsigned long cap, const char *src)
{
    unsigned long i;
    if (!dst || cap == 0UL) return;
    dst[0] = '\0';
    if (!src) return;
    i = 0UL;
    while (src[i] && i + 1UL < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static OMC_Fixed omc_fixed_mul_q16_safe(OMC_Fixed a, OMC_Fixed b);
static OMC_Fixed omc_fixed_div_q16_safe(OMC_Fixed a, OMC_Fixed b);
static OMC_Fixed omc_fixed_add_sat(OMC_Fixed a, OMC_Fixed b);
static OMC_Fixed omc_fixed_sub_sat(OMC_Fixed a, OMC_Fixed b);
static OMC_Fixed omc_fixed_lerp_q16(OMC_Fixed a, OMC_Fixed b, OMC_Fixed t);
static OMC_Fixed omc_fixed_clamp01(OMC_Fixed v);
static OMC_Fixed omc_fixed_sqrt_q16(OMC_Fixed v);
static OMC_U32 omc_submesh_actual_vertex_index(const OMC_Mesh *mesh, const OMC_Submesh *sm, OMC_U32 local_index);
static OMC_Vec3 omc_vec3_sub(OMC_Vec3 a, OMC_Vec3 b);
static OMC_Vec3 omc_vec3_cross(OMC_Vec3 a, OMC_Vec3 b);
static OMC_Vec3 omc_vec3_lerp(OMC_Vec3 a, OMC_Vec3 b, OMC_Fixed t);
static OMC_Fixed omc_vec3_dot(OMC_Vec3 a, OMC_Vec3 b);
static OMC_Vec3 omc_vec3_scale(OMC_Vec3 v, OMC_Fixed s);
static OMC_Vec3 omc_vec3_normalize(OMC_Vec3 v);

static OMC_U32 g_omc_triangulate_indices[OMC_MAX_INDICES];
static OMC_U32 g_omc_vertex_map[OMC_MAX_VERTICES];
static OMC_U8 g_omc_vertex_used[OMC_MAX_VERTICES];
static OMC_Vertex g_omc_vertex_scratch[OMC_MAX_VERTICES];

void omc_mesh_init(OMC_Mesh *mesh)
{
    if (!mesh) return;
    omc_zero_mem(mesh, (unsigned long)sizeof(OMC_Mesh));
    omc_copy_str(mesh->source_format, sizeof(mesh->source_format), "empty");
}

const char *omc_result_string(int code)
{
    switch (code) {
    case OMC_OK: return "OK";
    case OMC_ERR_NULL: return "null pointer";
    case OMC_ERR_CAPACITY: return "static capacity exceeded";
    case OMC_ERR_FORMAT: return "format error";
    case OMC_ERR_IO: return "I/O error";
    case OMC_ERR_UNSUPPORTED: return "unsupported feature";
    default: return "unknown error";
    }
}

static void omc_add_diag(OMC_Mesh *mesh, int code, const char *msg)
{
    OMC_U16 i;
    unsigned long j;
    if (!mesh) return;
    if (mesh->diagnostic_count >= (OMC_U16)OMC_DIAG_MAX) return;
    i = mesh->diagnostic_count;
    mesh->diagnostics[i].code = code;
    mesh->diagnostics[i].message[0] = '\0';
    if (msg) {
        j = 0UL;
        while (msg[j] && j + 1UL < OMC_MAX_NAME) {
            mesh->diagnostics[i].message[j] = msg[j];
            j++;
        }
        mesh->diagnostics[i].message[j] = '\0';
    }
    mesh->diagnostic_count++;
}

void omc_compute_bounds(OMC_Mesh *mesh)
{
    OMC_U32 i;
    OMC_U8 first;
    OMC_Vertex *v;
    OMC_Fixed dx;
    OMC_Fixed dy;
    OMC_Fixed dz;
    OMC_Fixed d2;
    OMC_Fixed max_d2;

    if (!mesh) return;
    first = 1U;
    i = 0UL;
    while (i < mesh->vertex_count) {
        v = &mesh->vertices[i];
        if ((v->flags & OMC_VTX_HAS_POSITION) != 0U) {
            if (first) {
                mesh->bounds_min = v->position;
                mesh->bounds_max = v->position;
                first = 0U;
            } else {
                mesh->bounds_min.x = omc_fixed_min(mesh->bounds_min.x, v->position.x);
                mesh->bounds_min.y = omc_fixed_min(mesh->bounds_min.y, v->position.y);
                mesh->bounds_min.z = omc_fixed_min(mesh->bounds_min.z, v->position.z);
                mesh->bounds_max.x = omc_fixed_max(mesh->bounds_max.x, v->position.x);
                mesh->bounds_max.y = omc_fixed_max(mesh->bounds_max.y, v->position.y);
                mesh->bounds_max.z = omc_fixed_max(mesh->bounds_max.z, v->position.z);
            }
        }
        i++;
    }

    if (!first) {
        mesh->bounds_center.x = (mesh->bounds_min.x + mesh->bounds_max.x) / 2L;
        mesh->bounds_center.y = (mesh->bounds_min.y + mesh->bounds_max.y) / 2L;
        mesh->bounds_center.z = (mesh->bounds_min.z + mesh->bounds_max.z) / 2L;
        max_d2 = 0;
        i = 0UL;
        while (i < mesh->vertex_count) {
            v = &mesh->vertices[i];
            if ((v->flags & OMC_VTX_HAS_POSITION) != 0U) {
                dx = v->position.x - mesh->bounds_center.x;
                dy = v->position.y - mesh->bounds_center.y;
                dz = v->position.z - mesh->bounds_center.z;
                d2 = omc_fixed_add_sat(omc_fixed_mul_q16_safe(dx, dx), omc_fixed_mul_q16_safe(dy, dy));
                d2 = omc_fixed_add_sat(d2, omc_fixed_mul_q16_safe(dz, dz));
                if (d2 > max_d2) max_d2 = d2;
            }
            i++;
        }
        mesh->bounds_radius = omc_fixed_sqrt_q16(max_d2);
        mesh->has_bounds = 1U;
    }
}


static int omc_indices_degenerate(OMC_U32 a, OMC_U32 b, OMC_U32 c)
{
    return a == b || b == c || a == c;
}

static OMC_U8 omc_effective_operation(const OMC_Submesh *sm)
{
    if (!sm || sm->operation_type == 0U) return (OMC_U8)OMC_OP_TRIANGLE_LIST;
    return sm->operation_type;
}

static int omc_operation_is_triangulatable(OMC_U8 op)
{
    return op == (OMC_U8)OMC_OP_TRIANGLE_STRIP ||
           op == (OMC_U8)OMC_OP_TRIANGLE_FAN ||
           op == (OMC_U8)OMC_OP_TRIANGLE_LIST_ADJ ||
           op == (OMC_U8)OMC_OP_TRIANGLE_STRIP_ADJ;
}

static int omc_get_triangle_local_indices(const OMC_Mesh *mesh, const OMC_Submesh *sm, OMC_U32 face, OMC_U32 *a, OMC_U32 *b, OMC_U32 *c)
{
    OMC_U8 op;
    OMC_U32 rel;
    OMC_U32 ia;
    OMC_U32 ib;
    OMC_U32 ic;

    if (!mesh || !sm || !a || !b || !c) return 0;
    op = omc_effective_operation(sm);
    if (op == (OMC_U8)OMC_OP_TRIANGLE_LIST) {
        rel = face * 3UL;
        if (rel + 2UL >= sm->index_count || sm->index_start + rel + 2UL >= mesh->index_count) return 0;
        *a = mesh->indices[sm->index_start + rel];
        *b = mesh->indices[sm->index_start + rel + 1UL];
        *c = mesh->indices[sm->index_start + rel + 2UL];
        return 1;
    }
    if (op == (OMC_U8)OMC_OP_TRIANGLE_STRIP) {
        rel = face;
        if (rel + 2UL >= sm->index_count || sm->index_start + rel + 2UL >= mesh->index_count) return 0;
        ia = mesh->indices[sm->index_start + rel];
        ib = mesh->indices[sm->index_start + rel + 1UL];
        ic = mesh->indices[sm->index_start + rel + 2UL];
        if ((face & 1UL) != 0UL) { *a = ib; *b = ia; *c = ic; }
        else { *a = ia; *b = ib; *c = ic; }
        return 1;
    }
    if (op == (OMC_U8)OMC_OP_TRIANGLE_FAN) {
        rel = face + 1UL;
        if (rel + 1UL >= sm->index_count || sm->index_start + rel + 1UL >= mesh->index_count || sm->index_start >= mesh->index_count) return 0;
        *a = mesh->indices[sm->index_start];
        *b = mesh->indices[sm->index_start + rel];
        *c = mesh->indices[sm->index_start + rel + 1UL];
        return 1;
    }
    if (op == (OMC_U8)OMC_OP_TRIANGLE_LIST_ADJ) {
        rel = face * 6UL;
        if (rel + 4UL >= sm->index_count || sm->index_start + rel + 4UL >= mesh->index_count) return 0;
        *a = mesh->indices[sm->index_start + rel];
        *b = mesh->indices[sm->index_start + rel + 2UL];
        *c = mesh->indices[sm->index_start + rel + 4UL];
        return 1;
    }
    if (op == (OMC_U8)OMC_OP_TRIANGLE_STRIP_ADJ) {
        rel = face * 2UL;
        if (rel + 4UL >= sm->index_count || sm->index_start + rel + 4UL >= mesh->index_count) return 0;
        ia = mesh->indices[sm->index_start + rel];
        ib = mesh->indices[sm->index_start + rel + 2UL];
        ic = mesh->indices[sm->index_start + rel + 4UL];
        if ((face & 1UL) != 0UL) { *a = ib; *b = ia; *c = ic; }
        else { *a = ia; *b = ib; *c = ic; }
        return 1;
    }
    return 0;
}

unsigned long omc_count_triangles(const OMC_Mesh *mesh)
{
    OMC_U16 si;
    OMC_U32 face;
    OMC_U32 count;
    OMC_U32 a;
    OMC_U32 b;
    OMC_U32 c;
    const OMC_Submesh *sm;

    if (!mesh) return 0UL;
    count = 0UL;
    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        face = 0UL;
        while (omc_get_triangle_local_indices(mesh, sm, face, &a, &b, &c)) {
            if (!omc_indices_degenerate(a, b, c)) count++;
            face++;
        }
        si++;
    }
    return (unsigned long)count;
}

static int omc_triangulate_append_raw(OMC_U32 *new_count, OMC_U32 v)
{
    if (!new_count) return OMC_ERR_NULL;
    if (*new_count >= (OMC_U32)OMC_MAX_INDICES) return OMC_ERR_CAPACITY;
    g_omc_triangulate_indices[*new_count] = v;
    *new_count = *new_count + 1UL;
    return OMC_OK;
}

static int omc_triangulate_append_tri(OMC_U32 *new_count, OMC_U32 a, OMC_U32 b, OMC_U32 c, OMC_U32 *skipped)
{
    int r;

    if (!new_count) return OMC_ERR_NULL;
    if (omc_indices_degenerate(a, b, c)) {
        if (skipped) *skipped = *skipped + 1UL;
        return OMC_OK;
    }
    if (*new_count + 3UL > (OMC_U32)OMC_MAX_INDICES) return OMC_ERR_CAPACITY;
    r = omc_triangulate_append_raw(new_count, a); if (r != OMC_OK) return r;
    r = omc_triangulate_append_raw(new_count, b); if (r != OMC_OK) return r;
    return omc_triangulate_append_raw(new_count, c);
}

static int omc_triangulate_copy_range(const OMC_Mesh *mesh, const OMC_Submesh *sm, OMC_U32 *new_count)
{
    OMC_U32 rel;
    int r;

    if (!mesh || !sm || !new_count) return OMC_ERR_NULL;
    rel = 0UL;
    while (rel < sm->index_count && sm->index_start + rel < mesh->index_count) {
        r = omc_triangulate_append_raw(new_count, mesh->indices[sm->index_start + rel]);
        if (r != OMC_OK) return r;
        rel++;
    }
    return OMC_OK;
}

static int omc_triangulate_triangle_range(const OMC_Mesh *mesh, const OMC_Submesh *sm, OMC_U32 *new_count, OMC_U32 *skipped)
{
    OMC_U32 face;
    OMC_U32 a;
    OMC_U32 b;
    OMC_U32 c;
    int r;

    if (!mesh || !sm || !new_count) return OMC_ERR_NULL;
    face = 0UL;
    while (omc_get_triangle_local_indices(mesh, sm, face, &a, &b, &c)) {
        r = omc_triangulate_append_tri(new_count, a, b, c, skipped);
        if (r != OMC_OK) return r;
        face++;
    }
    return OMC_OK;
}

int omc_triangulate_primitives(OMC_Mesh *mesh)
{
    OMC_U16 si;
    OMC_U32 new_count;
    OMC_U32 skipped;
    OMC_U32 new_starts[OMC_MAX_SUBMESHES];
    OMC_U32 new_counts[OMC_MAX_SUBMESHES];
    OMC_U8 new_ops[OMC_MAX_SUBMESHES];
    OMC_U8 converted;
    OMC_U32 i;
    OMC_Submesh *sm;
    const OMC_Submesh *csm;
    int r;

    if (!mesh) return OMC_ERR_NULL;
    if (mesh->submesh_count > (OMC_U16)OMC_MAX_SUBMESHES) return OMC_ERR_CAPACITY;
    new_count = 0UL;
    skipped = 0UL;
    converted = 0U;
    si = 0U;
    while (si < mesh->submesh_count) {
        csm = &mesh->submeshes[si];
        new_starts[si] = new_count;
        new_ops[si] = csm->operation_type;
        if (omc_operation_is_triangulatable(omc_effective_operation(csm))) {
            r = omc_triangulate_triangle_range(mesh, csm, &new_count, &skipped);
            if (r != OMC_OK) return r;
            new_ops[si] = (OMC_U8)OMC_OP_TRIANGLE_LIST;
            converted = 1U;
        } else {
            r = omc_triangulate_copy_range(mesh, csm, &new_count);
            if (r != OMC_OK) return r;
        }
        new_counts[si] = new_count - new_starts[si];
        si++;
    }

    i = 0UL;
    while (i < new_count) {
        mesh->indices[i] = g_omc_triangulate_indices[i];
        i++;
    }
    mesh->index_count = new_count;
    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        sm->index_start = new_starts[si];
        sm->index_count = new_counts[si];
        sm->operation_type = new_ops[si];
        si++;
    }
    if (converted) {
        if (skipped) omc_add_diag(mesh, OMC_OK, "triangulated strip/fan/adj primitives; skipped degenerate triangles");
        else omc_add_diag(mesh, OMC_OK, "triangulated strip/fan/adj primitives to triangle_list");
    }
    return OMC_OK;
}


static int omc_triangle_geometry_degenerate(const OMC_Mesh *mesh, const OMC_Submesh *sm, OMC_U32 la, OMC_U32 lb, OMC_U32 lc)
{
    OMC_U32 ia;
    OMC_U32 ib;
    OMC_U32 ic;
    OMC_Vec3 e1;
    OMC_Vec3 e2;
    OMC_Vec3 cr;

    if (!mesh || !sm) return 1;
    ia = omc_submesh_actual_vertex_index(mesh, sm, la);
    ib = omc_submesh_actual_vertex_index(mesh, sm, lb);
    ic = omc_submesh_actual_vertex_index(mesh, sm, lc);
    if (ia >= mesh->vertex_count || ib >= mesh->vertex_count || ic >= mesh->vertex_count) return 1;
    if ((mesh->vertices[ia].flags & OMC_VTX_HAS_POSITION) == 0U) return 0;
    if ((mesh->vertices[ib].flags & OMC_VTX_HAS_POSITION) == 0U) return 0;
    if ((mesh->vertices[ic].flags & OMC_VTX_HAS_POSITION) == 0U) return 0;
    e1 = omc_vec3_sub(mesh->vertices[ib].position, mesh->vertices[ia].position);
    e2 = omc_vec3_sub(mesh->vertices[ic].position, mesh->vertices[ia].position);
    cr = omc_vec3_cross(e1, e2);
    return cr.x == 0 && cr.y == 0 && cr.z == 0;
}

int omc_remove_degenerate_faces(OMC_Mesh *mesh)
{
    OMC_U16 si;
    OMC_U32 new_count;
    OMC_U32 new_starts[OMC_MAX_SUBMESHES];
    OMC_U32 new_counts[OMC_MAX_SUBMESHES];
    OMC_U8 new_ops[OMC_MAX_SUBMESHES];
    OMC_U32 face;
    OMC_U32 a;
    OMC_U32 b;
    OMC_U32 c;
    OMC_U32 skipped;
    OMC_U32 copied_non_tri;
    OMC_U32 i;
    const OMC_Submesh *sm;
    OMC_Submesh *wsm;
    OMC_U8 op;
    int r;

    if (!mesh) return OMC_ERR_NULL;
    if (mesh->submesh_count > (OMC_U16)OMC_MAX_SUBMESHES) return OMC_ERR_CAPACITY;
    new_count = 0UL;
    skipped = 0UL;
    copied_non_tri = 0UL;

    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        op = omc_effective_operation(sm);
        new_starts[si] = new_count;
        new_ops[si] = sm->operation_type;
        if (op == (OMC_U8)OMC_OP_TRIANGLE_LIST || omc_operation_is_triangulatable(op)) {
            face = 0UL;
            while (omc_get_triangle_local_indices(mesh, sm, face, &a, &b, &c)) {
                if (omc_indices_degenerate(a, b, c) || omc_triangle_geometry_degenerate(mesh, sm, a, b, c)) {
                    skipped++;
                } else {
                    r = omc_triangulate_append_tri(&new_count, a, b, c, (OMC_U32 *)0);
                    if (r != OMC_OK) return r;
                }
                face++;
            }
            new_ops[si] = (OMC_U8)OMC_OP_TRIANGLE_LIST;
        } else {
            copied_non_tri++;
            r = omc_triangulate_copy_range(mesh, sm, &new_count);
            if (r != OMC_OK) return r;
        }
        new_counts[si] = new_count - new_starts[si];
        si++;
    }

    i = 0UL;
    while (i < new_count) {
        mesh->indices[i] = g_omc_triangulate_indices[i];
        i++;
    }
    mesh->index_count = new_count;
    si = 0U;
    while (si < mesh->submesh_count) {
        wsm = &mesh->submeshes[si];
        wsm->index_start = new_starts[si];
        wsm->index_count = new_counts[si];
        wsm->operation_type = new_ops[si];
        si++;
    }
    if (skipped) omc_add_diag(mesh, OMC_OK, "removed degenerate triangle faces");
    if (copied_non_tri) omc_add_diag(mesh, OMC_OK, "non-triangle submeshes copied during degenerate cleanup");
    return OMC_OK;
}

int omc_reverse_winding(OMC_Mesh *mesh)
{
    OMC_U16 si;
    OMC_U32 rel;
    OMC_U32 tmp;
    OMC_Submesh *sm;
    int r;

    if (!mesh) return OMC_ERR_NULL;
    r = omc_triangulate_primitives(mesh);
    if (r != OMC_OK) return r;
    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        if (omc_effective_operation(sm) == (OMC_U8)OMC_OP_TRIANGLE_LIST) {
            rel = 0UL;
            while (rel + 2UL < sm->index_count && sm->index_start + rel + 2UL < mesh->index_count) {
                tmp = mesh->indices[sm->index_start + rel + 1UL];
                mesh->indices[sm->index_start + rel + 1UL] = mesh->indices[sm->index_start + rel + 2UL];
                mesh->indices[sm->index_start + rel + 2UL] = tmp;
                rel += 3UL;
            }
        }
        si++;
    }
    omc_add_diag(mesh, OMC_OK, "reversed triangle winding");
    return OMC_OK;
}

int omc_flip_normals(OMC_Mesh *mesh)
{
    OMC_U32 i;
    if (!mesh) return OMC_ERR_NULL;
    i = 0UL;
    while (i < mesh->vertex_count) {
        if ((mesh->vertices[i].flags & OMC_VTX_HAS_NORMAL) != 0U) {
            mesh->vertices[i].normal.x = -mesh->vertices[i].normal.x;
            mesh->vertices[i].normal.y = -mesh->vertices[i].normal.y;
            mesh->vertices[i].normal.z = -mesh->vertices[i].normal.z;
        }
        if ((mesh->vertices[i].flags & OMC_VTX_HAS_BINORMAL) != 0U) {
            mesh->vertices[i].binormal.x = -mesh->vertices[i].binormal.x;
            mesh->vertices[i].binormal.y = -mesh->vertices[i].binormal.y;
            mesh->vertices[i].binormal.z = -mesh->vertices[i].binormal.z;
        }
        if ((mesh->vertices[i].flags & OMC_VTX_HAS_TANGENT) != 0U && mesh->vertices[i].tangent.w != 0) {
            mesh->vertices[i].tangent.w = -mesh->vertices[i].tangent.w;
        }
        i++;
    }
    omc_add_diag(mesh, OMC_OK, "flipped vertex normal/binormal orientation");
    return OMC_OK;
}

static int omc_operation_is_patch(OMC_U8 op)
{
    return op >= (OMC_U8)OMC_OP_PATCH_1_CONTROL_POINT &&
           op <= (OMC_U8)OMC_OP_PATCH_32_CONTROL_POINT;
}

static OMC_U8 omc_patch_control_count(OMC_U8 op)
{
    if (!omc_operation_is_patch(op)) return 0U;
    return (OMC_U8)(op - (OMC_U8)OMC_OP_PATCH_1_CONTROL_POINT + 1U);
}

static int omc_validate_submesh_operation_indices(const OMC_Submesh *sm)
{
    OMC_U8 op;
    OMC_U8 controls;

    if (!sm) return OMC_ERR_NULL;
    op = sm->operation_type ? sm->operation_type : (OMC_U8)OMC_OP_TRIANGLE_LIST;
    if (op == (OMC_U8)OMC_OP_LINE_LIST) {
        if ((sm->index_count % 2UL) != 0UL) return OMC_ERR_FORMAT;
    } else if (op == (OMC_U8)OMC_OP_LINE_STRIP) {
        if (sm->index_count == 1UL) return OMC_ERR_FORMAT;
    } else if (op == (OMC_U8)OMC_OP_LINE_LIST_ADJ) {
        if ((sm->index_count % 4UL) != 0UL) return OMC_ERR_FORMAT;
    } else if (op == (OMC_U8)OMC_OP_LINE_STRIP_ADJ) {
        if (sm->index_count > 0UL && sm->index_count < 4UL) return OMC_ERR_FORMAT;
    } else if (op == (OMC_U8)OMC_OP_TRIANGLE_LIST) {
        if ((sm->index_count % 3UL) != 0UL) return OMC_ERR_FORMAT;
    } else if (op == (OMC_U8)OMC_OP_TRIANGLE_STRIP || op == (OMC_U8)OMC_OP_TRIANGLE_FAN) {
        if (sm->index_count > 0UL && sm->index_count < 3UL) return OMC_ERR_FORMAT;
    } else if (op == (OMC_U8)OMC_OP_TRIANGLE_LIST_ADJ) {
        if ((sm->index_count % 6UL) != 0UL) return OMC_ERR_FORMAT;
    } else if (op == (OMC_U8)OMC_OP_TRIANGLE_STRIP_ADJ) {
        if (sm->index_count > 0UL && (sm->index_count < 6UL || (sm->index_count % 2UL) != 0UL)) return OMC_ERR_FORMAT;
    } else if (omc_operation_is_patch(op)) {
        controls = omc_patch_control_count(op);
        if (controls == 0U || (sm->index_count % (OMC_U32)controls) != 0UL) return OMC_ERR_FORMAT;
    }
    return OMC_OK;
}

int omc_validate_mesh(const OMC_Mesh *mesh)
{
    OMC_U16 s;
    OMC_U32 i;
    OMC_U32 idx;
    OMC_U32 vcount;
    const OMC_Submesh *sm;
    const OMC_BoneAssignment *ba;
    const OMC_SubmeshBoneAssignment *sba;
    const OMC_LodFace *lf;
    const OMC_Pose *pose;
    const OMC_Animation *anim;
    const OMC_AnimationTrack *track;
    const OMC_AnimationKey *key;
    const OMC_MorphKey *mk;
    const OMC_MorphVertex *mv;

    if (!mesh) return OMC_ERR_NULL;
    if (mesh->vertex_count > (OMC_U32)OMC_MAX_VERTICES) return OMC_ERR_CAPACITY;
    if (mesh->index_count > (OMC_U32)OMC_MAX_INDICES) return OMC_ERR_CAPACITY;
    if (mesh->submesh_count > (OMC_U16)OMC_MAX_SUBMESHES) return OMC_ERR_CAPACITY;
    if (mesh->bone_assignment_count > (OMC_U32)OMC_MAX_BONE_ASSIGNMENTS) return OMC_ERR_CAPACITY;
    if (mesh->submesh_bone_assignment_count > (OMC_U32)OMC_MAX_SUBMESH_BONE_ASSIGNMENTS) return OMC_ERR_CAPACITY;
    if (mesh->pose_count > (OMC_U16)OMC_MAX_POSES) return OMC_ERR_CAPACITY;
    if (mesh->pose_offset_count > (OMC_U32)OMC_MAX_POSE_OFFSETS) return OMC_ERR_CAPACITY;
    if (mesh->animation_count > (OMC_U16)OMC_MAX_ANIMATIONS) return OMC_ERR_CAPACITY;
    if (mesh->track_count > (OMC_U32)OMC_MAX_ANIMATION_TRACKS) return OMC_ERR_CAPACITY;
    if (mesh->key_count > (OMC_U32)OMC_MAX_ANIMATION_KEYS) return OMC_ERR_CAPACITY;
    if (mesh->lod_level_count > (OMC_U16)OMC_MAX_LOD_LEVELS) return OMC_ERR_CAPACITY;
    if (mesh->lod_face_count > (OMC_U32)OMC_MAX_LOD_FACES) return OMC_ERR_CAPACITY;
    if (mesh->extreme_count > (OMC_U32)OMC_MAX_EXTREMES) return OMC_ERR_CAPACITY;
    if (mesh->morph_key_count > (OMC_U32)OMC_MAX_MORPH_KEYS) return OMC_ERR_CAPACITY;
    if (mesh->morph_vertex_count > (OMC_U32)OMC_MAX_MORPH_VERTICES) return OMC_ERR_CAPACITY;
    if (mesh->binary_chunk_count > (OMC_U16)OMC_MAX_BINARY_CHUNKS) return OMC_ERR_CAPACITY;
    if (mesh->edge_count > (OMC_U32)OMC_MAX_EDGES) return OMC_ERR_CAPACITY;
    if (mesh->texture_alias_count > (OMC_U32)OMC_MAX_TEXTURE_ALIASES) return OMC_ERR_CAPACITY;

    i = 0UL;
    while (i < mesh->texture_alias_count) {
        if (mesh->texture_aliases[i].submesh_index >= mesh->submesh_count) return OMC_ERR_FORMAT;
        i++;
    }

    i = 0UL;
    while (i < mesh->vertex_count) {
        if (mesh->vertices[i].blend_count > 4U) return OMC_ERR_FORMAT;
        i++;
    }

    s = 0U;
    while (s < mesh->submesh_count) {
        sm = &mesh->submeshes[s];
        if (sm->index_start + sm->index_count > mesh->index_count) return OMC_ERR_FORMAT;
        if (omc_validate_submesh_operation_indices(sm) != OMC_OK) return OMC_ERR_FORMAT;
        if (!sm->use_shared_vertices) {
            if (sm->vertex_start + sm->vertex_count > mesh->vertex_count) return OMC_ERR_FORMAT;
            vcount = sm->vertex_count;
        } else {
            vcount = mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count;
        }
        i = sm->index_start;
        while (i < sm->index_start + sm->index_count && i < mesh->index_count) {
            idx = mesh->indices[i];
            if (idx >= vcount) return OMC_ERR_FORMAT;
            i++;
        }
        s++;
    }

    i = 0UL;
    while (i < mesh->bone_assignment_count) {
        ba = &mesh->bone_assignments[i];
        vcount = mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count;
        if (ba->vertex_index >= vcount) return OMC_ERR_FORMAT;
        i++;
    }

    i = 0UL;
    while (i < mesh->submesh_bone_assignment_count) {
        sba = &mesh->submesh_bone_assignments[i];
        if (sba->submesh_index >= mesh->submesh_count) return OMC_ERR_FORMAT;
        sm = &mesh->submeshes[sba->submesh_index];
        if (sba->assignment.vertex_index >= sm->vertex_count) return OMC_ERR_FORMAT;
        i++;
    }

    i = 0UL;
    while (i < mesh->pose_count) {
        pose = &mesh->poses[i];
        if (pose->offset_start + pose->offset_count > mesh->pose_offset_count) return OMC_ERR_FORMAT;
        if (pose->target_is_submesh && pose->target_index >= mesh->submesh_count) return OMC_ERR_FORMAT;
        i++;
    }

    i = 0UL;
    while (i < mesh->pose_offset_count) {
        if (mesh->pose_offsets[i].pose_index >= mesh->pose_count) return OMC_ERR_FORMAT;
        pose = &mesh->poses[mesh->pose_offsets[i].pose_index];
        if (pose->target_is_submesh) {
            if (pose->target_index >= mesh->submesh_count) return OMC_ERR_FORMAT;
            sm = &mesh->submeshes[pose->target_index];
            vcount = sm->use_shared_vertices ? (mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count) : sm->vertex_count;
        } else {
            vcount = mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count;
        }
        if (mesh->pose_offsets[i].vertex_index >= vcount) return OMC_ERR_FORMAT;
        i++;
    }

    i = 0UL;
    while (i < mesh->animation_count) {
        anim = &mesh->animations[i];
        if (anim->track_start + anim->track_count > mesh->track_count) return OMC_ERR_FORMAT;
        i++;
    }

    i = 0UL;
    while (i < mesh->track_count) {
        track = &mesh->tracks[i];
        if (track->animation_index >= mesh->animation_count) return OMC_ERR_FORMAT;
        if (track->target_is_submesh && track->target_index >= mesh->submesh_count) return OMC_ERR_FORMAT;
        if (track->is_pose_track) {
            if (track->key_start + track->key_count > mesh->key_count) return OMC_ERR_FORMAT;
        } else {
            if (track->morph_key_start + track->morph_key_count > mesh->morph_key_count) return OMC_ERR_FORMAT;
        }
        i++;
    }

    i = 0UL;
    while (i < mesh->key_count) {
        key = &mesh->keys[i];
        if (key->track_index >= mesh->track_count) return OMC_ERR_FORMAT;
        if (key->pose_index >= mesh->pose_count) return OMC_ERR_FORMAT;
        i++;
    }

    i = 0UL;
    while (i < mesh->lod_face_count) {
        lf = &mesh->lod_faces[i];
        if (lf->lod_index >= mesh->lod_level_count) return OMC_ERR_FORMAT;
        if (lf->submesh_index >= mesh->submesh_count) return OMC_ERR_FORMAT;
        sm = &mesh->submeshes[lf->submesh_index];
        vcount = sm->use_shared_vertices ? (mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count) : sm->vertex_count;
        if (lf->v1 >= vcount || lf->v2 >= vcount || lf->v3 >= vcount) return OMC_ERR_FORMAT;
        i++;
    }

    i = 0UL;
    while (i < mesh->extreme_count) {
        if (mesh->extremes[i].submesh_index >= mesh->submesh_count) return OMC_ERR_FORMAT;
        i++;
    }


    i = 0UL;
    while (i < mesh->morph_key_count) {
        mk = &mesh->morph_keys[i];
        if (mk->track_index >= mesh->track_count) return OMC_ERR_FORMAT;
        if (mk->vertex_start + mk->vertex_count > mesh->morph_vertex_count) return OMC_ERR_FORMAT;
        track = &mesh->tracks[mk->track_index];
        if (track->target_is_submesh) {
            if (track->target_index >= mesh->submesh_count) return OMC_ERR_FORMAT;
            sm = &mesh->submeshes[track->target_index];
            vcount = sm->use_shared_vertices ? (mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count) : sm->vertex_count;
        } else {
            vcount = mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count;
        }
        if (mk->vertex_count > vcount) return OMC_ERR_FORMAT;
        i++;
    }

    i = 0UL;
    while (i < mesh->morph_vertex_count) {
        mv = &mesh->morph_vertices[i];
        if (mv->morph_key_index >= mesh->morph_key_count) return OMC_ERR_FORMAT;
        mk = &mesh->morph_keys[mv->morph_key_index];
        if (mv->local_index >= mk->vertex_count) return OMC_ERR_FORMAT;
        i++;
    }

    i = 0UL;
    while (i < mesh->edge_count) {
        if (mesh->edges[i].submesh_index >= mesh->submesh_count) return OMC_ERR_FORMAT;
        if (mesh->edges[i].v1 >= mesh->vertex_count || mesh->edges[i].v2 >= mesh->vertex_count) return OMC_ERR_FORMAT;
        i++;
    }

    return OMC_OK;
}

int omc_read_entire_file(const char *path, unsigned char *buffer, unsigned long cap, unsigned long *out_size)
{
    FILE *f;
    unsigned long n;
    int c;

    if (!path || !buffer || !out_size) return OMC_ERR_NULL;
    *out_size = 0UL;
    f = fopen(path, "rb");
    if (!f) return OMC_ERR_IO;

    n = 0UL;
    while ((c = fgetc(f)) != EOF) {
        if (n >= cap) {
            fclose(f);
            return OMC_ERR_CAPACITY;
        }
        buffer[n++] = (unsigned char)c;
    }
    fclose(f);
    *out_size = n;
    return OMC_OK;
}

int omc_load_file_auto(OMC_Mesh *mesh, const char *path, unsigned char *buffer, unsigned long cap)
{
    unsigned long size;
    int r;

    if (!mesh || !path || !buffer) return OMC_ERR_NULL;
    r = omc_read_entire_file(path, buffer, cap, &size);
    if (r != OMC_OK) return r;

    if (size >= 5UL && buffer[0] == '<') {
        return omc_load_mesh_xml_from_memory(mesh, (const char *)buffer, size);
    }
    if (size >= 8UL && buffer[0] == 0x00U && buffer[1] == 0x10U) {
        return omc_load_mesh_binary_from_memory(mesh, buffer, size);
    }
    if (size >= 8UL && buffer[0] == 0x10U && buffer[1] == 0x00U) {
        return omc_load_mesh_binary_from_memory(mesh, buffer, size);
    }

    /* Some XML files begin with BOM or whitespace. */
    return omc_load_mesh_xml_from_memory(mesh, (const char *)buffer, size);
}

/* exported inside parser modules through this small backdoor for diagnostics */
void omc__diag_public(OMC_Mesh *mesh, int code, const char *msg)
{
    omc_add_diag(mesh, code, msg);
}

void omc__copy_str_public(char *dst, unsigned long cap, const char *src)
{
    omc_copy_str(dst, cap, src);
}

static OMC_Fixed omc_fixed_mul_q16_safe(OMC_Fixed a, OMC_Fixed b)
{
    int neg;
    unsigned long au;
    unsigned long bu;
    unsigned long ah;
    unsigned long al;
    unsigned long bh;
    unsigned long bl;
    unsigned long high;
    unsigned long part;
    unsigned long r;
    unsigned long limit;

    limit = 2147483647UL;
    neg = 0;
    if (a < 0) {
        neg = !neg;
        if (a == (OMC_Fixed)(-2147483647L - 1L)) au = 2147483648UL;
        else au = (unsigned long)(-a);
    } else au = (unsigned long)a;
    if (b < 0) {
        neg = !neg;
        if (b == (OMC_Fixed)(-2147483647L - 1L)) bu = 2147483648UL;
        else bu = (unsigned long)(-b);
    } else bu = (unsigned long)b;
    if (au > limit) au = limit;
    if (bu > limit) bu = limit;

    ah = au >> 16;
    al = au & 65535UL;
    bh = bu >> 16;
    bl = bu & 65535UL;

    high = ah * bh;
    if (high > 32767UL) {
        r = limit;
    } else {
        r = high << 16;
        part = ah * bl;
        if (part > limit - r) r = limit;
        else r += part;
        part = al * bh;
        if (part > limit - r) r = limit;
        else r += part;
        part = (al * bl) >> 16;
        if (part > limit - r) r = limit;
        else r += part;
    }
    return neg ? -(OMC_Fixed)r : (OMC_Fixed)r;
}

static OMC_Fixed omc_fixed_add_sat(OMC_Fixed a, OMC_Fixed b)
{
    if (b > 0 && a > (OMC_Fixed)2147483647L - b) return (OMC_Fixed)2147483647L;
    if (b < 0 && a < (OMC_Fixed)-2147483647L - b) return (OMC_Fixed)-2147483647L;
    return a + b;
}

static OMC_Fixed omc_fixed_sub_sat(OMC_Fixed a, OMC_Fixed b)
{
    if (b == (OMC_Fixed)(-2147483647L - 1L)) {
        return a >= 0 ? (OMC_Fixed)2147483647L : omc_fixed_add_sat(a, (OMC_Fixed)2147483647L);
    }
    return omc_fixed_add_sat(a, -b);
}

static OMC_Fixed omc_fixed_clamp01(OMC_Fixed v)
{
    if (v < 0) return 0;
    if (v > OMC_FIXED_ONE) return OMC_FIXED_ONE;
    return v;
}

static OMC_Fixed omc_fixed_lerp_q16(OMC_Fixed a, OMC_Fixed b, OMC_Fixed t)
{
    t = omc_fixed_clamp01(t);
    return omc_fixed_add_sat(a, omc_fixed_mul_q16_safe(omc_fixed_sub_sat(b, a), t));
}

static OMC_Fixed omc_fixed_div_q16_safe(OMC_Fixed a, OMC_Fixed b)
{
    int neg;
    unsigned long ua;
    unsigned long ub;
    unsigned long whole;
    unsigned long rem;
    unsigned long q;
    int bit;

    if (b == 0) return 0;
    neg = 0;
    if (a < 0) { neg = !neg; ua = (unsigned long)(-a); } else ua = (unsigned long)a;
    if (b < 0) { neg = !neg; ub = (unsigned long)(-b); } else ub = (unsigned long)b;
    if (ub == 0UL) return 0;
    whole = ua / ub;
    rem = ua % ub;
    if (whole > 32767UL) return neg ? (OMC_Fixed)-2147483647L : (OMC_Fixed)2147483647L;
    q = whole << OMC_FIXED_SHIFT;
    bit = OMC_FIXED_SHIFT - 1;
    while (bit >= 0) {
        rem <<= 1;
        if (rem >= ub) {
            rem -= ub;
            q |= (1UL << bit);
        }
        bit--;
    }
    if (q > 2147483647UL) q = 2147483647UL;
    return neg ? -(OMC_Fixed)q : (OMC_Fixed)q;
}

static OMC_Fixed omc_fixed_sqrt_q16(OMC_Fixed v)
{
    OMC_Fixed lo;
    OMC_Fixed hi;
    OMC_Fixed mid;
    OMC_Fixed sq;
    int i;

    if (v <= 0) return 0;
    lo = 0;
    hi = (v > OMC_FIXED_ONE) ? v : OMC_FIXED_ONE;
    if (hi > (OMC_Fixed)(32767L * OMC_FIXED_ONE)) hi = (OMC_Fixed)(32767L * OMC_FIXED_ONE);
    i = 0;
    while (i < 32) {
        mid = lo + ((hi - lo) / 2L);
        sq = omc_fixed_mul_q16_safe(mid, mid);
        if (sq <= v) lo = mid;
        else hi = mid;
        i++;
    }
    return lo;
}

static OMC_Vec3 omc_vec3_sub(OMC_Vec3 a, OMC_Vec3 b)
{
    OMC_Vec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}

static OMC_Vec3 omc_vec3_cross(OMC_Vec3 a, OMC_Vec3 b)
{
    OMC_Vec3 r;
    r.x = omc_fixed_mul_q16_safe(a.y, b.z) - omc_fixed_mul_q16_safe(a.z, b.y);
    r.y = omc_fixed_mul_q16_safe(a.z, b.x) - omc_fixed_mul_q16_safe(a.x, b.z);
    r.z = omc_fixed_mul_q16_safe(a.x, b.y) - omc_fixed_mul_q16_safe(a.y, b.x);
    return r;
}

static OMC_Fixed omc_vec3_dot(OMC_Vec3 a, OMC_Vec3 b)
{
    OMC_Fixed r;
    r = omc_fixed_mul_q16_safe(a.x, b.x);
    r = omc_fixed_add_sat(r, omc_fixed_mul_q16_safe(a.y, b.y));
    r = omc_fixed_add_sat(r, omc_fixed_mul_q16_safe(a.z, b.z));
    return r;
}

static OMC_Vec3 omc_vec3_scale(OMC_Vec3 v, OMC_Fixed s)
{
    OMC_Vec3 r;
    r.x = omc_fixed_mul_q16_safe(v.x, s);
    r.y = omc_fixed_mul_q16_safe(v.y, s);
    r.z = omc_fixed_mul_q16_safe(v.z, s);
    return r;
}

static OMC_Vec3 omc_vec3_lerp(OMC_Vec3 a, OMC_Vec3 b, OMC_Fixed t)
{
    OMC_Vec3 r;
    r.x = omc_fixed_lerp_q16(a.x, b.x, t);
    r.y = omc_fixed_lerp_q16(a.y, b.y, t);
    r.z = omc_fixed_lerp_q16(a.z, b.z, t);
    return r;
}

static OMC_Vec3 omc_vec3_normalize(OMC_Vec3 v)
{
    OMC_Fixed len2;
    OMC_Fixed len;
    OMC_Vec3 r;

    len2 = omc_fixed_add_sat(omc_fixed_mul_q16_safe(v.x, v.x), omc_fixed_mul_q16_safe(v.y, v.y));
    len2 = omc_fixed_add_sat(len2, omc_fixed_mul_q16_safe(v.z, v.z));
    len = omc_fixed_sqrt_q16(len2);
    if (len == 0) {
        r.x = 0; r.y = OMC_FIXED_ONE; r.z = 0;
        return r;
    }
    r.x = omc_fixed_div_q16_safe(v.x, len);
    r.y = omc_fixed_div_q16_safe(v.y, len);
    r.z = omc_fixed_div_q16_safe(v.z, len);
    return r;
}

static OMC_U32 omc_submesh_actual_vertex_index(const OMC_Mesh *mesh, const OMC_Submesh *sm, OMC_U32 local_index)
{
    (void)mesh;
    if (!sm) return local_index;
    if (sm->use_shared_vertices) return local_index;
    return sm->vertex_start + local_index;
}

static int omc_add_unique_extreme(OMC_Mesh *mesh, OMC_U16 submesh_index, OMC_Vec3 p)
{
    OMC_U32 i;
    OMC_Extreme *ex;

    i = 0UL;
    while (i < mesh->extreme_count) {
        if (mesh->extremes[i].submesh_index == submesh_index &&
            mesh->extremes[i].position.x == p.x &&
            mesh->extremes[i].position.y == p.y &&
            mesh->extremes[i].position.z == p.z) {
            return OMC_OK;
        }
        i++;
    }
    if (mesh->extreme_count >= (OMC_U32)OMC_MAX_EXTREMES) {
        omc_add_diag(mesh, OMC_ERR_CAPACITY, "extreme capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    ex = &mesh->extremes[mesh->extreme_count++];
    ex->submesh_index = submesh_index;
    ex->position = p;
    return OMC_OK;
}

int omc_merge_texcoord_sets(OMC_Mesh *mesh, OMC_U8 dst_set, OMC_U8 src_set)
{
    OMC_U32 i;
    OMC_U8 j;
    OMC_Vertex *v;

    if (!mesh) return OMC_ERR_NULL;
    if (dst_set >= (OMC_U8)OMC_TEXCOORD_SETS || src_set >= (OMC_U8)OMC_TEXCOORD_SETS) return OMC_ERR_FORMAT;
    if ((OMC_U8)(dst_set + 1U) != src_set) {
        omc_add_diag(mesh, OMC_ERR_UNSUPPORTED, "merge texcoords expects adjacent sets like OgreXMLConverter -merge");
        return OMC_ERR_UNSUPPORTED;
    }

    i = 0UL;
    while (i < mesh->vertex_count) {
        v = &mesh->vertices[i];
        if (v->texcoord_count > src_set) {
            v->texcoord_w[dst_set] = v->texcoord[src_set].u;
            v->texcoord_x[dst_set] = v->texcoord[src_set].v;
            v->texcoord_dims[dst_set] = 4U;
            j = src_set;
            while ((unsigned long)j + 1UL < (unsigned long)v->texcoord_count && (unsigned long)j + 1UL < (unsigned long)OMC_TEXCOORD_SETS) {
                v->texcoord[j] = v->texcoord[j + 1U];
                v->texcoord_w[j] = v->texcoord_w[j + 1U];
                v->texcoord_x[j] = v->texcoord_x[j + 1U];
                v->texcoord_dims[j] = v->texcoord_dims[j + 1U];
                j++;
            }
            if (v->texcoord_count > 0U) v->texcoord_count--;
            v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_TEXCOORD);
        }
        i++;
    }
    omc_add_diag(mesh, OMC_OK, "merged adjacent texcoord sets into 4D texcoord");
    return OMC_OK;
}

int omc_generate_extremes(OMC_Mesh *mesh, OMC_U16 max_per_submesh)
{
    OMC_U16 si;
    OMC_U32 i;
    OMC_U32 local;
    OMC_U32 actual;
    OMC_U32 picked[6];
    OMC_U32 count;
    OMC_U32 added;
    const OMC_Submesh *sm;
    const OMC_Vertex *v;
    OMC_Vec3 p;
    int first;
    int axis;
    int r;

    if (!mesh) return OMC_ERR_NULL;
    if (max_per_submesh == 0U) { mesh->extreme_count = 0UL; return OMC_OK; }
    if (max_per_submesh > 6U) max_per_submesh = 6U;
    mesh->extreme_count = 0UL;

    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        count = sm->index_count;
        first = 1;
        axis = 0;
        while (axis < 6) { picked[axis] = 0UL; axis++; }
        i = 0UL;
        while (i < count && sm->index_start + i < mesh->index_count) {
            local = mesh->indices[sm->index_start + i];
            actual = omc_submesh_actual_vertex_index(mesh, sm, local);
            if (actual < mesh->vertex_count && (mesh->vertices[actual].flags & OMC_VTX_HAS_POSITION) != 0U) {
                v = &mesh->vertices[actual];
                if (first) {
                    axis = 0;
                    while (axis < 6) { picked[axis] = actual; axis++; }
                    first = 0;
                } else {
                    if (v->position.x < mesh->vertices[picked[0]].position.x) picked[0] = actual;
                    if (v->position.x > mesh->vertices[picked[1]].position.x) picked[1] = actual;
                    if (v->position.y < mesh->vertices[picked[2]].position.y) picked[2] = actual;
                    if (v->position.y > mesh->vertices[picked[3]].position.y) picked[3] = actual;
                    if (v->position.z < mesh->vertices[picked[4]].position.z) picked[4] = actual;
                    if (v->position.z > mesh->vertices[picked[5]].position.z) picked[5] = actual;
                }
            }
            i++;
        }
        if (first && sm->vertex_count > 0UL) {
            i = 0UL;
            while (i < sm->vertex_count) {
                actual = omc_submesh_actual_vertex_index(mesh, sm, i);
                if (actual < mesh->vertex_count && (mesh->vertices[actual].flags & OMC_VTX_HAS_POSITION) != 0U) {
                    if (first) {
                        axis = 0;
                        while (axis < 6) { picked[axis] = actual; axis++; }
                        first = 0;
                    } else {
                        v = &mesh->vertices[actual];
                        if (v->position.x < mesh->vertices[picked[0]].position.x) picked[0] = actual;
                        if (v->position.x > mesh->vertices[picked[1]].position.x) picked[1] = actual;
                        if (v->position.y < mesh->vertices[picked[2]].position.y) picked[2] = actual;
                        if (v->position.y > mesh->vertices[picked[3]].position.y) picked[3] = actual;
                        if (v->position.z < mesh->vertices[picked[4]].position.z) picked[4] = actual;
                        if (v->position.z > mesh->vertices[picked[5]].position.z) picked[5] = actual;
                    }
                }
                i++;
            }
        }
        if (!first) {
            added = 0UL;
            axis = 0;
            while (axis < 6 && added < (OMC_U32)max_per_submesh) {
                p = mesh->vertices[picked[axis]].position;
                r = omc_add_unique_extreme(mesh, si, p);
                if (r != OMC_OK) return r;
                added++;
                axis++;
            }
        }
        si++;
    }
    omc_add_diag(mesh, OMC_OK, "generated fixed-capacity submesh extremes");
    return OMC_OK;
}

int omc_recalculate_normals(OMC_Mesh *mesh)
{
    OMC_U16 si;
    OMC_U32 i;
    OMC_U32 a;
    OMC_U32 b;
    OMC_U32 c;
    OMC_U32 ia;
    OMC_U32 ib;
    OMC_U32 ic;
    OMC_U32 face;
    OMC_Vec3 e1;
    OMC_Vec3 e2;
    OMC_Vec3 n;
    OMC_Submesh *sm;

    if (!mesh) return OMC_ERR_NULL;
    i = 0UL;
    while (i < mesh->vertex_count) {
        mesh->vertices[i].normal.x = 0;
        mesh->vertices[i].normal.y = 0;
        mesh->vertices[i].normal.z = 0;
        i++;
    }
    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        face = 0UL;
        while (omc_get_triangle_local_indices(mesh, sm, face, &a, &b, &c)) {
            ia = omc_submesh_actual_vertex_index(mesh, sm, a);
            ib = omc_submesh_actual_vertex_index(mesh, sm, b);
            ic = omc_submesh_actual_vertex_index(mesh, sm, c);
            if (ia < mesh->vertex_count && ib < mesh->vertex_count && ic < mesh->vertex_count) {
                e1 = omc_vec3_sub(mesh->vertices[ib].position, mesh->vertices[ia].position);
                e2 = omc_vec3_sub(mesh->vertices[ic].position, mesh->vertices[ia].position);
                n = omc_vec3_normalize(omc_vec3_cross(e1, e2));
                mesh->vertices[ia].normal.x = omc_fixed_add_sat(mesh->vertices[ia].normal.x, n.x);
                mesh->vertices[ia].normal.y = omc_fixed_add_sat(mesh->vertices[ia].normal.y, n.y);
                mesh->vertices[ia].normal.z = omc_fixed_add_sat(mesh->vertices[ia].normal.z, n.z);
                mesh->vertices[ib].normal.x = omc_fixed_add_sat(mesh->vertices[ib].normal.x, n.x);
                mesh->vertices[ib].normal.y = omc_fixed_add_sat(mesh->vertices[ib].normal.y, n.y);
                mesh->vertices[ib].normal.z = omc_fixed_add_sat(mesh->vertices[ib].normal.z, n.z);
                mesh->vertices[ic].normal.x = omc_fixed_add_sat(mesh->vertices[ic].normal.x, n.x);
                mesh->vertices[ic].normal.y = omc_fixed_add_sat(mesh->vertices[ic].normal.y, n.y);
                mesh->vertices[ic].normal.z = omc_fixed_add_sat(mesh->vertices[ic].normal.z, n.z);
            }
            face++;
        }
        si++;
    }
    i = 0UL;
    while (i < mesh->vertex_count) {
        n = omc_vec3_normalize(mesh->vertices[i].normal);
        mesh->vertices[i].normal = n;
        mesh->vertices[i].flags = (OMC_U16)(mesh->vertices[i].flags | OMC_VTX_HAS_NORMAL);
        i++;
    }
    omc_add_diag(mesh, OMC_OK, "recalculated vertex normals");
    return OMC_OK;
}

int omc_generate_tangents(OMC_Mesh *mesh, OMC_U8 texcoord_set, OMC_U8 tangent_dimensions)
{
    OMC_U16 si;
    OMC_U32 i;
    OMC_U32 ia;
    OMC_U32 ib;
    OMC_U32 ic;
    OMC_U32 la;
    OMC_U32 lb;
    OMC_U32 lc;
    OMC_U32 face;
    OMC_Vertex *va;
    OMC_Vertex *vb;
    OMC_Vertex *vc;
    OMC_Submesh *sm;
    OMC_Vec3 e1;
    OMC_Vec3 e2;
    OMC_Vec3 t;
    OMC_Vec3 bvec;
    OMC_Vec3 n;
    OMC_Vec3 cr;
    OMC_Fixed d;
    OMC_Fixed du1;
    OMC_Fixed dv1;
    OMC_Fixed du2;
    OMC_Fixed dv2;
    OMC_Fixed denom;

    if (!mesh) return OMC_ERR_NULL;
    if (texcoord_set >= (OMC_U8)OMC_TEXCOORD_SETS) return OMC_ERR_FORMAT;
    if (tangent_dimensions != 3U && tangent_dimensions != 4U) tangent_dimensions = 4U;
    i = 0UL;
    while (i < mesh->vertex_count) {
        mesh->vertices[i].tangent.x = 0;
        mesh->vertices[i].tangent.y = 0;
        mesh->vertices[i].tangent.z = 0;
        mesh->vertices[i].tangent.w = OMC_FIXED_ONE;
        mesh->vertices[i].binormal.x = 0;
        mesh->vertices[i].binormal.y = 0;
        mesh->vertices[i].binormal.z = 0;
        i++;
    }
    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        face = 0UL;
        while (omc_get_triangle_local_indices(mesh, sm, face, &la, &lb, &lc)) {
            ia = omc_submesh_actual_vertex_index(mesh, sm, la);
            ib = omc_submesh_actual_vertex_index(mesh, sm, lb);
            ic = omc_submesh_actual_vertex_index(mesh, sm, lc);
            if (ia < mesh->vertex_count && ib < mesh->vertex_count && ic < mesh->vertex_count) {
                va = &mesh->vertices[ia]; vb = &mesh->vertices[ib]; vc = &mesh->vertices[ic];
                if (va->texcoord_count > texcoord_set && vb->texcoord_count > texcoord_set && vc->texcoord_count > texcoord_set) {
                    e1 = omc_vec3_sub(vb->position, va->position);
                    e2 = omc_vec3_sub(vc->position, va->position);
                    du1 = vb->texcoord[texcoord_set].u - va->texcoord[texcoord_set].u;
                    dv1 = vb->texcoord[texcoord_set].v - va->texcoord[texcoord_set].v;
                    du2 = vc->texcoord[texcoord_set].u - va->texcoord[texcoord_set].u;
                    dv2 = vc->texcoord[texcoord_set].v - va->texcoord[texcoord_set].v;
                    denom = omc_fixed_mul_q16_safe(du1, dv2) - omc_fixed_mul_q16_safe(du2, dv1);
                    if (denom != 0) {
                        t.x = omc_fixed_div_q16_safe(omc_fixed_mul_q16_safe(e1.x, dv2) - omc_fixed_mul_q16_safe(e2.x, dv1), denom);
                        t.y = omc_fixed_div_q16_safe(omc_fixed_mul_q16_safe(e1.y, dv2) - omc_fixed_mul_q16_safe(e2.y, dv1), denom);
                        t.z = omc_fixed_div_q16_safe(omc_fixed_mul_q16_safe(e1.z, dv2) - omc_fixed_mul_q16_safe(e2.z, dv1), denom);
                        bvec.x = omc_fixed_div_q16_safe(omc_fixed_mul_q16_safe(e2.x, du1) - omc_fixed_mul_q16_safe(e1.x, du2), denom);
                        bvec.y = omc_fixed_div_q16_safe(omc_fixed_mul_q16_safe(e2.y, du1) - omc_fixed_mul_q16_safe(e1.y, du2), denom);
                        bvec.z = omc_fixed_div_q16_safe(omc_fixed_mul_q16_safe(e2.z, du1) - omc_fixed_mul_q16_safe(e1.z, du2), denom);
                        t = omc_vec3_normalize(t);
                        bvec = omc_vec3_normalize(bvec);
                        va->tangent.x = omc_fixed_add_sat(va->tangent.x, t.x);
                        va->tangent.y = omc_fixed_add_sat(va->tangent.y, t.y);
                        va->tangent.z = omc_fixed_add_sat(va->tangent.z, t.z);
                        va->binormal.x = omc_fixed_add_sat(va->binormal.x, bvec.x);
                        va->binormal.y = omc_fixed_add_sat(va->binormal.y, bvec.y);
                        va->binormal.z = omc_fixed_add_sat(va->binormal.z, bvec.z);
                        vb->tangent.x = omc_fixed_add_sat(vb->tangent.x, t.x);
                        vb->tangent.y = omc_fixed_add_sat(vb->tangent.y, t.y);
                        vb->tangent.z = omc_fixed_add_sat(vb->tangent.z, t.z);
                        vb->binormal.x = omc_fixed_add_sat(vb->binormal.x, bvec.x);
                        vb->binormal.y = omc_fixed_add_sat(vb->binormal.y, bvec.y);
                        vb->binormal.z = omc_fixed_add_sat(vb->binormal.z, bvec.z);
                        vc->tangent.x = omc_fixed_add_sat(vc->tangent.x, t.x);
                        vc->tangent.y = omc_fixed_add_sat(vc->tangent.y, t.y);
                        vc->tangent.z = omc_fixed_add_sat(vc->tangent.z, t.z);
                        vc->binormal.x = omc_fixed_add_sat(vc->binormal.x, bvec.x);
                        vc->binormal.y = omc_fixed_add_sat(vc->binormal.y, bvec.y);
                        vc->binormal.z = omc_fixed_add_sat(vc->binormal.z, bvec.z);
                    }
                }
            }
            face++;
        }
        si++;
    }
    i = 0UL;
    while (i < mesh->vertex_count) {
        t.x = mesh->vertices[i].tangent.x;
        t.y = mesh->vertices[i].tangent.y;
        t.z = mesh->vertices[i].tangent.z;
        if ((mesh->vertices[i].flags & OMC_VTX_HAS_NORMAL) != 0U) {
            n = mesh->vertices[i].normal;
            d = omc_vec3_dot(n, t);
            t = omc_vec3_sub(t, omc_vec3_scale(n, d));
        }
        t = omc_vec3_normalize(t);
        mesh->vertices[i].tangent.x = t.x;
        mesh->vertices[i].tangent.y = t.y;
        mesh->vertices[i].tangent.z = t.z;
        mesh->vertices[i].tangent.w = OMC_FIXED_ONE;
        mesh->vertices[i].flags = (OMC_U16)(mesh->vertices[i].flags | OMC_VTX_HAS_TANGENT);
        bvec = omc_vec3_normalize(mesh->vertices[i].binormal);
        if ((mesh->vertices[i].flags & OMC_VTX_HAS_NORMAL) != 0U) {
            cr = omc_vec3_cross(mesh->vertices[i].normal, t);
            if (omc_vec3_dot(cr, bvec) < 0) mesh->vertices[i].tangent.w = -OMC_FIXED_ONE;
            cr = omc_vec3_normalize(cr);
            if (mesh->vertices[i].tangent.w < 0) cr = omc_vec3_scale(cr, -OMC_FIXED_ONE);
            mesh->vertices[i].binormal = cr;
            mesh->vertices[i].flags = (OMC_U16)(mesh->vertices[i].flags | OMC_VTX_HAS_BINORMAL);
        }
        if (tangent_dimensions == 3U) mesh->vertices[i].tangent.w = 0;
        i++;
    }
    omc_add_diag(mesh, OMC_OK, "generated tangent/binormal vectors");
    return OMC_OK;
}

static int omc_add_edge(OMC_Mesh *mesh, OMC_U16 submesh_index, OMC_U32 a, OMC_U32 b, OMC_U32 face_index)
{
    OMC_U32 lo;
    OMC_U32 hi;
    OMC_U32 i;
    OMC_Edge *e;

    if (a < b) { lo = a; hi = b; }
    else { lo = b; hi = a; }
    i = 0UL;
    while (i < mesh->edge_count) {
        e = &mesh->edges[i];
        if (e->submesh_index == submesh_index && e->v1 == lo && e->v2 == hi) {
            if (e->face_b == 0xffffffffUL) e->face_b = face_index;
            e->boundary = 0U;
            return OMC_OK;
        }
        i++;
    }
    if (mesh->edge_count >= (OMC_U32)OMC_MAX_EDGES) {
        omc_add_diag(mesh, OMC_ERR_CAPACITY, "edge capacity exceeded");
        return OMC_ERR_CAPACITY;
    }
    e = &mesh->edges[mesh->edge_count++];
    e->submesh_index = submesh_index;
    e->v1 = lo;
    e->v2 = hi;
    e->face_a = face_index;
    e->face_b = 0xffffffffUL;
    e->boundary = 1U;
    return OMC_OK;
}

static int omc_build_edges_for_local_triangle(OMC_Mesh *mesh, OMC_U16 si, const OMC_Submesh *sm, OMC_U32 face, OMC_U32 la, OMC_U32 lb, OMC_U32 lc)
{
    OMC_U32 a;
    OMC_U32 b;
    OMC_U32 c;
    int r;

    if (!mesh || !sm) return OMC_ERR_NULL;
    if (omc_indices_degenerate(la, lb, lc)) return OMC_OK;
    a = omc_submesh_actual_vertex_index(mesh, sm, la);
    b = omc_submesh_actual_vertex_index(mesh, sm, lb);
    c = omc_submesh_actual_vertex_index(mesh, sm, lc);
    if (a < mesh->vertex_count && b < mesh->vertex_count && c < mesh->vertex_count) {
        r = omc_add_edge(mesh, si, a, b, face);
        if (r != OMC_OK) return r;
        r = omc_add_edge(mesh, si, b, c, face);
        if (r != OMC_OK) return r;
        r = omc_add_edge(mesh, si, c, a, face);
        if (r != OMC_OK) return r;
    }
    return OMC_OK;
}

int omc_build_edge_list(OMC_Mesh *mesh)
{
    OMC_U16 si;
    OMC_U32 face;
    OMC_U32 a;
    OMC_U32 b;
    OMC_U32 c;
    OMC_Submesh *sm;
    int r;

    if (!mesh) return OMC_ERR_NULL;
    mesh->edge_count = 0UL;
    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        face = 0UL;
        while (omc_get_triangle_local_indices(mesh, sm, face, &a, &b, &c)) {
            r = omc_build_edges_for_local_triangle(mesh, si, sm, face, a, b, c);
            if (r != OMC_OK) return r;
            face++;
        }
        si++;
    }
    omc_add_diag(mesh, OMC_OK, "built in-memory triangle edge inventory");
    return OMC_OK;
}



static void omc_clear_vertex_blend(OMC_Vertex *v)
{
    OMC_U8 i;
    if (!v) return;
    i = 0U;
    while (i < 4U) {
        v->blend_weights[i] = 0;
        v->blend_indices[i] = 0U;
        i++;
    }
    v->blend_count = 0U;
    v->flags = (OMC_U16)(v->flags & (OMC_U16)~(OMC_VTX_HAS_BLEND_WEIGHTS | OMC_VTX_HAS_BLEND_INDICES));
}

static void omc_add_vertex_blend_influence(OMC_Vertex *v, OMC_U16 bone_index, OMC_Fixed weight, OMC_U8 max_influences)
{
    OMC_U8 i;
    OMC_U8 smallest;
    OMC_Fixed smallest_weight;
    if (!v || weight <= 0) return;
    if (max_influences == 0U || max_influences > 4U) max_influences = 4U;
    i = 0U;
    while (i < v->blend_count && i < max_influences) {
        if (v->blend_indices[i] == bone_index) {
            v->blend_weights[i] = omc_fixed_add_sat(v->blend_weights[i], weight);
            v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_BLEND_WEIGHTS | OMC_VTX_HAS_BLEND_INDICES);
            return;
        }
        i++;
    }
    if (v->blend_count < max_influences) {
        i = v->blend_count;
        v->blend_indices[i] = bone_index;
        v->blend_weights[i] = weight;
        v->blend_count = (OMC_U8)(v->blend_count + 1U);
        v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_BLEND_WEIGHTS | OMC_VTX_HAS_BLEND_INDICES);
        return;
    }
    smallest = 0U;
    smallest_weight = v->blend_weights[0];
    i = 1U;
    while (i < max_influences) {
        if (v->blend_weights[i] < smallest_weight) {
            smallest = i;
            smallest_weight = v->blend_weights[i];
        }
        i++;
    }
    if (weight > smallest_weight) {
        v->blend_indices[smallest] = bone_index;
        v->blend_weights[smallest] = weight;
        v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_BLEND_WEIGHTS | OMC_VTX_HAS_BLEND_INDICES);
    }
}

static void omc_sort_vertex_blend_desc(OMC_Vertex *v)
{
    OMC_U8 i;
    OMC_U8 j;
    OMC_Fixed tw;
    OMC_U16 ti;
    if (!v) return;
    if (v->blend_count > 4U) v->blend_count = 4U;
    i = 0U;
    while (i < v->blend_count) {
        j = (OMC_U8)(i + 1U);
        while (j < v->blend_count) {
            if (v->blend_weights[j] > v->blend_weights[i] ||
                (v->blend_weights[j] == v->blend_weights[i] && v->blend_indices[j] < v->blend_indices[i])) {
                tw = v->blend_weights[i];
                ti = v->blend_indices[i];
                v->blend_weights[i] = v->blend_weights[j];
                v->blend_indices[i] = v->blend_indices[j];
                v->blend_weights[j] = tw;
                v->blend_indices[j] = ti;
            }
            j++;
        }
        i++;
    }
}


static int omc_shared_vertex_rewrite_supported(OMC_Mesh *mesh)
{
    OMC_U16 si;
    if (!mesh) return 0;
    if (mesh->bone_assignment_count || mesh->submesh_bone_assignment_count ||
        mesh->pose_count || mesh->pose_offset_count ||
        mesh->animation_count || mesh->track_count || mesh->key_count ||
        mesh->morph_key_count || mesh->morph_vertex_count ||
        mesh->lod_level_count || mesh->lod_face_count || mesh->extreme_count) {
        omc_add_diag(mesh, OMC_ERR_UNSUPPORTED, "vertex compaction skipped: side tables would need remapping");
        return 0;
    }
    si = 0U;
    while (si < mesh->submesh_count) {
        if (!mesh->submeshes[si].use_shared_vertices) {
            omc_add_diag(mesh, OMC_ERR_UNSUPPORTED, "vertex compaction supports shared-geometry meshes only");
            return 0;
        }
        si++;
    }
    return 1;
}

static int omc_mark_used_shared_vertices(OMC_Mesh *mesh, OMC_U32 old_count)
{
    OMC_U32 i;
    OMC_U16 si;
    OMC_U32 idx;
    const OMC_Submesh *sm;

    i = 0UL;
    while (i < old_count && i < (OMC_U32)OMC_MAX_VERTICES) {
        g_omc_vertex_used[i] = 0U;
        g_omc_vertex_map[i] = 0xffffffffUL;
        i++;
    }
    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        if (sm->index_start + sm->index_count > mesh->index_count) return OMC_ERR_FORMAT;
        i = 0UL;
        while (i < sm->index_count) {
            idx = mesh->indices[sm->index_start + i];
            if (idx >= old_count || idx >= (OMC_U32)OMC_MAX_VERTICES) return OMC_ERR_FORMAT;
            g_omc_vertex_used[idx] = 1U;
            i++;
        }
        si++;
    }
    return OMC_OK;
}

static void omc_finish_shared_vertex_rewrite(OMC_Mesh *mesh, OMC_U32 new_count)
{
    OMC_U32 i;
    OMC_U16 si;
    OMC_Submesh *sm;

    i = 0UL;
    while (i < mesh->index_count) {
        mesh->indices[i] = g_omc_vertex_map[mesh->indices[i]];
        i++;
    }
    i = 0UL;
    while (i < new_count) {
        mesh->vertices[i] = g_omc_vertex_scratch[i];
        i++;
    }
    mesh->vertex_count = new_count;
    mesh->shared_vertex_count = new_count;
    si = 0U;
    while (si < mesh->submesh_count) {
        sm = &mesh->submeshes[si];
        sm->vertex_start = 0UL;
        sm->vertex_count = new_count;
        sm->has_dedicated_geometry = 0U;
        sm->use_shared_vertices = 1U;
        si++;
    }
    mesh->edge_count = 0UL;
    mesh->has_bounds = 0U;
    omc_compute_bounds(mesh);
}

int omc_compact_vertices(OMC_Mesh *mesh)
{
    OMC_U32 old_count;
    OMC_U32 new_count;
    OMC_U32 i;
    int r;

    if (!mesh) return OMC_ERR_NULL;
    old_count = mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count;
    if (old_count > (OMC_U32)OMC_MAX_VERTICES) return OMC_ERR_CAPACITY;
    if (!omc_shared_vertex_rewrite_supported(mesh)) return OMC_ERR_UNSUPPORTED;
    r = omc_mark_used_shared_vertices(mesh, old_count);
    if (r != OMC_OK) return r;

    new_count = 0UL;
    i = 0UL;
    while (i < old_count) {
        if (g_omc_vertex_used[i]) {
            g_omc_vertex_map[i] = new_count;
            g_omc_vertex_scratch[new_count] = mesh->vertices[i];
            new_count++;
        }
        i++;
    }
    omc_finish_shared_vertex_rewrite(mesh, new_count);
    if (new_count != old_count) omc_add_diag(mesh, OMC_OK, "compacted unused shared vertices");
    else omc_add_diag(mesh, OMC_OK, "vertex compaction found no unused shared vertices");
    return OMC_OK;
}

static int omc_vertex_equal_exact(const OMC_Vertex *a, const OMC_Vertex *b)
{
    if (!a || !b) return 0;
    return memcmp((const void *)a, (const void *)b, sizeof(OMC_Vertex)) == 0;
}

int omc_weld_vertices_exact(OMC_Mesh *mesh)
{
    OMC_U32 old_count;
    OMC_U32 new_count;
    OMC_U32 i;
    OMC_U32 j;
    int found;
    int r;

    if (!mesh) return OMC_ERR_NULL;
    old_count = mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count;
    if (old_count > (OMC_U32)OMC_MAX_VERTICES) return OMC_ERR_CAPACITY;
    if (!omc_shared_vertex_rewrite_supported(mesh)) return OMC_ERR_UNSUPPORTED;
    r = omc_mark_used_shared_vertices(mesh, old_count);
    if (r != OMC_OK) return r;

    new_count = 0UL;
    i = 0UL;
    while (i < old_count) {
        if (g_omc_vertex_used[i]) {
            found = 0;
            j = 0UL;
            while (j < new_count) {
                if (omc_vertex_equal_exact(&mesh->vertices[i], &g_omc_vertex_scratch[j])) {
                    g_omc_vertex_map[i] = j;
                    found = 1;
                    break;
                }
                j++;
            }
            if (!found) {
                g_omc_vertex_map[i] = new_count;
                g_omc_vertex_scratch[new_count] = mesh->vertices[i];
                new_count++;
            }
        }
        i++;
    }
    omc_finish_shared_vertex_rewrite(mesh, new_count);
    if (new_count != old_count) omc_add_diag(mesh, OMC_OK, "welded exact duplicate shared vertices");
    else omc_add_diag(mesh, OMC_OK, "exact vertex weld found no duplicates");
    return OMC_OK;
}

int omc_normalize_blend_weights(OMC_Mesh *mesh)
{
    OMC_U32 vi;
    OMC_U8 bi;
    OMC_Fixed sum;
    OMC_Vertex *v;
    if (!mesh) return OMC_ERR_NULL;
    vi = 0UL;
    while (vi < mesh->vertex_count) {
        v = &mesh->vertices[vi];
        if (v->blend_count > 4U) return OMC_ERR_FORMAT;
        if ((v->flags & OMC_VTX_HAS_BLEND_WEIGHTS) != 0U && v->blend_count > 0U) {
            sum = 0;
            bi = 0U;
            while (bi < v->blend_count) {
                if (v->blend_weights[bi] > 0) sum = omc_fixed_add_sat(sum, v->blend_weights[bi]);
                bi++;
            }
            if (sum <= 0) {
                v->blend_weights[0] = OMC_FIXED_ONE;
                bi = 1U;
                while (bi < v->blend_count) { v->blend_weights[bi] = 0; bi++; }
            } else {
                bi = 0U;
                while (bi < v->blend_count) {
                    v->blend_weights[bi] = omc_fixed_div_q16_safe(v->blend_weights[bi], sum);
                    bi++;
                }
            }
            v->flags = (OMC_U16)(v->flags | OMC_VTX_HAS_BLEND_WEIGHTS);
        }
        vi++;
    }
    return OMC_OK;
}

int omc_build_blend_elements_from_bone_assignments(OMC_Mesh *mesh, OMC_U8 max_influences)
{
    OMC_U32 i;
    OMC_U32 actual;
    const OMC_Submesh *sm;
    const OMC_BoneAssignment *ba;
    const OMC_SubmeshBoneAssignment *sba;
    int r;
    if (!mesh) return OMC_ERR_NULL;
    if (max_influences == 0U || max_influences > 4U) max_influences = 4U;
    i = 0UL;
    while (i < mesh->vertex_count) {
        omc_clear_vertex_blend(&mesh->vertices[i]);
        i++;
    }
    i = 0UL;
    while (i < mesh->bone_assignment_count) {
        ba = &mesh->bone_assignments[i];
        if (ba->vertex_index >= mesh->vertex_count) return OMC_ERR_FORMAT;
        omc_add_vertex_blend_influence(&mesh->vertices[ba->vertex_index], ba->bone_index, ba->weight, max_influences);
        i++;
    }
    i = 0UL;
    while (i < mesh->submesh_bone_assignment_count) {
        sba = &mesh->submesh_bone_assignments[i];
        if (sba->submesh_index >= mesh->submesh_count) return OMC_ERR_FORMAT;
        sm = &mesh->submeshes[sba->submesh_index];
        actual = omc_submesh_actual_vertex_index(mesh, sm, sba->assignment.vertex_index);
        if (actual >= mesh->vertex_count) return OMC_ERR_FORMAT;
        omc_add_vertex_blend_influence(&mesh->vertices[actual], sba->assignment.bone_index, sba->assignment.weight, max_influences);
        i++;
    }
    i = 0UL;
    while (i < mesh->vertex_count) {
        if (mesh->vertices[i].blend_count > 0U) {
            omc_sort_vertex_blend_desc(&mesh->vertices[i]);
            mesh->vertices[i].flags = (OMC_U16)(mesh->vertices[i].flags | OMC_VTX_HAS_BLEND_WEIGHTS | OMC_VTX_HAS_BLEND_INDICES);
        }
        i++;
    }
    r = omc_normalize_blend_weights(mesh);
    if (r != OMC_OK) return r;
    omc_add_diag(mesh, OMC_OK, "built vertex blend elements from bone assignments");
    return OMC_OK;
}

static OMC_U32 omc_pose_actual_vertex_index(const OMC_Mesh *mesh, const OMC_Pose *pose, OMC_U32 local_index)
{
    const OMC_Submesh *sm;
    if (!mesh || !pose) return local_index;
    if (!pose->target_is_submesh) return local_index;
    if (pose->target_index >= mesh->submesh_count) return local_index;
    sm = &mesh->submeshes[pose->target_index];
    if (!sm->use_shared_vertices) return sm->vertex_start + local_index;
    return local_index;
}

int omc_apply_pose_to_vertices(const OMC_Mesh *mesh, OMC_U16 pose_index, OMC_Fixed influence, OMC_Vertex *vertices, unsigned long vertex_cap)
{
    const OMC_Pose *pose;
    const OMC_PoseOffset *off;
    OMC_U32 i;
    OMC_U32 actual;
    OMC_Fixed sx;
    OMC_Fixed sy;
    OMC_Fixed sz;

    if (!mesh || !vertices) return OMC_ERR_NULL;
    if (pose_index >= mesh->pose_count) return OMC_ERR_FORMAT;
    pose = &mesh->poses[pose_index];
    i = pose->offset_start;
    while (i < pose->offset_start + pose->offset_count && i < mesh->pose_offset_count) {
        off = &mesh->pose_offsets[i];
        actual = omc_pose_actual_vertex_index(mesh, pose, off->vertex_index);
        if ((unsigned long)actual < vertex_cap) {
            sx = omc_fixed_mul_q16_safe(off->offset.x, influence);
            sy = omc_fixed_mul_q16_safe(off->offset.y, influence);
            sz = omc_fixed_mul_q16_safe(off->offset.z, influence);
            vertices[actual].position.x += sx;
            vertices[actual].position.y += sy;
            vertices[actual].position.z += sz;
            vertices[actual].flags = (OMC_U16)(vertices[actual].flags | OMC_VTX_HAS_POSITION);
            if (off->has_normal) {
                vertices[actual].normal.x += omc_fixed_mul_q16_safe(off->normal.x, influence);
                vertices[actual].normal.y += omc_fixed_mul_q16_safe(off->normal.y, influence);
                vertices[actual].normal.z += omc_fixed_mul_q16_safe(off->normal.z, influence);
                vertices[actual].flags = (OMC_U16)(vertices[actual].flags | OMC_VTX_HAS_NORMAL);
            }
        }
        i++;
    }
    return OMC_OK;
}

static int omc_track_actual_vertex_start_count(const OMC_Mesh *mesh, const OMC_AnimationTrack *track, OMC_U32 *start, OMC_U32 *count)
{
    const OMC_Submesh *sm;
    if (!mesh || !track || !start || !count) return OMC_ERR_NULL;
    if (!track->target_is_submesh) {
        *start = 0UL;
        *count = mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count;
        return OMC_OK;
    }
    if (track->target_index >= mesh->submesh_count) return OMC_ERR_FORMAT;
    sm = &mesh->submeshes[track->target_index];
    if (!sm->use_shared_vertices) {
        *start = sm->vertex_start;
        *count = sm->vertex_count;
    } else {
        *start = 0UL;
        *count = mesh->shared_vertex_count ? mesh->shared_vertex_count : mesh->vertex_count;
    }
    return OMC_OK;
}

static int omc_find_animation_index(const OMC_Mesh *mesh, const char *name, OMC_U16 *out_index)
{
    OMC_U16 i;
    if (!mesh || !out_index) return OMC_ERR_NULL;
    if (!name || !name[0]) {
        if (mesh->animation_count == 0U) return OMC_ERR_FORMAT;
        *out_index = 0U;
        return OMC_OK;
    }
    i = 0U;
    while (i < mesh->animation_count) {
        if (strcmp(mesh->animations[i].name, name) == 0) {
            *out_index = i;
            return OMC_OK;
        }
        i++;
    }
    return OMC_ERR_FORMAT;
}

static int omc_find_pose_track_times(const OMC_Mesh *mesh, const OMC_AnimationTrack *track, OMC_U32 track_index, OMC_Fixed time, OMC_Fixed *prev_time, OMC_Fixed *next_time)
{
    const OMC_AnimationKey *key;
    OMC_U32 i;
    int have_prev;
    int have_next;

    if (!mesh || !track || !prev_time || !next_time) return 0;
    have_prev = 0;
    have_next = 0;
    *prev_time = 0;
    *next_time = 0;
    i = track->key_start;
    while (i < track->key_start + track->key_count && i < mesh->key_count) {
        key = &mesh->keys[i];
        if (key->track_index == (OMC_U16)track_index) {
            if (key->time <= time) {
                if (!have_prev || key->time > *prev_time) {
                    *prev_time = key->time;
                    have_prev = 1;
                }
            }
            if (key->time >= time) {
                if (!have_next || key->time < *next_time) {
                    *next_time = key->time;
                    have_next = 1;
                }
            }
        }
        i++;
    }
    if (!have_prev && have_next) { *prev_time = *next_time; have_prev = 1; }
    if (have_prev && !have_next) { *next_time = *prev_time; have_next = 1; }
    return have_prev && have_next;
}

static OMC_Fixed omc_pose_influence_at_time(const OMC_Mesh *mesh, const OMC_AnimationTrack *track, OMC_U32 track_index, OMC_Fixed key_time, OMC_U16 pose_index, int *found)
{
    const OMC_AnimationKey *key;
    OMC_U32 i;
    OMC_Fixed sum;

    if (found) *found = 0;
    if (!mesh || !track) return 0;
    sum = 0;
    i = track->key_start;
    while (i < track->key_start + track->key_count && i < mesh->key_count) {
        key = &mesh->keys[i];
        if (key->track_index == (OMC_U16)track_index && key->time == key_time && key->pose_index == pose_index) {
            sum = omc_fixed_add_sat(sum, key->influence);
            if (found) *found = 1;
        }
        i++;
    }
    return sum;
}

static int omc_apply_interpolated_pose_keys(const OMC_Mesh *mesh, OMC_U32 track_index, OMC_Fixed time, OMC_Vertex *out_vertices, unsigned long vertex_cap)
{
    const OMC_AnimationTrack *track;
    OMC_U16 pose_index;
    OMC_Fixed prev_time;
    OMC_Fixed next_time;
    OMC_Fixed alpha;
    OMC_Fixed denom;
    OMC_Fixed inf_prev;
    OMC_Fixed inf_next;
    OMC_Fixed influence;
    int have_prev;
    int have_next;
    int r;

    if (!mesh || !out_vertices) return OMC_ERR_NULL;
    if (track_index >= mesh->track_count) return OMC_ERR_FORMAT;
    track = &mesh->tracks[track_index];
    if (!omc_find_pose_track_times(mesh, track, track_index, time, &prev_time, &next_time)) return OMC_OK;

    if (prev_time == next_time) alpha = 0;
    else {
        denom = omc_fixed_sub_sat(next_time, prev_time);
        alpha = omc_fixed_div_q16_safe(omc_fixed_sub_sat(time, prev_time), denom);
        alpha = omc_fixed_clamp01(alpha);
    }

    pose_index = 0U;
    while (pose_index < mesh->pose_count) {
        inf_prev = omc_pose_influence_at_time(mesh, track, track_index, prev_time, pose_index, &have_prev);
        inf_next = omc_pose_influence_at_time(mesh, track, track_index, next_time, pose_index, &have_next);
        if (prev_time == next_time) {
            influence = have_prev ? inf_prev : (have_next ? inf_next : 0);
        } else {
            if (!have_prev) inf_prev = 0;
            if (!have_next) inf_next = 0;
            influence = omc_fixed_lerp_q16(inf_prev, inf_next, alpha);
        }
        if (influence != 0) {
            r = omc_apply_pose_to_vertices(mesh, pose_index, influence, out_vertices, vertex_cap);
            if (r != OMC_OK) return r;
        }
        pose_index++;
    }
    return OMC_OK;
}

static const OMC_MorphVertex *omc_find_morph_vertex_in_key(const OMC_Mesh *mesh, OMC_U32 key_index, OMC_U32 local_index)
{
    const OMC_MorphKey *mk;
    const OMC_MorphVertex *mv;
    OMC_U32 i;
    if (!mesh || key_index >= mesh->morph_key_count) return (const OMC_MorphVertex *)0;
    mk = &mesh->morph_keys[key_index];
    i = mk->vertex_start;
    while (i < mk->vertex_start + mk->vertex_count && i < mesh->morph_vertex_count) {
        mv = &mesh->morph_vertices[i];
        if (mv->morph_key_index == (OMC_U16)key_index && mv->local_index == local_index) return mv;
        i++;
    }
    return (const OMC_MorphVertex *)0;
}

static int omc_find_morph_track_keys(const OMC_Mesh *mesh, const OMC_AnimationTrack *track, OMC_U32 track_index, OMC_Fixed time, OMC_U32 *prev_index, OMC_U32 *next_index)
{
    const OMC_MorphKey *mk;
    OMC_U32 i;
    int have_prev;
    int have_next;
    OMC_Fixed prev_time;
    OMC_Fixed next_time;

    if (!mesh || !track || !prev_index || !next_index) return 0;
    have_prev = 0;
    have_next = 0;
    prev_time = 0;
    next_time = 0;
    *prev_index = 0UL;
    *next_index = 0UL;
    i = track->morph_key_start;
    while (i < track->morph_key_start + track->morph_key_count && i < mesh->morph_key_count) {
        mk = &mesh->morph_keys[i];
        if (mk->track_index == (OMC_U16)track_index) {
            if (mk->time <= time) {
                if (!have_prev || mk->time > prev_time) {
                    prev_time = mk->time;
                    *prev_index = i;
                    have_prev = 1;
                }
            }
            if (mk->time >= time) {
                if (!have_next || mk->time < next_time) {
                    next_time = mk->time;
                    *next_index = i;
                    have_next = 1;
                }
            }
        }
        i++;
    }
    if (!have_prev && have_next) { *prev_index = *next_index; have_prev = 1; }
    if (have_prev && !have_next) { *next_index = *prev_index; have_next = 1; }
    return have_prev && have_next;
}

static OMC_U8 omc_morph_vertex_normal_at_key(const OMC_Mesh *mesh, OMC_U32 key_index, OMC_U32 local_index, const OMC_Vertex *base, OMC_Vec3 *out)
{
    const OMC_MorphVertex *mv;
    if (!out) return 0U;
    mv = omc_find_morph_vertex_in_key(mesh, key_index, local_index);
    if (mv && mv->has_normal) {
        *out = mv->normal;
        return 1U;
    }
    if (base && (base->flags & OMC_VTX_HAS_NORMAL) != 0U) {
        *out = base->normal;
        return 1U;
    }
    out->x = 0; out->y = OMC_FIXED_ONE; out->z = 0;
    return 0U;
}

static OMC_Vec3 omc_morph_vertex_position_at_key(const OMC_Mesh *mesh, OMC_U32 key_index, OMC_U32 local_index, const OMC_Vertex *base)
{
    const OMC_MorphVertex *mv;
    OMC_Vec3 p;
    mv = omc_find_morph_vertex_in_key(mesh, key_index, local_index);
    if (mv) return mv->position;
    if (base) return base->position;
    p.x = 0; p.y = 0; p.z = 0;
    return p;
}

static int omc_apply_interpolated_morph_key(const OMC_Mesh *mesh, OMC_U32 track_index, OMC_Fixed time, OMC_Vertex *out_vertices, unsigned long vertex_cap)
{
    const OMC_AnimationTrack *track;
    const OMC_MorphKey *prev_key;
    const OMC_MorphKey *next_key;
    const OMC_Vertex *base;
    OMC_U32 prev_index;
    OMC_U32 next_index;
    OMC_U32 start;
    OMC_U32 count;
    OMC_U32 local;
    OMC_U32 actual;
    OMC_Fixed denom;
    OMC_Fixed alpha;
    OMC_Vec3 pos_prev;
    OMC_Vec3 pos_next;
    OMC_Vec3 n_prev;
    OMC_Vec3 n_next;
    OMC_U8 has_n_prev;
    OMC_U8 has_n_next;
    int r;

    if (!mesh || !out_vertices) return OMC_ERR_NULL;
    if (track_index >= mesh->track_count) return OMC_ERR_FORMAT;
    track = &mesh->tracks[track_index];
    r = omc_track_actual_vertex_start_count(mesh, track, &start, &count);
    if (r != OMC_OK) return r;
    if (!omc_find_morph_track_keys(mesh, track, track_index, time, &prev_index, &next_index)) return OMC_OK;
    prev_key = &mesh->morph_keys[prev_index];
    next_key = &mesh->morph_keys[next_index];

    if (prev_key->time == next_key->time) alpha = 0;
    else {
        denom = omc_fixed_sub_sat(next_key->time, prev_key->time);
        alpha = omc_fixed_div_q16_safe(omc_fixed_sub_sat(time, prev_key->time), denom);
        alpha = omc_fixed_clamp01(alpha);
    }

    local = 0UL;
    while (local < count) {
        actual = start + local;
        if ((unsigned long)actual < vertex_cap && actual < mesh->vertex_count) {
            base = &mesh->vertices[actual];
            pos_prev = omc_morph_vertex_position_at_key(mesh, prev_index, local, base);
            pos_next = omc_morph_vertex_position_at_key(mesh, next_index, local, base);
            out_vertices[actual].position = (prev_index == next_index) ? pos_prev : omc_vec3_lerp(pos_prev, pos_next, alpha);
            out_vertices[actual].flags = (OMC_U16)(out_vertices[actual].flags | OMC_VTX_HAS_POSITION);

            has_n_prev = omc_morph_vertex_normal_at_key(mesh, prev_index, local, base, &n_prev);
            has_n_next = omc_morph_vertex_normal_at_key(mesh, next_index, local, base, &n_next);
            if (has_n_prev || has_n_next) {
                out_vertices[actual].normal = (prev_index == next_index) ? n_prev : omc_vec3_normalize(omc_vec3_lerp(n_prev, n_next, alpha));
                out_vertices[actual].flags = (OMC_U16)(out_vertices[actual].flags | OMC_VTX_HAS_NORMAL);
            }
        }
        local++;
    }
    return OMC_OK;
}

int omc_make_pose_animation_frame(const OMC_Mesh *mesh, const char *animation_name, OMC_Fixed time, OMC_Vertex *out_vertices, unsigned long vertex_cap, unsigned long *out_count)
{
    OMC_U16 ai;
    OMC_U32 i;
    OMC_U32 ti;
    const OMC_Animation *anim;
    const OMC_AnimationTrack *track;
    int r;

    if (!mesh || !out_vertices || !out_count) return OMC_ERR_NULL;
    if (mesh->vertex_count > vertex_cap) return OMC_ERR_CAPACITY;
    r = omc_find_animation_index(mesh, animation_name, &ai);
    if (r != OMC_OK) return r;

    i = 0UL;
    while (i < mesh->vertex_count) {
        out_vertices[i] = mesh->vertices[i];
        i++;
    }
    *out_count = (unsigned long)mesh->vertex_count;

    anim = &mesh->animations[ai];
    ti = anim->track_start;
    while (ti < anim->track_start + anim->track_count && ti < mesh->track_count) {
        track = &mesh->tracks[ti];
        if (track->animation_index == ai) {
            if (track->is_pose_track) r = omc_apply_interpolated_pose_keys(mesh, ti, time, out_vertices, vertex_cap);
            else r = omc_apply_interpolated_morph_key(mesh, ti, time, out_vertices, vertex_cap);
            if (r != OMC_OK) return r;
        }
        ti++;
    }
    return OMC_OK;
}

int omc_make_animation_frame(const OMC_Mesh *mesh, const char *animation_name, OMC_Fixed time, OMC_Vertex *out_vertices, unsigned long vertex_cap, unsigned long *out_count)
{
    return omc_make_pose_animation_frame(mesh, animation_name, time, out_vertices, vertex_cap, out_count);
}
