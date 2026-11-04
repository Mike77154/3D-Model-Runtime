#ifndef BC_RUNTIME_STREAM_H
#define BC_RUNTIME_STREAM_H
#include "bc.h"
/* Native in-process streaming adapter. No OBJ temporary or intermediate file.
 * Serialized floats are converted to Q16.16 by the BLEND vendor.
 * Polygon fans are a preview fallback; non-convex n-gons are not exact.
 */
typedef struct BC_RT_Mesh {
 unsigned int index, vertices, triangles, has_uv;
} BC_RT_Mesh;
typedef struct BC_RT_Vertex {
 int pos[3], uv[2];
 unsigned int has_uv;
} BC_RT_Vertex;
typedef struct BC_RT_Tri {
 unsigned int mesh_index;
 BC_RT_Vertex v[3];
} BC_RT_Tri;
typedef struct BC_RT_Sink {
 int (*begin)(void *, const BC_RT_Mesh *);
 int (*triangle)(void *, const BC_RT_Tri *);
 int (*end)(void *, const BC_RT_Mesh *);
} BC_RT_Sink;
int bc_rt_stream(BC_File *, const BC_RT_Sink *, void *,
                 unsigned int *mesh_count,unsigned int *triangle_count);
#endif
