#include <string.h>
#include "ask3d_xform.h"

void ask3d_matrix_identity(Ask3dMatrix34 *m)
{
    if (m == 0) {
        return;
    }
    memset(m, 0, sizeof(Ask3dMatrix34));
    m->row0.x = ASK3D_FIXED_ONE;
    m->row1.y = ASK3D_FIXED_ONE;
    m->row2.z = ASK3D_FIXED_ONE;
    m->valid = 1U;
}

void ask3d_matrix_from_rows(Ask3dMatrix34 *m, const Ask3dVec3 *r0, const Ask3dVec3 *r1, const Ask3dVec3 *r2, const Ask3dVec3 *r3)
{
    if (m == 0) {
        return;
    }
    ask3d_matrix_identity(m);
    if (r0 != 0) {
        m->row0 = *r0;
    }
    if (r1 != 0) {
        m->row1 = *r1;
    }
    if (r2 != 0) {
        m->row2 = *r2;
    }
    if (r3 != 0) {
        m->row3 = *r3;
    }
}

static ask3d_fixed ask3d_dot_col(const Ask3dVec3 *r0, const Ask3dVec3 *r1, const Ask3dVec3 *r2, const Ask3dVec3 *v, int col)
{
    ask3d_fixed a;
    ask3d_fixed b;
    ask3d_fixed c;
    if (col == 0) {
        a = r0->x; b = r1->x; c = r2->x;
    } else if (col == 1) {
        a = r0->y; b = r1->y; c = r2->y;
    } else {
        a = r0->z; b = r1->z; c = r2->z;
    }
    return ask3d_fixed_mul(v->x, a) + ask3d_fixed_mul(v->y, b) + ask3d_fixed_mul(v->z, c);
}

void ask3d_matrix_mul(const Ask3dMatrix34 *a, const Ask3dMatrix34 *b, Ask3dMatrix34 *out)
{
    Ask3dMatrix34 r;
    if (out == 0) {
        return;
    }
    ask3d_matrix_identity(&r);
    if (a == 0 || b == 0 || a->valid == 0U || b->valid == 0U) {
        *out = r;
        return;
    }
    r.row0.x = ask3d_dot_col(&b->row0, &b->row1, &b->row2, &a->row0, 0);
    r.row0.y = ask3d_dot_col(&b->row0, &b->row1, &b->row2, &a->row0, 1);
    r.row0.z = ask3d_dot_col(&b->row0, &b->row1, &b->row2, &a->row0, 2);
    r.row1.x = ask3d_dot_col(&b->row0, &b->row1, &b->row2, &a->row1, 0);
    r.row1.y = ask3d_dot_col(&b->row0, &b->row1, &b->row2, &a->row1, 1);
    r.row1.z = ask3d_dot_col(&b->row0, &b->row1, &b->row2, &a->row1, 2);
    r.row2.x = ask3d_dot_col(&b->row0, &b->row1, &b->row2, &a->row2, 0);
    r.row2.y = ask3d_dot_col(&b->row0, &b->row1, &b->row2, &a->row2, 1);
    r.row2.z = ask3d_dot_col(&b->row0, &b->row1, &b->row2, &a->row2, 2);
    r.row3.x = ask3d_dot_col(&b->row0, &b->row1, &b->row2, &a->row3, 0) + b->row3.x;
    r.row3.y = ask3d_dot_col(&b->row0, &b->row1, &b->row2, &a->row3, 1) + b->row3.y;
    r.row3.z = ask3d_dot_col(&b->row0, &b->row1, &b->row2, &a->row3, 2) + b->row3.z;
    r.valid = 1U;
    *out = r;
}

void ask3d_matrix_transform_point(const Ask3dMatrix34 *m, const Ask3dVec3 *p, Ask3dVec3 *out)
{
    Ask3dVec3 r;
    if (out == 0) {
        return;
    }
    memset(&r, 0, sizeof(Ask3dVec3));
    if (m == 0 || p == 0 || m->valid == 0U) {
        if (p != 0) {
            *out = *p;
        } else {
            *out = r;
        }
        return;
    }
    r.x = ask3d_fixed_mul(p->x, m->row0.x) + ask3d_fixed_mul(p->y, m->row1.x) + ask3d_fixed_mul(p->z, m->row2.x) + m->row3.x;
    r.y = ask3d_fixed_mul(p->x, m->row0.y) + ask3d_fixed_mul(p->y, m->row1.y) + ask3d_fixed_mul(p->z, m->row2.y) + m->row3.y;
    r.z = ask3d_fixed_mul(p->x, m->row0.z) + ask3d_fixed_mul(p->y, m->row1.z) + ask3d_fixed_mul(p->z, m->row2.z) + m->row3.z;
    *out = r;
}

void ask3d_matrix_transform_vector(const Ask3dMatrix34 *m, const Ask3dVec3 *p, Ask3dVec3 *out)
{
    Ask3dVec3 r;
    if (out == 0) {
        return;
    }
    memset(&r, 0, sizeof(Ask3dVec3));
    if (m == 0 || p == 0 || m->valid == 0U) {
        if (p != 0) {
            *out = *p;
        } else {
            *out = r;
        }
        return;
    }
    r.x = ask3d_fixed_mul(p->x, m->row0.x) + ask3d_fixed_mul(p->y, m->row1.x) + ask3d_fixed_mul(p->z, m->row2.x);
    r.y = ask3d_fixed_mul(p->x, m->row0.y) + ask3d_fixed_mul(p->y, m->row1.y) + ask3d_fixed_mul(p->z, m->row2.y);
    r.z = ask3d_fixed_mul(p->x, m->row0.z) + ask3d_fixed_mul(p->y, m->row1.z) + ask3d_fixed_mul(p->z, m->row2.z);
    *out = r;
}

int ask3d_get_local_matrix(const Ask3dScene *scene, int node_type, int node_index, Ask3dMatrix34 *out)
{
    if (out == 0) {
        return 0;
    }
    ask3d_matrix_identity(out);
    if (scene == 0 || node_index < 0) {
        return 0;
    }
    if (node_type == ASK3D_NODE_GEOM && (unsigned int)node_index < scene->object_count) {
        if (scene->objects[node_index].has_transform != 0U) {
            ask3d_matrix_from_rows(out, &scene->objects[node_index].tm_row0, &scene->objects[node_index].tm_row1, &scene->objects[node_index].tm_row2, &scene->objects[node_index].tm_row3);
        }
        return 1;
    }
    if (node_type == ASK3D_NODE_CAMERA && (unsigned int)node_index < scene->camera_count) {
        if (scene->cameras[node_index].has_transform != 0U) {
            ask3d_matrix_from_rows(out, &scene->cameras[node_index].tm_row0, &scene->cameras[node_index].tm_row1, &scene->cameras[node_index].tm_row2, &scene->cameras[node_index].tm_row3);
        }
        return 1;
    }
    if (node_type == ASK3D_NODE_LIGHT && (unsigned int)node_index < scene->light_count) {
        if (scene->lights[node_index].has_transform != 0U) {
            ask3d_matrix_from_rows(out, &scene->lights[node_index].tm_row0, &scene->lights[node_index].tm_row1, &scene->lights[node_index].tm_row2, &scene->lights[node_index].tm_row3);
        }
        return 1;
    }
    if (node_type == ASK3D_NODE_HELPER && (unsigned int)node_index < scene->helper_count) {
        if (scene->helpers[node_index].has_transform != 0U) {
            ask3d_matrix_from_rows(out, &scene->helpers[node_index].tm_row0, &scene->helpers[node_index].tm_row1, &scene->helpers[node_index].tm_row2, &scene->helpers[node_index].tm_row3);
        }
        return 1;
    }
    if (node_type == ASK3D_NODE_SHAPE && (unsigned int)node_index < scene->shape_count) {
        if (scene->shapes[node_index].has_transform != 0U) {
            ask3d_matrix_from_rows(out, &scene->shapes[node_index].tm_row0, &scene->shapes[node_index].tm_row1, &scene->shapes[node_index].tm_row2, &scene->shapes[node_index].tm_row3);
        }
        return 1;
    }
    return 0;
}

static int ask3d_parent_of(const Ask3dScene *scene, int node_type, int node_index, int *ptype, int *pindex)
{
    if (ptype != 0) {
        *ptype = ASK3D_NODE_NONE;
    }
    if (pindex != 0) {
        *pindex = -1;
    }
    if (scene == 0 || node_index < 0) {
        return 0;
    }
    if (node_type == ASK3D_NODE_GEOM && (unsigned int)node_index < scene->object_count) {
        if (ptype != 0) *ptype = scene->objects[node_index].parent_type;
        if (pindex != 0) *pindex = scene->objects[node_index].parent_index;
        return scene->objects[node_index].has_parent != 0U ? 1 : 0;
    }
    if (node_type == ASK3D_NODE_CAMERA && (unsigned int)node_index < scene->camera_count) {
        if (ptype != 0) *ptype = scene->cameras[node_index].parent_type;
        if (pindex != 0) *pindex = scene->cameras[node_index].parent_index;
        return scene->cameras[node_index].has_parent != 0U ? 1 : 0;
    }
    if (node_type == ASK3D_NODE_LIGHT && (unsigned int)node_index < scene->light_count) {
        if (ptype != 0) *ptype = scene->lights[node_index].parent_type;
        if (pindex != 0) *pindex = scene->lights[node_index].parent_index;
        return scene->lights[node_index].has_parent != 0U ? 1 : 0;
    }
    if (node_type == ASK3D_NODE_HELPER && (unsigned int)node_index < scene->helper_count) {
        if (ptype != 0) *ptype = scene->helpers[node_index].parent_type;
        if (pindex != 0) *pindex = scene->helpers[node_index].parent_index;
        return scene->helpers[node_index].has_parent != 0U ? 1 : 0;
    }
    if (node_type == ASK3D_NODE_SHAPE && (unsigned int)node_index < scene->shape_count) {
        if (ptype != 0) *ptype = scene->shapes[node_index].parent_type;
        if (pindex != 0) *pindex = scene->shapes[node_index].parent_index;
        return scene->shapes[node_index].has_parent != 0U ? 1 : 0;
    }
    return 0;
}

int ask3d_get_world_matrix(const Ask3dScene *scene, int node_type, int node_index, Ask3dMatrix34 *out)
{
    Ask3dMatrix34 stack[ASK3D_MAX_NODE_DEPTH];
    Ask3dMatrix34 local;
    Ask3dMatrix34 accum;
    int ptype;
    int pindex;
    int cur_type;
    int cur_index;
    unsigned int depth;
    unsigned int i;

    if (out == 0) {
        return 0;
    }
    ask3d_matrix_identity(out);
    if (scene == 0) {
        return 0;
    }
    cur_type = node_type;
    cur_index = node_index;
    depth = 0U;
    while (cur_type != ASK3D_NODE_NONE && cur_index >= 0 && depth < ASK3D_MAX_NODE_DEPTH) {
        if (!ask3d_get_local_matrix(scene, cur_type, cur_index, &local)) {
            return 0;
        }
        stack[depth] = local;
        depth++;
        ptype = ASK3D_NODE_NONE;
        pindex = -1;
        if (!ask3d_parent_of(scene, cur_type, cur_index, &ptype, &pindex)) {
            break;
        }
        cur_type = ptype;
        cur_index = pindex;
    }
    ask3d_matrix_identity(&accum);
    i = depth;
    while (i > 0U) {
        Ask3dMatrix34 tmp;
        i--;
        ask3d_matrix_mul(&accum, &stack[i], &tmp);
        accum = tmp;
    }
    *out = accum;
    return 1;
}
