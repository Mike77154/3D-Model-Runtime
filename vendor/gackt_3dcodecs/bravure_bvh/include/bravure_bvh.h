#ifndef BRAVURE_BVH_H
#define BRAVURE_BVH_H

/*
    bravure_bvh - fixed-point BioVision BVH codec.
    C89 core: static/caller-owned storage and integer Q16.16 math.

    The caller owns all memory by declaring bbvh_document / bbvh_pose.
    Increase/decrease limits before including this header if needed:

      #define BRAVURE_BVH_MAX_NODES 256
      #define BRAVURE_BVH_MAX_CHANNELS 768
      #define BRAVURE_BVH_MAX_FRAMES 2048
      #include "bravure_bvh.h"
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BRAVURE_BVH_MAX_NODES
#define BRAVURE_BVH_MAX_NODES 128
#endif

#ifndef BRAVURE_BVH_MAX_CHANNELS
#define BRAVURE_BVH_MAX_CHANNELS 384
#endif

#ifndef BRAVURE_BVH_MAX_FRAMES
#define BRAVURE_BVH_MAX_FRAMES 512
#endif

#ifndef BRAVURE_BVH_MAX_NAME
#define BRAVURE_BVH_MAX_NAME 64
#endif

#ifndef BRAVURE_BVH_MAX_NODE_CHANNELS
#define BRAVURE_BVH_MAX_NODE_CHANNELS 16
#endif

#ifndef BRAVURE_BVH_MAX_TOKEN
#define BRAVURE_BVH_MAX_TOKEN 128
#endif

#ifndef BRAVURE_BVH_MAX_DEPTH
#define BRAVURE_BVH_MAX_DEPTH 64
#endif

#define BRAVURE_BVH_VERSION_MAJOR 0
#define BRAVURE_BVH_VERSION_MINOR 4
#define BRAVURE_BVH_VERSION_PATCH 0

#define BBVH_FIXED_SHIFT 16
#define BBVH_FIXED_ONE ((bbvh_fixed)65536L)
#define BBVH_FIXED_HALF ((bbvh_fixed)32768L)
#define BBVH_FIXED_MAX ((bbvh_fixed)2147483647L)
#define BBVH_FIXED_MIN ((bbvh_fixed)(-2147483647L - 1L))

typedef long bbvh_fixed;

typedef enum bbvh_result_e {
    BBVH_OK = 0,
    BBVH_ERR_NULL = -1,
    BBVH_ERR_EOF = -2,
    BBVH_ERR_TOKEN_TOO_LONG = -3,
    BBVH_ERR_UNEXPECTED_TOKEN = -4,
    BBVH_ERR_BAD_NUMBER = -5,
    BBVH_ERR_BAD_CHANNEL = -6,
    BBVH_ERR_TOO_MANY_NODES = -7,
    BBVH_ERR_TOO_MANY_CHANNELS = -8,
    BBVH_ERR_TOO_MANY_NODE_CHANNELS = -9,
    BBVH_ERR_TOO_MANY_FRAMES = -10,
    BBVH_ERR_TOO_DEEP = -11,
    BBVH_ERR_RANGE = -12,
    BBVH_ERR_OUTPUT_FULL = -13,
    BBVH_ERR_BAD_ARGUMENT = -14,
    BBVH_ERR_BAD_DOCUMENT = -15
} bbvh_result;

typedef enum bbvh_channel_type_e {
    BBVH_CH_XPOSITION = 0,
    BBVH_CH_YPOSITION = 1,
    BBVH_CH_ZPOSITION = 2,
    BBVH_CH_XROTATION = 3,
    BBVH_CH_YROTATION = 4,
    BBVH_CH_ZROTATION = 5,
    BBVH_CH_UNKNOWN = 255
} bbvh_channel_type;

typedef struct bbvh_node_s {
    char name[BRAVURE_BVH_MAX_NAME];
    int parent;
    int first_child;
    int next_sibling;
    int depth;

    bbvh_fixed offset[3];
    int channel_start;
    int channel_count;
    unsigned char channel_types[BRAVURE_BVH_MAX_NODE_CHANNELS];

    int has_end_site;
    bbvh_fixed end_offset[3];
} bbvh_node;

typedef struct bbvh_document_s {
    int node_count;
    int root_index;
    int channel_count;
    int frame_count;
    bbvh_fixed frame_time;

    bbvh_node nodes[BRAVURE_BVH_MAX_NODES];
    unsigned char channel_types[BRAVURE_BVH_MAX_CHANNELS];
    int channel_nodes[BRAVURE_BVH_MAX_CHANNELS];
    bbvh_fixed motion[BRAVURE_BVH_MAX_FRAMES][BRAVURE_BVH_MAX_CHANNELS];

    int error_line;
    int error_col;
    char error_token[BRAVURE_BVH_MAX_TOKEN];
} bbvh_document;

typedef struct bbvh_mat4_s {
    bbvh_fixed m[16];
} bbvh_mat4;

typedef struct bbvh_pose_s {
    bbvh_mat4 world[BRAVURE_BVH_MAX_NODES];
    bbvh_fixed joint_position[BRAVURE_BVH_MAX_NODES][3];
} bbvh_pose;

typedef struct bbvh_quat_s {
    bbvh_fixed x;
    bbvh_fixed y;
    bbvh_fixed z;
    bbvh_fixed w;
} bbvh_quat;

typedef struct bbvh_segment_s {
    int parent_node;
    int child_node;
    int is_end_site;
    bbvh_fixed a[3];
    bbvh_fixed b[3];
} bbvh_segment;

typedef struct bbvh_axis_map_s {
    signed char src_axis_for_dst[3];
    signed char sign_for_dst[3];
} bbvh_axis_map;

typedef struct bbvh_info_s {
    int node_count;
    int channel_count;
    int frame_count;
    int end_site_count;
    int max_depth;
    bbvh_fixed frame_time;
    bbvh_fixed duration;
} bbvh_info;

/* fixed-point helpers */
bbvh_fixed bbvh_fixed_from_int(long v);
bbvh_fixed bbvh_fixed_add_sat(bbvh_fixed a, bbvh_fixed b);
bbvh_fixed bbvh_fixed_sub_sat(bbvh_fixed a, bbvh_fixed b);
bbvh_fixed bbvh_fixed_abs_sat(bbvh_fixed a);
bbvh_fixed bbvh_fixed_mul(bbvh_fixed a, bbvh_fixed b);
bbvh_fixed bbvh_fixed_div_int(bbvh_fixed a, long b);
bbvh_fixed bbvh_fixed_div(bbvh_fixed a, bbvh_fixed b);
bbvh_fixed bbvh_fixed_sqrt(bbvh_fixed value);
bbvh_fixed bbvh_fixed_ratio_clamped(bbvh_fixed numerator, bbvh_fixed denominator);
bbvh_fixed bbvh_fixed_lerp(bbvh_fixed a, bbvh_fixed b, bbvh_fixed t_q16);
bbvh_fixed bbvh_fixed_sin_deg(bbvh_fixed degrees_q16);
bbvh_fixed bbvh_fixed_cos_deg(bbvh_fixed degrees_q16);
int bbvh_fixed_from_ascii(const char *text, bbvh_fixed *out_value);
int bbvh_fixed_to_ascii(bbvh_fixed value, char *out_text, int out_cap, int decimals);
long bbvh_fixed_floor_int(bbvh_fixed value);
bbvh_fixed bbvh_vec3_length(const bbvh_fixed xyz[3]);


/* matrix helpers */
void bbvh_mat_identity(bbvh_mat4 *m);
void bbvh_mat_mul(bbvh_mat4 *out_m, const bbvh_mat4 *a, const bbvh_mat4 *b);
void bbvh_mat_translation(bbvh_mat4 *m, bbvh_fixed x, bbvh_fixed y, bbvh_fixed z);
void bbvh_mat_rotation_x(bbvh_mat4 *m, bbvh_fixed degrees_q16);
void bbvh_mat_rotation_y(bbvh_mat4 *m, bbvh_fixed degrees_q16);
void bbvh_mat_rotation_z(bbvh_mat4 *m, bbvh_fixed degrees_q16);
void bbvh_mat_apply_point(const bbvh_mat4 *m, bbvh_fixed x, bbvh_fixed y,
                          bbvh_fixed z, bbvh_fixed out_xyz[3]);

/* quaternion helpers */
void bbvh_quat_identity(bbvh_quat *q);
void bbvh_quat_normalize(bbvh_quat *q);
void bbvh_quat_mul(bbvh_quat *out_q, const bbvh_quat *a, const bbvh_quat *b);
void bbvh_quat_from_axis_deg(bbvh_quat *q, int axis, bbvh_fixed degrees_q16);
bbvh_fixed bbvh_quat_dot(const bbvh_quat *a, const bbvh_quat *b);
void bbvh_quat_lerp_shortest(bbvh_quat *out_q, const bbvh_quat *a,
                             const bbvh_quat *b, bbvh_fixed t_q16);
void bbvh_quat_to_mat4(bbvh_mat4 *out_m, const bbvh_quat *q);
int bbvh_get_node_rotation_quat(const bbvh_document *doc, int frame_index,
                                int node_index, bbvh_quat *out_q);
int bbvh_get_node_rotation_quat_lerp_frames(const bbvh_document *doc,
                                            int frame_a, int frame_b,
                                            bbvh_fixed t_q16, int node_index,
                                            bbvh_quat *out_q);

/* motion/frame helpers */
bbvh_fixed bbvh_get_last_frame_time(const bbvh_document *doc);
int bbvh_node_find_channel(const bbvh_document *doc, int node_index,
                           int channel_type, int occurrence);
int bbvh_get_frame_channel_value(const bbvh_document *doc, int frame_index,
                                 int channel_index, bbvh_fixed *out_value);
int bbvh_set_frame_channel_value(bbvh_document *doc, int frame_index,
                                 int channel_index, bbvh_fixed value);
int bbvh_sample_channel_lerp_frames(const bbvh_document *doc, int frame_a,
                                    int frame_b, bbvh_fixed t_q16,
                                    int channel_index, bbvh_fixed *out_value);
int bbvh_sample_channel_at_time(const bbvh_document *doc, bbvh_fixed time_seconds,
                                int loop, int channel_index, bbvh_fixed *out_value);
int bbvh_resample_uniform(const bbvh_document *src, bbvh_document *dst,
                          int new_frame_count);
int bbvh_compute_channel_range(const bbvh_document *doc, int channel_index,
                               bbvh_fixed *out_min, bbvh_fixed *out_max);
int bbvh_compute_all_channel_ranges(const bbvh_document *doc,
                                    bbvh_fixed out_min[BRAVURE_BVH_MAX_CHANNELS],
                                    bbvh_fixed out_max[BRAVURE_BVH_MAX_CHANNELS]);
int bbvh_compute_motion_bounds(const bbvh_document *doc, int include_end_sites,
                               bbvh_pose *scratch_pose, bbvh_fixed out_min_xyz[3],
                               bbvh_fixed out_max_xyz[3]);

/* skeleton mapping / segment extraction */
int bbvh_extract_pose_segments(const bbvh_document *doc, const bbvh_pose *pose,
                               int include_end_sites, bbvh_segment *out_segments,
                               int max_segments, int *out_count);
int bbvh_build_name_map(const bbvh_document *src, const bbvh_document *dst,
                        int out_src_to_dst[BRAVURE_BVH_MAX_NODES]);
int bbvh_transfer_frame_by_name(const bbvh_document *src, int src_frame,
                                bbvh_document *dst, int dst_frame,
                                int copy_position_channels,
                                int copy_rotation_channels);

/* axis vector remapping helpers */
void bbvh_axis_map_identity(bbvh_axis_map *map);
int bbvh_axis_map_set(bbvh_axis_map *map, int dst_axis, int src_axis, int sign);
int bbvh_axis_map_apply_vec3(const bbvh_axis_map *map, const bbvh_fixed in_xyz[3],
                             bbvh_fixed out_xyz[3]);
int bbvh_axis_map_apply_pose_positions(const bbvh_axis_map *map, bbvh_pose *pose,
                                       int node_count);


/* time / motion editing helpers */
int bbvh_build_pose_at_time_loop(const bbvh_document *doc, bbvh_fixed time_seconds,
                                 int loop, bbvh_pose *pose);
int bbvh_build_pose_quat_lerp_frames(const bbvh_document *doc, int frame_a,
                                     int frame_b, bbvh_fixed t_q16,
                                     bbvh_pose *pose);
int bbvh_build_pose_quat_at_time(const bbvh_document *doc, bbvh_fixed time_seconds,
                                 int loop, bbvh_pose *pose);
int bbvh_trim_motion(const bbvh_document *src, bbvh_document *dst,
                     int first_frame, int frame_count);
int bbvh_reverse_motion(const bbvh_document *src, bbvh_document *dst);
int bbvh_smooth_motion_box(const bbvh_document *src, bbvh_document *dst,
                           int radius, int preserve_first_last);
int bbvh_scale_document(const bbvh_document *src, bbvh_document *dst,
                        bbvh_fixed scale_q16, int scale_offsets,
                        int scale_position_channels);
int bbvh_recenter_root_translation(bbvh_document *doc, int base_frame,
                                   int axes_mask);
int bbvh_close_loop_tail_blend(const bbvh_document *src, bbvh_document *dst,
                               int blend_frame_count);
int bbvh_estimate_rest_height_axis(const bbvh_document *doc, int up_axis,
                                   int include_end_sites, bbvh_pose *scratch_pose,
                                   bbvh_fixed *out_height);
int bbvh_retarget_motion_by_name_scaled(const bbvh_document *src_motion,
                                        const bbvh_document *target_skeleton,
                                        bbvh_document *dst,
                                        bbvh_fixed position_scale,
                                        int copy_root_position,
                                        int copy_rotations);
int bbvh_compute_node_velocity(const bbvh_document *doc, int frame_a, int frame_b,
                               int node_index, bbvh_pose *scratch_a,
                               bbvh_pose *scratch_b, bbvh_fixed out_velocity[3]);
int bbvh_detect_stationary_contact(const bbvh_document *doc, int frame_index,
                                   int node_index, int up_axis,
                                   bbvh_fixed max_height,
                                   bbvh_fixed max_speed,
                                   bbvh_pose *scratch_a,
                                   bbvh_pose *scratch_b,
                                   int *out_contact);

/* debug / interchange text emitters */
int bbvh_write_skeleton_dot(const bbvh_document *doc,
                            char *out_text, long out_cap, long *out_length);
int bbvh_write_motion_csv(const bbvh_document *doc,
                          char *out_text, long out_cap, long *out_length);

/* document codec */
void bbvh_init(bbvh_document *doc);
int bbvh_parse_text(bbvh_document *doc, const char *text, long length);
int bbvh_write_text(const bbvh_document *doc, char *out_text, long out_cap, long *out_length);
const char *bbvh_result_string(int code);
const char *bbvh_channel_name(int channel_type);

/* validation / discovery */
int bbvh_validate(const bbvh_document *doc);
int bbvh_get_info(const bbvh_document *doc, bbvh_info *out_info);
int bbvh_find_node(const bbvh_document *doc, const char *name);
int bbvh_count_end_sites(const bbvh_document *doc);
bbvh_fixed bbvh_get_duration(const bbvh_document *doc);

/* pose evaluation */
void bbvh_pose_clear(bbvh_pose *pose);
int bbvh_build_rest_pose(const bbvh_document *doc, bbvh_pose *pose);
int bbvh_build_pose(const bbvh_document *doc, int frame_index, bbvh_pose *pose);
int bbvh_build_pose_lerp_frames(const bbvh_document *doc, int frame_a, int frame_b,
                                bbvh_fixed t_q16, bbvh_pose *pose);
int bbvh_build_pose_at_time(const bbvh_document *doc, bbvh_fixed time_seconds,
                            bbvh_pose *pose);
int bbvh_get_node_position(const bbvh_document *doc, const bbvh_pose *pose,
                           int node_index, bbvh_fixed out_xyz[3]);
int bbvh_get_end_site_position(const bbvh_document *doc, const bbvh_pose *pose,
                               int node_index, bbvh_fixed out_xyz[3]);
int bbvh_compute_pose_bounds(const bbvh_document *doc, const bbvh_pose *pose,
                             int include_end_sites,
                             bbvh_fixed out_min_xyz[3], bbvh_fixed out_max_xyz[3]);

/* debug / visualization text emitters */
int bbvh_write_pose_obj(const bbvh_document *doc, const bbvh_pose *pose,
                        int include_end_sites,
                        char *out_text, long out_cap, long *out_length);

#ifdef __cplusplus
}
#endif

#endif
