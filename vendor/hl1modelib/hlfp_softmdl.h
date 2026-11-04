#ifndef HLFP_SOFTMDL_H
#define HLFP_SOFTMDL_H

#include "hlfp_studiohalflife.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tiny fixed-point software renderer for baked GoldSrc meshes. */

typedef struct hlfp_soft_framebuffer_s {
    int width;
    int height;
    int stride_bytes;
    hlfp_u8 *rgba;
    hlfp_i32 *zbuffer;
} hlfp_soft_framebuffer;

typedef struct hlfp_soft_camera_s {
    hlfp_vec3 origin;
    hlfp_fixed scale;
    int center_x;
    int center_y;
    hlfp_bool flip_y;
} hlfp_soft_camera;

typedef struct hlfp_soft_options_s {
    int body_value;
    int skin_family;
    hlfp_bool use_weighted_bones;
    hlfp_bool wireframe;
    hlfp_bool depth_test;
    hlfp_bool draw_masked;
    hlfp_u8 clear_rgba[4];
} hlfp_soft_options;

typedef struct hlfp_soft_scratch_s {
    hlfp_mdl_mesh_expanded mesh;
    hlfp_mdl_pose_set pose;
    hlfp_mat3x4 mats[HLFP_MDL_MAX_BONES];
} hlfp_soft_scratch;

void hlfp_soft_camera_init(hlfp_soft_camera *out, int width, int height);
void hlfp_soft_options_init(hlfp_soft_options *out);

int hlfp_soft_clear(const hlfp_soft_framebuffer *fb,
                    const hlfp_soft_options *opt,
                    hlfp_error *err);

int hlfp_soft_draw_expanded_mesh(const hlfp_mdl *mdl,
                                 const hlfp_mdl_mesh_expanded *mesh,
                                 const hlfp_soft_framebuffer *fb,
                                 const hlfp_soft_camera *cam,
                                 const hlfp_soft_options *opt,
                                 hlfp_error *err);

int hlfp_soft_render_mdl_body(const hlfp_mdl *mdl,
                              const hlfp_mat3x4 *world_mats,
                              int world_mat_count,
                              const hlfp_soft_framebuffer *fb,
                              const hlfp_soft_camera *cam,
                              const hlfp_soft_options *opt,
                              hlfp_soft_scratch *scratch,
                              hlfp_error *err);

int hlfp_soft_render_mdl_sequence_frame(const hlfp_mdl *mdl,
                                        const hlfp_shl_extensions *ext,
                                        int sequence_index,
                                        int blend_index,
                                        int frame,
                                        const hlfp_shl_procedural_state *proc,
                                        const hlfp_soft_framebuffer *fb,
                                        const hlfp_soft_camera *cam,
                                        const hlfp_soft_options *opt,
                                        hlfp_soft_scratch *scratch,
                                        hlfp_error *err);

int hlfp_soft_write_ppm(const hlfp_soft_framebuffer *fb,
                        hlfp_write_fn write,
                        void *user,
                        hlfp_error *err);

#ifdef __cplusplus
}
#endif

#endif
