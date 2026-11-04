#ifndef HLFP_MATERIAL89_H
#define HLFP_MATERIAL89_H
#include "hlfp.h"
#ifdef __cplusplus
extern "C" {
#endif
/* GoldSrc indexed material interpretation for callers with their own renderer.
 * All input pixels and output colors are caller-owned; no heap, no floats.
 * Alpha flag uses caller-selected constant opacity; GoldSrc entity renderamt
 * is not part of the MDL texture metadata. Chrome UV is an orthographic
 * normal-space approximation: it is NOT an exact GoldSrc view-dependent map.
 */
int hlfp_mat89_is_translucent(int flags);
int hlfp_mat89_sample(const hlfp_u8 *texels, const hlfp_u8 *palette,
                      int width, int height, int s, int t, int flags,
                      hlfp_u8 out_rgba[4]);
void hlfp_mat89_chrome_uv(const hlfp_vec3 *normal, int width, int height,
                          int *s, int *t);
/* Camera-dependent GoldSrc-inspired chrome, world-space normals/positions.
 * Explicit eye position because soft2 camera only specifies an orthographic
 * pan/zoom origin, not a real observer position. */
void hlfp_mat89_chrome_uv_view(const hlfp_vec3 *normal,
                  const hlfp_vec3 *position, const hlfp_vec3 *eye,
                  int width, int height, int *s, int *t);
/* Approximate ambient + directional light. L is surface-to-light unit vector.
 * FULLBRIGHT bypasses the operation; FLATSHADE uses ambient+80% direct. */
void hlfp_mat89_directional(hlfp_u8 rgba[4], const hlfp_vec3 *normal,
            const hlfp_vec3 *light_direction, int ambient_8,
            int diffuse_8, int flags);
void hlfp_mat89_composite(hlfp_u8 dst_rgba[4], const hlfp_u8 src_rgba[4],
                          int flags, int alpha_8);
#ifdef __cplusplus
}
#endif
#endif
