#ifndef FRAG_GS_MDL_CORE_H
#define FRAG_GS_MDL_CORE_H
#include <stdio.h>
typedef struct GSStats {
 unsigned long version,skins,skin_vertices,vertices,triangles,frames;
 unsigned long bones,groups,group_vertices,group_triangles;
 unsigned long selected_frame,unsupported,deformers,influences,bone_matrices;
 char frame_name[17];
} GSStats;
/* Opt-in flat normals computed after MDL7 skinning, never original smooth normals. */
void gs_set_face_normals(int enabled);
void gs_set_smooth_normals(int enabled);
/* 1 successful mesh export, 0 malformed or unsupported variant. */
int gs_mdl_decode(FILE *in,FILE *obj,FILE *meta,GSStats *st,unsigned long frame);
#endif
