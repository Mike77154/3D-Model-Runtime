#ifndef MOTION_BVH_POSES_H
#define MOTION_BVH_POSES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MBVH_MAX_NODES
#define MBVH_MAX_NODES 128
#endif

#ifndef MBVH_MAX_CHANNELS
#define MBVH_MAX_CHANNELS 512
#endif

#ifndef MBVH_MAX_FRAMES
#define MBVH_MAX_FRAMES 512
#endif

#ifndef MBVH_MAX_NAME
#define MBVH_MAX_NAME 48
#endif

#ifndef MBVH_MAX_DEPTH
#define MBVH_MAX_DEPTH 64
#endif

typedef signed int mbvh_i32;
typedef unsigned int mbvh_u32;
#if defined(_MSC_VER)
typedef __int64 mbvh_i64;
#else
typedef signed long long mbvh_i64;
#endif

typedef mbvh_i32 mbvh_fixed;
#if defined(_MSC_VER)
typedef unsigned __int64 mbvh_u64;
#else
typedef unsigned long long mbvh_u64;
#endif

#define MBVH_FIXED_SHIFT 16
#define MBVH_FIXED_ONE ((mbvh_fixed)65536)
#define MBVH_FIXED_HALF ((mbvh_fixed)32768)
#define MBVH_FIXED_ZERO ((mbvh_fixed)0)

#define MBVH_AXIS_X 0
#define MBVH_AXIS_Y 1
#define MBVH_AXIS_Z 2

#define MBVH_ARM_AXIS_UNKNOWN 0
#define MBVH_ARM_AXIS_X 1
#define MBVH_ARM_AXIS_Z 2

#define MBVH_CHANNEL_XPOSITION 1
#define MBVH_CHANNEL_YPOSITION 2
#define MBVH_CHANNEL_ZPOSITION 3
#define MBVH_CHANNEL_XROTATION 4
#define MBVH_CHANNEL_YROTATION 5
#define MBVH_CHANNEL_ZROTATION 6

typedef enum mbvh_ResultTag {
    MBVH_OK = 0,
    MBVH_ERR_BAD_ARGUMENT,
    MBVH_ERR_UNEXPECTED_EOF,
    MBVH_ERR_UNEXPECTED_TOKEN,
    MBVH_ERR_BAD_NUMBER,
    MBVH_ERR_TOO_MANY_NODES,
    MBVH_ERR_TOO_MANY_CHANNELS,
    MBVH_ERR_TOO_MANY_FRAMES,
    MBVH_ERR_TOO_DEEP,
    MBVH_ERR_BAD_CHANNEL,
    MBVH_ERR_MOTION_MISMATCH,
    MBVH_ERR_DIVIDE_BY_ZERO,
    MBVH_ERR_OUTPUT_TOO_SMALL,
    MBVH_ERR_CHANNEL_NOT_FOUND
} mbvh_Result;

#define MBVH_WARN_NONE 0U
#define MBVH_WARN_NO_END_SITES 1U
#define MBVH_WARN_ROOT_WITHOUT_POSITION 2U
#define MBVH_WARN_NON_ROOT_POSITION_CHANNELS 4U
#define MBVH_WARN_UNUSUAL_CHANNEL_COUNT 8U
#define MBVH_WARN_ZERO_FRAME_TIME 16U
#define MBVH_WARN_EMPTY_MOTION 32U
#define MBVH_WARN_DUPLICATE_NAMES 64U
#define MBVH_WARN_POSER_UNMAPPED_NAMES 128U
#define MBVH_WARN_POSER_Z_ARM_AXIS 256U

#define MBVH_EVAL_SCALE_OFFSETS 1U
#define MBVH_EVAL_SCALE_POSITIONS 2U
#define MBVH_EVAL_ROOT_PRETRANSLATE 4U
#define MBVH_EVAL_DROP_TO_FLOOR_Y 8U

typedef struct mbvh_Channel {
    unsigned short node_index;
    unsigned char type;
    unsigned char axis;
} mbvh_Channel;

typedef struct mbvh_Node {
    char name[MBVH_MAX_NAME];
    short parent;
    short first_child;
    short next_sibling;
    unsigned short first_channel;
    unsigned char channel_count;
    unsigned char is_end_site;
    mbvh_fixed offset[3];
} mbvh_Node;

typedef struct mbvh_Mat34 {
    mbvh_fixed m[12];
} mbvh_Mat34;

typedef struct mbvh_Quat {
    mbvh_fixed x;
    mbvh_fixed y;
    mbvh_fixed z;
    mbvh_fixed w;
} mbvh_Quat;

typedef struct mbvh_Pose {
    mbvh_Mat34 local[MBVH_MAX_NODES];
    mbvh_Mat34 global[MBVH_MAX_NODES];
    mbvh_fixed world_position[MBVH_MAX_NODES][3];
} mbvh_Pose;

typedef struct mbvh_EvalOptions {
    mbvh_fixed scale;
    mbvh_fixed root_pretranslate[3];
    mbvh_fixed floor_y;
    unsigned int flags;
} mbvh_EvalOptions;

typedef struct mbvh_Stats {
    unsigned int node_count;
    unsigned int joint_count;
    unsigned int end_site_count;
    unsigned int channel_count;
    unsigned int position_channel_count;
    unsigned int rotation_channel_count;
    unsigned int frame_count;
    unsigned int max_depth;
    mbvh_fixed frame_time;
    mbvh_fixed duration;
    unsigned int warnings;
} mbvh_Stats;

typedef struct mbvh_RetargetMap {
    short node_to_target[MBVH_MAX_NODES];
    unsigned int target_count;
} mbvh_RetargetMap;


typedef enum mbvh_PoserActorTag {
    MBVH_POSER_ACTOR_UNKNOWN = -1,
    MBVH_POSER_ACTOR_HIP = 0,
    MBVH_POSER_ACTOR_WAIST,
    MBVH_POSER_ACTOR_ABDOMEN,
    MBVH_POSER_ACTOR_CHEST,
    MBVH_POSER_ACTOR_NECK,
    MBVH_POSER_ACTOR_HEAD,
    MBVH_POSER_ACTOR_L_EYE,
    MBVH_POSER_ACTOR_R_EYE,
    MBVH_POSER_ACTOR_L_COLLAR,
    MBVH_POSER_ACTOR_L_SHLDR,
    MBVH_POSER_ACTOR_L_FOREARM,
    MBVH_POSER_ACTOR_L_HAND,
    MBVH_POSER_ACTOR_R_COLLAR,
    MBVH_POSER_ACTOR_R_SHLDR,
    MBVH_POSER_ACTOR_R_FOREARM,
    MBVH_POSER_ACTOR_R_HAND,
    MBVH_POSER_ACTOR_L_BUTTOCK,
    MBVH_POSER_ACTOR_L_THIGH,
    MBVH_POSER_ACTOR_L_SHIN,
    MBVH_POSER_ACTOR_L_FOOT,
    MBVH_POSER_ACTOR_L_TOE,
    MBVH_POSER_ACTOR_R_BUTTOCK,
    MBVH_POSER_ACTOR_R_THIGH,
    MBVH_POSER_ACTOR_R_SHIN,
    MBVH_POSER_ACTOR_R_FOOT,
    MBVH_POSER_ACTOR_R_TOE,
    MBVH_POSER_ACTOR_L_THUMB1,
    MBVH_POSER_ACTOR_L_THUMB2,
    MBVH_POSER_ACTOR_L_THUMB3,
    MBVH_POSER_ACTOR_L_INDEX1,
    MBVH_POSER_ACTOR_L_INDEX2,
    MBVH_POSER_ACTOR_L_INDEX3,
    MBVH_POSER_ACTOR_L_MID1,
    MBVH_POSER_ACTOR_L_MID2,
    MBVH_POSER_ACTOR_L_MID3,
    MBVH_POSER_ACTOR_L_RING1,
    MBVH_POSER_ACTOR_L_RING2,
    MBVH_POSER_ACTOR_L_RING3,
    MBVH_POSER_ACTOR_L_PINKY1,
    MBVH_POSER_ACTOR_L_PINKY2,
    MBVH_POSER_ACTOR_L_PINKY3,
    MBVH_POSER_ACTOR_R_THUMB1,
    MBVH_POSER_ACTOR_R_THUMB2,
    MBVH_POSER_ACTOR_R_THUMB3,
    MBVH_POSER_ACTOR_R_INDEX1,
    MBVH_POSER_ACTOR_R_INDEX2,
    MBVH_POSER_ACTOR_R_INDEX3,
    MBVH_POSER_ACTOR_R_MID1,
    MBVH_POSER_ACTOR_R_MID2,
    MBVH_POSER_ACTOR_R_MID3,
    MBVH_POSER_ACTOR_R_RING1,
    MBVH_POSER_ACTOR_R_RING2,
    MBVH_POSER_ACTOR_R_RING3,
    MBVH_POSER_ACTOR_R_PINKY1,
    MBVH_POSER_ACTOR_R_PINKY2,
    MBVH_POSER_ACTOR_R_PINKY3,
    MBVH_POSER_ACTOR_COUNT
} mbvh_PoserActor;

typedef struct mbvh_Bounds {
    mbvh_fixed min_v[3];
    mbvh_fixed max_v[3];
    int valid;
} mbvh_Bounds;

typedef struct mbvh_PoserImportOptions {
    mbvh_fixed target_height;
    mbvh_fixed manual_scale;
    mbvh_fixed root_pretranslate[3];
    mbvh_fixed floor_y;
    int scale_automatically;
    int arm_axis;
    int drop_to_floor;
} mbvh_PoserImportOptions;


typedef struct mbvh_Context {
    mbvh_Node nodes[MBVH_MAX_NODES];
    mbvh_Channel channels[MBVH_MAX_CHANNELS];
    mbvh_fixed motion_values[MBVH_MAX_FRAMES * MBVH_MAX_CHANNELS];
    unsigned int node_count;
    unsigned int channel_count;
    unsigned int frame_count;
    mbvh_fixed frame_time;
    int root_index;
    mbvh_Result last_error;
    unsigned int error_line;
    unsigned int error_column;
    char error_token[64];
} mbvh_Context;

void mbvh_init(mbvh_Context *ctx);
mbvh_Result mbvh_parse_text(mbvh_Context *ctx, const char *text, size_t text_len);
mbvh_Result mbvh_evaluate_frame(const mbvh_Context *ctx, unsigned int frame_index, mbvh_Pose *pose);
mbvh_Result mbvh_evaluate_frame_ex(const mbvh_Context *ctx, unsigned int frame_index, const mbvh_EvalOptions *opts, mbvh_Pose *pose);
mbvh_Result mbvh_sample_time(const mbvh_Context *ctx, mbvh_fixed time_seconds, int loop, mbvh_Pose *pose);
mbvh_Result mbvh_sample_time_ex(const mbvh_Context *ctx, mbvh_fixed time_seconds, int loop, const mbvh_EvalOptions *opts, mbvh_Pose *pose);
mbvh_Result mbvh_sample_frame_q16(const mbvh_Context *ctx, mbvh_fixed frame_q16, int loop, mbvh_Pose *pose);
mbvh_Result mbvh_sample_frame_q16_ex(const mbvh_Context *ctx, mbvh_fixed frame_q16, int loop, const mbvh_EvalOptions *opts, mbvh_Pose *pose);
mbvh_Result mbvh_get_motion_value(const mbvh_Context *ctx, unsigned int frame_index, unsigned int channel_index, mbvh_fixed *out_value);
mbvh_Result mbvh_analyze(const mbvh_Context *ctx, mbvh_Stats *out_stats);
mbvh_Result mbvh_write_summary(const mbvh_Context *ctx, char *out_text, size_t out_cap, size_t *out_len);
mbvh_Result mbvh_write_bvh_text(const mbvh_Context *ctx, char *out_text, size_t out_cap, size_t *out_len);
const char *mbvh_result_name(mbvh_Result r);
const char *mbvh_channel_name(unsigned char channel_type);

void mbvh_eval_options_default(mbvh_EvalOptions *opts);
int mbvh_find_node(const mbvh_Context *ctx, const char *name);
int mbvh_node_depth(const mbvh_Context *ctx, unsigned int node_index);
int mbvh_detect_arm_axis(const mbvh_Context *ctx);
mbvh_fixed mbvh_duration(const mbvh_Context *ctx);
void mbvh_pose_translate_world(mbvh_Pose *pose, unsigned int node_count, mbvh_fixed dx, mbvh_fixed dy, mbvh_fixed dz);
mbvh_Result mbvh_pose_drop_to_floor(mbvh_Pose *pose, unsigned int node_count, mbvh_fixed floor_y);

void mbvh_retarget_clear(mbvh_RetargetMap *map);
mbvh_Result mbvh_retarget_bind_name(const mbvh_Context *ctx, mbvh_RetargetMap *map, const char *bvh_node_name, unsigned int target_index);
int mbvh_retarget_get_target(const mbvh_RetargetMap *map, unsigned int node_index);

void mbvh_quat_identity(mbvh_Quat *q);
void mbvh_quat_normalize(mbvh_Quat *q);
void mbvh_quat_mul(mbvh_Quat *out, const mbvh_Quat *a, const mbvh_Quat *b);
void mbvh_quat_from_axis_deg(mbvh_Quat *q, int axis, mbvh_fixed degrees);
void mbvh_quat_to_mat34(mbvh_Mat34 *out, const mbvh_Quat *q);
mbvh_Result mbvh_get_node_rotation_quat(const mbvh_Context *ctx, unsigned int node_index, unsigned int frame_index, mbvh_Quat *out_quat);
mbvh_Result mbvh_sample_node_rotation_quat(const mbvh_Context *ctx, unsigned int node_index, unsigned int f0, unsigned int f1, mbvh_fixed alpha, mbvh_Quat *out_quat);


void mbvh_bounds_init(mbvh_Bounds *bounds);
void mbvh_bounds_add_point(mbvh_Bounds *bounds, mbvh_fixed x, mbvh_fixed y, mbvh_fixed z);
mbvh_Result mbvh_measure_rest_bounds(const mbvh_Context *ctx, mbvh_Bounds *out_bounds);
mbvh_Result mbvh_measure_pose_bounds(const mbvh_Pose *pose, unsigned int node_count, mbvh_Bounds *out_bounds);
mbvh_fixed mbvh_bounds_height_y(const mbvh_Bounds *bounds);
mbvh_Result mbvh_compute_scale_to_height(const mbvh_Context *ctx, mbvh_fixed target_height, mbvh_fixed *out_scale);

void mbvh_poser_import_options_default(mbvh_PoserImportOptions *opts);
mbvh_Result mbvh_poser_make_eval_options(const mbvh_Context *ctx, const mbvh_PoserImportOptions *opts, mbvh_EvalOptions *out_eval, int *out_arm_axis);
const char *mbvh_poser_actor_name(int actor_index);
int mbvh_poser_guess_actor(const char *bvh_node_name);
mbvh_Result mbvh_poser_build_retarget_map(const mbvh_Context *ctx, mbvh_RetargetMap *map);
mbvh_Result mbvh_write_poser_report(const mbvh_Context *ctx, char *out_text, size_t out_cap, size_t *out_len);

int mbvh_find_channel(const mbvh_Context *ctx, unsigned int node_index, unsigned char channel_type);
int mbvh_node_rotation_order(const mbvh_Context *ctx, unsigned int node_index, char out_order[4]);
mbvh_Result mbvh_bake_root_translation(mbvh_Context *ctx, mbvh_fixed dx, mbvh_fixed dy, mbvh_fixed dz);
mbvh_Result mbvh_bake_root_motion_origin(mbvh_Context *ctx, unsigned int frame_index, int keep_y);
mbvh_Result mbvh_bake_drop_to_floor_y(mbvh_Context *ctx, mbvh_fixed floor_y, mbvh_Pose *scratch_pose);

mbvh_fixed mbvh_fixed_from_int(int v);
mbvh_fixed mbvh_fixed_from_milli(long milli);
int mbvh_fixed_to_int_floor(mbvh_fixed v);
long mbvh_fixed_to_milli(mbvh_fixed v);
mbvh_fixed mbvh_fixed_mul(mbvh_fixed a, mbvh_fixed b);
mbvh_fixed mbvh_fixed_div(mbvh_fixed a, mbvh_fixed b);
mbvh_fixed mbvh_fixed_lerp(mbvh_fixed a, mbvh_fixed b, mbvh_fixed alpha);
mbvh_fixed mbvh_sin_deg(mbvh_fixed degrees_q16);
mbvh_fixed mbvh_cos_deg(mbvh_fixed degrees_q16);

#ifdef __cplusplus
}
#endif

#endif
