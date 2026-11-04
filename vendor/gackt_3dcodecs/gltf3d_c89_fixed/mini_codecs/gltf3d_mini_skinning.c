#include "gltf3d_mini_skinning.h"

static gltf3d_fx fx_mul(gltf3d_fx a, gltf3d_fx b)
{
    /* Strict C89 fallback: split into 8.8 chunks to avoid long long.
       This is lower precision than a 64-bit multiply, but stays heapless and portable. */
    gltf3d_fx ah;
    gltf3d_fx bh;
    ah = a >> 8;
    bh = b >> 8;
    return ah * bh;
}

static void mat4_vec3(gltf3d_fx out[3], const gltf3d_fx m[16], gltf3d_fx x, gltf3d_fx y, gltf3d_fx z)
{
    out[0] = fx_mul(m[0], x) + fx_mul(m[4], y) + fx_mul(m[8], z) + m[12];
    out[1] = fx_mul(m[1], x) + fx_mul(m[5], y) + fx_mul(m[9], z) + m[13];
    out[2] = fx_mul(m[2], x) + fx_mul(m[6], y) + fx_mul(m[10], z) + m[14];
}

static void mat4_copy(gltf3d_fx d[16], const gltf3d_fx s[16])
{
    int i;
    for (i = 0; i < 16; ++i) d[i] = s[i];
}

int gltf3d_mini_skin_pose_from_node_matrices(const gltf3d_asset *asset, int skin_index, const gltf3d_fx node_world[GLTF3D_MAX_NODES][16], gltf3d_mini_skin_pose *pose)
{
    const gltf3d_skin *s;
    int i;
    int j;
    gltf3d_fx ibm[16];
    gltf3d_fx tmp[16];
    if (!asset || !node_world || !pose) return GLTF3D_MINI_ERR_BAD_ARG;
    if (skin_index < 0 || skin_index >= asset->skin_count) return GLTF3D_MINI_ERR_RANGE;
    s = &asset->skins[skin_index];
    pose->joint_count = s->joint_count;
    for (i = 0; i < s->joint_count; ++i) {
        for (j = 0; j < 16; ++j) ibm[j] = (j == 0 || j == 5 || j == 10 || j == 15) ? GLTF3D_FX_ONE : 0;
        if (s->inverse_bind_matrices >= 0) {
            for (j = 0; j < 16; ++j) {
                if (gltf3d_accessor_read_fixed(asset, s->inverse_bind_matrices, (unsigned long)i, j, &ibm[j]) != GLTF3D_OK) return GLTF3D_MINI_ERR_RANGE;
            }
        }
        if (s->joints[i] < 0 || s->joints[i] >= GLTF3D_MAX_NODES) return GLTF3D_MINI_ERR_RANGE;
        gltf3d_fx_mat4_mul(tmp, node_world[s->joints[i]], ibm);
        mat4_copy(pose->joint_matrices[i], tmp);
    }
    return GLTF3D_MINI_OK;
}

static int read_joint_weight(const gltf3d_asset *asset, int joints_acc, int weights_acc, unsigned long vertex_index, int k, unsigned long *joint, gltf3d_fx *weight)
{
    gltf3d_fx jf;
    gltf3d_fx wf;
    unsigned long ju;
    if (joints_acc < 0 || weights_acc < 0) return GLTF3D_MINI_ERR_UNSUPPORTED;
    if (gltf3d_accessor_read_u32(asset, joints_acc, vertex_index, &ju) == GLTF3D_OK && k == 0) {
        *joint = ju;
    } else {
        if (gltf3d_accessor_read_fixed(asset, joints_acc, vertex_index, k, &jf) != GLTF3D_OK) return GLTF3D_MINI_ERR_RANGE;
        *joint = (unsigned long)gltf3d_fx_to_int(jf);
    }
    if (gltf3d_accessor_read_fixed(asset, weights_acc, vertex_index, k, &wf) != GLTF3D_OK) return GLTF3D_MINI_ERR_RANGE;
    *weight = wf;
    return GLTF3D_MINI_OK;
}

int gltf3d_mini_skin_vertex(const gltf3d_asset *asset, int prim_index, unsigned long vertex_index, const gltf3d_mini_skin_pose *pose, gltf3d_mini_skinned_vertex *out)
{
    int pos_acc;
    int joints_acc;
    int weights_acc;
    gltf3d_fx x;
    gltf3d_fx y;
    gltf3d_fx z;
    gltf3d_fx acc[3];
    gltf3d_fx t[3];
    unsigned long joint;
    gltf3d_fx weight;
    int k;
    const gltf3d_primitive *p;
    if (!asset || !pose || !out) return GLTF3D_MINI_ERR_BAD_ARG;
    if (prim_index < 0 || prim_index >= asset->primitive_count) return GLTF3D_MINI_ERR_RANGE;
    p = &asset->primitives[prim_index];
    pos_acc = p->attributes[GLTF3D_ATTR_POSITION];
    joints_acc = p->attributes[GLTF3D_ATTR_JOINTS_0];
    weights_acc = p->attributes[GLTF3D_ATTR_WEIGHTS_0];
    if (pos_acc < 0) return GLTF3D_MINI_ERR_UNSUPPORTED;
    if (gltf3d_accessor_read_fixed(asset, pos_acc, vertex_index, 0, &x) != GLTF3D_OK) return GLTF3D_MINI_ERR_RANGE;
    if (gltf3d_accessor_read_fixed(asset, pos_acc, vertex_index, 1, &y) != GLTF3D_OK) return GLTF3D_MINI_ERR_RANGE;
    if (gltf3d_accessor_read_fixed(asset, pos_acc, vertex_index, 2, &z) != GLTF3D_OK) return GLTF3D_MINI_ERR_RANGE;
    if (joints_acc < 0 || weights_acc < 0) {
        out->x = x; out->y = y; out->z = z;
        return GLTF3D_MINI_OK;
    }
    acc[0] = 0; acc[1] = 0; acc[2] = 0;
    for (k = 0; k < 4; ++k) {
        if (read_joint_weight(asset, joints_acc, weights_acc, vertex_index, k, &joint, &weight) != GLTF3D_MINI_OK) continue;
        if (joint < (unsigned long)pose->joint_count && weight != 0) {
            mat4_vec3(t, pose->joint_matrices[joint], x, y, z);
            acc[0] += fx_mul(t[0], weight);
            acc[1] += fx_mul(t[1], weight);
            acc[2] += fx_mul(t[2], weight);
        }
    }
    out->x = acc[0]; out->y = acc[1]; out->z = acc[2];
    return GLTF3D_MINI_OK;
}

int gltf3d_mini_skin_primitive_positions(const gltf3d_asset *asset, int prim_index, const gltf3d_mini_skin_pose *pose, gltf3d_mini_skinned_vertex *out_vertices, unsigned long out_capacity, unsigned long *out_count)
{
    int pos_acc;
    unsigned long i;
    unsigned long count;
    if (!asset || !pose || !out_vertices || !out_count) return GLTF3D_MINI_ERR_BAD_ARG;
    if (prim_index < 0 || prim_index >= asset->primitive_count) return GLTF3D_MINI_ERR_RANGE;
    pos_acc = asset->primitives[prim_index].attributes[GLTF3D_ATTR_POSITION];
    if (pos_acc < 0 || pos_acc >= asset->accessor_count) return GLTF3D_MINI_ERR_RANGE;
    count = asset->accessors[pos_acc].count;
    if (count > out_capacity) return GLTF3D_MINI_ERR_RANGE;
    for (i = 0UL; i < count; ++i) {
        if (gltf3d_mini_skin_vertex(asset, prim_index, i, pose, &out_vertices[i]) != GLTF3D_MINI_OK) return GLTF3D_MINI_ERR_RANGE;
    }
    *out_count = count;
    return GLTF3D_MINI_OK;
}
