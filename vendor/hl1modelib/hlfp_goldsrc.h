#ifndef HLFP_GOLDSRC_H
#define HLFP_GOLDSRC_H

#include "hlfp.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * GoldSrc compatibility helpers for hlfp.
 * C89, no heap ownership. All buffers are caller-owned.
 */

#define HLFP_GOLDSRC_RGBA_BYTES 4
#define HLFP_GOLDSRC_OBJ_COMMENT_LIMIT 96

typedef struct hlfp_goldsrc_diag_s {
    int warnings;
    int errors;
    int bad_bone_parents;
    int bad_bodypart_bases;
    int bad_mesh_parents;
    int bad_skinrefs;
    int missing_external_textures;
    int missing_seqgroups;
} hlfp_goldsrc_diag;


int hlfp_goldsrc_make_texture_filename(const char *base_without_ext,
                                       hlfp_bool dreamcast_dol,
                                       char *dst,
                                       int dst_cap,
                                       hlfp_error *err);

int hlfp_goldsrc_make_seqgroup_filename(const char *base_without_ext,
                                        int seqgroup_index,
                                        hlfp_bool dreamcast_dol,
                                        char *dst,
                                        int dst_cap,
                                        hlfp_error *err);

int hlfp_goldsrc_get_bodypart_value(const hlfp_mdl *mdl,
                                    int body_value,
                                    int bodypart_index,
                                    int *out_value,
                                    hlfp_error *err);

int hlfp_goldsrc_set_bodypart_value(const hlfp_mdl *mdl,
                                    int bodypart_index,
                                    int model_choice,
                                    int *inout_body_value,
                                    hlfp_error *err);

int hlfp_goldsrc_model_for_bodypart(const hlfp_mdl *mdl,
                                    int body_value,
                                    int bodypart_index,
                                    int *out_model_index,
                                    hlfp_error *err);

int hlfp_goldsrc_resolve_texture_for_skinref(const hlfp_mdl *mdl,
                                             int skin_family,
                                             int skinref,
                                             int *out_texture_index,
                                             hlfp_error *err);

int hlfp_goldsrc_resolve_texture_for_mesh(const hlfp_mdl *mdl,
                                          int mesh_index,
                                          int skin_family,
                                          int *out_texture_index,
                                          hlfp_error *err);

int hlfp_goldsrc_expand_mesh_skin(const hlfp_mdl *mdl,
                                  int model_index,
                                  int mesh_index,
                                  int skin_family,
                                  hlfp_mdl_mesh_expanded *out,
                                  hlfp_error *err);

int hlfp_goldsrc_texture_to_rgba32(const hlfp_mdl *mdl,
                                   int texture_index,
                                   hlfp_bool force_masked_alpha,
                                   hlfp_u8 *rgba,
                                   size_t rgba_capacity,
                                   size_t *out_size,
                                   int *out_width,
                                   int *out_height,
                                   hlfp_error *err);

int hlfp_goldsrc_make_bind_pose(const hlfp_mdl *mdl,
                                hlfp_mdl_pose_set *out,
                                hlfp_error *err);

int hlfp_goldsrc_bake_mesh_with_matrices(const hlfp_mdl *mdl,
                                         int model_index,
                                         int mesh_index,
                                         int skin_family,
                                         const hlfp_mat3x4 *world_mats,
                                         int world_mat_count,
                                         hlfp_mdl_mesh_expanded *out,
                                         hlfp_error *err);

int hlfp_goldsrc_export_obj_body(const hlfp_mdl *mdl,
                                 int body_value,
                                 int skin_family,
                                 const hlfp_mat3x4 *world_mats,
                                 int world_mat_count,
                                 hlfp_write_fn write,
                                 void *user,
                                 hlfp_error *err);

int hlfp_goldsrc_export_obj_sequence_frame(const hlfp_mdl *mdl,
                                           int body_value,
                                           int skin_family,
                                           int sequence_index,
                                           int blend_index,
                                           int frame,
                                           hlfp_write_fn write,
                                           void *user,
                                           hlfp_error *err);

int hlfp_goldsrc_write_qc(const hlfp_mdl *mdl,
                          const char *model_name,
                          hlfp_write_fn write,
                          void *user,
                          hlfp_error *err);

int hlfp_goldsrc_analyze(const hlfp_mdl *mdl,
                         hlfp_goldsrc_diag *out,
                         hlfp_error *err);

int hlfp_goldsrc_write_report(const hlfp_mdl *mdl,
                              hlfp_write_fn write,
                              void *user,
                              hlfp_error *err);

#ifdef __cplusplus
}
#endif

#endif
