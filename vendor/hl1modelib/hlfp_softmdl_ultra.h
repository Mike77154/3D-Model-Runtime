#ifndef HLFP_SOFTMDL_ULTRA_H
#define HLFP_SOFTMDL_ULTRA_H

#include "hlfp_softmdl_plus.h"
#include "hlfp_studiohalflife_ultra.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Debug-sheet renderer: model + skeleton + hitboxes + attachments. */

typedef struct hlfp_softu_options_s {
    hlfp_soft2_options model;
    int body_value;
    int skin_family;
    hlfp_bool use_weighted_bones;
    hlfp_bool draw_hitboxes;
    hlfp_bool draw_attachments;
    hlfp_bool checker_background;
    hlfp_u8 hitbox_rgba[4];
    hlfp_u8 attachment_x_rgba[4];
    hlfp_u8 attachment_y_rgba[4];
    hlfp_u8 attachment_z_rgba[4];
    hlfp_u8 checker_a_rgba[4];
    hlfp_u8 checker_b_rgba[4];
} hlfp_softu_options;

void hlfp_softu_options_init(hlfp_softu_options *out);

int hlfp_softu_clear_checker(const hlfp_soft_framebuffer *fb,
                             const hlfp_softu_options *opt,
                             hlfp_error *err);

int hlfp_softu_draw_hitboxes(const hlfp_mdl *mdl,
                             const hlfp_mat3x4 *world_mats,
                             int world_mat_count,
                             const hlfp_soft_framebuffer *fb,
                             const hlfp_soft_camera *cam,
                             const hlfp_softu_options *opt,
                             hlfp_error *err);

int hlfp_softu_draw_attachments(const hlfp_mdl *mdl,
                                const hlfp_mat3x4 *world_mats,
                                int world_mat_count,
                                const hlfp_soft_framebuffer *fb,
                                const hlfp_soft_camera *cam,
                                const hlfp_softu_options *opt,
                                hlfp_error *err);

int hlfp_softu_render_debug_sheet(const hlfp_mdl *mdl,
                                  const hlfp_shl_extensions *ext,
                                  const hlfp_shlu_anim_driver *driver,
                                  const hlfp_soft_framebuffer *fb,
                                  const hlfp_soft_camera *cam,
                                  const hlfp_softu_options *opt,
                                  hlfp_soft_scratch *scratch,
                                  hlfp_shlu_ik_eval *ik_eval,
                                  hlfp_error *err);

#ifdef __cplusplus
}
#endif

#endif
