#include "md2/md2_anim.h"
#include "md2/md2_error.h"
#include "md2/md2_fixed.h"

int md2_anim_sample_frame_vertex(const md2_frame_t *frame,
                                 md2_i32 num_xyz,
                                 md2_i32 xyz_index,
                                 md2_fx_vec3_t *out_position,
                                 md2_u8 *out_normal_index)
{
    const md2_packed_vertex_t *packed;

    if (frame == 0 || out_position == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (xyz_index < 0 || xyz_index >= num_xyz)
    {
        return MD2_ERR_BAD_INDEX;
    }

    packed = &frame->verts[xyz_index];
    out_position->x = md2_fx_dequantize_byte(packed->v[0], frame->translate[0], frame->scale[0]);
    out_position->y = md2_fx_dequantize_byte(packed->v[1], frame->translate[1], frame->scale[1]);
    out_position->z = md2_fx_dequantize_byte(packed->v[2], frame->translate[2], frame->scale[2]);

    if (out_normal_index != 0)
    {
        *out_normal_index = packed->lightnormalindex;
    }

    return MD2_OK;
}

int md2_anim_lerp_vertex(const md2_frame_t *frame_a,
                         const md2_frame_t *frame_b,
                         md2_i32 num_xyz,
                         md2_i32 xyz_index,
                         md2_fx32_t t,
                         md2_fx_vec3_t *out_position,
                         md2_u8 *out_normal_index)
{
    md2_fx_vec3_t a_pos;
    md2_fx_vec3_t b_pos;
    md2_u8 a_norm;
    md2_u8 b_norm;
    int rc;
    md2_fx32_t clamped;

    if (frame_a == 0 || frame_b == 0 || out_position == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    rc = md2_anim_sample_frame_vertex(frame_a, num_xyz, xyz_index, &a_pos, &a_norm);
    if (rc != MD2_OK)
    {
        return rc;
    }

    rc = md2_anim_sample_frame_vertex(frame_b, num_xyz, xyz_index, &b_pos, &b_norm);
    if (rc != MD2_OK)
    {
        return rc;
    }

    clamped = md2_fx_clamp_unit(t);
    out_position->x = md2_fx_lerp(a_pos.x, b_pos.x, clamped);
    out_position->y = md2_fx_lerp(a_pos.y, b_pos.y, clamped);
    out_position->z = md2_fx_lerp(a_pos.z, b_pos.z, clamped);

    if (out_normal_index != 0)
    {
        *out_normal_index = (clamped < MD2_FX_HALF) ? a_norm : b_norm;
    }

    return MD2_OK;
}

int md2_anim_lerp_frames(const md2_frame_t *frame_a,
                         const md2_frame_t *frame_b,
                         md2_i32 num_xyz,
                         md2_fx32_t t,
                         md2_fx_vec3_t *out_positions,
                         md2_u8 *out_normal_indices)
{
    md2_i32 i;
    int rc;

    if (frame_a == 0 || frame_b == 0 || out_positions == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (num_xyz < 0 || num_xyz > MD2_MAX_VERTS)
    {
        return MD2_ERR_OVER_CAPACITY;
    }

    for (i = 0; i < num_xyz; ++i)
    {
        rc = md2_anim_lerp_vertex(frame_a,
                                  frame_b,
                                  num_xyz,
                                  i,
                                  t,
                                  &out_positions[i],
                                  (out_normal_indices != 0) ? &out_normal_indices[i] : 0);
        if (rc != MD2_OK)
        {
            return rc;
        }
    }

    return MD2_OK;
}

int md2_anim_lerp_model(const md2_model_t *model,
                        md2_i32 frame_a_index,
                        md2_i32 frame_b_index,
                        md2_fx32_t t,
                        md2_fx_vec3_t *out_positions,
                        md2_u8 *out_normal_indices)
{
    if (model == 0 || out_positions == 0)
    {
        return MD2_ERR_BAD_ARG;
    }
    if (frame_a_index < 0 || frame_a_index >= model->num_frames ||
        frame_b_index < 0 || frame_b_index >= model->num_frames)
    {
        return MD2_ERR_BAD_INDEX;
    }

    return md2_anim_lerp_frames(&model->frames[frame_a_index],
                                &model->frames[frame_b_index],
                                model->num_xyz,
                                t,
                                out_positions,
                                out_normal_indices);
}
