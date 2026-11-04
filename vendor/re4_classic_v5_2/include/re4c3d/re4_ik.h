#ifndef RE4_IK_H
#define RE4_IK_H
#include "re4_skeletal.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Candidate IK chains from FCV flags and bone name IDs. The root->child->tip
   topology is a *candidate*, not proof of the game's exact active IK chain.
   All results are caller-owned; no heap. */
typedef struct re4_ik_chain_s {
    re4_u32 channel_index;
    re4_u8 root_parts_id;
    re4_u8 kind_flags;
    re4_u8 bend_axis_code;
    re4_u8 toe_flag;
    re4_u8 four_joint_flag;
    re4_i32 root;
    re4_i32 joint;
    re4_i32 effector;
    re4_i32 toe;
    re4_bool topology_ok;
} re4_ik_chain;
re4_err re4_ik_discover(const re4_pmd_model *model, const re4_fcv *fcv,
    re4_ik_chain *out, re4_u32 capacity, re4_u32 *out_count,
    re4_u32 *out_topology_ok);
/* Optional contact constraint: lock each candidate effector to its position in
   reference pose. Not native RE4 IK; only explicitly activated by caller.
   axis comes from the source bend-axis code, mapped XYZ+sign. */
re4_err re4_ik_lock_contacts(const re4_pmd_model *model,
    const re4_ik_chain *chains, re4_u32 count,
    const re4_skel_pose *reference, re4_skel_pose *current,
    re4_u32 iterations, re4_q16 step_q16,
    re4_u32 *out_applied, re4_u32 *out_skipped);
#ifdef __cplusplus
}
#endif
#endif
