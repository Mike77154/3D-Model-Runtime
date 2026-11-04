#ifndef MF3D_FINAL_TEXTURE_H
#define MF3D_FINAL_TEXTURE_H

#include "../../src/mf3d.h"
#include "../mf3d_ext_materials.h"
#include "../eval_heavy/mf3d_eval_png.h"

#ifdef __cplusplus
extern "C" {
#endif

int mf3d_eval_texture_sample_rgba(const mf3d_eval_image_u8 *img, mf3d_fix u, mf3d_fix v, mf3d_ext_rgba *out_rgba);
int mf3d_eval_triangle_display_rgba(const mf3d_materials *mats, const mf3d_tri *tri, const mf3d_eval_image_u8 *bound_texture, mf3d_ext_rgba *out_rgba);
int mf3d_eval_materials_count_used(const mf3d_model *model, const mf3d_materials *mats, mf3d_u32 *base_hits, mf3d_u32 *color_hits, mf3d_u32 *tex_hits);

#ifdef __cplusplus
}
#endif
#endif
