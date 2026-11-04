#include "usd3d_skinning_bake.h"
#include <string.h>

static void sb_copy(char *dst, unsigned long cap, const char *src)
{
    unsigned long i;
    if (!dst || cap == 0UL) return;
    if (!src) src = "";
    i = 0UL;
    while (src[i] && i + 1UL < cap) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

void usd3d_skin_bake_identity(usd3d_fixed *m16)
{
    unsigned long i;
    if (!m16) return;
    for (i = 0UL; i < 16UL; i++) m16[i] = 0;
    m16[0] = USD3D_FIXED_ONE;
    m16[5] = USD3D_FIXED_ONE;
    m16[10] = USD3D_FIXED_ONE;
    m16[15] = USD3D_FIXED_ONE;
}

static void sb_mul_mat(const usd3d_fixed *a, const usd3d_fixed *b, usd3d_fixed *out)
{
    int r;
    int c;
    int k;
    usd3d_fixed tmp[16];
    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++) {
            usd3d_fixed acc;
            acc = 0;
            for (k = 0; k < 4; k++) {
                acc += usd3d_fixed_mul(a[(r * 4) + k], b[(k * 4) + c]);
            }
            tmp[(r * 4) + c] = acc;
        }
    }
    for (r = 0; r < 16; r++) out[r] = tmp[r];
}

void usd3d_skin_bake_skeleton_init(usd3d_skin_bake_skeleton *skel)
{
    if (!skel) return;
    memset(skel, 0, sizeof(*skel));
    usd3d_skin_bake_identity(skel->geom_bind_xform);
}

void usd3d_skin_bake_mesh_init(usd3d_skin_bake_mesh *mesh)
{
    if (!mesh) return;
    memset(mesh, 0, sizeof(*mesh));
}

int usd3d_skin_bake_add_joint(usd3d_skin_bake_skeleton *skel, const char *name, long parent, const usd3d_fixed *local, const usd3d_fixed *inverse_bind)
{
    usd3d_skin_bake_joint *j;
    if (!skel || !name) return USD3D_ERR_BAD_ARG;
    if (skel->joint_count >= USD3D_SKIN_BAKE_MAX_JOINTS) return USD3D_ERR_LIMIT;
    j = &skel->joints[skel->joint_count];
    memset(j, 0, sizeof(*j));
    sb_copy(j->name, USD3D_PATH_MAX, name);
    j->parent = parent;
    if (local) memcpy(j->local, local, sizeof(j->local)); else usd3d_skin_bake_identity(j->local);
    if (inverse_bind) memcpy(j->inverse_bind, inverse_bind, sizeof(j->inverse_bind)); else usd3d_skin_bake_identity(j->inverse_bind);
    usd3d_skin_bake_identity(j->world);
    usd3d_skin_bake_identity(j->skin_matrix);
    j->active = 1;
    skel->joint_count++;
    return USD3D_OK;
}

int usd3d_skin_bake_update_world(usd3d_skin_bake_skeleton *skel)
{
    unsigned long i;
    if (!skel) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < skel->joint_count; i++) {
        if (!skel->joints[i].active) continue;
        if (skel->joints[i].parent >= 0L && (unsigned long)skel->joints[i].parent < skel->joint_count) {
            sb_mul_mat(skel->joints[(unsigned long)skel->joints[i].parent].world, skel->joints[i].local, skel->joints[i].world);
        } else {
            memcpy(skel->joints[i].world, skel->joints[i].local, sizeof(skel->joints[i].world));
        }
    }
    return USD3D_OK;
}

int usd3d_skin_bake_compute_skin_matrices(usd3d_skin_bake_skeleton *skel)
{
    unsigned long i;
    if (!skel) return USD3D_ERR_BAD_ARG;
    (void)usd3d_skin_bake_update_world(skel);
    for (i = 0UL; i < skel->joint_count; i++) {
        if (!skel->joints[i].active) continue;
        sb_mul_mat(skel->joints[i].world, skel->joints[i].inverse_bind, skel->joints[i].skin_matrix);
    }
    return USD3D_OK;
}

int usd3d_skin_bake_set_vertex(usd3d_skin_bake_mesh *mesh, unsigned long index, const usd3d_vec3 *point, const usd3d_vec3 *normal, const usd3d_vec3 *tangent)
{
    if (!mesh || !point) return USD3D_ERR_BAD_ARG;
    if (index >= USD3D_SKIN_BAKE_MAX_VERTICES) return USD3D_ERR_LIMIT;
    mesh->vertices[index].point = *point;
    if (normal) mesh->vertices[index].normal = *normal;
    if (tangent) mesh->vertices[index].tangent = *tangent;
    if (index >= mesh->vertex_count) mesh->vertex_count = index + 1UL;
    return USD3D_OK;
}

int usd3d_skin_bake_set_influence(usd3d_skin_bake_mesh *mesh, unsigned long vertex, unsigned long slot, unsigned long joint, usd3d_fixed weight)
{
    if (!mesh) return USD3D_ERR_BAD_ARG;
    if (vertex >= USD3D_SKIN_BAKE_MAX_VERTICES || slot >= USD3D_SKINNING_RUNTIME_MAX_INFLUENCES) return USD3D_ERR_LIMIT;
    mesh->vertices[vertex].joint_indices[slot] = (usd3d_u32)joint;
    mesh->vertices[vertex].joint_weights[slot] = weight;
    if (slot + 1UL > mesh->vertices[vertex].influence_count) mesh->vertices[vertex].influence_count = slot + 1UL;
    if (vertex >= mesh->vertex_count) mesh->vertex_count = vertex + 1UL;
    return USD3D_OK;
}

int usd3d_skin_bake_run(const usd3d_skin_bake_skeleton *skel, usd3d_skin_bake_mesh *mesh)
{
    unsigned long i;
    usd3d_fixed mats[USD3D_SKIN_BAKE_MAX_JOINTS * 16UL];
    if (!skel || !mesh) return USD3D_ERR_BAD_ARG;
    if (skel->joint_count > USD3D_SKIN_BAKE_MAX_JOINTS) return USD3D_ERR_LIMIT;
    for (i = 0UL; i < skel->joint_count; i++) {
        memcpy(&mats[i * 16UL], skel->joints[i].skin_matrix, 16UL * sizeof(usd3d_fixed));
    }
    for (i = 0UL; i < mesh->vertex_count; i++) {
        (void)usd3d_skinning_vertex_runtime(&mesh->vertices[i], mats, skel->joint_count, skel->geom_bind_xform, 0, 0, 0UL, &mesh->results[i]);
    }
    return USD3D_OK;
}

int usd3d_skin_bake_emit(const usd3d_skin_bake_mesh *mesh, int (*vertex_cb)(void *user, unsigned long index, const usd3d_skinning_result *r), void *user)
{
    unsigned long i;
    int rc;
    if (!mesh) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < mesh->vertex_count; i++) {
        if (vertex_cb) {
            rc = vertex_cb(user, i, &mesh->results[i]);
            if (rc != 0) return rc;
        }
    }
    return USD3D_OK;
}
