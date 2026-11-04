#ifndef CODEC3D_POSER_EVAL_H
#define CODEC3D_POSER_EVAL_H

#include "codec3d_poser.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
   codec3d_poser_eval.h - evaluator helpers v0.3
   C89, Q16.16, no heap. These helpers sit above the text/PMD parser.
*/

#ifndef P3D_FLATMAT_MAX
#define P3D_FLATMAT_MAX P3D_MAX_MATERIALS
#endif

#define P3D_FLATMAT_HAS_DIFFUSE 0x0001u
#define P3D_FLATMAT_HAS_IMAGE   0x0002u
#define P3D_FLATMAT_HAS_BUMP    0x0004u
#define P3D_FLATMAT_HAS_ALPHA   0x0008u
#define P3D_FLATMAT_HAS_VALUE   0x0010u

#define P3D_BLEND_REPLACE 0u
#define P3D_BLEND_ADD     1u

typedef struct P3D_Mat34_s {
    p3d_fx m[3][4];
} P3D_Mat34;

typedef struct P3D_FlatMaterial_s {
    unsigned short material;
    char name[P3D_NAME_MAX];
    char diffuse_image[P3D_PATH_MAX];
    char bump_image[P3D_PATH_MAX];
    p3d_fx diffuse[4];
    p3d_fx value[4];
    unsigned int flags;
} P3D_FlatMaterial;

p3d_fx p3d_fx_clamp(p3d_fx v, p3d_fx lo, p3d_fx hi);
p3d_fx p3d_fx_sin_deg(p3d_fx degrees);
p3d_fx p3d_fx_cos_deg(p3d_fx degrees);

void p3d_mat_identity(P3D_Mat34 *m);
void p3d_mat_from_trs_xyz(P3D_Mat34 *m, p3d_fx tx, p3d_fx ty, p3d_fx tz, p3d_fx rx_deg, p3d_fx ry_deg, p3d_fx rz_deg, p3d_fx sx, p3d_fx sy, p3d_fx sz);
P3D_Vec3 p3d_mat_apply_point(const P3D_Mat34 *m, P3D_Vec3 p);
int p3d_make_joint_matrices_from_objects(const P3D_Context *ctx, P3D_Mat34 *joint_mats, unsigned long joint_mat_count);

int p3d_eval_dependencies_full(P3D_Context *ctx, unsigned int passes);
int p3d_sample_channels_linear(P3D_Context *ctx, p3d_fx time_value);
int p3d_build_zone_weights(P3D_Context *ctx, unsigned char blend_mode);
int p3d_apply_skin_matrices(const P3D_Context *ctx, const P3D_Mat34 *joint_mats, unsigned long joint_count, P3D_Vec3 *out_vertices, unsigned long max_vertices);
int p3d_apply_full_pose(const P3D_Context *ctx, const P3D_Mat34 *joint_mats, unsigned long joint_count, P3D_Vec3 *out_vertices, unsigned long max_vertices);
int p3d_flatten_materials(const P3D_Context *ctx, P3D_FlatMaterial *out_mats, unsigned long max_mats, unsigned long *out_count);

#ifdef __cplusplus
}
#endif

#endif
