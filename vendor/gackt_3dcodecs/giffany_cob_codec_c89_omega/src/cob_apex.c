#include "cob_private.h"

/*
   cob_apex.c
   More pipeline helpers for trueSpace COB/SCN:
   - hierarchy/world bake with parent-ID graph traversal
   - tangent/bitangent frame generation for normal/bump workflows
   - runtime/material/layer reports
   - CSV animation export

   Still C89, no heap allocation, fixed numeric path.
*/

#ifndef COB_APEX_TRI_MAX
#define COB_APEX_TRI_MAX 32768UL
#endif

static cob_triangle g_apx_tris[COB_APEX_TRI_MAX];

static unsigned long apx_abs(cob_fixed v)
{
    if (v == COB_FIXED_MIN) return 2147483648UL;
    if (v < 0) return (unsigned long)(-v);
    return (unsigned long)v;
}

static cob_fixed apx_from_mag(unsigned long mag, int sign)
{
    if (sign < 0) {
        if (mag >= 2147483648UL) return COB_FIXED_MIN;
        return -(cob_fixed)mag;
    }
    if (mag > (unsigned long)COB_FIXED_MAX) return COB_FIXED_MAX;
    return (cob_fixed)mag;
}

static cob_fixed apx_add(cob_fixed a, cob_fixed b)
{
    if (b > 0 && a > COB_FIXED_MAX - b) return COB_FIXED_MAX;
    if (b < 0 && a < COB_FIXED_MIN - b) return COB_FIXED_MIN;
    return a + b;
}

static cob_fixed apx_mul(cob_fixed a, cob_fixed b)
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
    am = apx_abs(a);
    bm = apx_abs(b);
    aw = am >> 16;
    af = am & 65535UL;
    bw = bm >> 16;
    bf = bm & 65535UL;
    if (aw != 0UL && bw > 32767UL / aw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out = (aw * bw) << 16;
    if (aw != 0UL && bf > ((unsigned long)COB_FIXED_MAX - out) / aw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out += aw * bf;
    if (bw != 0UL && af > ((unsigned long)COB_FIXED_MAX - out) / bw) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out += bw * af;
    out += (af * bf) >> 16;
    return apx_from_mag(out, sign);
}

static cob_fixed apx_div(cob_fixed num, cob_fixed den)
{
    unsigned long n;
    unsigned long d;
    unsigned long whole;
    unsigned long rem;
    unsigned long out;
    int sign;

    if (den == 0) return 0;
    sign = 1;
    if (num < 0) sign = -sign;
    if (den < 0) sign = -sign;
    n = apx_abs(num);
    d = apx_abs(den);
    whole = n / d;
    rem = n % d;
    if (whole > 32767UL) return sign < 0 ? COB_FIXED_MIN : COB_FIXED_MAX;
    out = (whole << 16) + ((rem << 16) / d);
    return apx_from_mag(out, sign);
}

static cob_fixed apx_sqrt_fixed(cob_fixed v)
{
    cob_fixed x;
    cob_fixed nx;
    int i;
    if (v <= 0) return 0;
    x = v > COB_FIXED_ONE ? v : COB_FIXED_ONE;
    for (i = 0; i < 10; ++i) {
        nx = (cob_fixed)((x + apx_div(v, x)) / 2L);
        if (nx == x) break;
        x = nx;
    }
    return x;
}

static void apx_vec_zero(cob_vec3q *v)
{
    v->x = 0;
    v->y = 0;
    v->z = 0;
}

static void apx_vec_add(cob_vec3q *dst, const cob_vec3q *src)
{
    dst->x = apx_add(dst->x, src->x);
    dst->y = apx_add(dst->y, src->y);
    dst->z = apx_add(dst->z, src->z);
}

static cob_fixed apx_dot(const cob_vec3q *a, const cob_vec3q *b)
{
    return apx_add(apx_add(apx_mul(a->x, b->x), apx_mul(a->y, b->y)), apx_mul(a->z, b->z));
}

static cob_vec3q apx_cross(const cob_vec3q *a, const cob_vec3q *b)
{
    cob_vec3q n;
    n.x = apx_add(apx_mul(a->y, b->z), -apx_mul(a->z, b->y));
    n.y = apx_add(apx_mul(a->z, b->x), -apx_mul(a->x, b->z));
    n.z = apx_add(apx_mul(a->x, b->y), -apx_mul(a->y, b->x));
    return n;
}

static void apx_normalize(cob_vec3q *v)
{
    unsigned long ax;
    unsigned long ay;
    unsigned long az;
    unsigned long scale;
    cob_fixed sfix;
    cob_fixed xq;
    cob_fixed yq;
    cob_fixed zq;
    cob_fixed sum;
    cob_fixed len;

    ax = apx_abs(v->x);
    ay = apx_abs(v->y);
    az = apx_abs(v->z);
    scale = ax;
    if (ay > scale) scale = ay;
    if (az > scale) scale = az;
    if (scale == 0UL) {
        v->x = 0;
        v->y = 0;
        v->z = COB_FIXED_ONE;
        return;
    }
    sfix = scale > (unsigned long)COB_FIXED_MAX ? COB_FIXED_MAX : (cob_fixed)scale;
    xq = apx_div(v->x, sfix);
    yq = apx_div(v->y, sfix);
    zq = apx_div(v->z, sfix);
    sum = apx_add(apx_add(apx_mul(xq, xq), apx_mul(yq, yq)), apx_mul(zq, zq));
    len = apx_sqrt_fixed(sum);
    if (len == 0) {
        v->x = 0;
        v->y = 0;
        v->z = COB_FIXED_ONE;
        return;
    }
    v->x = apx_div(xq, len);
    v->y = apx_div(yq, len);
    v->z = apx_div(zq, len);
}

static void apx_matrix_mul(cob_fixed out[16], const cob_fixed a[16], const cob_fixed b[16])
{
    cob_fixed r[16];
    int row;
    int col;
    int k;
    cob_fixed sum;

    for (row = 0; row < 4; ++row) {
        for (col = 0; col < 4; ++col) {
            sum = 0;
            for (k = 0; k < 4; ++k) {
                sum = apx_add(sum, apx_mul(a[row * 4 + k], b[k * 4 + col]));
            }
            r[row * 4 + col] = sum;
        }
    }
    memcpy(out, r, sizeof(r));
}

static int apx_find_transform_parent(const cob_scene *scene, cob_i32 id, const cob_fixed **out_transform, cob_i32 *out_parent)
{
    unsigned long i;
    if (scene == 0 || out_transform == 0 || out_parent == 0) return 0;
    for (i = 0; i < scene->group_count; ++i) {
        if (scene->groups[i].id == id) {
            *out_transform = scene->groups[i].transform;
            *out_parent = scene->groups[i].parent_id;
            return 1;
        }
    }
    for (i = 0; i < scene->mesh_count; ++i) {
        if (scene->meshes[i].id == id) {
            *out_transform = scene->meshes[i].transform;
            *out_parent = scene->meshes[i].parent_id;
            return 1;
        }
    }
    for (i = 0; i < scene->light_count; ++i) {
        if (scene->lights[i].id == id) {
            *out_transform = scene->lights[i].transform;
            *out_parent = scene->lights[i].parent_id;
            return 1;
        }
    }
    for (i = 0; i < scene->camera_count; ++i) {
        if (scene->cameras[i].id == id) {
            *out_transform = scene->cameras[i].transform;
            *out_parent = scene->cameras[i].parent_id;
            return 1;
        }
    }
    for (i = 0; i < scene->bone_count; ++i) {
        if (scene->bones[i].id == id) {
            *out_transform = scene->bones[i].transform;
            *out_parent = scene->bones[i].parent_id;
            return 1;
        }
    }
    return 0;
}

static int apx_compute_world_transform(const cob_scene *scene, cob_i32 self_id, cob_i32 parent_id, const cob_fixed local[16], cob_fixed out[16], cob_u32 *out_depth, int *out_cycle)
{
    cob_i32 visited[64];
    unsigned long visited_count;
    cob_i32 cur;
    cob_i32 next_parent;
    const cob_fixed *parent_transform;
    cob_fixed acc[16];
    cob_fixed tmp[16];
    unsigned long i;

    (void)self_id;
    if (scene == 0 || local == 0 || out == 0) return COB_ERR_ARGUMENT;
    memcpy(acc, local, sizeof(acc));
    visited_count = 0UL;
    cur = parent_id;
    if (out_cycle != 0) *out_cycle = 0;
    while (cur != 0) {
        for (i = 0; i < visited_count; ++i) {
            if (visited[i] == cur) {
                if (out_cycle != 0) *out_cycle = 1;
                memcpy(out, acc, sizeof(acc));
                if (out_depth != 0) *out_depth = (cob_u32)visited_count;
                return COB_OK;
            }
        }
        if (visited_count >= COB_ARRAY_COUNT(visited)) break;
        visited[visited_count++] = cur;
        if (!apx_find_transform_parent(scene, cur, &parent_transform, &next_parent)) {
            break;
        }
        apx_matrix_mul(tmp, parent_transform, acc);
        memcpy(acc, tmp, sizeof(acc));
        cur = next_parent;
    }
    memcpy(out, acc, sizeof(acc));
    if (out_depth != 0) *out_depth = (cob_u32)visited_count;
    return COB_OK;
}

int cob_scene_build_hierarchy_report(const cob_scene *scene, cob_hierarchy_report *report)
{
    unsigned long i;
    cob_u32 depth;
    int cycle;
    const cob_fixed *tf;
    cob_i32 pp;
    cob_fixed tmp[16];

    if (scene == 0 || report == 0) return COB_ERR_ARGUMENT;
    memset(report, 0, sizeof(*report));
    report->node_count = scene->mesh_count + scene->group_count + scene->light_count + scene->camera_count + scene->bone_count;
    report->layer_count = scene->scene_layer_count;
    report->object_layer_count = scene->object_layer_count;
    for (i = 0; i < scene->object_layer_count; ++i) {
        if (scene->object_layers[i].hidden) ++report->hidden_objects;
    }
#define APX_REPORT_NODE(PARENT,ID,LOCAL) \
    do { \
        if ((PARENT) != 0) { \
            if (apx_find_transform_parent(scene, (PARENT), &tf, &pp)) ++report->parent_links; else ++report->orphan_count; \
        } \
        depth = 0U; cycle = 0; \
        apx_compute_world_transform(scene, (ID), (PARENT), (LOCAL), tmp, &depth, &cycle); \
        if (cycle) ++report->cycle_count; \
        if (depth > report->max_depth) report->max_depth = depth; \
    } while (0)
    for (i = 0; i < scene->group_count; ++i) APX_REPORT_NODE(scene->groups[i].parent_id, scene->groups[i].id, scene->groups[i].transform);
    for (i = 0; i < scene->mesh_count; ++i) APX_REPORT_NODE(scene->meshes[i].parent_id, scene->meshes[i].id, scene->meshes[i].transform);
    for (i = 0; i < scene->light_count; ++i) APX_REPORT_NODE(scene->lights[i].parent_id, scene->lights[i].id, scene->lights[i].transform);
    for (i = 0; i < scene->camera_count; ++i) APX_REPORT_NODE(scene->cameras[i].parent_id, scene->cameras[i].id, scene->cameras[i].transform);
    for (i = 0; i < scene->bone_count; ++i) APX_REPORT_NODE(scene->bones[i].parent_id, scene->bones[i].id, scene->bones[i].transform);
#undef APX_REPORT_NODE
    return COB_OK;
}

int cob_scene_make_hierarchical_world_copy(cob_scene *dst, const cob_scene *src)
{
    unsigned long i;
    cob_fixed out[16];

    if (dst == 0 || src == 0) return COB_ERR_ARGUMENT;
    *dst = *src;
    for (i = 0; i < src->group_count; ++i) {
        apx_compute_world_transform(src, src->groups[i].id, src->groups[i].parent_id, src->groups[i].transform, out, 0, 0);
        memcpy(dst->groups[i].transform, out, sizeof(out));
        dst->groups[i].parent_id = 0;
    }
    for (i = 0; i < src->mesh_count; ++i) {
        unsigned long vi;
        cob_vec3q tmpv;
        apx_compute_world_transform(src, src->meshes[i].id, src->meshes[i].parent_id, src->meshes[i].transform, out, 0, 0);
        if (dst->meshes[i].first_vertex + dst->meshes[i].vertex_count > dst->vertex_count) return COB_ERR_PARSE;
        for (vi = 0UL; vi < dst->meshes[i].vertex_count; ++vi) {
            cob_transform_vec3(&tmpv, out, &dst->vertices[dst->meshes[i].first_vertex + vi]);
            dst->vertices[dst->meshes[i].first_vertex + vi] = tmpv;
        }
        cob_set_identity(dst->meshes[i].transform);
        dst->meshes[i].parent_id = 0;
    }
    for (i = 0; i < src->light_count; ++i) {
        apx_compute_world_transform(src, src->lights[i].id, src->lights[i].parent_id, src->lights[i].transform, out, 0, 0);
        memcpy(dst->lights[i].transform, out, sizeof(out));
        dst->lights[i].parent_id = 0;
    }
    for (i = 0; i < src->camera_count; ++i) {
        apx_compute_world_transform(src, src->cameras[i].id, src->cameras[i].parent_id, src->cameras[i].transform, out, 0, 0);
        memcpy(dst->cameras[i].transform, out, sizeof(out));
        dst->cameras[i].parent_id = 0;
    }
    for (i = 0; i < src->bone_count; ++i) {
        apx_compute_world_transform(src, src->bones[i].id, src->bones[i].parent_id, src->bones[i].transform, out, 0, 0);
        memcpy(dst->bones[i].transform, out, sizeof(out));
        dst->bones[i].parent_id = 0;
    }
    return COB_OK;
}

static unsigned long apx_global_vertex_index(const cob_scene *scene, const cob_triangle *t, int corner)
{
    const cob_mesh *m;
    if (scene == 0 || t == 0 || corner < 0 || corner > 2) return 0UL;
    if (t->mesh_index >= scene->mesh_count) return 0UL;
    m = &scene->meshes[t->mesh_index];
    if (t->refs[corner].pos_index < 0) return m->first_vertex;
    return m->first_vertex + (unsigned long)t->refs[corner].pos_index;
}

int cob_scene_build_tangent_frames(const cob_scene *scene, cob_tangent_frame *out_frames, unsigned long frame_capacity, unsigned long *out_count)
{
    unsigned long i;
    unsigned long tri_count;
    unsigned long gi;
    int st;
    int c;
    cob_vec3q e1;
    cob_vec3q e2;
    cob_vec3q tan;
    cob_vec3q bit;
    cob_vec3q n;
    cob_fixed du1;
    cob_fixed dv1;
    cob_fixed du2;
    cob_fixed dv2;
    cob_fixed den;
    cob_vec3q cr;

    if (scene == 0 || out_frames == 0 || out_count == 0) return COB_ERR_ARGUMENT;
    if (frame_capacity < scene->vertex_count) return COB_ERR_CAPACITY;
    for (i = 0; i < scene->vertex_count; ++i) {
        apx_vec_zero(&out_frames[i].normal);
        apx_vec_zero(&out_frames[i].tangent);
        apx_vec_zero(&out_frames[i].bitangent);
        out_frames[i].handedness = COB_FIXED_ONE;
        out_frames[i].flags = 0UL;
    }
    tri_count = 0UL;
    st = cob_scene_build_triangles_ex(scene, g_apx_tris, COB_APEX_TRI_MAX, &tri_count, 0);
    if (st != COB_OK && tri_count == 0UL) return st;
    for (i = 0; i < tri_count; ++i) {
        e1.x = g_apx_tris[i].p[1].x - g_apx_tris[i].p[0].x;
        e1.y = g_apx_tris[i].p[1].y - g_apx_tris[i].p[0].y;
        e1.z = g_apx_tris[i].p[1].z - g_apx_tris[i].p[0].z;
        e2.x = g_apx_tris[i].p[2].x - g_apx_tris[i].p[0].x;
        e2.y = g_apx_tris[i].p[2].y - g_apx_tris[i].p[0].y;
        e2.z = g_apx_tris[i].p[2].z - g_apx_tris[i].p[0].z;
        du1 = g_apx_tris[i].uv[1].u - g_apx_tris[i].uv[0].u;
        dv1 = g_apx_tris[i].uv[1].v - g_apx_tris[i].uv[0].v;
        du2 = g_apx_tris[i].uv[2].u - g_apx_tris[i].uv[0].u;
        dv2 = g_apx_tris[i].uv[2].v - g_apx_tris[i].uv[0].v;
        den = apx_add(apx_mul(du1, dv2), -apx_mul(du2, dv1));
        if (den == 0) {
            tan = e1;
            bit = e2;
        } else {
            tan.x = apx_div(apx_add(apx_mul(e1.x, dv2), -apx_mul(e2.x, dv1)), den);
            tan.y = apx_div(apx_add(apx_mul(e1.y, dv2), -apx_mul(e2.y, dv1)), den);
            tan.z = apx_div(apx_add(apx_mul(e1.z, dv2), -apx_mul(e2.z, dv1)), den);
            bit.x = apx_div(apx_add(apx_mul(e2.x, du1), -apx_mul(e1.x, du2)), den);
            bit.y = apx_div(apx_add(apx_mul(e2.y, du1), -apx_mul(e1.y, du2)), den);
            bit.z = apx_div(apx_add(apx_mul(e2.z, du1), -apx_mul(e1.z, du2)), den);
        }
        n = apx_cross(&e1, &e2);
        for (c = 0; c < 3; ++c) {
            gi = apx_global_vertex_index(scene, &g_apx_tris[i], c);
            if (gi < scene->vertex_count) {
                apx_vec_add(&out_frames[gi].normal, &n);
                apx_vec_add(&out_frames[gi].tangent, &tan);
                apx_vec_add(&out_frames[gi].bitangent, &bit);
                if (den == 0) out_frames[gi].flags |= 1UL;
            }
        }
    }
    for (i = 0; i < scene->vertex_count; ++i) {
        apx_normalize(&out_frames[i].normal);
        /* Gram-Schmidt: tangent = tangent - normal * dot(normal,tangent) */
        {
            cob_fixed ndott;
            ndott = apx_dot(&out_frames[i].normal, &out_frames[i].tangent);
            out_frames[i].tangent.x = apx_add(out_frames[i].tangent.x, -apx_mul(out_frames[i].normal.x, ndott));
            out_frames[i].tangent.y = apx_add(out_frames[i].tangent.y, -apx_mul(out_frames[i].normal.y, ndott));
            out_frames[i].tangent.z = apx_add(out_frames[i].tangent.z, -apx_mul(out_frames[i].normal.z, ndott));
        }
        apx_normalize(&out_frames[i].tangent);
        apx_normalize(&out_frames[i].bitangent);
        cr = apx_cross(&out_frames[i].normal, &out_frames[i].tangent);
        out_frames[i].handedness = apx_dot(&cr, &out_frames[i].bitangent) < 0 ? -COB_FIXED_ONE : COB_FIXED_ONE;
    }
    *out_count = scene->vertex_count;
    return COB_OK;
}

static int apx_emit_vec3(cob_write_fn fn, void *user, const char *tag, const cob_vec3q *v)
{
    int st;
    st = cob_emit(fn, user, tag); if (st != COB_OK) return st;
    st = cob_emit_fixed(fn, user, v->x, 6); if (st != COB_OK) return st;
    st = cob_emit(fn, user, " "); if (st != COB_OK) return st;
    st = cob_emit_fixed(fn, user, v->y, 6); if (st != COB_OK) return st;
    st = cob_emit(fn, user, " "); if (st != COB_OK) return st;
    st = cob_emit_fixed(fn, user, v->z, 6); if (st != COB_OK) return st;
    return cob_emit(fn, user, "\n");
}

int cob_export_tangent_obj(const cob_scene *scene, cob_write_fn write_fn, void *user)
{
    static cob_tangent_frame frames[COB_MAX_VERTICES];
    unsigned long frame_count;
    unsigned long tri_count;
    unsigned long i;
    unsigned long gi;
    int st;

    if (scene == 0 || write_fn == 0) return COB_ERR_ARGUMENT;
    frame_count = 0UL;
    st = cob_scene_build_tangent_frames(scene, frames, COB_MAX_VERTICES, &frame_count);
    if (st != COB_OK) return st;
    tri_count = 0UL;
    st = cob_scene_build_triangles_ex(scene, g_apx_tris, COB_APEX_TRI_MAX, &tri_count, 0);
    if (st != COB_OK && tri_count == 0UL) return st;
    st = cob_emit(write_fn, user, "# cob_chiqui3d tangent OBJ: tangents live in comments named cbt\n"); if (st != COB_OK) return st;
    for (i = 0; i < scene->vertex_count; ++i) {
        st = apx_emit_vec3(write_fn, user, "v ", &scene->vertices[i]); if (st != COB_OK) return st;
    }
    for (i = 0; i < scene->uv_count; ++i) {
        st = cob_emit(write_fn, user, "vt "); if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, scene->uvs[i].u, 6); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, " "); if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, scene->uvs[i].v, 6); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\n"); if (st != COB_OK) return st;
    }
    for (i = 0; i < frame_count; ++i) {
        st = apx_emit_vec3(write_fn, user, "vn ", &frames[i].normal); if (st != COB_OK) return st;
    }
    for (i = 0; i < frame_count; ++i) {
        st = cob_emit(write_fn, user, "# cbt "); if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, i + 1UL); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, " tangent "); if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, frames[i].tangent.x, 6); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, " "); if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, frames[i].tangent.y, 6); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, " "); if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, frames[i].tangent.z, 6); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, " bitangent "); if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, frames[i].bitangent.x, 6); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, " "); if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, frames[i].bitangent.y, 6); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, " "); if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, frames[i].bitangent.z, 6); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, " handed "); if (st != COB_OK) return st;
        st = cob_emit_fixed(write_fn, user, frames[i].handedness, 0); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\n"); if (st != COB_OK) return st;
    }
    for (i = 0; i < tri_count; ++i) {
        st = cob_emit(write_fn, user, "f"); if (st != COB_OK) return st;
        {
            int c;
            for (c = 0; c < 3; ++c) {
                gi = apx_global_vertex_index(scene, &g_apx_tris[i], c);
                st = cob_emit(write_fn, user, " "); if (st != COB_OK) return st;
                st = cob_emit_u(write_fn, user, gi + 1UL); if (st != COB_OK) return st;
                st = cob_emit(write_fn, user, "/"); if (st != COB_OK) return st;
                if (g_apx_tris[i].refs[c].uv_index >= 0) {
                    const cob_mesh *m;
                    unsigned long ugi;
                    m = &scene->meshes[g_apx_tris[i].mesh_index];
                    ugi = m->first_uv + (unsigned long)g_apx_tris[i].refs[c].uv_index;
                    st = cob_emit_u(write_fn, user, ugi + 1UL); if (st != COB_OK) return st;
                }
                st = cob_emit(write_fn, user, "/"); if (st != COB_OK) return st;
                st = cob_emit_u(write_fn, user, gi + 1UL); if (st != COB_OK) return st;
            }
        }
        st = cob_emit(write_fn, user, "\n"); if (st != COB_OK) return st;
    }
    return COB_OK;
}

int cob_scene_runtime_report(const cob_scene *scene, cob_runtime_report *report)
{
    unsigned long i;
    unsigned long tri_count;
    int st;

    if (scene == 0 || report == 0) return COB_ERR_ARGUMENT;
    memset(report, 0, sizeof(*report));
    report->mesh_count = scene->mesh_count;
    report->material_count = scene->material_count;
    report->light_count = scene->light_count;
    report->camera_count = scene->camera_count;
    report->layer_count = scene->scene_layer_count;
    report->object_layer_count = scene->object_layer_count;
    report->vertex_count = scene->vertex_count;
    report->anim_channel_count = scene->anim_channel_count;
    report->anim_key_count = scene->anim_key_count;
    for (i = 0; i < scene->material_count; ++i) {
        if (scene->materials[i].texture_color[0] != '\0') ++report->textured_materials;
        if (scene->materials[i].texture_bump[0] != '\0') ++report->bump_materials;
        if (scene->materials[i].texture_env[0] != '\0') ++report->env_materials;
    }
    tri_count = 0UL;
    st = cob_scene_build_triangles_ex(scene, g_apx_tris, COB_APEX_TRI_MAX, &tri_count, 0);
    if (st != COB_OK && tri_count == 0UL) return st;
    report->triangle_count = (cob_u32)tri_count;
    report->tangent_count = scene->vertex_count;
    return COB_OK;
}

static int apx_emit_fixed_csv(cob_write_fn fn, void *user, cob_fixed v)
{
    return cob_emit_fixed(fn, user, v, 6);
}

int cob_export_anim_csv(const cob_scene *scene, cob_write_fn write_fn, void *user)
{
    unsigned long i;
    const cob_anim_key *k;
    int st;

    if (scene == 0 || write_fn == 0) return COB_ERR_ARGUMENT;
    st = cob_emit(write_fn, user, "channel_id,target_id,frame,time,semantic,x,y,z,w\n"); if (st != COB_OK) return st;
    for (i = 0; i < scene->anim_key_count; ++i) {
        k = &scene->anim_keys[i];
        st = cob_emit_i(write_fn, user, k->channel_id); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, ","); if (st != COB_OK) return st;
        st = cob_emit_i(write_fn, user, k->target_id); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, ","); if (st != COB_OK) return st;
        st = cob_emit_i(write_fn, user, k->frame); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, ","); if (st != COB_OK) return st;
        st = apx_emit_fixed_csv(write_fn, user, k->time); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, ","); if (st != COB_OK) return st;
        st = cob_emit_u(write_fn, user, (unsigned long)k->semantic); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, ","); if (st != COB_OK) return st;
        st = apx_emit_fixed_csv(write_fn, user, k->value[0]); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, ","); if (st != COB_OK) return st;
        st = apx_emit_fixed_csv(write_fn, user, k->value[1]); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, ","); if (st != COB_OK) return st;
        st = apx_emit_fixed_csv(write_fn, user, k->value[2]); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, ","); if (st != COB_OK) return st;
        st = apx_emit_fixed_csv(write_fn, user, k->value[3]); if (st != COB_OK) return st;
        st = cob_emit(write_fn, user, "\n"); if (st != COB_OK) return st;
    }
    return COB_OK;
}

int cob_export_report_txt(const cob_scene *scene, cob_write_fn write_fn, void *user)
{
    cob_hierarchy_report h;
    cob_runtime_report r;
    cob_quality_report q;
    int st;

    if (scene == 0 || write_fn == 0) return COB_ERR_ARGUMENT;
    st = cob_scene_build_hierarchy_report(scene, &h); if (st != COB_OK) return st;
    st = cob_scene_runtime_report(scene, &r); if (st != COB_OK) return st;
    st = cob_scene_quality_report(scene, &q); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "cob_chiqui3d APEX report\n"); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "nodes="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, h.node_count); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " parent_links="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, h.parent_links); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " orphans="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, h.orphan_count); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " max_depth="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, h.max_depth); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " layers="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, h.layer_count); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " object_layers="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, h.object_layer_count); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "\n"); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "meshes="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.mesh_count); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " vertices="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.vertex_count); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " triangles="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.triangle_count); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " tangents="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.tangent_count); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " textured_materials="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.textured_materials); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " bump_materials="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.bump_materials); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " anim_keys="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, r.anim_key_count); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "\n"); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, "triangulation concave="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, q.concave_count); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " holes="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, q.hole_count); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " bridges="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, q.bridge_count); if (st != COB_OK) return st;
    st = cob_emit(write_fn, user, " failed="); if (st != COB_OK) return st;
    st = cob_emit_u(write_fn, user, q.failed_triangulations); if (st != COB_OK) return st;
    return cob_emit(write_fn, user, "\n");
}

typedef struct apx_file_writer_s {
    FILE *f;
} apx_file_writer;

static int apx_file_write(void *user, const void *bytes, unsigned long count)
{
    apx_file_writer *w;
    w = (apx_file_writer *)user;
    if (w == 0 || w->f == 0) return COB_ERR_WRITE;
    if (count == 0UL) return COB_OK;
    return fwrite(bytes, 1, (size_t)count, w->f) == (size_t)count ? COB_OK : COB_ERR_WRITE;
}

static int apx_save_with(const char *path, const cob_scene *scene, int (*exporter)(const cob_scene *, cob_write_fn, void *))
{
    FILE *f;
    apx_file_writer w;
    int st;
    if (path == 0 || scene == 0 || exporter == 0) return COB_ERR_ARGUMENT;
    f = fopen(path, "wb");
    if (f == 0) return COB_ERR_ARGUMENT;
    w.f = f;
    st = exporter(scene, apx_file_write, &w);
    fclose(f);
    return st;
}

int cob_save_tangent_obj_file(const char *path, const cob_scene *scene)
{
    return apx_save_with(path, scene, cob_export_tangent_obj);
}

int cob_save_anim_csv_file(const char *path, const cob_scene *scene)
{
    return apx_save_with(path, scene, cob_export_anim_csv);
}

int cob_save_report_txt_file(const char *path, const cob_scene *scene)
{
    return apx_save_with(path, scene, cob_export_report_txt);
}
