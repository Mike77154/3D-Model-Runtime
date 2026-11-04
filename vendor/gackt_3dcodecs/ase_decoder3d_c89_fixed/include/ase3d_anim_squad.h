#ifndef ASE3D_ANIM_SQUAD_H
#define ASE3D_ANIM_SQUAD_H

#include "ase3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
   Optional quaternion curve module.
   C89, fixed-point Q16.16, no heap.  This module is intentionally outside
   the core parser so tiny importers can keep only SLERP if desired.
*/

#define ASE3D_SQUAD_ENDPOINT_CLAMP 0
#define ASE3D_SQUAD_ENDPOINT_MIRROR 1

typedef struct ase3d_squad_basis_s {
    ase3d_quat q0;
    ase3d_quat q1;
    ase3d_quat s0;
    ase3d_quat s1;
    long tick0;
    long tick1;
    int valid;
} ase3d_squad_basis;

ase3d_quat ase3d_quat_conjugate(ase3d_quat q);
ase3d_quat ase3d_quat_inverse_unit(ase3d_quat q);
ase3d_vec3 ase3d_quat_log_vec(ase3d_quat q);
ase3d_quat ase3d_quat_exp_vec(ase3d_vec3 v);
ase3d_quat ase3d_quat_squad_control(ase3d_quat q_prev, ase3d_quat q_cur, ase3d_quat q_next);
ase3d_quat ase3d_quat_squad(ase3d_quat q0, ase3d_quat q1, ase3d_quat s0, ase3d_quat s1, ase3d_fixed t);
int ase3d_build_squad_basis_for_tick(const ase3d_model *m, int object_id, long tick, int endpoint_mode, ase3d_squad_basis *out);
int ase3d_sample_rotation_squad(const ase3d_model *m, int object_id, long tick, ase3d_quat *out);
int ase3d_sample_world_matrix_squad(const ase3d_model *m, int object_id, long tick, ase3d_mat43 *out);

#ifdef __cplusplus
}
#endif

#endif
