#ifndef HLFP_STUDIOHALFLIFE_ULTRA_H
#define HLFP_STUDIOHALFLIFE_ULTRA_H

#include "hlfp_studiohalflife_plus.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * StudioHalfLife Ultra: runtime/decompiler/audit helpers over Plus.
 * C89, no heap ownership, caller buffers, fixed-point 16.16 only.
 */

#define HLFP_SHLU_MAX_BODY_CHOICES HLFP_MDL_MAX_BODYPARTS
#define HLFP_SHLU_MAX_BLEND_INPUTS HLFP_MDL_MAX_SEQUENCE_BLEND_INPUTS
#define HLFP_SHLU_MAX_IK_TARGETS 16
#define HLFP_SHLU_MAX_DEBUG_LINES 512
#define HLFP_SHLU_MAX_MANIFEST_CHARS 8192

#define HLFP_SHLU_AUDIT_BAD_BODY        0x0001
#define HLFP_SHLU_AUDIT_BAD_SKIN        0x0002
#define HLFP_SHLU_AUDIT_BAD_ATTACHMENT  0x0004
#define HLFP_SHLU_AUDIT_BAD_EVENT       0x0008
#define HLFP_SHLU_AUDIT_BAD_SEQGROUP    0x0010
#define HLFP_SHLU_AUDIT_BAD_TRANSITION  0x0020
#define HLFP_SHLU_AUDIT_BAD_WEIGHT      0x0040
#define HLFP_SHLU_AUDIT_WARN_EXT        0x0080

typedef struct hlfp_shlu_body_choice_s {
    int bodypart;
    int local_model;
    int model_index;
    int base;
    char bodypart_name[64];
    char model_name[64];
} hlfp_shlu_body_choice;

typedef struct hlfp_shlu_body_map_s {
    int body_value;
    int count;
    hlfp_shlu_body_choice choices[HLFP_SHLU_MAX_BODY_CHOICES];
} hlfp_shlu_body_map;

typedef struct hlfp_shlu_skin_map_s {
    int skin_family;
    int skinref;
    int texture_index;
    char texture_name[64];
} hlfp_shlu_skin_map;

typedef struct hlfp_shlu_attachment_world_s {
    int attachment_index;
    int bone;
    char name[32];
    hlfp_vec3 origin;
    hlfp_vec3 axis[3];
    hlfp_mat3x4 matrix;
} hlfp_shlu_attachment_world;

typedef struct hlfp_shlu_anim_driver_s {
    int sequence_index;
    int blend_index;
    hlfp_fixed frame_position;
    hlfp_fixed seconds;
    hlfp_fixed cycle;
    hlfp_bool use_seconds;
    hlfp_bool apply_autolayers;
    hlfp_bool apply_procedural;
    hlfp_bool apply_ikrules;
    hlfp_bool apply_root_motion;
    const hlfp_shl_procedural_state *procedural_state;
} hlfp_shlu_anim_driver;

typedef struct hlfp_shlu_layer_stack_s {
    int count;
    int sequence_indices[HLFP_SHLU_MAX_BLEND_INPUTS];
    int blend_indices[HLFP_SHLU_MAX_BLEND_INPUTS];
    hlfp_fixed frame_positions[HLFP_SHLU_MAX_BLEND_INPUTS];
    hlfp_fixed seconds[HLFP_SHLU_MAX_BLEND_INPUTS];
    hlfp_fixed weights[HLFP_SHLU_MAX_BLEND_INPUTS];
    int layer_source[HLFP_SHLU_MAX_BLEND_INPUTS];
} hlfp_shlu_layer_stack;

typedef struct hlfp_shlu_ik_eval_s {
    int considered_rules;
    int active_rules;
    int applied_targets;
    int applied_chains;
    int skipped_rules;
} hlfp_shlu_ik_eval;

typedef struct hlfp_shlu_deep_audit_s {
    int warnings;
    int errors;
    int flags;
    int bodyparts_checked;
    int models_checked;
    int meshes_checked;
    int skinrefs_checked;
    int bad_skinrefs;
    int textures_checked;
    int bad_texture_ranges;
    int attachments_checked;
    int bad_attachment_bones;
    int events_checked;
    int bad_event_frames;
    int seqgroups_checked;
    int external_seqgroups;
    int missing_seqgroup_buffers;
    int transitions_checked;
    int bad_transitions;
    int xash_weight_tables_checked;
    int bad_xash_weight_tables;
} hlfp_shlu_deep_audit;

void hlfp_shlu_anim_driver_init(hlfp_shlu_anim_driver *out);
void hlfp_shlu_deep_audit_init(hlfp_shlu_deep_audit *out);

int hlfp_shlu_resolve_body_map(const hlfp_mdl *mdl,
                               int body_value,
                               hlfp_shlu_body_map *out,
                               hlfp_error *err);

int hlfp_shlu_resolve_skin_map(const hlfp_mdl *mdl,
                               int skin_family,
                               int skinref,
                               hlfp_shlu_skin_map *out,
                               hlfp_error *err);

int hlfp_shlu_build_attachment_worlds(const hlfp_mdl *mdl,
                                      const hlfp_mat3x4 *world_mats,
                                      int world_mat_count,
                                      hlfp_shlu_attachment_world *out,
                                      int out_cap,
                                      int *out_count,
                                      hlfp_error *err);

int hlfp_shlu_build_layer_stack(const hlfp_mdl *mdl,
                                const hlfp_shl_extensions *ext,
                                const hlfp_shlu_anim_driver *driver,
                                hlfp_shlu_layer_stack *out,
                                hlfp_error *err);

int hlfp_shlu_decode_pose_driver(const hlfp_mdl *mdl,
                                 const hlfp_shl_extensions *ext,
                                 const hlfp_shlu_anim_driver *driver,
                                 hlfp_mdl_pose_set *out,
                                 hlfp_error *err);

int hlfp_shlu_decode_world_driver(const hlfp_mdl *mdl,
                                  const hlfp_shl_extensions *ext,
                                  const hlfp_shlu_anim_driver *driver,
                                  hlfp_mat3x4 *world_mats,
                                  int world_mat_count,
                                  hlfp_shlu_ik_eval *ik_eval,
                                  hlfp_error *err);

int hlfp_shlu_apply_ikrules(const hlfp_mdl *mdl,
                            const hlfp_shl_extensions *ext,
                            hlfp_fixed cycle,
                            hlfp_mat3x4 *world_mats,
                            int world_mat_count,
                            hlfp_shlu_ik_eval *out,
                            hlfp_error *err);

int hlfp_shlu_deep_audit_model(const hlfp_mdl *mdl,
                               const hlfp_shl_extensions *ext,
                               hlfp_shlu_deep_audit *out,
                               hlfp_error *err);

int hlfp_shlu_write_qc_decompile_ultra(const hlfp_mdl *mdl,
                                       const hlfp_shl_extensions *ext,
                                       const char *model_name,
                                       hlfp_write_fn write,
                                       void *user,
                                       hlfp_error *err);

int hlfp_shlu_write_companion_manifest(const hlfp_mdl *mdl,
                                       const hlfp_shl_extensions *ext,
                                       const char *base_without_ext,
                                       hlfp_bool dreamcast_dol,
                                       hlfp_write_fn write,
                                       void *user,
                                       hlfp_error *err);

int hlfp_shlu_write_deep_audit_report(const hlfp_mdl *mdl,
                                      const hlfp_shl_extensions *ext,
                                      const hlfp_shlu_deep_audit *audit,
                                      hlfp_write_fn write,
                                      void *user,
                                      hlfp_error *err);

#ifdef __cplusplus
}
#endif

#endif
