#include "md4_internal.h"

static md4_rc_t md4_pose_common_check(const md4_model_t *model, const md4_pose_t *pose)
{
    if (model == 0 || pose == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (pose->num_bones != model->num_bones) {
        return MD4_RC_BAD_COUNTS;
    }
    return MD4_RC_OK;
}

void md4_pose_init(md4_pose_t *pose)
{
    if (pose == 0) {
        return;
    }
    memset(pose, 0, sizeof(*pose));
}

md4_rc_t md4_build_pose(const md4_model_t *model,
                        md4_i32 frame_index,
                        md4_pose_t *out_pose)
{
    md4_i32 i;
    md4_i32 first_bone;

    if (model == 0 || out_pose == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (frame_index < 0 || frame_index >= model->num_frames) {
        return MD4_RC_BAD_ARGUMENT;
    }

    md4_pose_init(out_pose);
    out_pose->num_bones = model->num_bones;
    first_bone = model->frames[frame_index].first_bone;
    if (first_bone < 0 || first_bone + model->num_bones > model->num_frame_bones) {
        return MD4_RC_BAD_COUNTS;
    }

    for (i = 0; i < model->num_bones; ++i) {
        out_pose->bones[i] = model->frame_bones[first_bone + i];
    }
    return MD4_RC_OK;
}

md4_rc_t md4_build_pose_lerp(const md4_model_t *model,
                             md4_i32 old_frame_index,
                             md4_i32 new_frame_index,
                             md4_fx_t lerp_t,
                             md4_pose_t *out_pose)
{
    md4_i32 i;
    md4_i32 r;
    md4_i32 c;
    md4_i32 old_first_bone;
    md4_i32 new_first_bone;

    if (model == 0 || out_pose == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (old_frame_index < 0 || old_frame_index >= model->num_frames) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (new_frame_index < 0 || new_frame_index >= model->num_frames) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (old_frame_index == new_frame_index) {
        return md4_build_pose(model, old_frame_index, out_pose);
    }

    md4_pose_init(out_pose);
    out_pose->num_bones = model->num_bones;
    old_first_bone = model->frames[old_frame_index].first_bone;
    new_first_bone = model->frames[new_frame_index].first_bone;
    if (old_first_bone < 0 || new_first_bone < 0) {
        return MD4_RC_BAD_COUNTS;
    }
    if (old_first_bone + model->num_bones > model->num_frame_bones) {
        return MD4_RC_BAD_COUNTS;
    }
    if (new_first_bone + model->num_bones > model->num_frame_bones) {
        return MD4_RC_BAD_COUNTS;
    }

    for (i = 0; i < model->num_bones; ++i) {
        const md4_bone_t *old_bone;
        const md4_bone_t *new_bone;
        md4_bone_t *dst;
        old_bone = &model->frame_bones[old_first_bone + i];
        new_bone = &model->frame_bones[new_first_bone + i];
        dst = &out_pose->bones[i];
        for (r = 0; r < 3; ++r) {
            for (c = 0; c < 4; ++c) {
                dst->matrix[r][c] = md4_fx_lerp(old_bone->matrix[r][c], new_bone->matrix[r][c], lerp_t);
            }
        }
    }

    return MD4_RC_OK;
}

md4_rc_t md4_skin_surface_pose(const md4_model_t *model,
                               md4_i32 lod_index,
                               md4_i32 surface_index_in_lod,
                               const md4_pose_t *pose,
                               md4_fx_t *out_positions_xyz,
                               md4_fx_t *out_normals_xyz,
                               md4_u32 out_vertex_capacity)
{
    md4_rc_t rc;
    md4_i32 global_surface_index;
    const md4_surface_t *surface;
    md4_i32 i;

    rc = md4_pose_common_check(model, pose);
    if (rc != MD4_RC_OK) {
        return rc;
    }
    if (out_positions_xyz == 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    if (!model->layout.weights_have_offset) {
        return MD4_RC_UNSUPPORTED;
    }

    global_surface_index = md4_get_global_surface_index(model, lod_index, surface_index_in_lod);
    if (global_surface_index < 0) {
        return MD4_RC_BAD_ARGUMENT;
    }
    surface = &model->surfaces[global_surface_index];
    if ((md4_u32)surface->num_vertices > out_vertex_capacity) {
        return MD4_RC_BUFFER_TOO_SMALL;
    }

    for (i = 0; i < surface->num_vertices; ++i) {
        const md4_vertex_t *v;
        md4_fx_t px;
        md4_fx_t py;
        md4_fx_t pz;
        md4_fx_t nx;
        md4_fx_t ny;
        md4_fx_t nz;
        md4_i32 j;

        v = &model->vertices[surface->first_vertex + i];
        px = 0;
        py = 0;
        pz = 0;
        nx = 0;
        ny = 0;
        nz = 0;

        for (j = 0; j < v->num_weights; ++j) {
            const md4_weight_t *w;
            md4_i32 bone_idx;
            const md4_bone_t *bone;
            md4_fx_t tx;
            md4_fx_t ty;
            md4_fx_t tz;
            md4_fx_t tnx;
            md4_fx_t tny;
            md4_fx_t tnz;

            w = &model->weights[v->first_weight + j];
            bone_idx = md4_resolve_bone_index(model, surface, w->bone_index);
            if (bone_idx < 0 || bone_idx >= pose->num_bones) {
                return MD4_RC_BAD_COUNTS;
            }
            bone = &pose->bones[bone_idx];

            tx = md4_fx_dot3(bone->matrix[0], w->offset) + bone->matrix[0][3];
            ty = md4_fx_dot3(bone->matrix[1], w->offset) + bone->matrix[1][3];
            tz = md4_fx_dot3(bone->matrix[2], w->offset) + bone->matrix[2][3];
            tnx = md4_fx_dot3(bone->matrix[0], v->normal);
            tny = md4_fx_dot3(bone->matrix[1], v->normal);
            tnz = md4_fx_dot3(bone->matrix[2], v->normal);

            px += md4_fx_mul(w->bone_weight, tx);
            py += md4_fx_mul(w->bone_weight, ty);
            pz += md4_fx_mul(w->bone_weight, tz);
            nx += md4_fx_mul(w->bone_weight, tnx);
            ny += md4_fx_mul(w->bone_weight, tny);
            nz += md4_fx_mul(w->bone_weight, tnz);
        }

        out_positions_xyz[i * 3 + 0] = px;
        out_positions_xyz[i * 3 + 1] = py;
        out_positions_xyz[i * 3 + 2] = pz;
        if (out_normals_xyz != 0) {
            out_normals_xyz[i * 3 + 0] = nx;
            out_normals_xyz[i * 3 + 1] = ny;
            out_normals_xyz[i * 3 + 2] = nz;
        }
    }

    return MD4_RC_OK;
}
