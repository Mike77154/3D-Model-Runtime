#include "pskpsa3d.h"

static psk3d_model g_model;
static psk3d_animset g_animset;
static psk3d_bone_map g_map;
static psk3d_skin_cache g_cache;
static psk3d_pose g_bind_local;
static psk3d_pose g_local_pose;
static psk3d_pose g_global_pose;
static psk3d_anim_stack g_stack;
static psk3d_skinned_points g_points;
static psk3d_skinned_normals g_normals;
static psk3d_triangle_stream g_stream;

int build_frame(unsigned long idle_seq,
                unsigned long aim_seq,
                long t_q16)
{
    if (psk3d_make_bind_pose_from_model(&g_model, &g_bind_local) != PSK3D_OK) return 0;
    if (psk3d_build_skin_cache(&g_model, &g_cache) != PSK3D_OK) return 0;
    if (psk3d_build_bone_map(&g_model, &g_animset,
        PSK3D_BONEMAP_CASE_INSENSITIVE | PSK3D_BONEMAP_ROOT_FALLBACK,
        &g_map) != PSK3D_OK) return 0;

    psk3d_clear_anim_stack(&g_stack);
    if (psk3d_anim_stack_push(&g_stack, (psk3d_u32)idle_seq, 0u, t_q16,
        PSK3D_Q16_ONE, PSK3D_ROT_NLERP, PSK3D_BLEND_OVERRIDE) != PSK3D_OK) return 0;
    if (psk3d_anim_stack_push(&g_stack, (psk3d_u32)aim_seq, 0u, t_q16,
        PSK3D_Q16_HALF, PSK3D_ROT_SLERP, PSK3D_BLEND_ADDITIVE) != PSK3D_OK) return 0;

    if (psk3d_sample_anim_stack(&g_animset, &g_stack, &g_bind_local, &g_local_pose) != PSK3D_OK) return 0;
    if (psk3d_globalize_pose(g_model.bones, g_model.bone_count, &g_local_pose, &g_global_pose) != PSK3D_OK) return 0;
    if (psk3d_skin_points_lbs_cached(&g_model, &g_cache, &g_global_pose, &g_points) != PSK3D_OK) return 0;
    if (psk3d_skin_normals_lbs_cached(&g_model, &g_cache, &g_global_pose, &g_normals) != PSK3D_OK) return 0;
    if (psk3d_build_triangle_stream(&g_model, &g_points, &g_normals, &g_stream) != PSK3D_OK) return 0;

    return 1;
}
