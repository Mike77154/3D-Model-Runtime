#ifndef GS_MDL7_RIG_H
#define GS_MDL7_RIG_H
#include <stdio.h>
#include "../mdl_core.h"
/* R7 streams the packed MD7_BONE and MD7_DEFORMER records.
 * No heap, no C99, no floating-point arithmetic. TSV values Q16.16.
 * Return 0 for structurally invalid and unsupported deformer variants.
 */
/* mode 0: no preview, 1: raw R10, 2: MDL7 3x4 pivot-space diagnostic, 3: hierarchical local-space diagnostic, 4: explicit inverse-bind. */
void gs7_set_skin_mode(int mode);
/* 0 disables; 1..65536 interpolates selected frame toward the next frame. */
void gs7_set_blend_next(long factor);
/* R14 opt-in shortest-arc quaternion NLERP (rigid 3x3 only). */
void gs7_set_blend_rot(int enabled);
int gs7_bones(FILE *f, unsigned long filesize, unsigned long bonecount,
              unsigned short stride, FILE *meta);
int gs7_deformers(FILE *f, unsigned long end, unsigned long group,
                  unsigned long n_bones, unsigned long n_verts,
                  unsigned char n_deformers, unsigned char max_weights,
                  long (*positions)[3], FILE *meta, GSStats *stats);
int gs7_frames(FILE *f,unsigned long end,unsigned long count,
               unsigned short hstride,unsigned short vstride,unsigned short mstride,
               unsigned long n_verts,unsigned long n_bones,
               unsigned long selected,long (*positions)[3],
               FILE *meta,GSStats *stats);
#endif
