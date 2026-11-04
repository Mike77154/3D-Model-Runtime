#ifndef RE4_SKELETAL_H
#define RE4_SKELETAL_H
#include "re4_model.h"
#include "re4_animation.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RE4_SKEL_MAX_BONES 512U
/* Row-major affine 3x4 matrix; vectors are columns. Q16.16 throughout. */
/* Unique FCV rotation ID -> PMD skeleton index; -1 if absent or ambiguous. */
re4_i32 re4_skel_joint_for_fcv_id(const re4_pmd_model *model,re4_u8 id);

typedef struct re4_skel_matrix_s { re4_q16 m[12]; } re4_skel_matrix;
typedef struct re4_skel_pose_s {
    re4_u32 bone_count;
    re4_skel_matrix *world; /* caller-owned, capacity >= model.skeleton_count */
    re4_u32 applied_rotation_channels;
    re4_u32 ignored_special_channels;
} re4_skel_pose;
/* Player state: input delta uses Q8.8 frames; speed 256 means 1x.
   Root motion/IK are external responsibilities, not hidden corrections. */
typedef struct re4_skel_clock_s {
    re4_u32 position_q8;
    re4_u16 end_frame;
    re4_u16 speed_q8;
    re4_bool looping;
    re4_bool playing;
} re4_skel_clock;
void re4_skel_clock_init(re4_skel_clock *clock, re4_u16 end_frame);
void re4_skel_clock_seek(re4_skel_clock *clock, re4_u16 frame);
void re4_skel_clock_step(re4_skel_clock *clock, re4_u16 delta_q8);
re4_u16 re4_skel_clock_frame(const re4_skel_clock *clock);
/* Pose generator. Only verified bone rotation channel 0x02 is applied.
   IK/root-motion/scale special channels are deliberately NOT interpreted yet. */
re4_err re4_skel_evaluate(const re4_pmd_model *model, re4_span fcv_bytes,
                          const re4_fcv *animation, re4_u16 frame,
                          re4_skel_pose *pose, re4_u32 capacity);
/* Crossfade LOCAL FCV Euler deltas before hierarchy evaluation; components
   then follow via re4_character_retarget(). weight_b: Q16.16, [0,65536].
   Missing FCV rotations use the PMD bind orientation. Source clips are not
   modified. This does not decode native IK targets or native blend timing. */
re4_err re4_skel_evaluate_blend(const re4_pmd_model *model,
    re4_span src_a,const re4_fcv *clip_a,re4_u16 frame_a,
    re4_span src_b,const re4_fcv *clip_b,re4_u16 frame_b,
    re4_q16 weight_b,re4_skel_pose *pose,re4_u32 capacity);
/* Two-influence linear blend skinning. PMD bone indices are local slots * 3.
   Each node has a table of (global bone ID, inverse bind matrix) entries. */
re4_err re4_skel_skin_vertex(re4_span pmd_bytes, const re4_pmd_node *node,
                             const re4_pmd_vertex_q16 *vertex,
                             const re4_skel_pose *pose,
                             re4_q16 position[3], re4_q16 normal[3]);
/* Rigid PMD node without a local inverse-bind slot table. This is an
   explicit, validated world-pose delta; caller supplies bind and animated
   skeletons. Does NOT infer native RE4 IK or skinning semantics. */
re4_err re4_skel_skin_rigid_node(const re4_pmd_node *node,
    const re4_pmd_vertex_q16 *vertex, const re4_skel_pose *rest,
    const re4_skel_pose *animated,re4_q16 position[3],re4_q16 normal[3]);
re4_err re4_skel_ik_ccd_two_bone(const re4_pmd_model *model,
        re4_skel_pose *pose, re4_u32 proximal, re4_u32 distal,
        re4_u32 tip, const re4_q16 target_xyz[3], re4_u32 iterations,
        re4_q16 initial_angle_step, re4_q16 *out_error_l1);
/* Explicit planar CCD IK. axis 0=X,1=Y,2=Z; world-space rotations.
   Deterministic C89 fixed-point helper; not a substitute for native RE4 IK. */
re4_err re4_skel_ik_ccd_axis(const re4_pmd_model *model,
        re4_skel_pose *pose, re4_u32 proximal, re4_u32 distal,
        re4_u32 tip, const re4_q16 target_xyz[3], re4_u32 iterations,
        re4_q16 initial_angle_step, re4_u8 axis,
        re4_q16 *out_error_l1);
/* Optional explicit correction budgets for two IK pivots; does not pretend
   to know RE4 native joint limits. Index 0 distal, 1 proximal. Bounds in
   Q16.16 radians around the input pose; 0 must lie inside each interval. */
re4_err re4_skel_ik_ccd_axis_limited(const re4_pmd_model *m,
    re4_skel_pose *pose,re4_u32 a,re4_u32 b,re4_u32 tip,
    const re4_q16 target[3],re4_u32 iterations,re4_q16 step,re4_u8 axis,
    const re4_q16 lower[2],const re4_q16 upper[2],re4_q16 *error);
#ifdef __cplusplus
}
#endif
#endif

