#ifndef HLFP_SOFTMDL_PLUS_H
#define HLFP_SOFTMDL_PLUS_H

#include "hlfp_softmdl.h"
#include "hlfp_studiohalflife_plus.h"
#include "hlfp_transparency89.h"
#include "hlfp_perspective89.h"
#include "hlfp_frustum89.h"
#include "hlfp_subpixel89.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Affine textured raster + debug overlays, still tiny and heapless. */

typedef struct hlfp_soft2_options_s {
    hlfp_bool depth_test;
    hlfp_bool draw_masked;
    hlfp_bool wire_overlay;
    hlfp_bool skeleton_overlay;
    hlfp_bool shade_flat;
    hlfp_u8 fallback_rgba[4];
    hlfp_u8 wire_rgba[4];
    hlfp_u8 skeleton_rgba[4];
    hlfp_bool enable_chrome;
    hlfp_bool enable_additive;
    hlfp_bool enable_alpha;
    hlfp_u8 alpha_8; /* NF_ALPHA blend; entity renderamt if available */
    hlfp_u8 additive_strength_8; /* additive blend intensity, 255 by default */
    hlfp_bool chrome_view_dependent; /* opt in: explicit eye position */
    hlfp_vec3 chrome_eye;
    hlfp_bool enable_directional_light; /* opt in; overrides legacy shade_flat */
    hlfp_vec3 light_direction; /* unit surface-to-light in WORLD coords */
    hlfp_u8 ambient_8;
    hlfp_u8 diffuse_8;
    hlfp_bool chrome_per_bone; /* opt-in GoldSrc-style bone-centric axes */
    hlfp_vec3 chrome_view_right; /* camera right vector in model world-space */
    hlfp_bool smooth_vertex_light; /* opt-in GoldSrc normal lighting + Gouraud */
    hlfp_fixed light_lambert; /* 16.16, minimum 1.0 */
    const hlfp_mat3x4 *bone_matrices; /* optional on per-mesh calls */
    int bone_matrix_count;
    const hlfp_p89_camera *perspective; /* opt-in camera; NULL preserves v4 orthographic behavior */
    hlfp_bool full_frustum_clipping; /* v6 opt-in, all six planes */
    hlfp_fixed perspective_far_z; /* positive camera z, v6 only */
    hlfp_bool subpixel_top_left; /* v7 opt-in 28.4 pixel-center ownership */
    int body_value; /* GoldSrc bodygroup selector */
    int skin_family; /* GoldSrc skin family selector */
} hlfp_soft2_options;

void hlfp_soft2_options_init(hlfp_soft2_options *out);

int hlfp_soft2_draw_expanded_mesh_textured(const hlfp_mdl *mdl,
                                           const hlfp_mdl_mesh_expanded *mesh,
                                           const hlfp_soft_framebuffer *fb,
                                           const hlfp_soft_camera *cam,
                                           const hlfp_soft2_options *opt,
                                           hlfp_error *err);

int hlfp_soft2_draw_skeleton(const hlfp_mdl *mdl,
                             const hlfp_mat3x4 *world_mats,
                             int world_mat_count,
                             const hlfp_soft_framebuffer *fb,
                             const hlfp_soft_camera *cam,
                             const hlfp_soft2_options *opt,
                             hlfp_error *err);

int hlfp_soft2_render_mdl_sequence_frame_textured(const hlfp_mdl *mdl,
                                                  const hlfp_shl_extensions *ext,
                                                  int sequence_index,
                                                  int blend_index,
                                                  int frame,
                                                  const hlfp_shl_procedural_state *proc,
                                                  const hlfp_soft_framebuffer *fb,
                                                  const hlfp_soft_camera *cam,
                                                  const hlfp_soft2_options *opt,
                                                  hlfp_soft_scratch *scratch,
                                                  hlfp_error *err);

/* Fractional animation frame (16.16), uses quaternion interpolation.
 * Unlike the old integer-frame preview, renders every selected bodypart/mesh.
 */
int hlfp_soft2_render_mdl_sequence_subframe_textured(const hlfp_mdl *mdl,
                 const hlfp_shl_extensions *ext,
                 int sequence_index, int blend_index, hlfp_fixed frame_position,
                 const hlfp_shl_procedural_state *proc,
                 const hlfp_soft_framebuffer *fb, const hlfp_soft_camera *cam,
                 const hlfp_soft2_options *opt, hlfp_soft_scratch *scratch,
                 hlfp_error *err);

/* Opt-in globally sorted alpha triangles over every bodypart/mesh.
 * Caller must provide sufficient queue capacity. Queue overflow is an error,
 * not a silent fallback; the framebuffer may then contain only opaque passes.
 */
int hlfp_soft3_render_mdl_sequence_subframe_textured(const hlfp_mdl *mdl,
                 const hlfp_shl_extensions *ext, int sequence_index,
                 int blend_index, hlfp_fixed frame_position,
                 const hlfp_shl_procedural_state *proc,
                 const hlfp_soft_framebuffer *fb, const hlfp_soft_camera *cam,
                 const hlfp_soft2_options *opt, hlfp_soft_scratch *scratch,
                 hlfp_trans89_queue *queue, hlfp_error *err);
int hlfp_soft2_write_tga32(const hlfp_soft_framebuffer *fb,
                           hlfp_write_fn write,
                           void *user,
                           hlfp_error *err);

#ifdef __cplusplus
}
#endif

#endif
