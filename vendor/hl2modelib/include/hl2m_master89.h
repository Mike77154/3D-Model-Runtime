#ifndef HL2M_MASTER89_H
#define HL2M_MASTER89_H
#include "hl2modelib.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Stable C89 vendor routing ABI. Caller allocates result, no heap, no globals. */
#define HL2M_MASTER_ABI89 0x0119
/* format: HL2M_FORMAT_*. out_result must point to matching hl2m_* type.
   MDL for ANI decoding; lod for VVD; VTF outputs hl2m_vtf_info only. */
int hl2m_master_decode89(int format,const hl2m_u8 *bytes,unsigned int bytes_count,
                          const hl2m_mdl *related_mdl,int lod,void *out_result);
int hl2m_master_guess89(const char *path,const hl2m_u8 *bytes,unsigned int bytes_count);
int hl2m_master_mesh89(const hl2m_mdl *mdl,const hl2m_vvd *vvd,
                        const hl2m_vtx *vtx,int lod,hl2m_engine_model *out);
int hl2m_master_anim89(const hl2m_u8 *mdl_data,unsigned int mdl_size,
                        const hl2m_mdl *mdl,int animation,int frame,
                        hl2m_pose_matrices *out_pose,hl2m_native_anim_report89 *report);
int hl2m_master_anim_external89(const hl2m_u8 *mdl_data,unsigned int mdl_size,
    const hl2m_u8 *ani_data,unsigned int ani_size,const hl2m_mdl *mdl,
    int anim,int frame,hl2m_pose_matrices *out,hl2m_native_anim_report89 *report);
/* Opt-in preview guard; non-native fallback on malformed local translations. */
int hl2m_master_anim_guard89(const hl2m_u8 *mdl_data,unsigned int mdl_size,
    const hl2m_u8 *ani_bytes,unsigned int ani_size,const hl2m_mdl *mdl,
    int animation,int frame,hl2m_fixed max_delta,hl2m_pose_matrices *out_pose,
    hl2m_native_anim_report89 *report,hl2m_anim_safety_report89 *safety);
/* Source-inspired fractional sequence runtime. Normalized Q16.16 controls.
   A gesture overlay requires an explicitly decoded DELTA sequence. */
int hl2m_master_sequence89(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int sequence,hl2m_fixed cycle,hl2m_fixed param_x,hl2m_fixed param_y,
    hl2m_pose_matrices *out,int animations_used[4]);
int hl2m_master_gesture89(const hl2m_pose_matrices *base,
    const hl2m_pose_matrices *delta,const hl2m_fixed *bone_weights,
    hl2m_fixed strength,hl2m_pose_matrices *out);
/* v1.0.19: native sequence weight masks, layer envelopes and pose keys. */
int hl2m_master_sequence_metadata89(const hl2m_u8 *data,unsigned int size,
    const hl2m_mdl *mdl,int seq,hl2m_seq_metadata89 *meta);
int hl2m_master_sequence_pose89(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int sequence,hl2m_fixed cycle,hl2m_fixed physical_x,hl2m_fixed physical_y,
    hl2m_pose_matrices *out,int used[4]);
/* Evaluates a main sequence plus directly attached Source autolayers.
   Supports cycle-driven, non-LOCAL, non-world-space layers. A STUDIO_DELTA
   child requires a true DELTA animation descriptor. Unsupported rules fail
   explicitly instead of silently mangling an animation. */
typedef struct hl2m_sequence_layers_report89_s {
    int declared;
    int applied;
    int skipped_zero_weight;
    int child_delta;
    int unsupported_layer;
} hl2m_sequence_layers_report89;
int hl2m_master_sequence_layered89(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int sequence,hl2m_fixed cycle,hl2m_fixed physical_x,hl2m_fixed physical_y,
    hl2m_pose_matrices *out,hl2m_sequence_layers_report89 *report);
/* Opt-in Source 2013 sequence evaluator: normal and pose-driven auto-
   layers, including nested sequences. Inputs are physical Source pose
   parameter values, indexed by MDL-local pose parameter ID. For MDL with
   virtual-model/shared mappings, the caller must supply remapped values.
   Rejects world/local special layers and cyclic sequence dependencies. */
typedef struct hl2m_sequence_layers_report120_s {
    int declared, applied, skipped_zero_weight, child_delta;
    int pose_driven, nested_applied, max_depth;
    int failing_sequence, failing_layer, cycle_detected;
} hl2m_sequence_layers_report120;
int hl2m_master_sequence_layers120(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int sequence,hl2m_fixed cycle,hl2m_fixed physical_x,
    hl2m_fixed physical_y,const hl2m_fixed *pose_values,
    unsigned int pose_count,hl2m_pose_matrices *out,
    hl2m_sequence_layers_report120 *report);
/* v1.0.21 Source normalized global controls (0..ONE), mapped separately
   for EVERY sequence, including auto-layer children. apply_layers=0 returns
   only the base sequence; =1 follows recursive MDL-local auto-layers.
   Transactional output; no virtual-model remaps or LOCAL/WORLD layers. */
int hl2m_master_sequence_controls121(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int sequence,hl2m_fixed cycle,const hl2m_fixed *normalized,
    unsigned int normalized_count,int apply_layers,hl2m_pose_matrices *out,
    hl2m_sequence_layers_report120 *report);

/* Opt-in two-group virtual sequence evaluation. Map inherited Source
   sequence ID to the correct local MDL, and remap child poses to base bones.
   Uses the v1.0.21 sequence player; special IK/SRC-transforms excluded. */
int hl2m_master_virtual_sequence122(const hl2m_u8 *base_bytes,
    unsigned int base_size,const hl2m_u8 *base_ani,unsigned int base_ani_size,
    const hl2m_mdl *base,const hl2m_u8 *child_bytes,
    unsigned int child_size,const hl2m_u8 *child_ani,
    unsigned int child_ani_size,const hl2m_mdl *child,
    const hl2m_virtual_link122 *link,int virtual_seq,hl2m_fixed cycle,
    const hl2m_fixed *global_controls,unsigned int controls_count,
    int apply_layers,hl2m_pose_matrices *out,
    hl2m_sequence_layers_report120 *report);

/* v1.0.23: Evaluate a sequence in any of up to four bound groups.
   Caller supplies pre-built catalog, backing MDL/ANI bytes and global
   normalized pose parameters. Cross-group autolayers are NOT implemented. */
int hl2m_master_virtual_sequence_multi123(const hl2m_virtual_multi123 *catalog,
    int virtual_seq,hl2m_fixed cycle,const hl2m_fixed *global_controls,
    unsigned int controls_count,int apply_local_layers,
    hl2m_pose_matrices *out,hl2m_sequence_layers_report120 *report);

/* v1.0.24: opt-in virtual group autolayer traversal. Interprets layer
   relative sequence through Source iRelativeSeq/group masterSeq, allowing
   STUDIO_OVERRIDE to route a gesture to another bank. Inherited weights,
   per-bone masks, and global normalized POSE controls. Refuses IK, LOCAL,
   WORLD, incompatible parent skeletons and impossible/cyclic layers.
   Output is transactional; old local-only multi123 API is unchanged. */
int hl2m_master_virtual_sequence_layers124(const hl2m_virtual_multi123 *catalog,
    int virtual_seq,hl2m_fixed cycle,const hl2m_fixed *global_controls,
    unsigned int control_count,hl2m_pose_matrices *out,
    hl2m_sequence_layers_report120 *report);

int hl2m_master_layer_mask89(const hl2m_pose_matrices *base,
    const hl2m_pose_matrices *overlay,const hl2m_fixed *weights,
    hl2m_fixed strength,int flags,hl2m_pose_matrices *out);
int hl2m_master_skin89(const hl2m_engine_model *mesh,
                        const hl2m_pose_matrices *inverse_bind,
                        const hl2m_pose_matrices *anim_pose,
                        hl2m_engine_model *out,hl2m_skin_report *report);
int hl2m_master_texture89(const hl2m_u8 *bytes,unsigned int size,
                           hl2m_u8 *rgba,unsigned int rgba_capacity,
                           hl2m_vtf_info *info);
int hl2m_master_material89(const hl2m_u8 *vmt,unsigned int size,
                            hl2m_vmt *out_vmt,hl2m_material_fx89 *out_fx);
int hl2m_master_motion89(const hl2m_u8 *data,unsigned int size,
    const hl2m_mdl *mdl,int anim,int frame,hl2m_vec3 *position,hl2m_fixed *yaw);
/* Portable C89 runtime helpers. These are not the Source VPhysics/IK GPU solvers. */
typedef struct hl2m_physics_body89_s {
    hl2m_vec3 center;
    hl2m_vec3 velocity;
    hl2m_vec3 half_extent;
    int grounded;
} hl2m_physics_body89;
typedef struct hl2m_phy_summary89_s {
    int source_solid_count;
    int parsed_solid_count;
    int checksum;
    int compact_solids;
    int legacy_solids;
} hl2m_phy_summary89;
int hl2m_master_phy_summary89(const hl2m_phy *phy,hl2m_phy_summary89 *out);
int hl2m_master_physics_step89(hl2m_physics_body89 *body,hl2m_fixed dt,
    hl2m_fixed gravity_z,hl2m_fixed ground_z);
int hl2m_master_physics_overlap89(const hl2m_physics_body89 *a,
    const hl2m_physics_body89 *b,int *overlap);
/* Foot plant root correction; preserves bone hierarchy, no 2-bone knee solve. */
int hl2m_master_ik_root_plant89(const hl2m_pose_matrices *pose,int tip_bone,
    hl2m_vec3 contact_world,hl2m_fixed max_shift,hl2m_vec3 *out_root_shift);
int hl2m_master_ik_apply_root89(const hl2m_pose_matrices *pose,
    hl2m_vec3 root_shift,hl2m_pose_matrices *out_pose);
int hl2m_master_ik_qc89(const hl2m_qc *qc,const hl2m_ik_pose *pose,
    int frame,hl2m_ik_pose *out_pose,hl2m_ik_report *report);
/* v1.0.9 VMT/VTF/VVD -> renderer neutral fragment binding.
   Input RGBAs are caller-managed image buffers. Shadow and lighting direction
   must be supplied by the presenter. */
int hl2m_master_fragment89(hl2m_fragment89 *frag,const hl2m_material_fx89 *fx,
    hl2m_vec3 normal,hl2m_vec4 tangent,
    const hl2m_texture_view89 *base,const hl2m_texture_view89 *bump,
    const hl2m_texture_view89 *detail,hl2m_fixed u,hl2m_fixed v);
int hl2m_master_shade89(const hl2m_material_fx89 *fx,
    const hl2m_fragment89 *frag,hl2m_u8 out_rgba[4]);
/* v1.0.10: Smooth (per-fragment) raster interpolation.
   A presenter supplies screen-space barycentric weights (Q16.16).
   Source VVD normals and tangents are preserved; no mesh subdivision.
   Interpolation must occur at EACH rasterized fragment, not face center.
   Caller still owns light/view directions and shadow inputs in frag. */
int hl2m_master_smooth_fragment89(hl2m_fragment89 *frag,
    const hl2m_material_fx89 *fx,
    const hl2m_engine_vertex *a,const hl2m_engine_vertex *b,
    const hl2m_engine_vertex *c,
    hl2m_fixed wa,hl2m_fixed wb,hl2m_fixed wc,
    const hl2m_texture_view89 *base,const hl2m_texture_view89 *bump,
    const hl2m_texture_view89 *detail);

/* Optional Source UV per-fragment bilinear sampling.
   Weights must already be perspective-corrected by the presenter. */
int hl2m_master_uv_sample_linear89(const hl2m_texture_view89 *texture,
    hl2m_fixed u,hl2m_fixed v,hl2m_u8 rgba[4]);
int hl2m_master_smooth_fragment_linear89(hl2m_fragment89 *frag,
    const hl2m_material_fx89 *fx,
    const hl2m_engine_vertex *a,const hl2m_engine_vertex *b,
    const hl2m_engine_vertex *c,
    hl2m_fixed wa,hl2m_fixed wb,hl2m_fixed wc,
    const hl2m_texture_view89 *base,const hl2m_texture_view89 *bump,
    const hl2m_texture_view89 *detail);

/* v1.0.6 SDK-inspired extensions. Original ABI entry points remain intact.
   Two-segment IK uses existing animated pose, keeps all bone lengths, rotates
   descendants with joint corrections and rebuilds local joint matrices.
   Goal is parent/model space, same as pose.world[]. Coordinates are 16.16. */
typedef struct hl2m_ik_chain89_s {
    int hip;
    int knee;
    int foot;
    hl2m_vec3 goal;
    hl2m_fixed weight; /* 0..65536 */
} hl2m_ik_chain89;
typedef struct hl2m_ik_chain_report89_s {
    hl2m_fixed error_before;
    hl2m_fixed error_after;
    hl2m_fixed upper_length;
    hl2m_fixed lower_length;
    int reach_clamped;
    int applied;
} hl2m_ik_chain_report89;
/* Input and output may alias. World and local matrices stay consistent. */
int hl2m_master_ik_two_bone89(const hl2m_pose_matrices *pose,
    const hl2m_ik_chain89 *chain,hl2m_pose_matrices *out,
    hl2m_ik_chain_report89 *report);
/* Caller-owned stance capture. Contact Z offset is foot contact plane in
   model/world coordinates. Only plant when near surface and slow enough. */
typedef struct hl2m_foot_lock89_s {
    hl2m_vec3 last;
    hl2m_vec3 anchor;
    int initialized;
    int planted;
} hl2m_foot_lock89;
int hl2m_master_foot_lock89(hl2m_foot_lock89 *state,
    hl2m_vec3 world_foot,hl2m_fixed floor_z,hl2m_fixed stance_height,
    hl2m_fixed max_frame_motion,hl2m_vec3 *target,int *planted);
/* Kinematic character controller: separated world root motion from bone pose;
   grounded when feet/body bottom reaches floor; prevents floor penetration. */
int hl2m_master_character_move89(hl2m_physics_body89 *body,
    hl2m_vec3 desired_world_motion,hl2m_fixed ground_z,
    hl2m_fixed max_step_up);

#ifdef __cplusplus
}
#endif
#endif
