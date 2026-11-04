#ifndef SS89RESOURCES_H
#define SS89RESOURCES_H
#include "ss89.h"
#ifndef SS89_SKA_VERTICES
#define SS89_SKA_VERTICES 65536U
#endif
#ifndef SS89_SKA_TRIANGLES
#define SS89_SKA_TRIANGLES 131072U
#endif
/* Inspect and optionally export original SE1 resource formats.
   Formats: binary MESH (.bm), SKEL, ANIM (skeletal), ADAT (.ani),
   texture TVER/TDAT/FRMS (.tex); textual .smc/.amc/.sha metadata.
   filename is independent file (extract from .gro first). */
int ss89_resource(const char *filename, const char *output, unsigned lod, unsigned frame);
/* Mesh buffers, available after a successful MESH read. Q16.16. */
unsigned ss89_ska_vertex_count(void);
unsigned ss89_ska_triangle_count(void);
unsigned ss89_ska_uv_count(void);
int ss89_ska_get_vertex(unsigned i, SS89_Vertex *out);
int ss89_ska_get_triangle(unsigned i, SS89_Triangle *out);
int ss89_ska_get_uv(unsigned i, SS89_i32 *u, SS89_i32 *v);
#endif
