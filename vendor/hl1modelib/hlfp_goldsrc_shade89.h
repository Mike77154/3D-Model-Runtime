#ifndef HLFP_GOLDSRC_SHADE89_H
#define HLFP_GOLDSRC_SHADE89_H
#include "hlfp.h"
#ifdef __cplusplus
extern "C" {
#endif
/* GoldSrc bone-origin chrome in world space: normals already transformed by
 * the associated normal bone. Corresponds to Valve inverse-rotate of axes;
 * same result for ideal orthonormal bone matrices. Approximated L2 unit vectors
 * (integer fixed-point) and direct bone origin; no caching or heap. */
void hlfp_gs89_chrome_bone_uv(const hlfp_vec3 *world_normal,
 const hlfp_mat3x4 *bone_matrix, const hlfp_vec3 *eye,
 const hlfp_vec3 *view_right, int width, int height, int *s, int *t);
/* StudioModel::Lighting-style hemispherical intensity. Input lightvec is
 * GoldSrc model-space g_lightvec transformed to world space, NOT surface-to-light.
 * lambert >= 1.0, 16.16 fixed. Normal is in world space. */
int hlfp_gs89_light_value(const hlfp_vec3 *world_normal,
 const hlfp_vec3 *world_lightvec, int ambient_8, int shade_8,
 hlfp_fixed lambert, int flags);
/* Interpolate 0..255 vertex light factors, barycentric screen-space. */
int hlfp_gs89_interpolate_light(int a,int b,int c,
 long w0,long w1,long w2,long area);
#ifdef __cplusplus
}
#endif
#endif
