#ifndef MF3D_EVAL_CSG_H
#define MF3D_EVAL_CSG_H

#include "../../src/mf3d.h"
#include "../mf3d_ext_boolean.h"
#include "mf3d_eval_implicit.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_eval_csg_options_s {
    mf3d_u32 cells_x, cells_y, cells_z;
    mf3d_fix padding;
} mf3d_eval_csg_options;

/* Experimental real evaluator path: converts boolean primitives/AABB fallbacks to an SDF tree and polygonizes it.
   Mesh operands without analytic data are represented by their AABB, which is deterministic and no-heap but not a solid B-Rep kernel. */
int mf3d_eval_csg_object_aabb(const mf3d_model *src, mf3d_u32 objectid, mf3d_eval_aabb *out_box);
int mf3d_eval_csg_boolean_shape_to_implicit(const mf3d_model *src, const mf3d_booleans *bo, const mf3d_boolean_shape *shape, mf3d_impl_program *prog, mf3d_i32 *root);
int mf3d_eval_csg_polygonize_shape(const mf3d_model *src, const mf3d_booleans *bo, const mf3d_boolean_shape *shape, const mf3d_eval_csg_options *opt, mf3d_eval_mesh *out_mesh);

#ifdef __cplusplus
}
#endif
#endif
