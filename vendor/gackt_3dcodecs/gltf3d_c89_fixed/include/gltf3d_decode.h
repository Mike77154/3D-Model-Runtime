#ifndef GLTF3D_DECODE_H
#define GLTF3D_DECODE_H

#include "gltf3d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gltf3d_decoded_vertex_s {
    gltf3d_fx position[3];
    gltf3d_fx normal[3];
    gltf3d_fx tangent[4];
    gltf3d_fx texcoord0[2];
    gltf3d_fx texcoord1[2];
    gltf3d_fx color0[4];
    unsigned long joints0[4];
    gltf3d_fx weights0[4];
    unsigned long flags;
} gltf3d_decoded_vertex;

#define GLTF3D_DV_HAS_POSITION 1UL
#define GLTF3D_DV_HAS_NORMAL 2UL
#define GLTF3D_DV_HAS_TANGENT 4UL
#define GLTF3D_DV_HAS_TEXCOORD0 8UL
#define GLTF3D_DV_HAS_TEXCOORD1 16UL
#define GLTF3D_DV_HAS_COLOR0 32UL
#define GLTF3D_DV_HAS_JOINTS0 64UL
#define GLTF3D_DV_HAS_WEIGHTS0 128UL

typedef struct gltf3d_decode_stats_s {
    unsigned long vertex_count;
    unsigned long index_count;
    unsigned long triangle_count;
    unsigned long attribute_mask;
    int mode;
    int material;
} gltf3d_decode_stats;

int gltf3d_decode_primitive_stats(const gltf3d_asset *a, int prim_index, gltf3d_decode_stats *stats);
int gltf3d_decode_primitive_vertices(const gltf3d_asset *a, int prim_index,
                                     gltf3d_decoded_vertex *vertices,
                                     unsigned long vertex_capacity,
                                     unsigned long *vertex_count);
int gltf3d_decode_primitive_triangle_indices(const gltf3d_asset *a, int prim_index,
                                             unsigned long *indices,
                                             unsigned long index_capacity,
                                             unsigned long *index_count);

#ifdef __cplusplus
}
#endif
#endif
