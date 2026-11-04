#include "md4_internal.h"

md4_rc_t md4_skin_surface(const md4_model_t *model,
                          md4_i32 lod_index,
                          md4_i32 surface_index_in_lod,
                          md4_i32 frame_index,
                          md4_fx_t *out_positions_xyz,
                          md4_fx_t *out_normals_xyz,
                          md4_u32 out_vertex_capacity)
{
    md4_pose_t pose;
    md4_rc_t rc;

    rc = md4_build_pose(model, frame_index, &pose);
    if (rc != MD4_RC_OK) {
        return rc;
    }
    return md4_skin_surface_pose(model, lod_index, surface_index_in_lod,
                                 &pose,
                                 out_positions_xyz, out_normals_xyz,
                                 out_vertex_capacity);
}

md4_rc_t md4_skin_surface_lerp(const md4_model_t *model,
                               md4_i32 lod_index,
                               md4_i32 surface_index_in_lod,
                               md4_i32 old_frame_index,
                               md4_i32 new_frame_index,
                               md4_fx_t lerp_t,
                               md4_fx_t *out_positions_xyz,
                               md4_fx_t *out_normals_xyz,
                               md4_u32 out_vertex_capacity)
{
    md4_pose_t pose;
    md4_rc_t rc;

    rc = md4_build_pose_lerp(model, old_frame_index, new_frame_index, lerp_t, &pose);
    if (rc != MD4_RC_OK) {
        return rc;
    }
    return md4_skin_surface_pose(model, lod_index, surface_index_in_lod,
                                 &pose,
                                 out_positions_xyz, out_normals_xyz,
                                 out_vertex_capacity);
}
