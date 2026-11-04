#ifndef MF3D_FINAL_EXPORT_H
#define MF3D_FINAL_EXPORT_H

#include "../../src/mf3d.h"
#include "../eval_heavy/mf3d_eval_mesh.h"

#ifdef __cplusplus
extern "C" {
#endif

int mf3d_eval_mesh_write_ascii_stl(const mf3d_eval_mesh *mesh, char *dst, mf3d_u32 dst_cap, mf3d_u32 *out_len, const char *name);
int mf3d_eval_model_write_ascii_stl(const mf3d_model *model, char *dst, mf3d_u32 dst_cap, mf3d_u32 *out_len, const char *name);

#ifdef __cplusplus
}
#endif
#endif
