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
/* Optional sampled animation curves. Uses the clip ID as stored in CAnimRef.
   Emits POSE_* Q16.16 TSV rows; does not assume Euler axis order or skin vertices. */
typedef struct ARG_PoseSelection {
 unsigned long clip,frame,clips_found,bones_seen,channels_sampled;
 int enabled;
} ARG_PoseSelection;
int arg_decode_pose_ex(FILE *src,FILE *obj,FILE *rig,FILE *mtl,const char *mtl_name,ARG_Stats *stats,int verbose,ARG_PoseSelection *pose);
int arg_decode_ex(FILE *src,FILE *obj,FILE *rig,FILE *mtl,const char *mtl_name,ARG_Stats *stats,int verbose);
int arg_decode(FILE *src,FILE *obj,FILE *rig,ARG_Stats *stats,int verbose);
#endif
