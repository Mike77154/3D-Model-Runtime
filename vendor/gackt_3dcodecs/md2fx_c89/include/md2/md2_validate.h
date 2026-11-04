#ifndef MD2_VALIDATE_H
#define MD2_VALIDATE_H

#include "md2_model.h"

int md2_validate_model(const md2_model_t *model);
int md2_validate_indices(const md2_model_t *model);
int md2_validate_glcmds(const md2_model_t *model);
md2_i32 md2_calc_framesize(md2_i32 num_xyz);
int md2_calc_offsets(const md2_model_t *model,
                     md2_i32 *out_ofs_skins,
                     md2_i32 *out_ofs_st,
                     md2_i32 *out_ofs_tris,
                     md2_i32 *out_ofs_frames,
                     md2_i32 *out_ofs_glcmds,
                     md2_i32 *out_ofs_end);

#endif /* MD2_VALIDATE_H */
