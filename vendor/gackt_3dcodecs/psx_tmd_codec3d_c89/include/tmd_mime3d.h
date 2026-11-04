#ifndef TMD_MIME3D_H
#define TMD_MIME3D_H

/*
   tmd_mime3d.h
   Small MIMe-style deformation helpers for PS1/HMD workflows.

   The Sony docs describe MIMe as a category-4 HMD primitive family for joint
   and vertex/normal interpolation. This module does not execute HMD primitive
   bytecode; it provides deterministic fixed-point building blocks that a HMD
   extractor can feed with decoded MIMe diffs.

   C89, no heap, caller-owned arrays, no float/double.
*/

#include "tmd_codec3d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TMD_MIME_MAX_DIFFS_PER_PRIMITIVE ((tmd_u32)32UL)

#define TMD_MIME_CODE0_JOINT             ((tmd_u8)0U)
#define TMD_MIME_CODE0_VERTEX_NORMAL     ((tmd_u8)1U)
#define TMD_MIME_CODE1_AXES_OR_VERTEX    ((tmd_u8)0U)
#define TMD_MIME_CODE1_RPY_OR_NORMAL     ((tmd_u8)1U)

typedef struct TMD_MIMe_VertexDiff TMD_MIMe_VertexDiff;
typedef struct TMD_MIMe_NormalDiff TMD_MIMe_NormalDiff;
typedef struct TMD_MIMe_PrimitiveInfo TMD_MIMe_PrimitiveInfo;

struct TMD_MIMe_VertexDiff {
    tmd_u16 vertex_index;
    tmd_i16 dx;
    tmd_i16 dy;
    tmd_i16 dz;
};

struct TMD_MIMe_NormalDiff {
    tmd_u16 normal_index;
    tmd_i16 dx_fx12;
    tmd_i16 dy_fx12;
    tmd_i16 dz_fx12;
};

struct TMD_MIMe_PrimitiveInfo {
    tmd_u8 is_reset;
    tmd_u8 code0;
    tmd_u8 code1;
};

int tmd_mime_decode_primitive_type(tmd_u16 primitive_code,
                                   TMD_MIMe_PrimitiveInfo *out_info);

int tmd_mime_apply_vertex_diffs(const TMD_Vertex *base_vertices,
                                tmd_u32 vertex_count,
                                const TMD_MIMe_VertexDiff *diffs,
                                tmd_u32 diff_count,
                                tmd_i32 weight_fx12,
                                TMD_Vertex *out_vertices,
                                tmd_u32 out_vertex_count);

int tmd_mime_apply_normal_diffs(const TMD_Normal *base_normals,
                                tmd_u32 normal_count,
                                const TMD_MIMe_NormalDiff *diffs,
                                tmd_u32 diff_count,
                                tmd_i32 weight_fx12,
                                TMD_Normal *out_normals,
                                tmd_u32 out_normal_count);

int tmd_mime_blend_vertices(const TMD_Vertex *a_vertices,
                            const TMD_Vertex *b_vertices,
                            tmd_u32 vertex_count,
                            tmd_i32 weight_fx12,
                            TMD_Vertex *out_vertices,
                            tmd_u32 out_vertex_count);

int tmd_mime_blend_normals(const TMD_Normal *a_normals,
                           const TMD_Normal *b_normals,
                           tmd_u32 normal_count,
                           tmd_i32 weight_fx12,
                           TMD_Normal *out_normals,
                           tmd_u32 out_normal_count);

#ifdef __cplusplus
}
#endif

#endif /* TMD_MIME3D_H */
