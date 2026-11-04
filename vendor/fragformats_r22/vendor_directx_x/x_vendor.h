#ifndef FRAG_DIRECTX_X_VENDOR_H
#define FRAG_DIRECTX_X_VENDOR_H
#include <stdio.h>
typedef struct X_Stats {
 unsigned long meshes, vertices, faces, triangles, normals, texcoords;
 unsigned long frames, transforms, materials, textures, skin_clusters;
 unsigned long animation_sets, animation_keys, keyframes;
 unsigned long unsupported_objects, error_offset, baked_vertices;
 int binary, float_width, compressed;
} X_Stats;
/* .x text/bin 0302/0303: geometry and metadata, not animated playback.
   No heap, no float/double, no third-party libraries. */
int x_decode(FILE *src, FILE *obj, FILE *metadata, X_Stats *stats, int verbose);
/* bake=1 applies static FrameTransformMatrix hierarchy to OBJ positions.
 * Skinning remains unimplemented, normals are suppressed for recomputation.
 * Matrix must precede Mesh in each Frame; otherwise rejects input. */
int x_decode_ex(FILE *src, FILE *obj, FILE *metadata, X_Stats *stats, int verbose, int bake);
#endif
