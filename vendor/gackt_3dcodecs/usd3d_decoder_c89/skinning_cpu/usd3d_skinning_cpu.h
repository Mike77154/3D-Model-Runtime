#ifndef USD3D_SKINNING_CPU_H
#define USD3D_SKINNING_CPU_H

#include "usd3d.h"

#define USD3D_SKINNING_MAX_INFLUENCES 16

int usd3d_skinning_transform_point(const usd3d_fixed *m16, const usd3d_vec3 *in_point, usd3d_vec3 *out_point);
int usd3d_skinning_cpu_vertex(const usd3d_vec3 *bind_point,
                              const usd3d_u32 *joint_indices,
                              const usd3d_fixed *joint_weights,
                              unsigned long influence_count,
                              const usd3d_fixed *joint_matrices,
                              unsigned long joint_count,
                              usd3d_vec3 *out_point);
int usd3d_skinning_normalize_weights(usd3d_fixed *weights, unsigned long count);

#endif
