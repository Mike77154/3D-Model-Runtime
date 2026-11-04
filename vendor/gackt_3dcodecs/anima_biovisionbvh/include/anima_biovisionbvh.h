#ifndef ANIMA_BIOVISIONBVH_H
#define ANIMA_BIOVISIONBVH_H

/*
    anima_biovisionbvh
    ANSI C89 / fixed point / caller-owned BVH decoder and pose evaluator.

    Core rules:
    - no runtime allocator calls
    - no binary FP types in public structs or core math
    - caller-owned / static storage only
    - BVH text is parsed from a caller-provided memory buffer
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ANIMA_BVH_MAX_JOINTS
#define ANIMA_BVH_MAX_JOINTS 160
#endif

#ifndef ANIMA_BVH_MAX_CHANNELS
#define ANIMA_BVH_MAX_CHANNELS 768
#endif

#ifndef ANIMA_BVH_MAX_FRAMES
#define ANIMA_BVH_MAX_FRAMES 512
#endif

#ifndef ANIMA_BVH_NAME_MAX
#define ANIMA_BVH_NAME_MAX 48
#endif

#ifndef ANIMA_BVH_ERROR_MAX
#define ANIMA_BVH_ERROR_MAX 160
#endif

#ifndef ANIMA_BVH_TOKEN_MAX
#define ANIMA_BVH_TOKEN_MAX 96
#endif

#define ANIMA_BVH_FIXED_SHIFT 16
#define ANIMA_BVH_FIXED_ONE   ((anima_bvh_fixed)65536L)
#define ANIMA_BVH_Q15_ONE     32767L
#define ANIMA_BVH_OK          1
#define ANIMA_BVH_FAIL        0


#define ANIMA_BVH_VALIDATE_NONE                 0
#define ANIMA_BVH_VALIDATE_NO_ROOT              1
#define ANIMA_BVH_VALIDATE_BAD_PARENT_ORDER     2
#define ANIMA_BVH_VALIDATE_DUPLICATE_NAMES      4
#define ANIMA_BVH_VALIDATE_NON_ROOT_POSITION    8
#define ANIMA_BVH_VALIDATE_ZERO_LENGTH_BONES    16
#define ANIMA_BVH_VALIDATE_NO_FRAMES            32
#define ANIMA_BVH_VALIDATE_CHANNEL_MISMATCH     64

/* Q16.16 signed fixed point. Uses long for ANSI C89 portability. */
typedef long anima_bvh_fixed;

/* Q1.15-ish matrix coefficients stored in long for simple C89 arithmetic. */
typedef long anima_bvh_q15;

typedef enum AnimaBVHChannelType_e {
    ANIMA_BVH_CHAN_INVALID = 0,
    ANIMA_BVH_CHAN_XPOSITION,
    ANIMA_BVH_CHAN_YPOSITION,
    ANIMA_BVH_CHAN_ZPOSITION,
    ANIMA_BVH_CHAN_XROTATION,
    ANIMA_BVH_CHAN_YROTATION,
    ANIMA_BVH_CHAN_ZROTATION
} AnimaBVHChannelType;

typedef enum AnimaBVHTruncationFlags_e {
    ANIMA_BVH_TRUNC_NONE     = 0,
    ANIMA_BVH_TRUNC_JOINTS   = 1,
    ANIMA_BVH_TRUNC_CHANNELS = 2,
    ANIMA_BVH_TRUNC_FRAMES   = 4,
    ANIMA_BVH_TRUNC_NAMES    = 8
} AnimaBVHTruncationFlags;

typedef struct AnimaBVHJoint_s {
    char name[ANIMA_BVH_NAME_MAX];
    int parent;
    int first_child;
    int next_sibling;
    int first_channel;
    int channel_count;
    int is_end_site;
    anima_bvh_fixed offset[3];
} AnimaBVHJoint;

typedef struct AnimaBVHChannel_s {
    int joint;
    AnimaBVHChannelType type;
} AnimaBVHChannel;

typedef struct AnimaBVH_s {
    AnimaBVHJoint joints[ANIMA_BVH_MAX_JOINTS];
    AnimaBVHChannel channels[ANIMA_BVH_MAX_CHANNELS];
    anima_bvh_fixed frame_values[ANIMA_BVH_MAX_FRAMES][ANIMA_BVH_MAX_CHANNELS];

    int joint_count;
    int channel_count;
    int channels_declared;
    int root_joint;
    int frames_declared;
    int frames_stored;
    anima_bvh_fixed frame_time;
    int truncation_flags;

    unsigned int error_line;
    unsigned int error_col;
    char error[ANIMA_BVH_ERROR_MAX];
} AnimaBVH;

typedef struct AnimaBVHPose_s {
    anima_bvh_fixed world_pos[ANIMA_BVH_MAX_JOINTS][3];
    anima_bvh_q15 world_rot[ANIMA_BVH_MAX_JOINTS][9];
} AnimaBVHPose;


typedef struct AnimaBVHSample_s {
    anima_bvh_fixed channel_values[ANIMA_BVH_MAX_CHANNELS];
} AnimaBVHSample;

typedef struct AnimaBVHAabb_s {
    int initialized;
    anima_bvh_fixed min_v[3];
    anima_bvh_fixed max_v[3];
} AnimaBVHAabb;

typedef struct AnimaBVHValidation_s {
    int issue_flags;
    int duplicate_name_count;
    int end_site_count;
    int max_depth;
    int non_root_position_channel_count;
    int zero_length_bone_count;
    int bad_parent_count;
    int channel_mismatch_count;
} AnimaBVHValidation;


typedef struct AnimaBVHChannelRange_s {
    int initialized;
    anima_bvh_fixed min_v;
    anima_bvh_fixed max_v;
    int min_frame;
    int max_frame;
} AnimaBVHChannelRange;

typedef struct AnimaBVHSkeletonStats_s {
    int joint_count;
    int end_site_count;
    int animated_joint_count;
    int max_depth;
    int zero_length_bone_count;
    int longest_bone_joint;
    anima_bvh_fixed total_bone_length;
    anima_bvh_fixed longest_bone_length;
} AnimaBVHSkeletonStats;

typedef struct AnimaBVHJointMap_s {
    int dst_to_src_joint[ANIMA_BVH_MAX_JOINTS];
    int mapped_count;
    int missing_count;
} AnimaBVHJointMap;

typedef struct AnimaBVHRetargetMap_s {
    int dst_to_src_channel[ANIMA_BVH_MAX_CHANNELS];
    int mapped_count;
    int missing_count;
} AnimaBVHRetargetMap;

typedef struct AnimaBVHTimeCursor_s {
    int frame_a;
    int frame_b;
    anima_bvh_fixed alpha_q16;
    int wrapped;
} AnimaBVHTimeCursor;

typedef struct AnimaBVHAxisMap_s {
    int src_axis_for_dst[3];
    int sign[3];
    anima_bvh_fixed scale_q16;
} AnimaBVHAxisMap;



typedef enum AnimaBVHRotationOrder_e {
    ANIMA_BVH_ROT_ORDER_NONE = 0,
    ANIMA_BVH_ROT_ORDER_XYZ,
    ANIMA_BVH_ROT_ORDER_XZY,
    ANIMA_BVH_ROT_ORDER_YXZ,
    ANIMA_BVH_ROT_ORDER_YZX,
    ANIMA_BVH_ROT_ORDER_ZXY,
    ANIMA_BVH_ROT_ORDER_ZYX,
    ANIMA_BVH_ROT_ORDER_MIXED,
    ANIMA_BVH_ROT_ORDER_COUNT
} AnimaBVHRotationOrder;

typedef struct AnimaBVHProfile_s {
    int root_is_hips_like;
    int root_has_xyz_position;
    int root_has_6_channels;
    AnimaBVHRotationOrder root_rotation_order;
    int rotation_order_counts[ANIMA_BVH_ROT_ORDER_COUNT];
    int joints_with_rotation;
    int non_root_position_channel_count;
    int motionbuilder_name_hits;
    int humanoid_name_hits;
    int end_site_count;
    int first_frame_nonzero_rotation_count;
    int dominant_offset_axis;
    int dominant_offset_negative_count;
    int dominant_offset_positive_count;
    int biovision_like_score;
} AnimaBVHProfile;

typedef struct AnimaBVHNamePair_s {
    char canonical[ANIMA_BVH_NAME_MAX];
    char alias[ANIMA_BVH_NAME_MAX];
} AnimaBVHNamePair;

typedef int (*AnimaBVHWriteFn)(void *user, const char *text);

void anima_bvh_init(AnimaBVH *bvh);
int anima_bvh_parse_text(AnimaBVH *bvh, const char *text, unsigned long length);
const char *anima_bvh_error(const AnimaBVH *bvh);

int anima_bvh_eval_frame(const AnimaBVH *bvh, int frame_index, AnimaBVHPose *pose);


int anima_bvh_make_sample_frame(const AnimaBVH *bvh, int frame_index, AnimaBVHSample *sample);
int anima_bvh_make_sample_lerp(const AnimaBVH *bvh, int frame_a, int frame_b,
                               anima_bvh_fixed alpha_q16, AnimaBVHSample *sample);
int anima_bvh_eval_sample(const AnimaBVH *bvh, const AnimaBVHSample *sample, AnimaBVHPose *pose);
int anima_bvh_eval_time_loop(const AnimaBVH *bvh, anima_bvh_fixed time_q16,
                             int loop, AnimaBVHSample *sample, AnimaBVHPose *pose);
int anima_bvh_eval_frame_scaled(const AnimaBVH *bvh, int frame_index,
                                anima_bvh_fixed scale_q16, AnimaBVHPose *pose);
int anima_bvh_compute_pose_aabb(const AnimaBVH *bvh, const AnimaBVHPose *pose, AnimaBVHAabb *aabb);
int anima_bvh_validate(const AnimaBVH *bvh, AnimaBVHValidation *report);
int anima_bvh_count_end_sites(const AnimaBVH *bvh);
int anima_bvh_joint_depth(const AnimaBVH *bvh, int joint_index);
anima_bvh_fixed anima_bvh_duration_frames(const AnimaBVH *bvh);
int anima_bvh_extract_root_delta(const AnimaBVH *bvh, int frame_a, int frame_b,
                                 anima_bvh_fixed out_delta[3]);
int anima_bvh_find_joint(const AnimaBVH *bvh, const char *name);
const char *anima_bvh_channel_name(AnimaBVHChannelType type);

int anima_bvh_find_channel(const AnimaBVH *bvh, int joint_index,
                           AnimaBVHChannelType type, int occurrence);
int anima_bvh_get_frame_channel(const AnimaBVH *bvh, int frame_index, int joint_index,
                                AnimaBVHChannelType type, int occurrence,
                                anima_bvh_fixed *out_value);
int anima_bvh_set_frame_channel(AnimaBVH *bvh, int frame_index, int channel_index,
                                anima_bvh_fixed value);
int anima_bvh_time_to_cursor(const AnimaBVH *bvh, anima_bvh_fixed time_q16,
                             int loop, AnimaBVHTimeCursor *cursor);
int anima_bvh_make_sample_time(const AnimaBVH *bvh, anima_bvh_fixed time_q16,
                               int loop, AnimaBVHSample *sample,
                               AnimaBVHTimeCursor *cursor);
int anima_bvh_make_sample_blend(const AnimaBVH *bvh,
                                const AnimaBVHSample *sample_a,
                                const AnimaBVHSample *sample_b,
                                anima_bvh_fixed alpha_q16,
                                AnimaBVHSample *out_sample);
int anima_bvh_make_sample_additive(const AnimaBVH *bvh,
                                   const AnimaBVHSample *base_sample,
                                   const AnimaBVHSample *add_sample,
                                   const AnimaBVHSample *ref_sample,
                                   anima_bvh_fixed weight_q16,
                                   AnimaBVHSample *out_sample);
int anima_bvh_make_sample_root_relative(const AnimaBVH *bvh,
                                        const AnimaBVHSample *in_sample,
                                        const anima_bvh_fixed origin[3],
                                        AnimaBVHSample *out_sample);
int anima_bvh_build_joint_map_by_name(const AnimaBVH *src, const AnimaBVH *dst,
                                      AnimaBVHJointMap *map);
int anima_bvh_build_retarget_map_by_name(const AnimaBVH *src, const AnimaBVH *dst,
                                         AnimaBVHRetargetMap *map);
int anima_bvh_retarget_sample(const AnimaBVH *src, const AnimaBVH *dst,
                              const AnimaBVHRetargetMap *map,
                              const AnimaBVHSample *src_sample,
                              const AnimaBVHSample *dst_ref_sample,
                              AnimaBVHSample *dst_out_sample);
int anima_bvh_compute_channel_range(const AnimaBVH *bvh, int channel_index,
                                    int first_frame, int frame_count,
                                    AnimaBVHChannelRange *range);
int anima_bvh_compute_motion_aabb(const AnimaBVH *bvh, int first_frame,
                                  int frame_count, AnimaBVHPose *scratch_pose,
                                  AnimaBVHAabb *aabb);
int anima_bvh_compute_skeleton_stats(const AnimaBVH *bvh,
                                     AnimaBVHSkeletonStats *stats);
anima_bvh_fixed anima_bvh_bone_length_estimate(const AnimaBVH *bvh, int joint_index);
unsigned long anima_bvh_skeleton_hash(const AnimaBVH *bvh);
int anima_bvh_export_bone_csv(const AnimaBVH *bvh, int decimals,
                              AnimaBVHWriteFn write_fn, void *user);
int anima_bvh_export_root_path_csv(const AnimaBVH *bvh, int decimals,
                                   AnimaBVHWriteFn write_fn, void *user);
void anima_bvh_axis_map_identity(AnimaBVHAxisMap *map);
int anima_bvh_axis_map_apply_vec(const AnimaBVHAxisMap *map,
                                 const anima_bvh_fixed in_v[3],
                                 anima_bvh_fixed out_v[3]);
int anima_bvh_axis_map_apply_pose(const AnimaBVH *bvh,
                                  const AnimaBVHPose *in_pose,
                                  const AnimaBVHAxisMap *map,
                                  AnimaBVHPose *out_pose);
AnimaBVHChannelType anima_bvh_channel_type_from_name(const char *name);

const char *anima_bvh_rotation_order_name(AnimaBVHRotationOrder order);
AnimaBVHRotationOrder anima_bvh_joint_rotation_order(const AnimaBVH *bvh, int joint_index);
void anima_bvh_normalize_name(const char *in_name, char *out_name, unsigned int out_cap);
int anima_bvh_find_joint_normalized(const AnimaBVH *bvh, const char *name);
int anima_bvh_parse_name_map_text(const char *text, unsigned long length,
                                  AnimaBVHNamePair *pairs, int max_pairs,
                                  int *out_count);
int anima_bvh_build_joint_map_by_name_pairs(const AnimaBVH *src,
                                            const AnimaBVH *dst,
                                            const AnimaBVHNamePair *pairs,
                                            int pair_count,
                                            AnimaBVHJointMap *map);
int anima_bvh_build_retarget_map_by_joint_map(const AnimaBVH *src,
                                              const AnimaBVH *dst,
                                              const AnimaBVHJointMap *joint_map,
                                              AnimaBVHRetargetMap *map);
int anima_bvh_analyze_profile(const AnimaBVH *bvh, AnimaBVHProfile *profile);
int anima_bvh_export_profile_csv(const AnimaBVH *bvh, int decimals,
                                 AnimaBVHWriteFn write_fn, void *user);
int anima_bvh_make_zero_rotation_sample(const AnimaBVH *bvh, int frame_index_for_positions,
                                        AnimaBVHSample *sample);
int anima_bvh_count_nonzero_rotations(const AnimaBVH *bvh, int frame_index,
                                      anima_bvh_fixed tolerance_q16);
int anima_bvh_clone_skeleton_empty(const AnimaBVH *src, AnimaBVH *dst,
                                   int frame_count, anima_bvh_fixed frame_time_q16);
int anima_bvh_copy_frame_range(const AnimaBVH *src, AnimaBVH *dst,
                               int first_frame, int frame_count);
int anima_bvh_resample_clip(const AnimaBVH *src, AnimaBVH *dst,
                            anima_bvh_fixed new_frame_time_q16,
                            int out_frames, int loop);
int anima_bvh_copy_with_position_axis_map(const AnimaBVH *src, AnimaBVH *dst,
                                          const AnimaBVHAxisMap *map);
int anima_bvh_apply_root_translation_offset(AnimaBVH *bvh,
                                            const anima_bvh_fixed offset[3]);
int anima_bvh_normalize_root_translation(AnimaBVH *bvh, int origin_frame,
                                         int keep_y_axis);


anima_bvh_fixed anima_bvh_fixed_from_int(long value);
anima_bvh_fixed anima_bvh_parse_fixed_cstr(const char *text, int *ok);
void anima_bvh_fixed_to_cstr(anima_bvh_fixed value, char *out, unsigned int out_cap, int decimals);

anima_bvh_q15 anima_bvh_sin_deg_q15(anima_bvh_fixed degrees_q16);
anima_bvh_q15 anima_bvh_cos_deg_q15(anima_bvh_fixed degrees_q16);

int anima_bvh_export_pose_obj(const AnimaBVH *bvh, const AnimaBVHPose *pose,
                              AnimaBVHWriteFn write_fn, void *user);
int anima_bvh_export_summary(const AnimaBVH *bvh, AnimaBVHWriteFn write_fn, void *user);

int anima_bvh_export_skeleton_dot(const AnimaBVH *bvh, AnimaBVHWriteFn write_fn, void *user);
int anima_bvh_export_channels_csv_header(const AnimaBVH *bvh, AnimaBVHWriteFn write_fn, void *user);
int anima_bvh_export_frame_csv(const AnimaBVH *bvh, int frame_index, int decimals,
                               AnimaBVHWriteFn write_fn, void *user);
int anima_bvh_export_bvh(const AnimaBVH *bvh, int frames_to_write, int decimals,
                         AnimaBVHWriteFn write_fn, void *user);

#ifdef __cplusplus
}
#endif

#endif
