#ifndef HLFP_STUDIOHALFLIFE_H
#define HLFP_STUDIOHALFLIFE_H

#include "hlfp_goldsrc.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * StudioHalfLife extensions for GoldSrc/Xash-like MDL v10 variants.
 * C89, no heap ownership, fixed-point runtime math.
 *
 * This layer does not mutate the base hlfp_mdl parser. It interprets optional
 * extension data such as studiohdr2, boneinfo, boneweights, pose parameters,
 * IK chains/locks, autolayers, and Dreamcast .dol companion headers.
 */

#define HLFP_SHL_MAX_POSEPARAMS 24
#define HLFP_SHL_MAX_IK_CHAINS 16
#define HLFP_SHL_MAX_IK_LINKS 64
#define HLFP_SHL_MAX_IK_LOCKS 64
#define HLFP_SHL_MAX_IK_RULES 128
#define HLFP_SHL_MAX_AUTOLAYERS 256
#define HLFP_SHL_MAX_KEYVALUES 4096
#define HLFP_SHL_MAX_WEIGHT_BONES 4

#define HLFP_SHL_STUDIO_HAS_BONEINFO    ((hlfp_u32)0x40000000U)
#define HLFP_SHL_STUDIO_HAS_BONEWEIGHTS ((hlfp_u32)0x80000000U)

#define HLFP_SHL_BONE_ALWAYS_PROCEDURAL      0x0001
#define HLFP_SHL_BONE_SCREEN_ALIGN_SPHERE    0x0002
#define HLFP_SHL_BONE_SCREEN_ALIGN_CYLINDER  0x0004
#define HLFP_SHL_BONE_JIGGLE_PROCEDURAL      0x0008
#define HLFP_SHL_BONE_FIXED_ALIGNMENT        0x0010

#define HLFP_SHL_STUDIO_AUTOPLAY       0x0008
#define HLFP_SHL_STUDIO_DELTA          0x0004
#define HLFP_SHL_STUDIO_BLENDPOSE      0x0040
#define HLFP_SHL_STUDIO_CYCLEPOSE      0x0080
#define HLFP_SHL_STUDIO_IKRULES        0x0800

#define HLFP_SHL_STUDIO_AL_POST        0x0001
#define HLFP_SHL_STUDIO_AL_SPLINE      0x0002
#define HLFP_SHL_STUDIO_AL_XFADE       0x0004
#define HLFP_SHL_STUDIO_AL_NOBLEND     0x0008
#define HLFP_SHL_STUDIO_AL_LOCAL       0x0010
#define HLFP_SHL_STUDIO_AL_POSE        0x0020

#define HLFP_SHL_PROC_AXISINTERP       1
#define HLFP_SHL_PROC_QUATINTERP       2
#define HLFP_SHL_PROC_AIMATBONE        3
#define HLFP_SHL_PROC_AIMATATTACH      4
#define HLFP_SHL_PROC_JIGGLE           5

#define HLFP_SHL_IK_SELF       1
#define HLFP_SHL_IK_WORLD      2
#define HLFP_SHL_IK_GROUND     3
#define HLFP_SHL_IK_RELEASE    4
#define HLFP_SHL_IK_ATTACHMENT 5
#define HLFP_SHL_IK_UNLATCH    6

enum {
    HLFP_SHL_COMPANION_UNKNOWN = 0,
    HLFP_SHL_COMPANION_MAIN_MDL = 1,
    HLFP_SHL_COMPANION_TEXTURE_MDL = 2,
    HLFP_SHL_COMPANION_SEQGROUP_MDL = 3,
    HLFP_SHL_COMPANION_MAIN_DOL = 4,
    HLFP_SHL_COMPANION_TEXTURE_DOL = 5,
    HLFP_SHL_COMPANION_SEQGROUP_DOL = 6
};

typedef struct hlfp_shl_studiohdr2_s {
    int numposeparameters;
    int poseparamindex;
    int numikautoplaylocks;
    int ikautoplaylockindex;
    int numikchains;
    int ikchainindex;
    int keyvalueindex;
    int keyvaluesize;
    int numhitboxsets;
    int hitboxsetindex;
} hlfp_shl_studiohdr2;

typedef struct hlfp_shl_poseparam_s {
    char name[32];
    int flags;
    hlfp_fixed start;
    hlfp_fixed end;
    hlfp_fixed loop;
} hlfp_shl_poseparam;

typedef struct hlfp_shl_boneinfo_s {
    int valid;
    hlfp_mat3x4 pose_to_bone;
    hlfp_quat q_alignment;
    int proctype;
    int procindex;
    hlfp_quat quat;
} hlfp_shl_boneinfo;

typedef struct hlfp_shl_boneweight_s {
    int count;
    int bone[HLFP_SHL_MAX_WEIGHT_BONES];
    hlfp_fixed weight[HLFP_SHL_MAX_WEIGHT_BONES];
} hlfp_shl_boneweight;

typedef struct hlfp_shl_model_boneweights_s {
    int has_vertex_weights;
    int has_normal_weights;
    int numverts;
    int numnorms;
    hlfp_shl_boneweight vert[HLFP_MDL_MAX_VERTICES];
    hlfp_shl_boneweight norm[HLFP_MDL_MAX_NORMALS];
} hlfp_shl_model_boneweights;

typedef struct hlfp_shl_iklink_s {
    int bone;
    hlfp_vec3 knee_dir;
} hlfp_shl_iklink;

typedef struct hlfp_shl_ikchain_s {
    char name[32];
    int linktype;
    int numlinks;
    int first_link;
} hlfp_shl_ikchain;

typedef struct hlfp_shl_iklock_s {
    int sequence;
    int chain;
    hlfp_fixed pos_weight;
    hlfp_fixed local_q_weight;
    int flags;
} hlfp_shl_iklock;

typedef struct hlfp_shl_ikrule_s {
    int index;
    int type;
    int chain;
    int bone;
    int attachment;
    int slot;
    hlfp_fixed height;
    hlfp_fixed radius;
    hlfp_fixed floor;
    hlfp_vec3 pos;
    hlfp_quat quat;
    int start_frame;
    hlfp_fixed start;
    hlfp_fixed peak;
    hlfp_fixed tail;
    hlfp_fixed end;
    hlfp_fixed contact;
    hlfp_fixed drop;
    hlfp_fixed top;
} hlfp_shl_ikrule;

typedef struct hlfp_shl_autolayer_s {
    int parent_sequence;
    int sequence;
    int pose;
    int flags;
    hlfp_fixed start;
    hlfp_fixed peak;
    hlfp_fixed tail;
    hlfp_fixed end;
} hlfp_shl_autolayer;

typedef struct hlfp_shl_extensions_s {
    int has_studiohdr2;
    int has_boneinfo;
    int has_boneweights;
    hlfp_shl_studiohdr2 hdr2;

    int num_poseparams;
    hlfp_shl_poseparam poseparams[HLFP_SHL_MAX_POSEPARAMS];

    int num_boneinfos;
    hlfp_shl_boneinfo boneinfos[HLFP_MDL_MAX_BONES];

    int num_ikchains;
    hlfp_shl_ikchain ikchains[HLFP_SHL_MAX_IK_CHAINS];
    int num_iklinks;
    hlfp_shl_iklink iklinks[HLFP_SHL_MAX_IK_LINKS];

    int num_iklocks;
    hlfp_shl_iklock iklocks[HLFP_SHL_MAX_IK_LOCKS];

    int num_ikrules;
    hlfp_shl_ikrule ikrules[HLFP_SHL_MAX_IK_RULES];

    int num_autolayers;
    hlfp_shl_autolayer autolayers[HLFP_SHL_MAX_AUTOLAYERS];

    int keyvalue_size;
    char keyvalues[HLFP_SHL_MAX_KEYVALUES];

    int warnings;
    int unsupported_proc_rules;
    int bad_ranges;
    int clipped_counts;
} hlfp_shl_extensions;

typedef struct hlfp_shl_procedural_state_s {
    int enable_pose_to_bone;
    int enable_billboard;
    int enable_jiggle_lite;
    hlfp_vec3 camera_right;
    hlfp_vec3 camera_up;
    hlfp_vec3 camera_forward;
    hlfp_fixed jiggle_phase;
    hlfp_fixed jiggle_amount;
} hlfp_shl_procedural_state;

typedef struct hlfp_shl_ik_target_s {
    int bone;
    hlfp_vec3 target;
    hlfp_fixed weight;
} hlfp_shl_ik_target;

typedef struct hlfp_shl_companion_info_s {
    int kind;
    int is_dol;
    int seqgroup_index;
    hlfp_u32 id;
    int version;
    char name[64];
    int length;
    int has_studiohdr2;
    int likely_texture_file;
    int likely_seqgroup_file;
} hlfp_shl_companion_info;

void hlfp_shl_extensions_init(hlfp_shl_extensions *out);
void hlfp_shl_procedural_state_init(hlfp_shl_procedural_state *out);

int hlfp_shl_parse_extensions(const hlfp_mdl *mdl,
                              hlfp_shl_extensions *out,
                              hlfp_error *err);

int hlfp_shl_parse_model_boneweights(const hlfp_mdl *mdl,
                                      int model_index,
                                      hlfp_shl_model_boneweights *out,
                                      hlfp_error *err);

int hlfp_shl_compute_boneweight_matrix(const hlfp_shl_boneweight *weights,
                                        const hlfp_mat3x4 *world_mats,
                                        int world_mat_count,
                                        hlfp_mat3x4 *out,
                                        hlfp_error *err);

int hlfp_shl_skin_vec3_weighted(const hlfp_vec3 *src,
                                const hlfp_shl_boneweight *weights,
                                const hlfp_mat3x4 *world_mats,
                                int world_mat_count,
                                hlfp_bool translate,
                                hlfp_vec3 *out,
                                hlfp_error *err);

int hlfp_shl_bake_mesh_weighted(const hlfp_mdl *mdl,
                                int model_index,
                                int mesh_index,
                                int skin_family,
                                const hlfp_mat3x4 *world_mats,
                                int world_mat_count,
                                hlfp_mdl_mesh_expanded *out,
                                hlfp_error *err);

int hlfp_shl_autolayer_weight(const hlfp_shl_autolayer *layer,
                              hlfp_fixed cycle,
                              hlfp_fixed pose_value,
                              hlfp_fixed *out_weight,
                              hlfp_error *err);

int hlfp_shl_blend_sequence_with_autolayers(const hlfp_mdl *mdl,
                                            const hlfp_shl_extensions *ext,
                                            int sequence_index,
                                            hlfp_fixed seconds,
                                            hlfp_fixed cycle,
                                            hlfp_mdl_pose_set *out,
                                            hlfp_error *err);

int hlfp_shl_apply_procedural_bones(const hlfp_mdl *mdl,
                                    const hlfp_shl_extensions *ext,
                                    const hlfp_shl_procedural_state *state,
                                    hlfp_mat3x4 *world_mats,
                                    int world_mat_count,
                                    hlfp_error *err);

int hlfp_shl_apply_ik_targets(const hlfp_mdl *mdl,
                              const hlfp_shl_ik_target *targets,
                              int target_count,
                              hlfp_mat3x4 *world_mats,
                              int world_mat_count,
                              hlfp_error *err);

int hlfp_shl_read_ikrule_bytes(const void *data,
                               size_t size,
                               hlfp_shl_ikrule *out,
                               hlfp_error *err);

int hlfp_shl_classify_companion_name(const char *filename,
                                     hlfp_shl_companion_info *out,
                                     hlfp_error *err);

int hlfp_shl_read_companion_header(const void *data,
                                   size_t size,
                                   const char *filename,
                                   hlfp_shl_companion_info *out,
                                   hlfp_error *err);

int hlfp_shl_write_report(const hlfp_mdl *mdl,
                          const hlfp_shl_extensions *ext,
                          hlfp_write_fn write,
                          void *user,
                          hlfp_error *err);

#ifdef __cplusplus
}
#endif

#endif
