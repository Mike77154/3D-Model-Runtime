#ifndef MF3D_EXT_VALIDATE_H
#define MF3D_EXT_VALIDATE_H

#include "../src/mf3d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_validate_report_s {
    mf3d_u32 invalid_triangle_indices;
    mf3d_u32 degenerate_triangles;
    mf3d_u32 missing_build_objects;
    mf3d_u32 empty_objects;
    mf3d_u32 aabb_valid;
    mf3d_vec3 aabb_min;
    mf3d_vec3 aabb_max;
} mf3d_validate_report;

int mf3d_ext_unit_microns_per_unit(const char *unit, mf3d_u32 *microns_per_unit);
int mf3d_ext_compute_aabb(const mf3d_model *m, mf3d_vec3 *out_min, mf3d_vec3 *out_max);
int mf3d_ext_validate_model(const mf3d_model *m, mf3d_validate_report *report);
int mf3d_ext_flatten_build(const mf3d_model *src, mf3d_model *dst);

#ifdef __cplusplus
}
#endif
#endif
