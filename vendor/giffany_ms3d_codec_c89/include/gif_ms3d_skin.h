#ifndef GIF_MS3D_SKIN_H
#define GIF_MS3D_SKIN_H
/* MilkShape 3D skeletal posing, model-space LBS and corner normals.
 * Portable C89, signed Q16.16, no dynamic allocation, no float/double,
 * no 64-bit integer. User owns pose/vertex buffers (use static storage).
 * MS3D angles are RADIANS; local pose = reference * animated delta.
 */
#include "gif_ms3d_normals.h"

#define MS3D_ROTATION_EULER 0
#define MS3D_ROTATION_QUAT_NLERP 1
#define MS3D_ROTATION_QUAT_SLERP 2

typedef struct ms3d_affine_s {
    ms3d_fx m[3][4]; /* row-major [R|T], column-vector convention */
} ms3d_affine;

typedef struct ms3d_skin_pose_s {
    ms3d_affine bind_global[MS3D_MAX_JOINTS];
    ms3d_affine bind_inverse[MS3D_MAX_JOINTS];
    ms3d_affine global[MS3D_MAX_JOINTS];
    ms3d_affine skin[MS3D_MAX_JOINTS];
    ms3d_u16 joint_order[MS3D_MAX_JOINTS];
    ms3d_u16 joint_count;
    ms3d_fx time;
} ms3d_skin_pose;

typedef struct ms3d_skin_vertices_s {
    ms3d_fx xyz[MS3D_MAX_VERTICES][3];
} ms3d_skin_vertices;

/* bind stage once/model, then evaluate for each time in seconds Q16.16 */
int ms3d_skin_prepare(const ms3d_model *model, ms3d_skin_pose *pose);
int ms3d_skin_evaluate(const ms3d_model *model, ms3d_skin_pose *pose, ms3d_fx time);
/* Optional fixed-point quaternion NLERP, original Euler path retained. */
int ms3d_skin_evaluate_mode(const ms3d_model *model, ms3d_skin_pose *pose,
                            ms3d_fx time, int rotation_mode);
/* Crossfade two sampled poses of the SAME model/skeleton, in local-joint space.
 * Blends relative rotations as quaternions and translations linearly;
 * then rebuilds global hierarchy and skin matrices. */
int ms3d_skin_evaluate_blend(const ms3d_model *model,ms3d_skin_pose *pose,
         ms3d_fx time_a,ms3d_fx time_b,ms3d_fx blend,
         int rotation_mode);
int ms3d_skin_positions(const ms3d_model *model, const ms3d_skin_pose *pose,
                        ms3d_skin_vertices *out);
int ms3d_skin_normal(const ms3d_model *model, const ms3d_skin_pose *pose,
                     ms3d_u16 vertex_id, const ms3d_fx normal[3],
                     ms3d_fx out_normal[3]);
int ms3d_emit_skinned_corners(const ms3d_model *model,
                              const ms3d_skin_pose *pose,
                              const ms3d_skin_vertices *vertices,
                              const ms3d_corner_normals *normals,
                              const ms3d_shaded_sink *sink);

/* Minimal diagnostic math entrypoints for unit testing. */
void ms3d_affine_identity(ms3d_affine *out);
void ms3d_affine_mul(const ms3d_affine *a, const ms3d_affine *b, ms3d_affine *out);
void ms3d_affine_point(const ms3d_affine *m, const ms3d_fx p[3], ms3d_fx out[3]);
void ms3d_affine_direction(const ms3d_affine *m, const ms3d_fx p[3], ms3d_fx out[3]);
void ms3d_affine_inverse_rigid(const ms3d_affine *m, ms3d_affine *out);
#endif
