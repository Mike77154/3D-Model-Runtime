#include <string.h>
#include "usd3d_skel_mapper.h"

static void sm_copy(char *dst, unsigned long cap, const char *src)
{
    unsigned long i;
    if (!dst || cap == 0UL) return;
    if (!src) src = "";
    i = 0UL;
    while (i + 1UL < cap && src[i] != '\0') { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

void usd3d_skel_mapper_init(usd3d_skel_mapper *m)
{
    unsigned long i;
    if (!m) return;
    memset(m, 0, sizeof(*m));
    for (i = 0UL; i < USD3D_SKEL_MAPPER_MAX_MESH_JOINTS; ++i) m->remap[i] = -1L;
}

int usd3d_skel_mapper_add_skeleton_joint(usd3d_skel_mapper *m, const char *name, long parent)
{
    usd3d_skel_joint_name *j;
    if (!m || !name) return USD3D_ERR_BAD_ARG;
    if (m->skeleton_count >= USD3D_SKEL_MAPPER_MAX_JOINTS) return USD3D_ERR_LIMIT;
    j = &m->skeleton_joints[m->skeleton_count];
    memset(j, 0, sizeof(*j));
    sm_copy(j->name, USD3D_TOKEN_MAX, name);
    j->parent = parent;
    j->active = 1;
    m->skeleton_count++;
    return USD3D_OK;
}

int usd3d_skel_mapper_add_mesh_joint(usd3d_skel_mapper *m, const char *name)
{
    usd3d_skel_joint_name *j;
    if (!m || !name) return USD3D_ERR_BAD_ARG;
    if (m->mesh_count >= USD3D_SKEL_MAPPER_MAX_MESH_JOINTS) return USD3D_ERR_LIMIT;
    j = &m->mesh_joints[m->mesh_count];
    memset(j, 0, sizeof(*j));
    sm_copy(j->name, USD3D_TOKEN_MAX, name);
    j->parent = -1L;
    j->active = 1;
    m->remap[m->mesh_count] = -1L;
    m->mesh_count++;
    return USD3D_OK;
}

long usd3d_skel_mapper_find_skeleton_joint(const usd3d_skel_mapper *m, const char *name)
{
    unsigned long i;
    if (!m || !name) return -1L;
    for (i = 0UL; i < m->skeleton_count; ++i) {
        if (m->skeleton_joints[i].active && strcmp(m->skeleton_joints[i].name, name) == 0) return (long)i;
    }
    return -1L;
}

int usd3d_skel_mapper_build(usd3d_skel_mapper *m)
{
    unsigned long i;
    long sk;
    if (!m) return USD3D_ERR_BAD_ARG;
    m->missing_count = 0UL;
    for (i = 0UL; i < m->mesh_count; ++i) {
        sk = usd3d_skel_mapper_find_skeleton_joint(m, m->mesh_joints[i].name);
        m->remap[i] = sk;
        if (sk < 0L) m->missing_count++;
    }
    return m->missing_count ? USD3D_ERR_PARSE : USD3D_OK;
}

int usd3d_skel_mapper_remap_vertex(usd3d_skel_mapper *m, usd3d_skinning_vertex_io *v)
{
    unsigned long i;
    unsigned long idx;
    usd3d_fixed sum;
    int r;
    if (!m || !v) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < v->influence_count && i < USD3D_SKINNING_RUNTIME_MAX_INFLUENCES; ++i) {
        idx = (unsigned long)v->joint_indices[i];
        if (idx < m->mesh_count && m->remap[idx] >= 0L) v->joint_indices[i] = (usd3d_u32)m->remap[idx];
        else {
            v->joint_indices[i] = 0UL;
            v->joint_weights[i] = 0;
        }
    }
    sum = 0;
    r = usd3d_skinning_runtime_normalize_weights(v->joint_weights, v->influence_count, &sum);
    return r;
}

int usd3d_skel_mapper_normalize_and_remap_mesh(usd3d_skel_mapper *m, usd3d_skinning_vertex_io *vertices, unsigned long vertex_count)
{
    unsigned long i;
    int r;
    if (!m || !vertices) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < vertex_count; ++i) {
        r = usd3d_skel_mapper_remap_vertex(m, &vertices[i]);
        if (r != USD3D_OK) return r;
    }
    return USD3D_OK;
}

int usd3d_skel_mapper_emit(const usd3d_skel_mapper *m, int (*joint_cb)(void *user, unsigned long mesh_joint, long skeleton_joint, const char *name), void *user)
{
    unsigned long i;
    int r;
    if (!m) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < m->mesh_count; ++i) {
        if (joint_cb) {
            r = joint_cb(user, i, m->remap[i], m->mesh_joints[i].name);
            if (r != 0) return r;
        }
    }
    return USD3D_OK;
}
