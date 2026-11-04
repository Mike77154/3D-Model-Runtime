#ifndef HLFP_STUDIOHALFLIFE_PLUS_H
#define HLFP_STUDIOHALFLIFE_PLUS_H

#include "hlfp_studiohalflife.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * StudioHalfLife Plus: extra decompiler/runtime helpers layered over
 * hlfp_studiohalflife. C89, caller-owned buffers, fixed-point math.
 */

#define HLFP_SHLP_MAX_AUDIT_LINES 64
#define HLFP_SHLP_MAX_LOD_LEVELS 8
#define HLFP_SHLP_MAX_LAYER_WEIGHTS 256

typedef struct hlfp_shlp_model_extra_s {
    int model_index;
    int has_xash_boneweights;
    int blendvertinfoindex;
    int blendnorminfoindex;
    int vertex_weight_bytes;
    int normal_weight_bytes;
    int vertex_weight_range_ok;
    int normal_weight_range_ok;
} hlfp_shlp_model_extra;

typedef struct hlfp_shlp_aabb_s {
    hlfp_vec3 min;
    hlfp_vec3 max;
} hlfp_shlp_aabb;

typedef struct hlfp_shlp_hitbox_world_s {
    int hitbox_index;
    int bone;
    int group;
    hlfp_shlp_aabb aabb;
} hlfp_shlp_hitbox_world;

typedef struct hlfp_shlp_root_motion_s {
    int sequence_index;
    int motionbone;
    int frames;
    hlfp_vec3 linearmovement;
    hlfp_vec3 per_frame;
} hlfp_shlp_root_motion;

typedef struct hlfp_shlp_lod_rule_s {
    hlfp_fixed max_distance;
    int body_value;
    int skin_family;
} hlfp_shlp_lod_rule;

typedef struct hlfp_shlp_audit_s {
    int warnings;
    int errors;
    int xash_model_extras;
    int checked_weight_ranges;
    int bad_weight_ranges;
    int normalized_weights;
    int hitboxes_checked;
    int bad_hitbox_bones;
    int sequences_checked;
    int seqgroups_external;
    int autolayers_checked;
    int ikrules_checked;
} hlfp_shlp_audit;

void hlfp_shlp_audit_init(hlfp_shlp_audit *out);

int hlfp_shlp_analyze_model_extra(const hlfp_mdl *mdl,
                                  int model_index,
                                  hlfp_shlp_model_extra *out,
                                  hlfp_error *err);

int hlfp_shlp_normalize_boneweight(hlfp_shl_boneweight *io,
                                   hlfp_error *err);

int hlfp_shlp_read_xash_weight_table(const void *data,
                                     size_t size,
                                     int count,
                                     hlfp_shl_boneweight *out,
                                     int out_cap,
                                     hlfp_error *err);

int hlfp_shlp_eval_autolayer_weights(const hlfp_shl_extensions *ext,
                                     int parent_sequence,
                                     hlfp_fixed cycle,
                                     hlfp_fixed pose_value,
                                     hlfp_fixed *out_weights,
                                     int out_cap,
                                     int *out_count,
                                     hlfp_error *err);

int hlfp_shlp_extract_root_motion(const hlfp_mdl *mdl,
                                  int sequence_index,
                                  hlfp_shlp_root_motion *out,
                                  hlfp_error *err);

int hlfp_shlp_build_hitbox_world_aabbs(const hlfp_mdl *mdl,
                                       const hlfp_mat3x4 *world_mats,
                                       int world_mat_count,
                                       hlfp_shlp_hitbox_world *out,
                                       int out_cap,
                                       int *out_count,
                                       hlfp_error *err);

int hlfp_shlp_apply_two_bone_ik_lite(int root_bone,
                                     int mid_bone,
                                     int tip_bone,
                                     const hlfp_vec3 *target,
                                     hlfp_fixed weight,
                                     hlfp_mat3x4 *world_mats,
                                     int world_mat_count,
                                     hlfp_error *err);

int hlfp_shlp_select_lod(const hlfp_shlp_lod_rule *rules,
                         int rule_count,
                         hlfp_fixed distance,
                         int *out_body_value,
                         int *out_skin_family,
                         hlfp_error *err);

int hlfp_shlp_audit_model(const hlfp_mdl *mdl,
                          const hlfp_shl_extensions *ext,
                          hlfp_shlp_audit *out,
                          hlfp_error *err);

int hlfp_shlp_write_qc_decompile(const hlfp_mdl *mdl,
                                 const hlfp_shl_extensions *ext,
                                 const char *model_name,
                                 hlfp_write_fn write,
                                 void *user,
                                 hlfp_error *err);

int hlfp_shlp_write_audit_report(const hlfp_mdl *mdl,
                                 const hlfp_shl_extensions *ext,
                                 const hlfp_shlp_audit *audit,
                                 hlfp_write_fn write,
                                 void *user,
                                 hlfp_error *err);

#ifdef __cplusplus
}
#endif

#endif
