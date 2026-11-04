#ifndef DTS89_H
#define DTS89_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef signed char dts89_s8;
typedef unsigned char dts89_u8;
typedef signed short dts89_s16;
typedef unsigned short dts89_u16;
typedef signed int dts89_s32;
typedef unsigned int dts89_u32;

typedef dts89_s32 dts89_fx16_16;

#define DTS89_TRUE 1
#define DTS89_FALSE 0

#define DTS89_OK 0
#define DTS89_ERR_BAD_ARG -1
#define DTS89_ERR_RANGE -2
#define DTS89_ERR_FORMAT -3
#define DTS89_ERR_UNSUPPORTED -4
#define DTS89_ERR_NO_SPACE -5
#define DTS89_ERR_TRUNCATED -6

#define DTS89_VERSION_CURRENT 28
#define DTS89_VERSION_KNOWN_MAX 29
#define DTS89_EXPORTER_CURRENT 124

#define DTS89_MAX_TS_SET_DWORDS 64
#define DTS89_MAX_TS_SET_SIZE (32 * DTS89_MAX_TS_SET_DWORDS)

#define DTS89_MESH_STANDARD 0u
#define DTS89_MESH_SKIN     1u
#define DTS89_MESH_DECAL    2u
#define DTS89_MESH_SORTED   3u
#define DTS89_MESH_NULL     4u

#define DTS89_PRIM_TRIANGLES 0x00000000u
#define DTS89_PRIM_STRIP     0x40000000u
#define DTS89_PRIM_FAN       0x80000000u
#define DTS89_PRIM_INDEXED   0x20000000u
#define DTS89_PRIM_NOMAT     0x10000000u
#define DTS89_PRIM_TYPE_MASK 0xC0000000u
#define DTS89_PRIM_MAT_MASK  (~(0xC0000000u | 0x20000000u | 0x10000000u))

#define DTS89_MESHF_BILLBOARD         0x80000000u
#define DTS89_MESHF_HAS_DETAIL_TEX    0x40000000u
#define DTS89_MESHF_BILLBOARD_Z       0x20000000u
#define DTS89_MESHF_USE_ENCODED_NORM  0x10000000u
#define DTS89_MESHF_HAS_COLOR         0x08000000u
#define DTS89_MESHF_HAS_TVERT2        0x04000000u

#define DTS89_DSQ_VERSION_CURRENT 26

#define DTS89_MATF_S_WRAP              0x00000001u
#define DTS89_MATF_T_WRAP              0x00000002u
#define DTS89_MATF_TRANSLUCENT         0x00000004u
#define DTS89_MATF_ADDITIVE            0x00000008u
#define DTS89_MATF_SUBTRACTIVE         0x00000010u
#define DTS89_MATF_SELF_ILLUMINATING   0x00000020u
#define DTS89_MATF_NEVER_ENVMAP        0x00000040u
#define DTS89_MATF_NO_MIPMAP           0x00000080u
#define DTS89_MATF_MIPMAP_ZERO_BORDER  0x00000100u
#define DTS89_MATF_AUXILIARY_MAP       0xF8000000u

#define DTS89_SEQ_UNIFORM_SCALE   0x00000001u
#define DTS89_SEQ_ALIGNED_SCALE   0x00000002u
#define DTS89_SEQ_ARBITRARY_SCALE 0x00000004u
#define DTS89_SEQ_BLEND           0x00000008u
#define DTS89_SEQ_CYCLIC          0x00000010u
#define DTS89_SEQ_MAKE_PATH       0x00000020u
#define DTS89_SHAPE_HAS_TRANS     0x00000040u
#define DTS89_SEQ_HAS_TRANS       DTS89_SHAPE_HAS_TRANS
#define DTS89_SEQ_ANY_SCALE       (DTS89_SEQ_UNIFORM_SCALE | DTS89_SEQ_ALIGNED_SCALE | DTS89_SEQ_ARBITRARY_SCALE)

#define DTS89_DIRTY_TRANSFORM 0x00000001u
#define DTS89_DIRTY_VIS       0x00000002u
#define DTS89_DIRTY_FRAME     0x00000004u
#define DTS89_DIRTY_MATFRAME  0x00000008u

#define DTS89_RENDER_NO_NON_TRANS 0x00000001u
#define DTS89_RENDER_NO_TRANS     0x00000002u
#define DTS89_RENDER_BILLBOARD    0x00000004u
#define DTS89_RENDER_HAS_TRANS    0x00000008u


#define DTS89_F32_BITS_ZERO 0x00000000u
#define DTS89_F32_BITS_ONE  0x3F800000u
#define DTS89_FX_ONE        ((dts89_fx16_16)0x00010000)
#define DTS89_FX_PI_OVER_2  ((dts89_fx16_16)102944)

#define DTS89_PROBE_KIND_UNKNOWN      0u
#define DTS89_PROBE_KIND_DTS          1u
#define DTS89_PROBE_KIND_DSQ          2u
#define DTS89_PROBE_KIND_TORQUESCRIPT 3u

#define DTS89_PROBE_FLAG_SUPPORTED      0x00000001u
#define DTS89_PROBE_FLAG_BINARY         0x00000002u
#define DTS89_PROBE_FLAG_TEXT           0x00000004u
#define DTS89_PROBE_FLAG_ENCRYPTED      0x00000008u
#define DTS89_PROBE_FLAG_ANIMATION_ONLY 0x00000010u


typedef struct dts89_probe_result {
    dts89_u32 kind;
    dts89_u32 flags;
    dts89_u32 confidence;
    dts89_u32 version;
    dts89_u32 exporter_version;
    dts89_u32 mem_buffer_dwords;
    dts89_u32 start16_dwords;
    dts89_u32 start8_dwords;
    dts89_s32 num_nodes;
    dts89_s32 num_objects;
    dts89_s32 num_subshapes;
    dts89_s32 num_meshes;
    dts89_s32 num_names;
    dts89_s32 num_sequences;
    dts89_s32 error_code;
} dts89_probe_result;

typedef struct dts89_arena {
    dts89_u8 *base;
    dts89_u32 capacity;
    dts89_u32 used;
} dts89_arena;

typedef struct dts89_reader {
    const dts89_u8 *data;
    dts89_u32 size;
    dts89_u32 pos;
} dts89_reader;

typedef struct dts89_writer {
    dts89_u8 *data;
    dts89_u32 size;
    dts89_u32 pos;
} dts89_writer;

typedef struct dts89_header {
    dts89_u32 version_word;
    dts89_u32 exporter_version;
    dts89_u32 read_version;
    dts89_u32 mem_buffer_dwords;
    dts89_u32 start16_dwords;
    dts89_u32 start8_dwords;
} dts89_header;

typedef struct dts89_pool_cursor {
    const dts89_u32 *p32;
    const dts89_u16 *p16;
    const dts89_u8 *p8;
    dts89_u32 left32;
    dts89_u32 left16;
    dts89_u32 left8;
} dts89_pool_cursor;

typedef struct dts89_pools {
    const dts89_u8 *base;
    dts89_u32 total_bytes;
    const dts89_u32 *pool32;
    const dts89_u16 *pool16;
    const dts89_u8 *pool8;
    dts89_u32 count32;
    dts89_u32 count16;
    dts89_u32 count8;
} dts89_pools;

typedef struct dts89_string {
    char *ptr;
    dts89_u32 len;
} dts89_string;

typedef struct dts89_bitset {
    dts89_u32 size_dwords;
    dts89_u32 bits[DTS89_MAX_TS_SET_DWORDS];
} dts89_bitset;

typedef struct dts89_fx_vec2 {
    dts89_fx16_16 x;
    dts89_fx16_16 y;
} dts89_fx_vec2;

typedef struct dts89_fx_vec3 {
    dts89_fx16_16 x;
    dts89_fx16_16 y;
    dts89_fx16_16 z;
} dts89_fx_vec3;

typedef struct dts89_quat16 {
    dts89_s16 x;
    dts89_s16 y;
    dts89_s16 z;
    dts89_s16 w;
} dts89_quat16;

typedef struct dts89_node {
    dts89_s32 name_index;
    dts89_s32 parent_index;
    dts89_s32 first_object;
    dts89_s32 first_child;
    dts89_s32 next_sibling;
} dts89_node;

typedef struct dts89_object {
    dts89_s32 name_index;
    dts89_s32 num_meshes;
    dts89_s32 start_mesh_index;
    dts89_s32 node_index;
    dts89_s32 next_sibling;
    dts89_s32 first_decal;
} dts89_object;

typedef struct dts89_object_state {
    dts89_u32 vis_bits;
    dts89_s32 frame_index;
    dts89_s32 mat_frame_index;
} dts89_object_state;

typedef struct dts89_trigger {
    dts89_u32 state;
    dts89_u32 pos_bits;
} dts89_trigger;

typedef struct dts89_detail {
    dts89_s32 name_index;
    dts89_s32 subshape_num;
    dts89_s32 object_detail_num;
    dts89_u32 size_bits;
    dts89_u32 average_error_bits;
    dts89_u32 max_error_bits;
    dts89_s32 poly_count;
    dts89_s32 bb_dimension;
    dts89_s32 bb_detail_level;
    dts89_u32 bb_equator_steps;
    dts89_u32 bb_polar_steps;
    dts89_u32 bb_polar_angle_bits;
    dts89_u32 bb_include_poles;
} dts89_detail;

typedef struct dts89_sequence {
    dts89_s32 name_index;
    dts89_s32 num_keyframes;
    dts89_u32 duration_bits;
    dts89_s32 base_rotation;
    dts89_s32 base_translation;
    dts89_s32 base_scale;
    dts89_s32 base_object_state;
    dts89_s32 base_decal_state;
    dts89_s32 first_ground_frame;
    dts89_s32 num_ground_frames;
    dts89_s32 first_trigger;
    dts89_s32 num_triggers;
    dts89_u32 tool_begin_bits;
    dts89_bitset rotation_matters;
    dts89_bitset translation_matters;
    dts89_bitset scale_matters;
    dts89_bitset decal_matters;
    dts89_bitset ifl_matters;
    dts89_bitset vis_matters;
    dts89_bitset frame_matters;
    dts89_bitset mat_frame_matters;
    dts89_s32 priority;
    dts89_u32 flags;
    dts89_u32 dirty_flags;
} dts89_sequence;

typedef struct dts89_draw_primitive {
    dts89_s32 start;
    dts89_s32 num_elements;
    dts89_s32 mat_index;
} dts89_draw_primitive;

typedef struct dts89_mesh_view {
    dts89_u32 mesh_type;
    dts89_s32 num_frames;
    dts89_s32 num_mat_frames;
    dts89_s32 parent_mesh;
    dts89_u32 bounds_bits[6];
    dts89_u32 center_bits[3];
    dts89_u32 radius_bits;
    dts89_s32 vert_offset;
    dts89_s32 num_verts;
    dts89_s32 vert_size;
    dts89_s32 num_verts_editable;
    dts89_s32 num_tverts;
    dts89_s32 num_tverts2;
    dts89_s32 num_colors;
    dts89_s32 num_primitives;
    dts89_s32 num_indices;
    dts89_s32 merge_indices_count;
    dts89_s32 verts_per_frame;
    dts89_u32 flags;
    const dts89_u32 *verts_bits;
    const dts89_u32 *tverts_bits;
    const dts89_u32 *tverts2_bits;
    const dts89_u32 *colors_bits;
    const dts89_u32 *norms_bits;
    const dts89_u8  *encoded_norms;
    const dts89_draw_primitive *primitives;
    const dts89_s32 *indices;
    const dts89_u16 *merge_indices16;

    dts89_s32 skin_max_bones;
    dts89_s32 skin_initial_vert_count;
    const dts89_u32 *skin_initial_verts_bits;
    const dts89_u32 *skin_initial_norms_bits;
    const dts89_u8  *skin_initial_encoded_norms;
    dts89_s32 skin_initial_transform_count;
    const dts89_u32 *skin_initial_transforms_bits;
    dts89_s32 skin_vertex_index_count;
    const dts89_s32 *skin_vertex_index;
    const dts89_s32 *skin_bone_index;
    const dts89_u32 *skin_weight_bits;
    dts89_s32 skin_node_index_count;
    const dts89_s32 *skin_node_index;

    dts89_s32 sorted_num_clusters;
    const dts89_u32 *sorted_clusters_bits;
    dts89_s32 sorted_start_cluster_count;
    const dts89_s32 *sorted_start_cluster;
    dts89_s32 sorted_first_verts_count;
    const dts89_s32 *sorted_first_verts;
    dts89_s32 sorted_num_verts_count;
    const dts89_s32 *sorted_num_verts;
    dts89_s32 sorted_first_tverts_count;
    const dts89_s32 *sorted_first_tverts;
    dts89_s32 sorted_always_write_depth;

    dts89_s32 decal_start_primitive_count;
    const dts89_s32 *decal_start_primitives;
    const dts89_u32 *decal_texgen_s_bits;
    const dts89_u32 *decal_texgen_t_bits;
    dts89_s32 decal_material_index;
} dts89_mesh_view;

typedef struct dts89_material {
    dts89_string name;
    dts89_u32 flags;
    dts89_u32 reflectance_map;
    dts89_u32 bump_map;
    dts89_u32 detail_map;
    dts89_u32 detail_scale_bits;
    dts89_u32 reflection_amount_bits;
} dts89_material;

typedef struct dts89_shape {
    dts89_header header;
    dts89_pools pools;
    dts89_s32 num_nodes;
    dts89_s32 num_objects;
    dts89_s32 num_decals_deprecated;
    dts89_s32 num_subshapes;
    dts89_s32 num_ifl_deprecated;
    dts89_s32 num_node_rotations;
    dts89_s32 num_node_translations;
    dts89_s32 num_node_uniform_scales;
    dts89_s32 num_node_aligned_scales;
    dts89_s32 num_node_arbitrary_scale_rots;
    dts89_s32 num_node_arbitrary_scale_factors;
    dts89_s32 num_ground_rotations;
    dts89_s32 num_ground_translations;
    dts89_s32 num_object_states;
    dts89_s32 num_decal_states_deprecated;
    dts89_s32 num_triggers;
    dts89_s32 num_details;
    dts89_s32 num_meshes;
    dts89_s32 num_names;
    dts89_s32 smallest_visible_size;
    dts89_s32 smallest_visible_dl;
    dts89_u32 bounds_bits[6];
    dts89_u32 center_bits[3];
    dts89_u32 radius_bits;
    dts89_u32 tube_radius_bits;
    dts89_node *nodes;
    dts89_object *objects;
    dts89_s32 *subshape_first_node;
    dts89_s32 *subshape_first_object;
    dts89_s32 *subshape_first_decal;
    dts89_s32 *subshape_num_nodes;
    dts89_s32 *subshape_num_objects;
    dts89_s32 *subshape_num_decals;
    dts89_u32 *default_rotations_bits;
    dts89_u32 *default_translations_bits;
    dts89_u32 *node_rotations_bits;
    dts89_u32 *node_translations_bits;
    dts89_u32 *node_uniform_scales_bits;
    dts89_u32 *node_aligned_scales_bits;
    dts89_u32 *node_arbitrary_scale_rots_bits;
    dts89_u32 *node_arbitrary_scale_factors_bits;
    dts89_u32 *ground_rotations_bits;
    dts89_u32 *ground_translations_bits;
    dts89_object_state *object_states;
    dts89_trigger *triggers;
    dts89_detail *details;
    dts89_u32 vertex_format_words[5];
    dts89_u32 packed_vertex_size;
    const dts89_u8 *packed_vertex_data;
    dts89_u32 packed_vertex_data_owned_offset;
    dts89_mesh_view *meshes;
    dts89_string *names;
    dts89_s32 num_sequences;
    dts89_sequence *sequences;
    dts89_s32 num_materials;
    dts89_material *materials;
    const dts89_u8 *sequence_tail_start;
    dts89_u32 sequence_tail_bytes;
} dts89_shape;

typedef struct dts89_dsq_sequence {
    dts89_string name;
    dts89_u32 flags;
    dts89_s32 num_keyframes;
    dts89_u32 duration_bits;
    dts89_s32 priority;
    dts89_s32 first_ground_frame;
    dts89_s32 num_ground_frames;
    dts89_s32 base_rotation;
    dts89_s32 base_translation;
    dts89_s32 base_scale;
    dts89_s32 base_object_state;
    dts89_s32 base_decal_state;
    dts89_s32 first_trigger;
    dts89_s32 num_triggers;
    dts89_u32 tool_begin_bits;
    dts89_bitset rotation_matters;
    dts89_bitset translation_matters;
    dts89_bitset scale_matters;
    dts89_bitset decal_matters;
    dts89_bitset ifl_matters;
    dts89_bitset vis_matters;
    dts89_bitset frame_matters;
    dts89_bitset mat_frame_matters;
} dts89_dsq_sequence;

typedef struct dts89_dsq_file {
    dts89_s16 version;
    dts89_s16 exporter_version;
    dts89_s32 num_nodes;
    dts89_string *node_names;
    dts89_s32 dummy_a;
    dts89_s32 num_objects;
    dts89_s32 num_node_rotations;
    dts89_quat16 *node_rotations;
    dts89_s32 num_node_translations;
    dts89_u32 *node_translations_bits;
    dts89_s32 num_node_uniform_scales;
    dts89_u32 *node_uniform_scales_bits;
    dts89_s32 num_node_aligned_scales;
    dts89_u32 *node_aligned_scales_bits;
    dts89_s32 num_node_arbitrary_scales;
    dts89_quat16 *node_arbitrary_scale_rots;
    dts89_u32 *node_arbitrary_scale_factors_bits;
    dts89_s32 num_ground_frames;
    dts89_u32 *ground_translations_bits;
    dts89_quat16 *ground_rotations;
    dts89_s32 dummy_b;
    dts89_s32 num_sequences;
    dts89_dsq_sequence *sequences;
    dts89_s32 num_triggers;
    dts89_trigger *triggers;
} dts89_dsq_file;

typedef struct dts89_ts_assignment {
    dts89_string key;
    dts89_s32 array_index;
    dts89_string value;
    dts89_u32 value_was_quoted;
} dts89_ts_assignment;

typedef struct dts89_ts_object {
    dts89_string class_name;
    dts89_string object_name;
    dts89_s32 num_assignments;
    dts89_ts_assignment *assignments;
} dts89_ts_object;

typedef struct dts89_ts_file {
    dts89_s32 num_objects;
    dts89_ts_object *objects;
} dts89_ts_file;

typedef struct dts89_lod_pair {
    dts89_s8 level;
    dts89_u8 intra;
} dts89_lod_pair;

typedef struct dts89_runtime_options {
    dts89_s32 skip_load_details;
    dts89_s32 skip_draw_details;
    dts89_fx16_16 alpha_out_last_detail;
    dts89_fx16_16 alpha_in_billboard;
    dts89_fx16_16 alpha_out_billboard;
    dts89_fx16_16 alpha_in_default;
    dts89_fx16_16 alpha_out_default;
} dts89_runtime_options;

typedef struct dts89_runtime_shape {
    dts89_s32 smallest_visible_dl;
    dts89_fx16_16 smallest_visible_size;
    dts89_u32 flags;
    dts89_u32 use_detail_from_screen_error;
    dts89_s32 *subshape_first_trans_object;
    dts89_fx16_16 *alpha_in;
    dts89_fx16_16 *alpha_out;
    dts89_s32 *detail_subshape_num;
    dts89_s32 *detail_object_detail_num;
    dts89_s32 *detail_poly_count;
    dts89_lod_pair *detail_lookup;
    dts89_u32 detail_lookup_count;
} dts89_runtime_shape;

typedef struct dts89_detail_choice {
    dts89_s32 detail_level;
    dts89_u8 intra;
    dts89_fx16_16 pixel_size;
} dts89_detail_choice;

typedef struct dts89_render_span {
    dts89_s32 detail_level;
    dts89_s32 subshape_index;
    dts89_s32 object_detail;
    dts89_s32 object_start;
    dts89_s32 object_end;
    dts89_u8 alpha;
    dts89_u32 flags;
} dts89_render_span;

typedef struct dts89_draw_command {
    dts89_s32 object_index;
    dts89_s32 mesh_index;
    dts89_s32 primitive_index;
    dts89_s32 material_index;
    dts89_s32 index_start;
    dts89_s32 index_count;
    dts89_u32 primitive_type;
    dts89_u32 flags;
} dts89_draw_command;

typedef struct dts89_triangle {
    dts89_s32 i0;
    dts89_s32 i1;
    dts89_s32 i2;
    dts89_s32 material_index;
    dts89_u32 primitive_flags;
} dts89_triangle;

#define DTS89_VALIDATE_WARN_UNSUPPORTED_MESH 0x00000001u
#define DTS89_VALIDATE_WARN_EMPTY_DETAIL     0x00000002u
#define DTS89_VALIDATE_WARN_NAMED_RANGE      0x00000004u

#define DTS89_SCALE_KIND_NONE      0u
#define DTS89_SCALE_KIND_UNIFORM   1u
#define DTS89_SCALE_KIND_ALIGNED   2u
#define DTS89_SCALE_KIND_ARBITRARY 3u

#define DTS89_TRIGGER_STATE_ON          0x80000000u
#define DTS89_TRIGGER_INVERT_ON_REVERSE 0x40000000u
#define DTS89_TRIGGER_STATE_MASK        0x3FFFFFFFu

typedef struct dts89_validate_report {
    dts89_s32 errors;
    dts89_s32 warnings;
    dts89_s32 first_error_code;
    dts89_s32 bad_count;
    dts89_s32 bad_name_index;
    dts89_s32 bad_node_index;
    dts89_s32 bad_object_index;
    dts89_s32 bad_subshape_range;
    dts89_s32 bad_detail_range;
    dts89_s32 bad_mesh_range;
    dts89_s32 bad_material_index;
    dts89_s32 bad_primitive_range;
    dts89_s32 bad_sequence_range;
    dts89_s32 bad_trigger_range;
    dts89_s32 unsupported_meshes;
    dts89_u32 warning_flags;
} dts89_validate_report;

typedef struct dts89_sequence_cursor {
    dts89_s32 sequence_index;
    dts89_s32 key0;
    dts89_s32 key1;
    dts89_fx16_16 key_pos;
    dts89_fx16_16 position;
    dts89_u32 flags;
    dts89_s32 ground_index0;
    dts89_s32 ground_index1;
    dts89_fx16_16 ground_pos;
} dts89_sequence_cursor;

typedef struct dts89_node_anim_indices {
    dts89_s32 node_index;
    dts89_s32 rotation_index0;
    dts89_s32 rotation_index1;
    dts89_s32 translation_index0;
    dts89_s32 translation_index1;
    dts89_s32 scale_index0;
    dts89_s32 scale_index1;
    dts89_u32 scale_kind;
    dts89_u32 dirty_flags;
} dts89_node_anim_indices;

typedef struct dts89_object_anim_indices {
    dts89_s32 object_index;
    dts89_s32 vis_state_index0;
    dts89_s32 vis_state_index1;
    dts89_s32 frame_state_index0;
    dts89_s32 frame_state_index1;
    dts89_s32 mat_frame_state_index0;
    dts89_s32 mat_frame_state_index1;
    dts89_u32 dirty_flags;
} dts89_object_anim_indices;

typedef struct dts89_trigger_event {
    dts89_s32 trigger_index;
    dts89_u32 state_bits;
    dts89_u32 state_mask;
    dts89_u32 enabled;
    dts89_u32 reverse;
    dts89_u32 pos_bits;
} dts89_trigger_event;

typedef struct dts89_dsq_import_plan {
    dts89_s32 num_node_map;
    dts89_s32 *node_map;
    dts89_s32 mapped_nodes;
    dts89_s32 ignored_nodes;
    dts89_s32 imported_sequences;
    dts89_s32 append_rotations;
    dts89_s32 append_translations;
    dts89_s32 append_uniform_scales;
    dts89_s32 append_aligned_scales;
    dts89_s32 append_arbitrary_scales;
    dts89_s32 append_ground_frames;
    dts89_s32 append_triggers;
} dts89_dsq_import_plan;

/* arena */
void dts89_arena_init(dts89_arena *a, void *buffer, dts89_u32 size);
void *dts89_arena_alloc(dts89_arena *a, dts89_u32 size, dts89_u32 align);

/* stream */
void dts89_reader_init(dts89_reader *r, const void *data, dts89_u32 size);
void dts89_writer_init(dts89_writer *w, void *data, dts89_u32 size);
int dts89_read_u8(dts89_reader *r, dts89_u8 *v);
int dts89_read_u16le(dts89_reader *r, dts89_u16 *v);
int dts89_read_u32le(dts89_reader *r, dts89_u32 *v);
int dts89_write_u8(dts89_writer *w, dts89_u8 v);
int dts89_write_u16le(dts89_writer *w, dts89_u16 v);
int dts89_write_u32le(dts89_writer *w, dts89_u32 v);
int dts89_write_bytes(dts89_writer *w, const void *p, dts89_u32 n);

/* fixed */
dts89_fx16_16 dts89_fx_from_int(dts89_s32 v);
dts89_s32 dts89_fx_to_int(dts89_fx16_16 v);
dts89_fx16_16 dts89_fx_mul(dts89_fx16_16 a, dts89_fx16_16 b);
dts89_fx16_16 dts89_fx_div(dts89_fx16_16 a, dts89_fx16_16 b);
dts89_u32 dts89_ieee32_from_raw_parts(dts89_u32 sign, dts89_s32 exp2, dts89_u32 mant24);
dts89_fx16_16 dts89_f32bits_to_fx(dts89_u32 bits);
dts89_u32 dts89_fx_to_f32bits(dts89_fx16_16 fx);
int dts89_fx_vec2_from_bits2(const dts89_u32 *bits2, dts89_fx_vec2 *out_v);
int dts89_fx_vec3_from_bits3(const dts89_u32 *bits3, dts89_fx_vec3 *out_v);
int dts89_shape_get_fx_bounds(const dts89_shape *shape, dts89_fx_vec3 *out_min, dts89_fx_vec3 *out_max);
int dts89_mesh_get_fx_bounds(const dts89_mesh_view *mesh, dts89_fx_vec3 *out_min, dts89_fx_vec3 *out_max);
int dts89_mesh_get_fx_center(const dts89_mesh_view *mesh, dts89_fx_vec3 *out_center);
int dts89_mesh_get_fx_vert(const dts89_mesh_view *mesh, dts89_s32 index, dts89_fx_vec3 *out_vert);
int dts89_mesh_get_fx_tvert(const dts89_mesh_view *mesh, dts89_s32 layer, dts89_s32 index, dts89_fx_vec2 *out_uv);

/* bitset */
void dts89_bitset_clear(dts89_bitset *s);
int dts89_bitset_read(dts89_reader *r, dts89_bitset *s);
int dts89_bitset_write(dts89_writer *w, const dts89_bitset *s);
int dts89_bitset_test(const dts89_bitset *s, dts89_s32 bit_index);
dts89_s32 dts89_bitset_count(const dts89_bitset *s);
dts89_s32 dts89_bitset_count_before(const dts89_bitset *s, dts89_s32 bit_index);
int dts89_bitset_set(dts89_bitset *s, dts89_s32 bit_index);

/* probe */
int dts89_probe(const void *bytes, dts89_u32 size, dts89_probe_result *out_result);
const char *dts89_probe_kind_name(dts89_u32 kind);

/* parse/encode */
int dts89_decode_header(dts89_reader *r, dts89_header *h);
int dts89_decode_pools(const dts89_header *h, const dts89_u8 *mem, dts89_u32 mem_bytes, dts89_pools *p);
int dts89_decode_shape(const void *bytes, dts89_u32 size, dts89_arena *arena, dts89_shape *out_shape);
int dts89_decode_shape_with_key(const void *bytes, dts89_u32 size, dts89_arena *arena, dts89_shape *out_shape, const dts89_u8 *key, dts89_u32 key_len);
int dts89_make_v29_key(const char *seed, dts89_u32 seed_len, dts89_u8 *out_key, dts89_u32 key_len);
void dts89_xor_bytes_at_offset(void *data, dts89_u32 byte_count, dts89_u32 start_offset, const dts89_u8 *key, dts89_u32 key_len);
int dts89_encode_shape(const dts89_shape *shape, dts89_writer *w, int save_old_format);
int dts89_detach_shape(dts89_shape *shape, dts89_arena *arena);
int dts89_decode_dsq(const void *bytes, dts89_u32 size, dts89_arena *arena, dts89_dsq_file *out_dsq);
int dts89_encode_dsq(const dts89_dsq_file *dsq, dts89_writer *w);
int dts89_validate_shape(const dts89_shape *shape, dts89_validate_report *out_report);
int dts89_validate_dsq(const dts89_dsq_file *dsq, dts89_validate_report *out_report);
int dts89_sequence_select_keyframes(const dts89_sequence *seq, dts89_fx16_16 position, dts89_sequence_cursor *out_cursor);
int dts89_sequence_get_node_tracks(const dts89_shape *shape, dts89_s32 sequence_index, dts89_s32 node_index, const dts89_sequence_cursor *cursor, dts89_node_anim_indices *out_indices);
int dts89_sequence_get_object_tracks(const dts89_shape *shape, dts89_s32 sequence_index, dts89_s32 object_index, const dts89_sequence_cursor *cursor, dts89_object_anim_indices *out_indices);
int dts89_sequence_scan_triggers(const dts89_shape *shape, dts89_s32 sequence_index, dts89_fx16_16 from_position, dts89_fx16_16 to_position, dts89_trigger_event *out_events, dts89_u32 max_events, dts89_u32 *out_count);

/* TorqueScript sidecars */
int dts89_tscript_parse_singletons(const void *text, dts89_u32 size, dts89_arena *arena, dts89_ts_file *out_file);
int dts89_tscript_write_singletons(const dts89_ts_file *file, dts89_writer *w);
int dts89_emit_materials_cs(const dts89_shape *shape, const char *default_extension, dts89_writer *w);
int dts89_emit_shape_constructor_cs(const char *object_name, const char *base_shape, const dts89_dsq_file *dsq, const char *dsq_path, dts89_writer *w);

/* runtime */
void dts89_runtime_options_default(dts89_runtime_options *opt);
int dts89_runtime_build(const dts89_shape *shape, const dts89_runtime_options *opt, dts89_arena *arena, dts89_runtime_shape *out_rt);
int dts89_runtime_select_detail_from_pixel_size(const dts89_shape *shape, const dts89_runtime_shape *rt, dts89_fx16_16 pixel_size, dts89_detail_choice *out_choice);
int dts89_runtime_select_detail_from_projected_radius(const dts89_shape *shape, const dts89_runtime_shape *rt, dts89_fx16_16 projected_radius, dts89_fx16_16 detail_adjust, dts89_fx16_16 smallest_visible_pixel_size, dts89_detail_choice *out_choice);
int dts89_runtime_select_detail_from_screen_error(const dts89_shape *shape, const dts89_runtime_shape *rt, dts89_fx16_16 error_tolerance, dts89_detail_choice *out_choice);
int dts89_runtime_make_render_spans(const dts89_shape *shape, const dts89_runtime_shape *rt, const dts89_detail_choice *choice, dts89_u32 render_flags, dts89_render_span *out_spans, dts89_u32 max_spans, dts89_u32 *out_count);
int dts89_runtime_emit_draws_for_span(const dts89_shape *shape, const dts89_render_span *span, dts89_draw_command *out_cmds, dts89_u32 max_cmds, dts89_u32 *out_count);
int dts89_runtime_emit_triangles_for_draw(const dts89_shape *shape, const dts89_draw_command *draw, dts89_triangle *out_tris, dts89_u32 max_tris, dts89_u32 *out_count);
int dts89_runtime_count_triangles_for_draw(const dts89_shape *shape, const dts89_draw_command *draw, dts89_u32 *out_count);
int dts89_dsq_build_import_plan(const dts89_shape *shape, const dts89_dsq_file *dsq, dts89_arena *arena, dts89_dsq_import_plan *out_plan);
int dts89_shape_merge_dsq(const dts89_shape *shape, const dts89_dsq_file *dsq, dts89_arena *arena, dts89_shape *out_shape);

/* helpers */
const char *dts89_name_at(const dts89_shape *shape, dts89_s32 idx);
const char *dts89_material_name_at(const dts89_shape *shape, dts89_s32 idx);
int dts89_can_write_old_format(const dts89_shape *shape);
int dts89_is_mesh_supported_semantically(const dts89_mesh_view *mesh);

#ifdef __cplusplus
}
#endif
#endif
