#ifndef GLTF3D_UTIL_H
#define GLTF3D_UTIL_H

#include "gltf3d.h"

#ifdef __cplusplus
extern "C" {
#endif

int gltf3d_component_size(int component_type);
unsigned long gltf3d_accessor_elem_size(const gltf3d_accessor *ac);
int gltf3d_accessor_byte_range(const gltf3d_asset *a, int accessor_index,
                               unsigned long *byte_offset, unsigned long *byte_length,
                               unsigned long *stride);
int gltf3d_accessor_compute_bounds(const gltf3d_asset *a, int accessor_index,
                                   gltf3d_fx *minv, gltf3d_fx *maxv, int max_comp);
unsigned long gltf3d_primitive_vertex_count(const gltf3d_asset *a, int prim_index);
unsigned long gltf3d_primitive_index_count(const gltf3d_asset *a, int prim_index);
unsigned long gltf3d_primitive_triangle_count(const gltf3d_asset *a, int prim_index);
int gltf3d_primitive_read_triangle(const gltf3d_asset *a, int prim_index,
                                   unsigned long tri_index,
                                   unsigned long out_indices[3]);
int gltf3d_node_world_matrices(const gltf3d_asset *a, int scene_index,
                               gltf3d_fx *out_matrices16,
                               int out_node_capacity);
int gltf3d_transform_point(const gltf3d_fx m[16],
                           gltf3d_fx x, gltf3d_fx y, gltf3d_fx z,
                           gltf3d_fx out_xyz[3]);

#ifdef __cplusplus
}
#endif
#endif
