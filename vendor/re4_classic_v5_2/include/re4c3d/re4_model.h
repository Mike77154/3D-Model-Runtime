#ifndef RE4_MODEL_H
#define RE4_MODEL_H

#include "re4_types.h"
#include "re4_arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum re4_model_kind_e {
    RE4_MODEL_UNKNOWN = 0,
    RE4_MODEL_PMD = 1,
    RE4_MODEL_BIN = 2,
    RE4_MODEL_SMD_ROOM = 3,
    RE4_MODEL_SMX = 4
} re4_model_kind;

typedef struct re4_model_probe_s {
    re4_model_kind kind;
    re4_u32 file_size;
    re4_u32 first_word_le;
    re4_u32 first_word_be;
    re4_u32 probable_offset_count;
    re4_u32 probable_string_count;
    re4_u32 probable_tpl_count;
    re4_u32 probable_fcv_count;
} re4_model_probe;

#define RE4_BIN_FLAG_ALWAYS_ACTIVATED      ((re4_u32)0x80000000U)
#define RE4_BIN_FLAG_ENABLE_VERTEX_COLORS  ((re4_u32)0x40000000U)
#define RE4_BIN_FLAG_ENABLE_ALT_NORMALS    ((re4_u32)0x20000000U)
#define RE4_BIN_FLAG_ENABLE_ADJACENT_TAG   ((re4_u32)0x00000200U)
#define RE4_BIN_FLAG_ENABLE_BONEPAIR_TAG   ((re4_u32)0x00000100U)

/* RE4 UHD/modern BIN structural reader. IEEE-754 binary32 words are
   exposed as Q16.16 integers so the public API avoids C real types. */
typedef struct re4_bin_header_s {
    re4_endian endian;
    re4_u32 header_size;
    re4_u32 bone_offset;
    re4_u32 unknown_x04;
    re4_u32 unknown_x08;
    re4_u32 vertex_colour_offset;
    re4_u32 vertex_texcoord_offset;
    re4_u32 weightmap_offset;
    re4_u8 weightmap_count;
    re4_u8 bone_count;
    re4_u16 material_count;
    re4_u32 material_offset;
    re4_u32 flags;
    re4_u32 texture_count;
    re4_u8 vertex_scale;
    re4_u8 unknown_x29;
    re4_u16 weightmap2_count;
    re4_u32 morph_offset;
    re4_u32 vertex_position_offset;
    re4_u32 vertex_normal_offset;
    re4_u16 vertex_position_count;
    re4_u16 vertex_normal_count;
    re4_u32 version_flags;
    re4_u32 bonepair_offset;
    re4_u32 adjacent_offset;
    re4_u32 vertex_weight_index_offset;
    re4_u32 vertex_weight2_index_offset;
} re4_bin_header;

typedef struct re4_bin_material_s {
    re4_u8 raw[24];
    re4_u32 byte_offset;
    re4_u32 byte_size;
    re4_u8 material_flag;
    re4_u8 diffuse_map;
    re4_u8 bump_map;
    re4_u8 opacity_map;
    re4_u8 generic_specular_map;
    re4_u8 custom_specular_map;
    re4_u8 specular_rgb[3];
    re4_u8 specular_scale;
    re4_u32 face_buffer_size;
    re4_u32 face_count_word;
    re4_u32 strip_count;
    re4_u32 first_strip;
    re4_u32 vertex_base;
    re4_u32 vertex_count_hint;
    re4_u32 triangle_count_hint;
} re4_bin_material;

#define RE4_BIN_MATERIAL_F_BUMP_MAP          0x01U
#define RE4_BIN_MATERIAL_F_GENERIC_SPECULAR  0x02U
#define RE4_BIN_MATERIAL_F_OPACITY_MAP       0x04U
#define RE4_BIN_MATERIAL_F_CUSTOM_SPECULAR   0x10U
#define RE4_BIN_MATERIAL_F_NORMALS_EFFECT    0x80U

typedef enum re4_bin_material_core_variant_e {
    RE4_BIN_MATERIAL_CORE_PS2_12 = 12,
    RE4_BIN_MATERIAL_CORE_UHD_24 = 24
} re4_bin_material_core_variant;

typedef struct re4_bin_material_core_s {
    re4_bin_material_core_variant variant;
    re4_u8 unknown_min[11];
    re4_u8 material_flag;
    re4_u8 diffuse_map;
    re4_u8 bump_map;
    re4_u8 opacity_map;
    re4_u8 generic_specular_map;
    re4_u8 specular_rgb[3];
    re4_u8 unknown_08;
    re4_u8 unknown_09;
    re4_u8 specular_scale;
    re4_u8 unknown_11;
    re4_u8 custom_specular_map;
} re4_bin_material_core;


#define RE4_IDXMATERIAL_SEEN_NAME              0x00000001U
#define RE4_IDXMATERIAL_SEEN_FLAG              0x00000002U
#define RE4_IDXMATERIAL_SEEN_DIFFUSE           0x00000004U
#define RE4_IDXMATERIAL_SEEN_BUMP              0x00000008U
#define RE4_IDXMATERIAL_SEEN_OPACITY           0x00000010U
#define RE4_IDXMATERIAL_SEEN_GENERIC_SPECULAR  0x00000020U
#define RE4_IDXMATERIAL_SEEN_CUSTOM_SPECULAR   0x00000040U

typedef struct re4_idxmaterial_entry_s {
    re4_strref name;
    re4_bin_material_core core;
    re4_u32 seen_mask;
} re4_idxmaterial_entry;

typedef struct re4_idxmaterial_s {
    re4_u32 entry_count;
    re4_idxmaterial_entry *entries;
} re4_idxmaterial;

typedef enum re4_mtl_texture_role_e {
    RE4_MTL_TEXROLE_UNKNOWN = 0,
    RE4_MTL_TEXROLE_DIFFUSE = 1,
    RE4_MTL_TEXROLE_BUMP = 2,
    RE4_MTL_TEXROLE_OPACITY = 3,
    RE4_MTL_TEXROLE_SPECULAR = 4
} re4_mtl_texture_role;

typedef enum re4_mtl_texture_ext_e {
    RE4_MTL_EXT_UNKNOWN = 0,
    RE4_MTL_EXT_DDS = 1,
    RE4_MTL_EXT_TGA = 2
} re4_mtl_texture_ext;

typedef struct re4_mtl_texture_ref_s {
    re4_mtl_texture_role role;
    re4_strref command;
    re4_strref path;
    re4_u32 pack_id;
    re4_u32 texture_id;
    re4_u8 has_pack;
    re4_u8 has_texture;
    re4_u8 has_scale;
    re4_u8 scale_x_units;
    re4_u8 scale_y_units;
    re4_mtl_texture_ext ext;
} re4_mtl_texture_ref;

#define RE4_MTL_MATERIAL_SEEN_NAME       0x00000001U
#define RE4_MTL_MATERIAL_SEEN_KS         0x00000002U
#define RE4_MTL_MATERIAL_SEEN_DIFFUSE    0x00000004U
#define RE4_MTL_MATERIAL_SEEN_BUMP       0x00000008U
#define RE4_MTL_MATERIAL_SEEN_OPACITY    0x00000010U
#define RE4_MTL_MATERIAL_SEEN_SPECULAR   0x00000020U
#define RE4_MTL_MATERIAL_BUMP_IS_BUMP    0x00000040U

typedef struct re4_mtl_material_s {
    re4_strref name;
    re4_u8 specular_rgb[3];
    re4_mtl_texture_ref diffuse;
    re4_mtl_texture_ref bump;
    re4_mtl_texture_ref opacity;
    re4_mtl_texture_ref specular;
    re4_u32 seen_mask;
} re4_mtl_material;

typedef struct re4_mtl_s {
    re4_u32 material_count;
    re4_mtl_material *materials;
} re4_mtl;

struct re4_idxuhdtpl_s;

typedef enum re4_scenario_prefix_e {
    RE4_SCENARIO_PREFIX_UNKNOWN = 0,
    RE4_SCENARIO_PREFIX_UHD = 1,
    RE4_SCENARIO_PREFIX_UU = 2,
    RE4_SCENARIO_PREFIX_GG = 3,
    RE4_SCENARIO_PREFIX_MAIN = 4,
    RE4_SCENARIO_PREFIX_FILE = 5
} re4_scenario_prefix;

typedef struct re4_scenario_group_ref_s {
    re4_scenario_prefix prefix;
    re4_u32 file_id;
    re4_u32 smd_id;
    re4_u32 smx_id;
    re4_u32 bin_id;
    re4_u32 object_status;
    re4_u32 seen_mask;
} re4_scenario_group_ref;

#define RE4_SCENARIO_GROUP_SEEN_FILE 0x00000001U
#define RE4_SCENARIO_GROUP_SEEN_SMD  0x00000002U
#define RE4_SCENARIO_GROUP_SEEN_SMX  0x00000004U
#define RE4_SCENARIO_GROUP_SEEN_TYPE 0x00000008U
#define RE4_SCENARIO_GROUP_SEEN_BIN  0x00000010U

typedef struct re4_scenario_idx_options_s {
    re4_u32 magic;
    re4_strref smd_file_name;
    re4_strref tpl_file_name;
    re4_strref bin_folder;
    re4_strref shared_file_name;
    re4_bool use_idxuhdtpl;
    re4_bool use_idxmaterial;
    re4_bool enable_vertex_color;
    re4_bool enable_dynamic_vertex_color;
    re4_u32 seen_mask;
} re4_scenario_idx_options;

#define RE4_SCENARIO_IDX_SEEN_MAGIC                  0x00000001U
#define RE4_SCENARIO_IDX_SEEN_SMD_FILE_NAME          0x00000002U
#define RE4_SCENARIO_IDX_SEEN_TPL_FILE_NAME          0x00000004U
#define RE4_SCENARIO_IDX_SEEN_BIN_FOLDER             0x00000008U
#define RE4_SCENARIO_IDX_SEEN_SHARED_FILE_NAME       0x00000010U
#define RE4_SCENARIO_IDX_SEEN_USE_IDXUHDTPL          0x00000020U
#define RE4_SCENARIO_IDX_SEEN_USE_IDXMATERIAL        0x00000040U
#define RE4_SCENARIO_IDX_SEEN_ENABLE_VERTEX_COLOR    0x00000080U
#define RE4_SCENARIO_IDX_SEEN_ENABLE_DYNAMIC_COLOR   0x00000100U

#define RE4_SCENARIO_STATUS_EXE_SCRIPTED       0x01U
#define RE4_SCENARIO_STATUS_UNKNOWN_02         0x02U
#define RE4_SCENARIO_STATUS_ASSIGN_SMX_GROUP   0x04U
#define RE4_SCENARIO_STATUS_ENDS_SMX_GROUP     0x08U
#define RE4_SCENARIO_STATUS_USE_SHARED_BIN      0x10U
#define RE4_SCENARIO_STATUS_USE_SHARED_TPL      0x20U
#define RE4_SCENARIO_STATUS_USE_SHARED_MOT      0x40U
#define RE4_SCENARIO_STATUS_UNKNOWN_80         0x80U


typedef struct re4_scenario_idx_entry_s {
    re4_u32 file_id;
    re4_u32 smd_id;
    re4_u32 seen_mask;
    re4_q16 position[3];
    re4_q16 angle[3];
    re4_q16 scale[3];
    re4_u32 tpl_file_id;
    re4_u32 bin_file_id;
    re4_u32 smx_id;
    re4_u32 object_status;
    re4_u32 fixed_ff;
    re4_u32 unused[7];
} re4_scenario_idx_entry;

#define RE4_SCENARIO_ENTRY_SEEN_FILE        0x00000001U
#define RE4_SCENARIO_ENTRY_SEEN_SMD         0x00000002U
#define RE4_SCENARIO_ENTRY_SEEN_POS_X       0x00000004U
#define RE4_SCENARIO_ENTRY_SEEN_POS_Y       0x00000008U
#define RE4_SCENARIO_ENTRY_SEEN_POS_Z       0x00000010U
#define RE4_SCENARIO_ENTRY_SEEN_ANG_X       0x00000020U
#define RE4_SCENARIO_ENTRY_SEEN_ANG_Y       0x00000040U
#define RE4_SCENARIO_ENTRY_SEEN_ANG_Z       0x00000080U
#define RE4_SCENARIO_ENTRY_SEEN_SCALE_X     0x00000100U
#define RE4_SCENARIO_ENTRY_SEEN_SCALE_Y     0x00000200U
#define RE4_SCENARIO_ENTRY_SEEN_SCALE_Z     0x00000400U
#define RE4_SCENARIO_ENTRY_SEEN_TPL_FILE    0x00000800U
#define RE4_SCENARIO_ENTRY_SEEN_BIN_FILE    0x00001000U
#define RE4_SCENARIO_ENTRY_SEEN_SMX         0x00002000U
#define RE4_SCENARIO_ENTRY_SEEN_STATUS      0x00004000U
#define RE4_SCENARIO_ENTRY_SEEN_FIXED_FF    0x00008000U
#define RE4_SCENARIO_ENTRY_SEEN_UNUSED_BASE 0x00010000U

typedef struct re4_scenario_idx_doc_s {
    re4_scenario_idx_options options;
    re4_u32 entry_count;
    re4_scenario_idx_entry *entries;
    re4_u32 extra_parameter_count;
    re4_u32 extra_file_name_count;
    re4_u32 extra_tpl_file_name_count;
} re4_scenario_idx_doc;

#define RE4_IDXMATERIAL_ORDER_OK             0x00000000U
#define RE4_IDXMATERIAL_ORDER_ALPHA_BREAK    0x00000001U
#define RE4_IDXMATERIAL_ORDER_OPAQUE_AFTER_A 0x00000002U

typedef struct re4_idxmaterial_order_report_s {
    re4_u32 flags;
    re4_u32 first_bad_index;
    re4_u32 transparent_count;
} re4_idxmaterial_order_report;

typedef struct re4_idxuubin_kv_s {
    re4_strref section;
    re4_strref key;
    re4_strref value;
    re4_u32 line_no;
    re4_u32 value_u32;
    re4_q16 value_q16;
    re4_u32 flags;
} re4_idxuubin_kv;

#define RE4_IDXUUBIN_KV_HAS_U32   0x00000001U
#define RE4_IDXUUBIN_KV_HAS_BOOL  0x00000002U
#define RE4_IDXUUBIN_KV_HAS_Q16   0x00000004U
#define RE4_IDXUUBIN_KV_BOOL_TRUE 0x00000008U

#define RE4_IDXUUBIN_SEEN_USE_VERTEX_COLOR 0x00000001U
#define RE4_IDXUUBIN_SEEN_MATERIAL_COUNT   0x00000002U
#define RE4_IDXUUBIN_SEEN_BONE_COUNT       0x00000004U
#define RE4_IDXUUBIN_SEEN_WEIGHTMAP_COUNT  0x00000008U
#define RE4_IDXUUBIN_SEEN_TEX_COUNT        0x00000010U
#define RE4_IDXUUBIN_SEEN_MORPH            0x00000020U

typedef struct re4_idxuubin_s {
    re4_u32 kv_count;
    re4_idxuubin_kv *kvs;
    re4_bool use_vertex_color;
    re4_u32 material_count;
    re4_u32 bone_count;
    re4_u32 weightmap_count;
    re4_u32 texture_count;
    re4_u32 morph_count;
    re4_u32 seen_mask;
} re4_idxuubin;


#define RE4_IDXPS2BIN_SEEN_USE_IDXMATERIAL      0x00000001U
#define RE4_IDXPS2BIN_SEEN_AUTOCALC_BBOX        0x00000002U
#define RE4_IDXPS2BIN_SEEN_MATERIAL_COUNT       0x00000004U
#define RE4_IDXPS2BIN_SEEN_BONE_COUNT           0x00000008U
#define RE4_IDXPS2BIN_SEEN_WEIGHTMAP_COUNT      0x00000010U

typedef struct re4_idxps2bin_s {
    re4_u32 kv_count;
    re4_idxuubin_kv *kvs;
    re4_bool use_idxmaterial;
    re4_bool auto_calc_bounding_box;
    re4_u32 material_count;
    re4_u32 bone_count;
    re4_u32 weightmap_count;
    re4_u32 seen_mask;
} re4_idxps2bin;

typedef struct re4_scenario_r100_extra_parameter_s {
    re4_u32 id;
    re4_u32 value;
} re4_scenario_r100_extra_parameter;

typedef struct re4_scenario_r100_extra_name_s {
    re4_u32 id;
    re4_strref name;
} re4_scenario_r100_extra_name;

typedef struct re4_scenario_r100_s {
    re4_u32 extra_parameter_count;
    re4_scenario_r100_extra_parameter *extra_parameters;
    re4_u32 extra_smd_count;
    re4_scenario_r100_extra_name *extra_smd_files;
    re4_u32 extra_tpl_count;
    re4_scenario_r100_extra_name *extra_tpl_files;
} re4_scenario_r100;

typedef struct re4_bin_strip_s {
    re4_u32 material_index;
    re4_u16 primitive_type;
    re4_u16 vertex_count;
    re4_u32 vertex_base;
    re4_u32 triangle_count_hint;
} re4_bin_strip;

typedef struct re4_bin_bone_s {
    re4_u8 id;
    re4_u8 parent;
    re4_q16 position[3];
} re4_bin_bone;

typedef struct re4_bin_weight_map_s {
    re4_u8 bone_id[3];
    re4_u8 count;
    re4_u8 weight[3];
    re4_u8 unused;
} re4_bin_weight_map;

typedef struct re4_bin_bone_pair_s {
    re4_u16 bone[4];
} re4_bin_bone_pair;

typedef struct re4_bin_vertex_s {
    re4_q16 position[3];
    re4_q16 normal[3];
    re4_q16 uv[2];
    re4_u8 color_argb[4];
    re4_u16 weight_index;
    re4_u32 flags;
} re4_bin_vertex;

#define RE4_BIN_VERTEX_HAS_POSITION    1U
#define RE4_BIN_VERTEX_HAS_NORMAL      2U
#define RE4_BIN_VERTEX_HAS_UV          4U
#define RE4_BIN_VERTEX_HAS_COLOR       8U
#define RE4_BIN_VERTEX_HAS_WEIGHT      16U

typedef struct re4_bin_model_s {
    re4_bin_header header;
    re4_u32 material_count;
    re4_bin_material *materials;
    re4_u32 strip_count;
    re4_bin_strip *strips;
    re4_u32 bone_count;
    re4_bin_bone *bones;
    re4_u32 weightmap_count;
    re4_bin_weight_map *weightmaps;
    re4_u32 bonepair_count;
    re4_bin_bone_pair *bonepairs;
    re4_u32 true_vertex_count;
    re4_u32 triangle_count_hint;
} re4_bin_model;

typedef struct re4_pmd_name_s {
    re4_strref name;
    re4_u32 id;
} re4_pmd_name;

typedef struct re4_pmd_skeleton_s {
    re4_i32 parent;
    re4_q16 data[26];
} re4_pmd_skeleton;

typedef struct re4_pmd_mesh_s {
    re4_i32 texture_index;
    re4_u32 primitive_magic;
    re4_u32 index_count;
    re4_u32 vertex_count;
    re4_u32 index_offset;
    re4_u32 vertex_offset;
    re4_u32 byte_size;
} re4_pmd_mesh;

typedef struct re4_pmd_node_s {
    re4_i32 skeleton_index;
    re4_u32 mesh_count;
    re4_pmd_mesh *meshes;
    re4_u32 node_bone_count;
    re4_u32 node_bone_offset; /* start of 72-byte inverse-bind entries */
} re4_pmd_node;

typedef struct re4_pmd_material_s {
    re4_q16 params[17];
    re4_i32 texture_enabled;
    re4_strref texture_name;
} re4_pmd_material;

typedef struct re4_pmd_model_s {
    re4_u32 skeleton_name_count;
    re4_pmd_name *skeleton_names;
    re4_u32 node_group_name_count;
    re4_pmd_name *node_group_names;
    re4_u32 skeleton_count;
    re4_pmd_skeleton *skeletons;
    re4_u32 node_count;
    re4_pmd_node *nodes;
    re4_u32 material_count;
    re4_pmd_material *materials;
    re4_u32 total_mesh_count;
    re4_u32 total_vertex_count_hint;
    re4_u32 total_index_count_hint;
} re4_pmd_model;

typedef struct re4_smd_line_s {
    re4_q16 position[3];
    re4_q16 angle[3];
    re4_q16 scale[3];
    re4_u8 bin_file_id;
    re4_u8 tpl_file_id;
    re4_u8 fixed_ff;
    re4_u8 smx_id;
    re4_u32 unused[7];
    re4_u32 object_status;
} re4_smd_line;

typedef struct re4_smd_bone_rule_s {
    re4_strref old_name;
    re4_i32 new_id;
    re4_strref new_name;
    re4_i32 parent_id;
} re4_smd_bone_rule;

typedef struct re4_smd_room_s {
    re4_u16 magic;
    re4_u16 line_count;
    re4_u32 offset_bin_array;
    re4_u32 offset_tpl_array;
    re4_u32 first_bin_or_file_end;
    re4_u32 extra_count;
    re4_u32 *extra_parameters;
    re4_smd_line *lines;
    re4_endian endian;
} re4_smd_room;

typedef struct re4_smx_record_s {
    re4_u8 use_smx_id;
    re4_u8 mode;
    re4_u8 opacity_hierarchy;
    re4_u8 face_culling;
    re4_u32 light_switch;
    re4_u8 alpha_hierarchy;
    re4_u8 unknown_x09;
    re4_u8 unknown_x0a;
    re4_u8 unknown_x0b;
    re4_u8 color_rgb[3];
    re4_u8 color_alpha;
    re4_u32 specular_or_unknown;
    re4_q16 texture_movement[2];
    re4_u32 raw_words[27];
} re4_smx_record;

typedef struct re4_smx_table_s {
    re4_u32 record_count;
    re4_smx_record *records;
    re4_endian endian;
} re4_smx_table;

re4_err re4_bin_model_read(re4_span src, re4_endian endian, re4_arena *arena, re4_bin_model *out);
re4_err re4_bin_material_core_read_uhd24(re4_span src, re4_bin_material_core *out);
re4_err re4_bin_material_core_read_ps2_12(re4_span src, re4_bin_material_core *out);
re4_bool re4_bin_material_core_has(const re4_bin_material_core *m, re4_u8 flag);
const char *re4_bin_material_flag_label(re4_u8 flag);
re4_err re4_bin_material_core_emit_idxmaterial(const re4_bin_material_core *m, re4_u32 index,
                                               re4_emit_fn emit, void *emit_user);
re4_err re4_idxmaterial_read_text(re4_span text, re4_arena *arena, re4_idxmaterial *out);
re4_err re4_idxmaterial_emit_text(const re4_idxmaterial *idx, re4_emit_fn emit, void *emit_user);
re4_err re4_mtl_texture_ref_parse(re4_strref line, re4_mtl_texture_ref *out);
re4_err re4_mtl_read_text(re4_span text, re4_arena *arena, re4_mtl *out);
re4_err re4_mtl_collect_idxuhdtpl(const re4_mtl *mtl, re4_u32 width, re4_u32 height,
                                  re4_arena *arena, struct re4_idxuhdtpl_s *out);
re4_err re4_mtl_material_make_core(const re4_mtl_material *mat,
                                   const struct re4_idxuhdtpl_s *tpl_order,
                                   re4_bin_material_core *out);
re4_err re4_idxmaterial_from_mtl(const re4_mtl *mtl,
                                 const struct re4_idxuhdtpl_s *tpl_order,
                                 re4_arena *arena, re4_idxmaterial *out);
re4_u8 re4_mtl_specular_scale_pack(re4_u32 x_units, re4_u32 y_units);
void re4_mtl_specular_scale_unpack(re4_u8 packed, re4_u32 *x_units, re4_u32 *y_units);
re4_u32 re4_mtl_pixel_format_type_for_ref(const re4_mtl_texture_ref *ref);
re4_err re4_scenario_group_name_parse(re4_strref name, re4_scenario_group_ref *out);
const char *re4_scenario_prefix_name(re4_scenario_prefix prefix);
const char *re4_scenario_status_flag_label(re4_u32 flag);
re4_err re4_scenario_idx_options_read_text(re4_span text, re4_scenario_idx_options *out);

re4_err re4_scenario_idx_read_text(re4_span text, re4_arena *arena, re4_scenario_idx_doc *out);
re4_bool re4_scenario_idx_entry_has_status(const re4_scenario_idx_entry *entry, re4_u32 flag);
re4_bool re4_idxmaterial_entry_is_transparent(const re4_idxmaterial_entry *entry);
re4_err re4_idxmaterial_validate_order(const re4_idxmaterial *idx, re4_idxmaterial_order_report *out);
re4_err re4_idxmaterial_make_order_plan(const re4_idxmaterial *idx, re4_arena *arena, re4_u32 **out_indices, re4_u32 *out_count);

re4_err re4_idxps2bin_read_text(re4_span text, re4_arena *arena, re4_idxps2bin *out);
re4_err re4_idxps2bin_find_first(const re4_idxps2bin *doc, const char *key, re4_idxuubin_kv *out);
re4_err re4_mtl_ps2_texture_id_from_path(re4_strref path, re4_u32 *out_texture_id);
re4_u32 re4_scenario_alignment_for_prefix(re4_scenario_prefix prefix);
re4_err re4_scenario_r100_read_text(re4_span text, re4_arena *arena, re4_scenario_r100 *out);
re4_err re4_idxuubin_read_text(re4_span text, re4_arena *arena, re4_idxuubin *out);
re4_err re4_idxuubin_find_first(const re4_idxuubin *doc, const char *key, re4_idxuubin_kv *out);

re4_err re4_bin_vertex_read(re4_span src, const re4_bin_model *model, re4_u32 index, re4_bin_vertex *out);
re4_err re4_bin_position_q16(re4_span src, const re4_bin_model *model, re4_u32 index, re4_q16 out_xyz[3]);
re4_err re4_bin_material_strip_span(re4_span src, const re4_bin_material *mat, re4_span *out);
re4_bool re4_bin_header_has_flag(const re4_bin_header *h, re4_u32 flag);

/* PMD 2007 static-mesh extraction, fixed-point (Q16.16), caller-owned buffers.
   These represent the mesh's bind-pose data; animations and compositing
   accessory PMDs are independent tasks. */
typedef struct re4_pmd_vertex_q16_s {
    re4_q16 position[3];
    re4_q16 weights[2];
    re4_q16 bone_indices[2];
    re4_q16 normal[3];
    re4_q16 uv[2];
    re4_q16 color_rgba[4];
} re4_pmd_vertex_q16;

typedef struct re4_pmd_triangle_s {
    re4_u16 vertex[3];
} re4_pmd_triangle;

re4_err re4_pmd_mesh_vertex(re4_span src, const re4_pmd_mesh *mesh,
                            re4_u32 index, re4_pmd_vertex_q16 *out);
re4_err re4_pmd_mesh_triangle(re4_span src, const re4_pmd_mesh *mesh,
                              re4_u32 triangle_index, re4_pmd_triangle *out);

/* Normalize a PMD texture reference to a bounded, safe, lowercase basename. */
re4_err re4_pmd_texture_key(re4_strref name, char *out, re4_size capacity);
re4_err re4_pmd_read_survey(re4_span src, re4_arena *arena, re4_pmd_model *out);
re4_err re4_smd_room_read(re4_span src, re4_endian endian, re4_arena *arena, re4_smd_room *out);
re4_err re4_smd_room_bin_table_span(re4_span src, const re4_smd_room *room, re4_span *out);
re4_err re4_smd_room_tpl_table_span(re4_span src, const re4_smd_room *room, re4_span *out);
re4_err re4_smd_room_payload_tail_span(re4_span src, const re4_smd_room *room, re4_span *out);
re4_bool re4_smd_line_references_tpl(const re4_smd_line *line);
re4_bool re4_smd_line_references_smx(const re4_smd_line *line);
re4_err re4_smd_bone_id_from_name(re4_strref name, re4_u32 *out_id);
re4_err re4_smd_replacebone_line_parse(re4_strref line, re4_smd_bone_rule *out);
re4_err re4_smx_table_read(re4_span src, re4_endian endian, re4_arena *arena, re4_smx_table *out);
re4_err re4_smx_record_read(re4_span src, re4_endian endian, re4_smx_record *out);

const char *re4_model_kind_name(re4_model_kind kind);
re4_err re4_pmd_probe(re4_span src, re4_model_probe *out);
re4_err re4_bin_model_probe(re4_span src, re4_model_probe *out);
re4_err re4_smd_room_probe(re4_span src, re4_model_probe *out);
re4_err re4_smx_probe(re4_span src, re4_model_probe *out);

#ifdef __cplusplus
}
#endif

#endif
