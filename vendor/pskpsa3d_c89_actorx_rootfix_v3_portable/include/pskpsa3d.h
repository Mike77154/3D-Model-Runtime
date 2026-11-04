#ifndef PSKPSA3D_H
#define PSKPSA3D_H

#include <limits.h> /* C89 compile-time integer width selection */

/*
   pskpsa3d - tiny ActorX PSK/PSA decoder
   C89, no heap allocation, no float/double types in the public ABI.
   All real-valued fields are converted from binary IEEE-754 float32 to Q16.16.

   Define PSK3D_*_MAX before including this header to tune static capacities.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PSK3D_MAX_POINTS
#define PSK3D_MAX_POINTS 8192u
#endif

#ifndef PSK3D_MAX_WEDGES
#define PSK3D_MAX_WEDGES 16384u
#endif

#ifndef PSK3D_MAX_FACES
#define PSK3D_MAX_FACES 8192u
#endif

#ifndef PSK3D_MAX_MATERIALS
#define PSK3D_MAX_MATERIALS 128u
#endif

#ifndef PSK3D_MAX_BONES
#define PSK3D_MAX_BONES 256u
#endif

#ifndef PSK3D_MAX_INFLUENCES
#define PSK3D_MAX_INFLUENCES 32768u
#endif

#ifndef PSK3D_MAX_SEQUENCES
#define PSK3D_MAX_SEQUENCES 128u
#endif

#ifndef PSK3D_MAX_KEYS
#define PSK3D_MAX_KEYS 65536u
#endif

#ifndef PSK3D_MAX_CHUNKS
#define PSK3D_MAX_CHUNKS 64u
#endif

#ifndef PSK3D_MAX_EXTRA_UV_SETS
#define PSK3D_MAX_EXTRA_UV_SETS 4u
#endif

#ifndef PSK3D_MAX_MORPH_TARGETS
#define PSK3D_MAX_MORPH_TARGETS 128u
#endif

#ifndef PSK3D_MAX_MORPH_DELTAS
#define PSK3D_MAX_MORPH_DELTAS 65536u
#endif

#ifndef PSK3D_MAX_MORPH_LAYERS
#define PSK3D_MAX_MORPH_LAYERS 16u
#endif

#ifndef PSK3D_MAX_SCALE_KEYS
#define PSK3D_MAX_SCALE_KEYS PSK3D_MAX_KEYS
#endif

#ifndef PSK3D_MAX_ANIM_LAYERS
#define PSK3D_MAX_ANIM_LAYERS 8u
#endif

#ifndef PSK3D_MAX_TRI_VERTS
#define PSK3D_MAX_TRI_VERTS (PSK3D_MAX_FACES * 3u)
#endif

#define PSK3D_NAME_BYTES 64u
#define PSK3D_CHUNK_ID_BYTES 20u
#define PSK3D_U32_INVALID 4294967295u

#define PSK3D_Q16_ONE 65536
#define PSK3D_Q16_HALF 32768
#define PSK3D_Q16_PI 205887
#define PSK3D_Q16_HALF_PI 102944
#define PSK3D_Q16_MAX 2147483647
#define PSK3D_Q16_MIN (-2147483647)

#define PSK3D_BONEMAP_CASE_INSENSITIVE 1u
#define PSK3D_BONEMAP_ROOT_FALLBACK 2u
#define PSK3D_BONEMAP_ALLOW_MISSING 4u

/* ActorX's byte formats require exact 8/16/32-bit integers, not just
   minimum widths. Fail at compile time on unsupported targets.
   Prefer long on Win32/Win64 (original public ABI), but select int on
   LP64 (Linux/macOS x86-64) where long would otherwise be 64 bits.
   This uses only <limits.h>, available in strict ANSI C89. */
#if CHAR_BIT != 8 || UCHAR_MAX != 255 || SCHAR_MAX != 127
#error "pskpsa3d requires 8-bit bytes"
#endif
#if USHRT_MAX != 65535U || SHRT_MAX != 32767
#error "pskpsa3d requires 16-bit short"
#endif

typedef unsigned char psk3d_u8;
typedef signed char psk3d_i8;
typedef unsigned short psk3d_u16;
typedef short psk3d_i16;

#if ULONG_MAX == 4294967295UL && LONG_MAX == 2147483647 && LONG_MIN == (-2147483647 - 1L)
typedef unsigned long psk3d_u32;
typedef long psk3d_i32;
#define PSK3D_I32_STORAGE_LONG 1
#elif UINT_MAX == 4294967295UL && INT_MAX == 2147483647 && INT_MIN == (-2147483647 - 1)
typedef unsigned int psk3d_u32;
typedef int psk3d_i32;
#define PSK3D_I32_STORAGE_LONG 0
#else
#error "pskpsa3d requires an exact 32-bit signed/unsigned int or long"
#endif

/* Compile-time ABI checks: 1 byte = 8 bits on supported targets. */
typedef char psk3d_abi_u8_1[(sizeof(psk3d_u8) == 1u) ? 1 : -1];
typedef char psk3d_abi_u16_2[(sizeof(psk3d_u16) == 2u) ? 1 : -1];
typedef char psk3d_abi_u32_4[(sizeof(psk3d_u32) == 4u) ? 1 : -1];
typedef char psk3d_abi_i32_4[(sizeof(psk3d_i32) == 4u) ? 1 : -1];

typedef enum psk3d_result_e {
    PSK3D_OK = 0,
    PSK3D_ERR_NULL = 1,
    PSK3D_ERR_TYPESIZE = 2,
    PSK3D_ERR_EOF = 3,
    PSK3D_ERR_BAD_MAGIC = 4,
    PSK3D_ERR_BAD_SIZE = 5,
    PSK3D_ERR_CAPACITY = 6,
    PSK3D_ERR_BAD_INDEX = 7,
    PSK3D_ERR_UNSUPPORTED = 8,
    PSK3D_ERR_OVERFLOW = 9
} psk3d_result;

typedef enum psk3d_rot_interp_e {
    PSK3D_ROT_NLERP = 0,
    PSK3D_ROT_SLERP = 1
} psk3d_rot_interp;

typedef enum psk3d_blend_mode_e {
    PSK3D_BLEND_OVERRIDE = 0,
    PSK3D_BLEND_ADDITIVE = 1
} psk3d_blend_mode;

typedef struct psk3d_vec3_s {
    psk3d_i32 x;
    psk3d_i32 y;
    psk3d_i32 z;
} psk3d_vec3;

typedef struct psk3d_quat_s {
    psk3d_i32 x;
    psk3d_i32 y;
    psk3d_i32 z;
    psk3d_i32 w;
} psk3d_quat;

typedef struct psk3d_uv_s {
    psk3d_i32 u;
    psk3d_i32 v;
} psk3d_uv;

typedef struct psk3d_color_s {
    psk3d_u8 r;
    psk3d_u8 g;
    psk3d_u8 b;
    psk3d_u8 a;
} psk3d_color;

typedef struct psk3d_chunk_s {
    char id[PSK3D_CHUNK_ID_BYTES + 1u];
    psk3d_u32 type_flags;
    psk3d_u32 data_size;
    psk3d_u32 data_count;
    psk3d_u32 payload_offset;
} psk3d_chunk;

typedef struct psk3d_report_s {
    psk3d_u32 chunks_seen;
    psk3d_u32 unknown_chunks;
    psk3d_u32 last_chunk_index;
    psk3d_u32 last_chunk_offset;
    psk3d_result last_error;
    char last_chunk_id[PSK3D_CHUNK_ID_BYTES + 1u];
    char error_text[96];
    psk3d_chunk chunks[PSK3D_MAX_CHUNKS];
} psk3d_report;

typedef struct psk3d_point_s {
    psk3d_vec3 p;
} psk3d_point;

typedef struct psk3d_wedge_s {
    psk3d_u32 point_index;
    psk3d_i32 u;
    psk3d_i32 v;
    psk3d_u8 mat_index;
    psk3d_u8 reserved;
} psk3d_wedge;

typedef struct psk3d_face_s {
    psk3d_u32 wedge[3];
    psk3d_u8 mat_index;
    psk3d_u8 aux_mat_index;
    psk3d_u32 smoothing_groups;
} psk3d_face;

typedef struct psk3d_material_s {
    char name[PSK3D_NAME_BYTES + 1u];
    psk3d_u32 texture_index;
    psk3d_u32 poly_flags;
    psk3d_u32 aux_material;
    psk3d_u32 aux_flags;
    psk3d_i32 lod_bias;
    psk3d_i32 lod_style;
} psk3d_material;

typedef struct psk3d_bone_s {
    char name[PSK3D_NAME_BYTES + 1u];
    psk3d_u32 flags;
    psk3d_i32 num_children;
    psk3d_i32 parent_index;
    psk3d_quat orientation;
    psk3d_vec3 position;
    psk3d_i32 length;
    psk3d_vec3 size;
} psk3d_bone;

typedef struct psk3d_influence_s {
    psk3d_i32 weight;
    psk3d_u32 point_index;
    psk3d_u32 bone_index;
} psk3d_influence;

typedef struct psk3d_morph_info_s {
    char name[PSK3D_NAME_BYTES + 1u];
    psk3d_u32 vertex_count;
    psk3d_u32 data_offset;
} psk3d_morph_info;

typedef struct psk3d_morph_delta_s {
    psk3d_u32 point_index;
    psk3d_vec3 position_delta;
} psk3d_morph_delta;

typedef struct psk3d_model_s {
    psk3d_u32 version_flags;
    psk3d_u32 point_count;
    psk3d_u32 wedge_count;
    psk3d_u32 face_count;
    psk3d_u32 material_count;
    psk3d_u32 bone_count;
    psk3d_u32 influence_count;
    psk3d_u32 extra_uv_set_count;
    psk3d_u32 extra_uv_counts[PSK3D_MAX_EXTRA_UV_SETS];
    psk3d_u32 vertex_color_count;
    psk3d_u32 normal_count;
    psk3d_u32 morph_target_count;
    psk3d_u32 morph_delta_count;
    psk3d_point points[PSK3D_MAX_POINTS];
    psk3d_wedge wedges[PSK3D_MAX_WEDGES];
    psk3d_face faces[PSK3D_MAX_FACES];
    psk3d_material materials[PSK3D_MAX_MATERIALS];
    psk3d_bone bones[PSK3D_MAX_BONES];
    psk3d_influence influences[PSK3D_MAX_INFLUENCES];
    psk3d_uv extra_uvs[PSK3D_MAX_EXTRA_UV_SETS][PSK3D_MAX_WEDGES];
    psk3d_color vertex_colors[PSK3D_MAX_WEDGES];
    psk3d_vec3 normals[PSK3D_MAX_WEDGES];
    psk3d_morph_info morph_targets[PSK3D_MAX_MORPH_TARGETS];
    psk3d_morph_delta morph_deltas[PSK3D_MAX_MORPH_DELTAS];
} psk3d_model;

typedef struct psk3d_anim_info_s {
    char name[PSK3D_NAME_BYTES + 1u];
    char group[PSK3D_NAME_BYTES + 1u];
    psk3d_i32 total_bones;
    psk3d_i32 root_include;
    psk3d_i32 key_compression_style;
    psk3d_i32 key_quotum;
    psk3d_i32 key_reduction;
    psk3d_i32 track_time;
    psk3d_i32 anim_rate;
    psk3d_i32 start_bone;
    psk3d_i32 first_raw_frame;
    psk3d_i32 num_raw_frames;
    psk3d_u32 key_offset;
} psk3d_anim_info;

typedef struct psk3d_anim_key_s {
    psk3d_vec3 position;
    psk3d_quat orientation;
    psk3d_i32 time;
} psk3d_anim_key;

typedef struct psk3d_anim_scale_key_s {
    psk3d_vec3 scale;
    psk3d_i32 time;
} psk3d_anim_scale_key;

typedef struct psk3d_animset_s {
    psk3d_u32 version_flags;
    psk3d_u32 bone_count;
    psk3d_u32 sequence_count;
    psk3d_u32 key_count;
    psk3d_u32 scale_key_count;
    psk3d_bone bones[PSK3D_MAX_BONES];
    psk3d_anim_info sequences[PSK3D_MAX_SEQUENCES];
    psk3d_anim_key keys[PSK3D_MAX_KEYS];
    psk3d_anim_scale_key scale_keys[PSK3D_MAX_SCALE_KEYS];
} psk3d_animset;

typedef struct psk3d_pose_s {
    psk3d_u32 bone_count;
    psk3d_vec3 positions[PSK3D_MAX_BONES];
    psk3d_quat orientations[PSK3D_MAX_BONES];
    psk3d_vec3 scales[PSK3D_MAX_BONES];
} psk3d_pose;


typedef struct psk3d_skinned_points_s {
    psk3d_u32 point_count;
    psk3d_vec3 points[PSK3D_MAX_POINTS];
} psk3d_skinned_points;

typedef struct psk3d_morph_weights_s {
    psk3d_u32 target_count;
    psk3d_i32 weights[PSK3D_MAX_MORPH_TARGETS];
} psk3d_morph_weights;

typedef struct psk3d_morph_layer_s {
    psk3d_u32 target_index;
    psk3d_i32 weight;
} psk3d_morph_layer;

typedef struct psk3d_morph_stack_s {
    psk3d_u32 layer_count;
    psk3d_morph_layer layers[PSK3D_MAX_MORPH_LAYERS];
} psk3d_morph_stack;

typedef struct psk3d_skinned_normals_s {
    psk3d_u32 normal_count;
    psk3d_vec3 normals[PSK3D_MAX_WEDGES];
} psk3d_skinned_normals;

typedef struct psk3d_bone_map_s {
    psk3d_u32 model_bone_count;
    psk3d_u32 anim_bone_count;
    psk3d_u32 missing_count;
    psk3d_u32 anim_index_for_model_bone[PSK3D_MAX_BONES];
} psk3d_bone_map;

typedef struct psk3d_anim_cursor_s {
    psk3d_u32 sequence_index;
    psk3d_u32 frame_index;
    psk3d_i32 frame_t_q16;
    psk3d_u32 looped;
    psk3d_u32 done;
} psk3d_anim_cursor;

typedef struct psk3d_anim_layer_s {
    psk3d_u32 enabled;
    psk3d_u32 sequence_index;
    psk3d_u32 frame_index;
    psk3d_i32 frame_t_q16;
    psk3d_i32 weight_q16;
    psk3d_rot_interp rot_interp;
    psk3d_blend_mode blend_mode;
} psk3d_anim_layer;

typedef struct psk3d_anim_stack_s {
    psk3d_u32 layer_count;
    psk3d_anim_layer layers[PSK3D_MAX_ANIM_LAYERS];
} psk3d_anim_stack;

typedef struct psk3d_final_vertex_s {
    psk3d_vec3 position;
    psk3d_vec3 normal;
    psk3d_uv uv;
    psk3d_uv extra_uv0;
    psk3d_color color;
    psk3d_u32 point_index;
    psk3d_u32 wedge_index;
    psk3d_u32 material_index;
} psk3d_final_vertex;

typedef struct psk3d_triangle_stream_s {
    psk3d_u32 vertex_count;
    psk3d_u32 triangle_count;
    psk3d_final_vertex vertices[PSK3D_MAX_TRI_VERTS];
} psk3d_triangle_stream;

typedef struct psk3d_skin_cache_s {
    psk3d_u32 point_count;
    psk3d_u32 bone_count;
    psk3d_u32 influence_count;
    psk3d_pose bind_global_pose;
    psk3d_u32 first_influence[PSK3D_MAX_POINTS];
    psk3d_u32 next_influence[PSK3D_MAX_INFLUENCES];
    psk3d_i32 weight_sum[PSK3D_MAX_POINTS];
} psk3d_skin_cache;

void psk3d_clear_report(psk3d_report *report);
void psk3d_clear_model(psk3d_model *model);
void psk3d_clear_animset(psk3d_animset *animset);

psk3d_result psk3d_parse_psk(const psk3d_u8 *data,
                             psk3d_u32 size,
                             psk3d_model *out_model,
                             psk3d_report *report);

psk3d_result psk3d_parse_psa(const psk3d_u8 *data,
                             psk3d_u32 size,
                             psk3d_animset *out_animset,
                             psk3d_report *report);

psk3d_result psk3d_validate_model_indices(const psk3d_model *model,
                                          psk3d_report *report);

psk3d_result psk3d_validate_animset(const psk3d_animset *animset,
                                    psk3d_report *report);

psk3d_i32 psk3d_f32bits_to_q16(psk3d_u32 bits);
psk3d_u32 psk3d_q16_to_i16_rounded(psk3d_i32 q);

psk3d_i32 psk3d_q16_add_sat(psk3d_i32 a, psk3d_i32 b);
psk3d_i32 psk3d_q16_sub_sat(psk3d_i32 a, psk3d_i32 b);
psk3d_i32 psk3d_q16_mul(psk3d_i32 a, psk3d_i32 b);
psk3d_i32 psk3d_q16_div(psk3d_i32 a, psk3d_i32 b);
psk3d_i32 psk3d_q16_sqrt(psk3d_i32 x);
psk3d_i32 psk3d_q16_lerp(psk3d_i32 a, psk3d_i32 b, psk3d_i32 t);

void psk3d_quat_identity(psk3d_quat *q);
psk3d_i32 psk3d_quat_dot(const psk3d_quat *a, const psk3d_quat *b);
void psk3d_quat_normalize(psk3d_quat *q);
void psk3d_quat_mul(const psk3d_quat *a, const psk3d_quat *b, psk3d_quat *out);
void psk3d_quat_conjugate(const psk3d_quat *q, psk3d_quat *out);
void psk3d_quat_nlerp(const psk3d_quat *a, const psk3d_quat *b, psk3d_i32 t, psk3d_quat *out);
void psk3d_quat_slerp(const psk3d_quat *a, const psk3d_quat *b, psk3d_i32 t, psk3d_quat *out);
void psk3d_quat_rotate_vec3(const psk3d_quat *q, const psk3d_vec3 *v, psk3d_vec3 *out);
void psk3d_vec3_normalize(psk3d_vec3 *v);

psk3d_i32 psk3d_find_sequence(const psk3d_animset *animset,
                              const char *name);

psk3d_result psk3d_get_key(const psk3d_animset *animset,
                           psk3d_u32 sequence_index,
                           psk3d_u32 frame_index,
                           psk3d_u32 bone_index,
                           psk3d_anim_key *out_key);

psk3d_result psk3d_make_bind_pose_from_model(const psk3d_model *model,
                                             psk3d_pose *out_pose);

psk3d_result psk3d_make_bind_pose_from_animset(const psk3d_animset *animset,
                                               psk3d_pose *out_pose);

psk3d_result psk3d_sample_sequence_nearest(const psk3d_animset *animset,
                                           psk3d_u32 sequence_index,
                                           psk3d_u32 frame_index,
                                           psk3d_pose *out_pose);

psk3d_result psk3d_sample_sequence_linear(const psk3d_animset *animset,
                                          psk3d_u32 sequence_index,
                                          psk3d_u32 frame_index,
                                          psk3d_i32 frame_t_q16,
                                          psk3d_pose *out_pose);

psk3d_result psk3d_sample_sequence_slerp(const psk3d_animset *animset,
                                         psk3d_u32 sequence_index,
                                         psk3d_u32 frame_index,
                                         psk3d_i32 frame_t_q16,
                                         psk3d_pose *out_pose);

psk3d_result psk3d_sample_sequence_linear_ex(const psk3d_animset *animset,
                                             psk3d_u32 sequence_index,
                                             psk3d_u32 frame_index,
                                             psk3d_i32 frame_t_q16,
                                             psk3d_rot_interp rot_interp,
                                             psk3d_pose *out_pose);

psk3d_result psk3d_pose_blend(const psk3d_pose *a,
                              const psk3d_pose *b,
                              psk3d_i32 weight_q16,
                              psk3d_rot_interp rot_interp,
                              psk3d_pose *out_pose);

psk3d_result psk3d_pose_apply_additive(const psk3d_pose *base_pose,
                                       const psk3d_pose *add_pose,
                                       psk3d_i32 weight_q16,
                                       psk3d_rot_interp rot_interp,
                                       psk3d_pose *inout_pose);

void psk3d_clear_anim_stack(psk3d_anim_stack *stack);
psk3d_result psk3d_anim_stack_push(psk3d_anim_stack *stack,
                                   psk3d_u32 sequence_index,
                                   psk3d_u32 frame_index,
                                   psk3d_i32 frame_t_q16,
                                   psk3d_i32 weight_q16,
                                   psk3d_rot_interp rot_interp,
                                   psk3d_blend_mode blend_mode);
psk3d_result psk3d_sample_anim_stack(const psk3d_animset *animset,
                                     const psk3d_anim_stack *stack,
                                     const psk3d_pose *base_pose,
                                     psk3d_pose *out_pose);


void psk3d_clear_morph_weights(psk3d_morph_weights *weights);
psk3d_result psk3d_set_morph_weight(psk3d_morph_weights *weights,
                                     const psk3d_model *model,
                                     psk3d_u32 target_index,
                                     psk3d_i32 weight_q16);
psk3d_i32 psk3d_find_morph_target(const psk3d_model *model,
                                   const char *name);
void psk3d_clear_morph_stack(psk3d_morph_stack *stack);
psk3d_result psk3d_morph_stack_push(psk3d_morph_stack *stack,
                                     psk3d_u32 target_index,
                                     psk3d_i32 weight_q16);
psk3d_result psk3d_apply_shape_keys_points(const psk3d_model *model,
                                           const psk3d_morph_weights *weights,
                                           psk3d_skinned_points *out_points);
psk3d_result psk3d_apply_shape_key_stack_points(const psk3d_model *model,
                                                const psk3d_morph_stack *stack,
                                                psk3d_skinned_points *out_points);

psk3d_result psk3d_build_bone_map(const psk3d_model *model,
                                  const psk3d_animset *animset,
                                  psk3d_u32 flags,
                                  psk3d_bone_map *out_map);

psk3d_result psk3d_sample_sequence_mapped_ex(const psk3d_model *model,
                                             const psk3d_animset *animset,
                                             const psk3d_bone_map *map,
                                             psk3d_u32 sequence_index,
                                             psk3d_u32 frame_index,
                                             psk3d_i32 frame_t_q16,
                                             psk3d_rot_interp rot_interp,
                                             psk3d_pose *out_pose);


psk3d_result psk3d_sample_sequence_mapped_scaled_ex(const psk3d_model *model,
                                                    const psk3d_animset *animset,
                                                    const psk3d_bone_map *map,
                                                    psk3d_u32 sequence_index,
                                                    psk3d_u32 frame_index,
                                                    psk3d_i32 frame_t_q16,
                                                    psk3d_rot_interp rot_interp,
                                                    psk3d_pose *out_pose);

psk3d_result psk3d_extract_sequence_track_range(const psk3d_animset *animset,
                                                psk3d_u32 sequence_index,
                                                psk3d_u32 bone_index,
                                                psk3d_u32 *out_first_key,
                                                psk3d_u32 *out_key_count);

void psk3d_cursor_reset(psk3d_anim_cursor *cursor, psk3d_u32 sequence_index);
psk3d_result psk3d_cursor_advance(const psk3d_animset *animset,
                                  psk3d_anim_cursor *cursor,
                                  psk3d_i32 delta_seconds_q16,
                                  psk3d_u32 loop);

psk3d_result psk3d_globalize_pose(const psk3d_bone *bones,
                                  psk3d_u32 bone_count,
                                  const psk3d_pose *local_pose,
                                  psk3d_pose *out_global_pose);

psk3d_result psk3d_skin_points_lbs_global(const psk3d_model *model,
                                          const psk3d_pose *bind_global_pose,
                                          const psk3d_pose *current_global_pose,
                                          psk3d_skinned_points *out_points);

psk3d_result psk3d_build_skin_cache(const psk3d_model *model,
                                    psk3d_skin_cache *out_cache);

psk3d_result psk3d_skin_points_lbs_cached_from_points(const psk3d_model *model,
                                                     const psk3d_skin_cache *cache,
                                                     const psk3d_pose *current_global_pose,
                                                     const psk3d_skinned_points *source_points,
                                                     psk3d_skinned_points *out_points);

psk3d_result psk3d_skin_points_lbs_cached_morphed(const psk3d_model *model,
                                                  const psk3d_skin_cache *cache,
                                                  const psk3d_pose *current_global_pose,
                                                  const psk3d_morph_weights *weights,
                                                  psk3d_skinned_points *out_points);

psk3d_result psk3d_skin_points_lbs_cached(const psk3d_model *model,
                                          const psk3d_skin_cache *cache,
                                          const psk3d_pose *current_global_pose,
                                          psk3d_skinned_points *out_points);

psk3d_result psk3d_skin_normals_lbs_cached(const psk3d_model *model,
                                           const psk3d_skin_cache *cache,
                                           const psk3d_pose *current_global_pose,
                                           psk3d_skinned_normals *out_normals);

psk3d_result psk3d_skin_points_lbs(const psk3d_model *model,
                                   const psk3d_pose *current_local_pose,
                                   psk3d_skinned_points *out_points);

psk3d_result psk3d_build_triangle_stream(const psk3d_model *model,
                                         const psk3d_skinned_points *points,
                                         const psk3d_skinned_normals *normals,
                                         psk3d_triangle_stream *out_stream);

const char *psk3d_result_name(psk3d_result r);

#ifdef __cplusplus
}
#endif

#endif
