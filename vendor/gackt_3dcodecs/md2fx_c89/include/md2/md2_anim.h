#ifndef MD2_ANIM_H
#define MD2_ANIM_H

#include "md2_model.h"

int md2_anim_sample_frame_vertex(const md2_frame_t *frame,
                                 md2_i32 num_xyz,
                                 md2_i32 xyz_index,
                                 md2_fx_vec3_t *out_position,
                                 md2_u8 *out_normal_index);

int md2_anim_lerp_vertex(const md2_frame_t *frame_a,
                         const md2_frame_t *frame_b,
                         md2_i32 num_xyz,
                         md2_i32 xyz_index,
                         md2_fx32_t t,
                         md2_fx_vec3_t *out_position,
                         md2_u8 *out_normal_index);

int md2_anim_lerp_frames(const md2_frame_t *frame_a,
                         const md2_frame_t *frame_b,
                         md2_i32 num_xyz,
                         md2_fx32_t t,
                         md2_fx_vec3_t *out_positions,
                         md2_u8 *out_normal_indices);

int md2_anim_lerp_model(const md2_model_t *model,
                        md2_i32 frame_a_index,
                        md2_i32 frame_b_index,
                        md2_fx32_t t,
                        md2_fx_vec3_t *out_positions,
                        md2_u8 *out_normal_indices);

#endif /* MD2_ANIM_H */
