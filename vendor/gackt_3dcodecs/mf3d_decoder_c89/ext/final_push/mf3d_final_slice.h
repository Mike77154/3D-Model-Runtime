#ifndef MF3D_FINAL_SLICE_H
#define MF3D_FINAL_SLICE_H

#include "../../src/mf3d.h"
#include "../eval_heavy/mf3d_eval_mesh.h"

#ifndef MF3D_FINAL_MAX_SEGMENTS
#define MF3D_FINAL_MAX_SEGMENTS 65536
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_slice_segment_s {
    mf3d_vec3 a;
    mf3d_vec3 b;
} mf3d_slice_segment;

typedef struct mf3d_slice_segments_s {
    mf3d_slice_segment segments[MF3D_FINAL_MAX_SEGMENTS];
    mf3d_u32 segment_count;
    mf3d_fix z;
} mf3d_slice_segments;

void mf3d_slice_segments_init(mf3d_slice_segments *s, mf3d_fix z);
int mf3d_eval_slice_mesh_z(const mf3d_eval_mesh *mesh, mf3d_fix z, mf3d_slice_segments *out_segments);
int mf3d_eval_slice_model_z(const mf3d_model *model, mf3d_fix z, mf3d_slice_segments *out_segments);

#ifdef __cplusplus
}
#endif
#endif
