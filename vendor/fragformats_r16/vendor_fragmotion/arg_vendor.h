#ifndef FRAG_ARG_VENDOR_H
#define FRAG_ARG_VENDOR_H
#include <stdio.h>
#define ARG_MAX_BONE_DEPTH 64
#define ARG_MAX_ITEMS 1000000UL
typedef struct ARG_Stats {
 unsigned long version, animations,events,bones,attachments;
 unsigned long bone_animation_refs,rotate_keys,translate_keys,scale_keys;
 unsigned long vertices,triangles,textures,materials,smoothing_groups,groups;
 unsigned long skinned_vertices,weighted_influences,invalid_indices;
 unsigned long file_bytes,error_offset;
 char character[65];
} ARG_Stats;
/* Optional no-heap native visitation: per-corner UV and normals are Q16.16.
 * Rigid vertices are passed before face callbacks; animation/skeleton entries
 * are metadata only and are NOT authored poses reconstructed by this parser. */
typedef struct ARG_NativeSink {
 int (*vertex)(void *u,unsigned long index,const long position[3]);
 int (*face)(void *u,unsigned long face_index,const unsigned long indices[3],
             long normals[3][3],long uv[3][2],unsigned long material);
 int (*bone)(void *u,unsigned long id,int parent,const char *name);
 int (*animation)(void *u,unsigned long index,const char *name,unsigned long frames);
 int (*texture)(void *u,unsigned long index,const char *name);
 int (*material)(void *u,unsigned long index,const char *name);
} ARG_NativeSink;
int arg_decode_native(FILE *src, ARG_Stats *stats, const ARG_NativeSink *sink, void *user);
int arg_decode(FILE *src,FILE *obj,FILE *rig,ARG_Stats *stats,int verbose);
#endif
