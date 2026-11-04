#ifndef RE4_CHARACTER_H
#define RE4_CHARACTER_H
#include "re4_skeletal.h"
#ifdef __cplusplus
extern "C" {
#endif
/* A part's inverse-bind matrices remain owned by the part PMD. The
   animation comes from the master skeleton, by stable bone identity.
   All buffers are caller-owned. No heap, float, file IO or hidden assets. */
typedef struct re4_character_links_s {
    re4_i32 master_index[RE4_SKEL_MAX_BONES];
    re4_u8 link_kind[RE4_SKEL_MAX_BONES]; /* 0=unmapped,1=direct,2=inherited */
    re4_u32 direct_links;
    re4_u32 inherited_links;
    re4_u32 stationary_links;
} re4_character_links;
/* Connect named born__NN and audited hand aliases (including pl04 helper joints).
   Unnamed descendants inherit the delta of their closest mapped ancestor.
   If no such ancestor exists, they keep their bind transformation. */
re4_err re4_character_link(const re4_pmd_model *master,
                            const re4_pmd_model *part,
                            re4_character_links *links);
/* Explicit rigid-parent fallback for independent accessory rigs.
   All part joints inherit the same master joint WORLD-SPACE motion delta.
   This preserves the part's rest pose and does not infer native IK or a
   canonical character assembly. Opt-in only. */
re4_err re4_character_rigid_anchor(const re4_pmd_model *master,
    const re4_pmd_model *part,re4_u32 master_joint,
    re4_character_links *links);
/* Diagnostic-only risk report. This is NOT proof that the candidate is a
   canonical character part. Parent hierarchy violations and duplicate direct
   targets can cause visually invalid attachments despite matching bone names. */
typedef struct re4_character_link_audit_s {
    re4_u32 direct;
    re4_u32 inherited;
    re4_u32 stationary;
    re4_u32 unique_direct_targets;
    re4_u32 repeated_direct_targets;
    re4_u32 recognized_hand_alias_pairs; /* pl04 hand helper + born__ same target */
    re4_u32 ancestry_conflicts;
    re4_u32 out_of_range_targets;
} re4_character_link_audit;
re4_err re4_character_audit(const re4_pmd_model *master,
                           const re4_pmd_model *part,
                           const re4_character_links *links,
                           re4_character_link_audit *out);
/* Structural link policy for a declared part (not a canonical-identity proof).
   A reviewed topology may differ from the master's ancestry, but must never
   contain repeated direct targets or invalid links. The caller must opt in.
   The retargeter operates in world-space: animated_master * inverse(bind_master)
   * bind_part. Its bind/rest result is unchanged under this policy. */
#define RE4_PART_POLICY_STRICT 0U
#define RE4_PART_POLICY_REVIEW_ANCESTRY 1U
re4_err re4_character_check_policy(const re4_character_link_audit *audit,
                                   re4_u32 policy);
/* MASTER animated/rest poses and PART bind/rest pose must be valid.
   out->world contains the part pose, compatible with re4_skel_skin_vertex.
   Recompute every frame after re4_skel_evaluate(master,...). */
re4_err re4_character_retarget(const re4_pmd_model *master,
                               const re4_skel_pose *master_rest,
                               const re4_skel_pose *master_animated,
                               const re4_pmd_model *part,
                               const re4_skel_pose *part_rest,
                               const re4_character_links *links,
                               re4_skel_pose *out, re4_u32 capacity);
#ifdef __cplusplus
}
#endif
#endif
