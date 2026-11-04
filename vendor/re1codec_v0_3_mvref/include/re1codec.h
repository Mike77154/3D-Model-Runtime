#ifndef RE1CODEC_H
#define RE1CODEC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char re1c_u8;
typedef signed char re1c_i8;
typedef unsigned short re1c_u16;
typedef signed short re1c_i16;
typedef unsigned int re1c_u32;
typedef signed int re1c_i32;

#define RE1C_OK                 0
#define RE1C_ERR_ARGUMENT      -1
#define RE1C_ERR_HOST          -2
#define RE1C_ERR_FORMAT        -3
#define RE1C_ERR_RANGE         -4
#define RE1C_ERR_CAPACITY      -5
#define RE1C_ERR_MISMATCH      -6
#define RE1C_ERR_UNSUPPORTED   -7
#define RE1C_ERR_NOT_PRESENT   -8

#define RE1C_SECTION_COUNT 6u
#define RE1C_BANK_A 0u
#define RE1C_BANK_B 1u
#define RE1C_Q16_ONE 65536
#define RE1C_MV_FRAME_MS 40u
#define RE1C_MV_FPS 25u

#define RE1C_MODEL_EMD 1u
#define RE1C_MODEL_EMW 2u
#define RE1C_MODEL_TMD 3u

#define RE1C_SEC_EMR_A 0u
#define RE1C_SEC_EDD_A 1u
#define RE1C_SEC_EMR_B 2u
#define RE1C_SEC_EDD_B 3u
#define RE1C_SEC_TMD   4u
#define RE1C_SEC_TIM   5u

#define RE1C_PRIM_FT3_A 0x34000609u
#define RE1C_PRIM_FT3_B 0x30000609u
#define RE1C_PRIM_FT3_C 0x24000609u
#define RE1C_PRIM_FT4_A 0x3C00080Cu
#define RE1C_PRIM_FT4_B 0x3800080Cu
#define RE1C_PRIM_FT4_C 0x2C00080Cu

typedef struct re1c_section_s {
    const re1c_u8 *data;
    re1c_u32 size;
    re1c_u32 source_offset;
} re1c_section;

typedef struct re1c_view_s {
    const re1c_u8 *file_data;
    re1c_u32 file_size;
    re1c_section section[RE1C_SECTION_COUNT];
    re1c_u32 section_count;
} re1c_view;

typedef struct re1c_bank_info_s {
    re1c_u16 part_list_offset;
    re1c_u16 animation_offset;
    re1c_u16 object_count;
    re1c_u16 frame_size;
    re1c_u32 pose_count;
    re1c_u32 clip_count;
    re1c_u32 emd_type;
} re1c_bank_info;

typedef struct re1c_pivot_s {
    re1c_i32 x_centi;
    re1c_i32 y_centi;
    re1c_i32 z_centi;
    re1c_i16 parent;
} re1c_pivot;

typedef struct re1c_clip_s {
    re1c_u16 frame_count;
    re1c_u16 stream_offset;
} re1c_clip;

typedef struct re1c_frame_ref_s {
    re1c_u16 pose_index;
    re1c_u16 unknown_word;
    re1c_u16 step_units; /* deprecated v0.2 alias; mirrors unknown_word */
} re1c_frame_ref;

typedef struct re1c_pose_root_s {
    re1c_i32 x_centi;
    re1c_i32 y_centi;
    re1c_i32 z_centi;
} re1c_pose_root;

typedef struct re1c_pose_angle_s {
    re1c_u16 file_ax12;
    re1c_u16 file_ay12;
    re1c_u16 file_az12;
    re1c_i16 render_x12;
    re1c_i16 render_y12;
    re1c_i16 render_z12;
} re1c_pose_angle;

typedef struct re1c_tmd_object_s {
    re1c_u32 vertex_offset;
    re1c_u32 vertex_count;
    re1c_u32 normal_offset;
    re1c_u32 normal_count;
    re1c_u32 primitive_offset;
    re1c_u32 primitive_count;
    re1c_u32 scale_word;
} re1c_tmd_object;

typedef struct re1c_vertex_raw_s {
    re1c_u16 file_x;
    re1c_u16 file_y;
    re1c_u16 file_z;
    re1c_u16 pad;
} re1c_vertex_raw;

typedef struct re1c_vec3_centi_s {
    re1c_i32 x;
    re1c_i32 y;
    re1c_i32 z;
} re1c_vec3_centi;

typedef struct re1c_primitive_s {
    re1c_u32 code;
    re1c_u8 sides;
    re1c_u8 u[4];
    re1c_u8 v[4];
    re1c_u16 cba;
    re1c_u16 tsb;
    re1c_u16 normal_index[4];
    re1c_u16 vertex_index[4];
    const re1c_u8 *raw_packet;
    re1c_u32 raw_packet_size;
} re1c_primitive;

typedef struct re1c_tim_info_s {
    re1c_u32 pixel_mode;
    re1c_u32 has_clut;
    re1c_u16 clut_x;
    re1c_u16 clut_y;
    re1c_u16 clut_w;
    re1c_u16 clut_h;
    re1c_u16 pixel_x;
    re1c_u16 pixel_y;
    re1c_u16 pixel_w_words;
    re1c_u16 pixel_h;
    re1c_u32 width_pixels;
    re1c_u32 height_pixels;
    const re1c_u8 *clut_data;
    re1c_u32 clut_bytes;
    const re1c_u8 *pixel_data;
    re1c_u32 pixel_bytes;
} re1c_tim_info;

typedef struct re1c_stats_s {
    re1c_u32 object_count;
    re1c_u32 vertex_count;
    re1c_u32 normal_count;
    re1c_u32 primitive_count;
    re1c_u32 supported_primitive_count;
    re1c_u32 unsupported_primitive_count;
    re1c_u32 bank_a_clips;
    re1c_u32 bank_a_poses;
    re1c_u32 bank_b_clips;
    re1c_u32 bank_b_poses;
    re1c_u32 texture_width;
    re1c_u32 texture_height;
    re1c_u32 bank_a_present;
    re1c_u32 bank_b_present;
    re1c_u32 bank_a_object_count;
    re1c_u32 bank_b_object_count;
    re1c_u32 texture_present;
} re1c_stats;

typedef struct re1c_compare_s {
    re1c_u32 size_a;
    re1c_u32 size_b;
    re1c_u32 first_diff;
    re1c_u8 byte_a;
    re1c_u8 byte_b;
} re1c_compare;

int re1c_check_host(void);
const char *re1c_result_string(int code);
const char *re1c_texture_locator_name(void);

int re1c_open_emd(const re1c_u8 *data, re1c_u32 size, re1c_view *out_view);
int re1c_open_emw(const re1c_u8 *data, re1c_u32 size, re1c_view *out_view);
int re1c_open_tmd(const re1c_u8 *data, re1c_u32 size, re1c_view *out_view);
int re1c_open_model(const re1c_u8 *data, re1c_u32 size, re1c_view *out_view, re1c_u32 *out_model_kind);
int re1c_open_sections(const re1c_section sections[RE1C_SECTION_COUNT], re1c_view *out_view);
int re1c_validate(const re1c_view *view, re1c_stats *out_stats);
int re1c_has_section(const re1c_view *view, re1c_u32 section_index, re1c_u32 *out_present);
int re1c_bank_available(const re1c_view *view, re1c_u32 bank, re1c_u32 *out_present);
int re1c_get_preferred_bank(const re1c_view *view, re1c_u32 *out_bank);

re1c_i32 re1c_position_word_to_centi(re1c_u16 word);
re1c_i32 re1c_centi_to_q16(re1c_i32 centi);
re1c_i32 re1c_angle12_to_turn_q16(re1c_i16 angle12);

int re1c_get_bank_info(const re1c_view *view, re1c_u32 bank, re1c_bank_info *out_info);
int re1c_get_pivot(const re1c_view *view, re1c_u32 bank, re1c_u32 object_index, re1c_pivot *out_pivot);
int re1c_get_bind_global_centi(const re1c_view *view, re1c_u32 bank, re1c_u32 object_index, re1c_vec3_centi *out_position);
int re1c_get_child_count(const re1c_view *view, re1c_u32 bank, re1c_u32 object_index, re1c_u16 *out_count);
int re1c_get_child(const re1c_view *view, re1c_u32 bank, re1c_u32 object_index, re1c_u32 child_index, re1c_u8 *out_child);
int re1c_get_clip(const re1c_view *view, re1c_u32 bank, re1c_u32 clip_index, re1c_clip *out_clip);
int re1c_get_frame_ref(const re1c_view *view, re1c_u32 bank, re1c_u32 clip_index, re1c_u32 frame_index, re1c_frame_ref *out_ref);
int re1c_get_mv_frame_duration_ms(re1c_u32 *out_ms);
int re1c_get_clip_duration_ms(const re1c_view *view, re1c_u32 bank, re1c_u32 clip_index, re1c_u32 *out_ms);
int re1c_get_pose_root(const re1c_view *view, re1c_u32 bank, re1c_u32 pose_index, re1c_pose_root *out_root);
int re1c_get_pose_angle(const re1c_view *view, re1c_u32 bank, re1c_u32 pose_index, re1c_u32 object_index, re1c_pose_angle *out_angle);

int re1c_get_tmd_object_count(const re1c_view *view, re1c_u32 *out_count);
int re1c_get_tmd_object(const re1c_view *view, re1c_u32 object_index, re1c_tmd_object *out_object);
int re1c_get_vertex_raw(const re1c_view *view, re1c_u32 object_index, re1c_u32 vertex_index, re1c_vertex_raw *out_vertex);
int re1c_get_vertex_centi(const re1c_view *view, re1c_u32 object_index, re1c_u32 vertex_index, re1c_vec3_centi *out_vertex);
int re1c_get_normal_raw(const re1c_view *view, re1c_u32 object_index, re1c_u32 normal_index, re1c_vertex_raw *out_normal);
int re1c_get_primitive(const re1c_view *view, re1c_u32 object_index, re1c_u32 primitive_index, re1c_primitive *out_primitive);

int re1c_get_tim_info(const re1c_view *view, re1c_tim_info *out_info);
int re1c_get_primitive_page_xy(const re1c_tim_info *tim, const re1c_primitive *prim, re1c_i32 *out_page_x, re1c_i32 *out_page_y);
int re1c_get_primitive_clut_xy(const re1c_primitive *prim, re1c_i32 *out_clut_x, re1c_i32 *out_clut_y);
int re1c_tim_sample_index(const re1c_tim_info *tim, const re1c_primitive *prim, re1c_u8 u, re1c_u8 v, re1c_u16 *out_psx1555);
void re1c_psx1555_to_rgba8(re1c_u16 color, re1c_u8 rgba[4]);

int re1c_canonical_build(const re1c_view *view, re1c_u8 *out_bytes, re1c_u32 capacity, re1c_u32 *out_size);
re1c_u32 re1c_fnv1a32(const re1c_u8 *data, re1c_u32 size);
int re1c_compare_canonical(const re1c_u8 *a, re1c_u32 size_a,
                           const re1c_u8 *b, re1c_u32 size_b,
                           re1c_compare *out_compare);
int re1c_compare_views(const re1c_view *a, const re1c_view *b,
                       re1c_u8 *scratch_a, re1c_u8 *scratch_b,
                       re1c_u32 scratch_capacity, re1c_compare *out_compare);

#ifdef __cplusplus
}
#endif

#endif
