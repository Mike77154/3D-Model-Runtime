#ifndef LWS3D_LWO_SUBD_H
#define LWS3D_LWO_SUBD_H

/*
   lws3d_lwo_subd - fixed point PTCH/SubPatch evaluator.
   C89, no malloc/free/realloc, no float/double.

   Note: LightWave's exact SubPatch evaluator is not publicly specified.
   This module generates a deterministic Catmull-Clark style render mesh
   from PTCH control cages, preserving the original control cage in lws3d_lwo.
*/

#include "lws3d_lwo.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LWS3D_LWO_SUBD_MAX_POINTS
#define LWS3D_LWO_SUBD_MAX_POINTS     32768
#endif
#ifndef LWS3D_LWO_SUBD_MAX_FACES
#define LWS3D_LWO_SUBD_MAX_FACES      32768
#endif
#ifndef LWS3D_LWO_SUBD_MAX_FACE_VERTS
#define LWS3D_LWO_SUBD_MAX_FACE_VERTS    8
#endif
#ifndef LWS3D_LWO_SUBD_MAX_EDGES
#define LWS3D_LWO_SUBD_MAX_EDGES      65536
#endif
#ifndef LWS3D_LWO_SUBD_MAX_TRIANGLES
#define LWS3D_LWO_SUBD_MAX_TRIANGLES  65536
#endif

#define LWS3D_LWO_SUBD_OK               0
#define LWS3D_LWO_SUBD_ERR_BAD_ARG     -1
#define LWS3D_LWO_SUBD_ERR_OVERFLOW    -2
#define LWS3D_LWO_SUBD_ERR_BAD_INDEX   -3

#define LWS3D_LWO_SUBD_MODE_CATMARK     1
#define LWS3D_LWO_SUBD_MODE_CAGE_FAN    2

typedef struct LWS3D_LWO_SubdPoint_tag {
    LWS3D_Fixed x;
    LWS3D_Fixed y;
    LWS3D_Fixed z;
    unsigned long source_point;
} LWS3D_LWO_SubdPoint;

typedef struct LWS3D_LWO_SubdFace_tag {
    int vertex_count;
    unsigned long vertices[LWS3D_LWO_SUBD_MAX_FACE_VERTS];
    int surface_index;
    unsigned long source_polygon;
} LWS3D_LWO_SubdFace;

typedef struct LWS3D_LWO_SubdTriangle_tag {
    unsigned long v0;
    unsigned long v1;
    unsigned long v2;
    int surface_index;
    unsigned long source_polygon;
} LWS3D_LWO_SubdTriangle;

typedef struct LWS3D_LWO_SubdStats_tag {
    long cage_points;
    long cage_faces;
    long edges_seen;
    long boundary_edges;
    long nonmanifold_edges;
    long subdivision_steps;
    long overflow_events;
    long bad_indices;
} LWS3D_LWO_SubdStats;

typedef struct LWS3D_LWO_SubdMesh_tag {
    long point_count;
    long face_count;
    long triangle_count;
    int mode;
    int overflowed;
    int bad_indices;
    LWS3D_LWO_SubdPoint points[LWS3D_LWO_SUBD_MAX_POINTS];
    LWS3D_LWO_SubdFace faces[LWS3D_LWO_SUBD_MAX_FACES];
    LWS3D_LWO_SubdTriangle triangles[LWS3D_LWO_SUBD_MAX_TRIANGLES];
} LWS3D_LWO_SubdMesh;

void lws3d_lwo_subd_mesh_init(LWS3D_LWO_SubdMesh *mesh);
int lws3d_lwo_subd_build_cage_from_ptch(const LWS3D_LWO_Object *obj, LWS3D_LWO_SubdMesh *mesh, LWS3D_LWO_SubdStats *stats);
int lws3d_lwo_subd_catmull_clark_step(const LWS3D_LWO_SubdMesh *src, LWS3D_LWO_SubdMesh *dst, LWS3D_LWO_SubdStats *stats);
int lws3d_lwo_subd_eval_ptch(const LWS3D_LWO_Object *obj, int levels, LWS3D_LWO_SubdMesh *work_a, LWS3D_LWO_SubdMesh *work_b, LWS3D_LWO_SubdMesh **out_mesh, LWS3D_LWO_SubdStats *stats);
int lws3d_lwo_subd_build_triangles(LWS3D_LWO_SubdMesh *mesh, LWS3D_LWO_SubdStats *stats);
const char *lws3d_lwo_subd_error_name(int err);

#ifdef __cplusplus
}
#endif

#endif
