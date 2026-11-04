#ifndef MF3D_EVAL_DISPLACE_H
#define MF3D_EVAL_DISPLACE_H

#include "../../src/mf3d.h"
#include "../mf3d_ext_displacement.h"
#include "mf3d_eval_mesh.h"
#include "mf3d_eval_png.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_eval_displace_options_s {
    mf3d_u32 subdivisions; /* 1..16 */
    mf3d_fix height;       /* override if nonzero; 16.16 model units */
    mf3d_fix offset;       /* added after height scaling */
} mf3d_eval_displace_options;

int mf3d_eval_displace_mesh(const mf3d_model *src, const mf3d_displacement *disp, const mf3d_eval_image_u8 *heightmap, mf3d_eval_mesh *out_mesh, const mf3d_eval_displace_options *opt);

#ifdef __cplusplus
}
#endif
#endif
