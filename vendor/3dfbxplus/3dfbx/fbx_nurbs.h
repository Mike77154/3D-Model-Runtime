#ifndef FBX_NURBS_H
#define FBX_NURBS_H

#include "fbx_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Lightweight fixed-point NURBS/polyline tessellation helpers.
   Output buffers are allocated with fbx_alloc()/fbx_resize() and must be
   released with fbx_release(). Positions are xyz triples in fbx_real. */
int fbx_nurbs_curve_tessellate(const FbxCurveGeometry *curve,
                               int segments,
                               fbx_real **out_positions,
                               fbx_u32 *out_vertex_count);

int fbx_nurbs_surface_tessellate(const FbxSurfaceGeometry *surface,
                                 int segments_u,
                                 int segments_v,
                                 fbx_real **out_positions,
                                 fbx_u32 **out_indices,
                                 fbx_u32 *out_vertex_count,
                                 fbx_u32 *out_index_count);

#ifdef __cplusplus
}
#endif

#endif /* FBX_NURBS_H */
