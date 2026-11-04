#ifndef GLTF3D_MINI_SKINNING_H
#define GLTF3D_MINI_SKINNING_H

#include "gltf3d_mini_common.h"

#define GLTF3D_SKIN_VERTEX_LIMIT 65535UL

typedef struct gltf3d_mini_skin_pose_s {
    gltf3d_fx joint_matrices[GLTF3D_MAX_JOINTS_PER_SKIN][16];
    int joint_count;
} gltf3d_mini_skin_pose;

typedef struct gltf3d_mini_skinned_vertex_s {
    gltf3d_fx x;
    gltf3d_fx y;
    gltf3d_fx z;
} gltf3d_mini_skinned_vertex;

int gltf3d_mini_skin_pose_from_node_matrices(const gltf3d_asset *asset, int skin_index, const gltf3d_fx node_world[GLTF3D_MAX_NODES][16], gltf3d_mini_skin_pose *pose);
int gltf3d_mini_skin_vertex(const gltf3d_asset *asset, int prim_index, unsigned long vertex_index, const gltf3d_mini_skin_pose *pose, gltf3d_mini_skinned_vertex *out);
int gltf3d_mini_skin_primitive_positions(const gltf3d_asset *asset, int prim_index, const gltf3d_mini_skin_pose *pose, gltf3d_mini_skinned_vertex *out_vertices, unsigned long out_capacity, unsigned long *out_count);

#endif
