#ifndef FRAG_SKIN_FIXED_H
#define FRAG_SKIN_FIXED_H
/* Four-influence Q16.16 linear blend skinning building block.
 * Matrices supplied by caller (e.g. offset * global bone pose).
 * Weight[3] may be residual as in ARG, but normalization is caller's job. */
int skin_blend_point(const long base[3],const long matrices[][16],
                     unsigned long matrix_count,const unsigned char bone_ids[4],
                     const long weights[4],long out[3]);
#endif
