#ifndef P3_BAM_SCENE_H
#define P3_BAM_SCENE_H

#include "p3_bam_archive.h"
#include "p3_config.h"
#include "p3_fixed.h"
#include "p3_types.h"

#define P3_TRANSFORM_F_IS_IDENTITY      0x00000001UL
#define P3_TRANSFORM_F_IS_SINGULAR      0x00000002UL
#define P3_TRANSFORM_F_SINGULAR_KNOWN   0x00000004UL
#define P3_TRANSFORM_F_COMPONENTS_GIVEN 0x00000008UL
#define P3_TRANSFORM_F_COMPONENTS_KNOWN 0x00000010UL
#define P3_TRANSFORM_F_HAS_COMPONENTS   0x00000020UL
#define P3_TRANSFORM_F_MAT_KNOWN        0x00000040UL
#define P3_TRANSFORM_F_IS_INVALID       0x00000080UL
#define P3_TRANSFORM_F_QUAT_GIVEN       0x00000100UL
#define P3_TRANSFORM_F_QUAT_KNOWN       0x00000200UL
#define P3_TRANSFORM_F_HPR_GIVEN        0x00000400UL
#define P3_TRANSFORM_F_HPR_KNOWN        0x00000800UL
#define P3_TRANSFORM_F_UNIFORM_SCALE    0x00001000UL
#define P3_TRANSFORM_F_IDENTITY_SCALE   0x00002000UL
#define P3_TRANSFORM_F_HAS_NONZERO_SHEAR 0x00004000UL
#define P3_TRANSFORM_F_IS_2D            0x00010000UL
#define P3_TRANSFORM_F_NORM_QUAT_KNOWN  0x00040000UL

#define P3_MATERIAL_F_AMBIENT           0x001
#define P3_MATERIAL_F_DIFFUSE           0x002
#define P3_MATERIAL_F_SPECULAR          0x004
#define P3_MATERIAL_F_EMISSION          0x008
#define P3_MATERIAL_F_LOCAL             0x010
#define P3_MATERIAL_F_TWOSIDE           0x020
#define P3_MATERIAL_F_ATTRIB_LOCK       0x040
#define P3_MATERIAL_F_ROUGHNESS         0x080
#define P3_MATERIAL_F_METALLIC          0x100
#define P3_MATERIAL_F_BASE_COLOR        0x200
#define P3_MATERIAL_F_REFRACTIVE_INDEX  0x400
#define P3_MATERIAL_F_AUTO_SHADER       0x800

#define P3_TEXTURE_STAGE_COMBINE_OPERANDS 3U

enum {
    P3_TEXTURE_TT_1D_TEXTURE = 0,
    P3_TEXTURE_TT_2D_TEXTURE = 1,
    P3_TEXTURE_TT_3D_TEXTURE = 2,
    P3_TEXTURE_TT_2D_TEXTURE_ARRAY = 3,
    P3_TEXTURE_TT_CUBE_MAP = 4,
    P3_TEXTURE_TT_BUFFER_TEXTURE = 5,
    P3_TEXTURE_TT_CUBE_MAP_ARRAY = 6,
    P3_TEXTURE_TT_1D_TEXTURE_ARRAY = 7
};

enum {
    P3_TEXTURE_STAGE_M_MODULATE = 0,
    P3_TEXTURE_STAGE_M_DECAL = 1,
    P3_TEXTURE_STAGE_M_BLEND = 2,
    P3_TEXTURE_STAGE_M_REPLACE = 3,
    P3_TEXTURE_STAGE_M_ADD = 4,
    P3_TEXTURE_STAGE_M_COMBINE = 5,
    P3_TEXTURE_STAGE_M_BLEND_COLOR_SCALE = 6,
    P3_TEXTURE_STAGE_M_MODULATE_GLOW = 7,
    P3_TEXTURE_STAGE_M_MODULATE_GLOSS = 8,
    P3_TEXTURE_STAGE_M_NORMAL = 9,
    P3_TEXTURE_STAGE_M_NORMAL_HEIGHT = 10,
    P3_TEXTURE_STAGE_M_GLOW = 11,
    P3_TEXTURE_STAGE_M_GLOSS = 12,
    P3_TEXTURE_STAGE_M_HEIGHT = 13,
    P3_TEXTURE_STAGE_M_SELECTOR = 14,
    P3_TEXTURE_STAGE_M_NORMAL_GLOSS = 15,
    P3_TEXTURE_STAGE_M_EMISSION = 16
};

enum {
    P3_TEXTURE_STAGE_CM_UNDEFINED = 0,
    P3_TEXTURE_STAGE_CM_REPLACE = 1,
    P3_TEXTURE_STAGE_CM_MODULATE = 2,
    P3_TEXTURE_STAGE_CM_ADD = 3,
    P3_TEXTURE_STAGE_CM_ADD_SIGNED = 4,
    P3_TEXTURE_STAGE_CM_INTERPOLATE = 5,
    P3_TEXTURE_STAGE_CM_SUBTRACT = 6,
    P3_TEXTURE_STAGE_CM_DOT3_RGB = 7,
    P3_TEXTURE_STAGE_CM_DOT3_RGBA = 8
};

enum {
    P3_TEXTURE_STAGE_CS_UNDEFINED = 0,
    P3_TEXTURE_STAGE_CS_TEXTURE = 1,
    P3_TEXTURE_STAGE_CS_CONSTANT = 2,
    P3_TEXTURE_STAGE_CS_PRIMARY_COLOR = 3,
    P3_TEXTURE_STAGE_CS_PREVIOUS = 4,
    P3_TEXTURE_STAGE_CS_CONSTANT_COLOR_SCALE = 5,
    P3_TEXTURE_STAGE_CS_LAST_SAVED_RESULT = 6
};

enum {
    P3_TEXTURE_STAGE_CO_UNDEFINED = 0,
    P3_TEXTURE_STAGE_CO_SRC_COLOR = 1,
    P3_TEXTURE_STAGE_CO_ONE_MINUS_SRC_COLOR = 2,
    P3_TEXTURE_STAGE_CO_SRC_ALPHA = 3,
    P3_TEXTURE_STAGE_CO_ONE_MINUS_SRC_ALPHA = 4
};

enum {
    P3_BAM_SCENE_UNKNOWN = 0,
    P3_BAM_SCENE_RENDER_STATE,
    P3_BAM_SCENE_TRANSFORM_STATE,
    P3_BAM_SCENE_TEXTURE_STAGE,
    P3_BAM_SCENE_TEXTURE,
    P3_BAM_SCENE_MATERIAL,
    P3_BAM_SCENE_TEXTURE_ATTRIB,
    P3_BAM_SCENE_MATERIAL_ATTRIB,
    P3_BAM_SCENE_TRANSFORM_TABLE,
    P3_BAM_SCENE_TRANSFORM_BLEND,
    P3_BAM_SCENE_TRANSFORM_BLEND_TABLE,
    P3_BAM_SCENE_JOINT_VERTEX_TRANSFORM,
    P3_BAM_SCENE_CHARACTER_JOINT,
    P3_BAM_SCENE_COLOR_ATTRIB,
    P3_BAM_SCENE_COLOR_SCALE_ATTRIB,
    P3_BAM_SCENE_TEX_MATRIX_ATTRIB,
    P3_BAM_SCENE_TEX_GEN_ATTRIB
};

typedef struct p3_bam_sampler_state {
    p3_u8 wrap_u;
    p3_u8 wrap_v;
    p3_u8 wrap_w;
    p3_u8 minfilter;
    p3_u8 magfilter;
    p3_u16 anisotropic_degree;
    p3_fixed border_color[4];
    p3_fixed min_lod;
    p3_fixed max_lod;
    p3_fixed lod_bias;
} p3_bam_sampler_state;

typedef struct p3_bam_render_state_entry {
    p3_u32 attrib_object_id;
    p3_s32 override_value;
} p3_bam_render_state_entry;

typedef struct p3_bam_render_state {
    p3_u16 attrib_count;
    p3_u16 total_attrib_count;
    p3_bam_render_state_entry attribs[P3_BAM_SCENE_MAX_ATTRIBS];
} p3_bam_render_state;

typedef struct p3_bam_transform_state {
    p3_u32 flags;
    p3_u8 representation;
    p3_fixed pos[3];
    p3_fixed quat[4];
    p3_fixed hpr[3];
    p3_fixed scale[3];
    p3_fixed shear[3];
    p3_fixed mat[16];
} p3_bam_transform_state;

typedef struct p3_bam_texture_stage {
    p3_u8 is_default_stage;
    char name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_s32 sort;
    p3_s32 priority;
    p3_u32 texcoord_name_object_id;
    char texcoord_name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u8 mode;
    p3_fixed color[4];
    p3_u8 rgb_scale;
    p3_u8 alpha_scale;
    p3_u8 saved_result;
    p3_s32 tex_view_offset;
    p3_u8 combine_rgb_mode;
    p3_u8 num_combine_rgb_operands;
    p3_u8 combine_rgb_source[P3_TEXTURE_STAGE_COMBINE_OPERANDS];
    p3_u8 combine_rgb_operand[P3_TEXTURE_STAGE_COMBINE_OPERANDS];
    p3_u8 combine_alpha_mode;
    p3_u8 num_combine_alpha_operands;
    p3_u8 combine_alpha_source[P3_TEXTURE_STAGE_COMBINE_OPERANDS];
    p3_u8 combine_alpha_operand[P3_TEXTURE_STAGE_COMBINE_OPERANDS];
    p3_u8 involves_color_scale;
    p3_u8 uses_color;
    p3_u8 uses_primary_color;
    p3_u8 uses_last_saved_result;
} p3_bam_texture_stage;

typedef struct p3_bam_texture {
    char name[P3_BAM_SPECIAL_TEXT_CAP];
    char filename[P3_BAM_SPECIAL_TEXT_CAP];
    char alpha_filename[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u8 primary_file_num_channels;
    p3_u8 alpha_file_channel;
    p3_u8 has_rawdata;
    p3_u8 texture_type;
    p3_u8 has_read_mipmaps;
    p3_bam_sampler_state sampler;
    p3_u8 compression;
    p3_u8 quality_level;
    p3_u8 format;
    p3_u8 num_components;
    p3_u8 usage_hint;
    p3_u8 auto_texture_scale;
    p3_u16 orig_file_x;
    p3_u16 orig_file_y;
    p3_u16 size_x;
    p3_u16 size_y;
    p3_u16 size_z;
    p3_u16 pad_xsize;
    p3_u16 pad_ysize;
    p3_u16 pad_zsize;
    p3_u16 num_views;
    p3_u8 component_type;
    p3_u8 component_width;
    p3_u8 ram_image_compression;
    p3_u8 ram_image_count;
    p3_u32 ram_image_page_size;
    p3_u32 ram_image_size;
    p3_u8 ram_image_preview_size;
    p3_u8 ram_image_preview[P3_BAM_SPECIAL_DATA_PREVIEW];
    p3_u8 has_clear_color;
    p3_fixed clear_color[4];
} p3_bam_texture;

typedef struct p3_bam_material {
    char name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u32 flags;
    p3_fixed base_color[4];
    p3_fixed ambient[4];
    p3_fixed diffuse[4];
    p3_fixed specular[4];
    p3_fixed emission[4];
    p3_fixed metallic;
    p3_fixed roughness;
    p3_fixed shininess;
    p3_fixed refractive_index;
} p3_bam_material;

typedef struct p3_bam_texture_attrib_stage {
    p3_u32 stage_object_id;
    p3_u32 texture_object_id;
    p3_s32 implicit_sort;
    p3_s32 override_value;
    p3_u8 has_sampler;
    p3_bam_sampler_state sampler;
} p3_bam_texture_attrib_stage;

typedef struct p3_bam_texture_attrib {
    p3_u8 off_all_stages;
    p3_u16 off_stage_count;
    p3_u16 total_off_stage_count;
    p3_u16 on_stage_count;
    p3_u16 total_on_stage_count;
    p3_u32 off_stage_object_ids[P3_BAM_SCENE_MAX_OFF_STAGES];
    p3_bam_texture_attrib_stage on_stages[P3_BAM_SCENE_MAX_ON_STAGES];
} p3_bam_texture_attrib;

typedef struct p3_bam_material_attrib {
    p3_u32 material_object_id;
} p3_bam_material_attrib;

enum {
    P3_COLOR_ATTRIB_T_VERTEX = 0,
    P3_COLOR_ATTRIB_T_FLAT = 1,
    P3_COLOR_ATTRIB_T_OFF = 2
};

enum {
    P3_TEX_GEN_M_OFF = 0,
    P3_TEX_GEN_M_EYE_SPHERE_MAP = 1,
    P3_TEX_GEN_M_WORLD_CUBE_MAP = 2,
    P3_TEX_GEN_M_EYE_CUBE_MAP = 3,
    P3_TEX_GEN_M_WORLD_NORMAL = 4,
    P3_TEX_GEN_M_EYE_NORMAL = 5,
    P3_TEX_GEN_M_WORLD_POSITION = 6,
    P3_TEX_GEN_M_UNUSED = 7,
    P3_TEX_GEN_M_EYE_POSITION = 8,
    P3_TEX_GEN_M_POINT_SPRITE = 9,
    P3_TEX_GEN_M_UNUSED2 = 10,
    P3_TEX_GEN_M_CONSTANT = 11
};

typedef struct p3_bam_color_attrib {
    p3_u8 color_type;
    p3_fixed color[4];
} p3_bam_color_attrib;

typedef struct p3_bam_color_scale_attrib {
    p3_u8 off;
    p3_fixed scale[4];
    p3_u8 has_scale;
    p3_u8 has_rgb_scale;
    p3_u8 has_alpha_scale;
} p3_bam_color_scale_attrib;

typedef struct p3_bam_tex_matrix_attrib_stage {
    p3_u32 stage_object_id;
    p3_u32 transform_object_id;
    p3_s32 override_value;
} p3_bam_tex_matrix_attrib_stage;

typedef struct p3_bam_tex_matrix_attrib {
    p3_u16 stage_count;
    p3_u16 total_stage_count;
    p3_bam_tex_matrix_attrib_stage stages[P3_BAM_SCENE_MAX_TEX_MATRIX_STAGES];
} p3_bam_tex_matrix_attrib;

typedef struct p3_bam_tex_gen_attrib_stage {
    p3_u32 stage_object_id;
    p3_u8 mode;
} p3_bam_tex_gen_attrib_stage;

typedef struct p3_bam_tex_gen_attrib {
    p3_u16 stage_count;
    p3_u16 total_stage_count;
    p3_bam_tex_gen_attrib_stage stages[P3_BAM_SCENE_MAX_TEX_GEN_STAGES];
} p3_bam_tex_gen_attrib;

typedef struct p3_bam_transform_table {
    p3_u16 transform_count;
    p3_u16 total_transform_count;
    p3_u32 transform_object_ids[P3_BAM_SCENE_MAX_TRANSFORMS];
} p3_bam_transform_table;

typedef struct p3_bam_transform_blend_entry {
    p3_u32 transform_object_id;
    p3_fixed weight;
} p3_bam_transform_blend_entry;

typedef struct p3_bam_transform_blend {
    p3_u16 entry_count;
    p3_u16 total_entry_count;
    p3_bam_transform_blend_entry entries[P3_BAM_SCENE_MAX_BLEND_ENTRIES];
} p3_bam_transform_blend;

typedef struct p3_bam_transform_blend_table {
    p3_u16 blend_count;
    p3_u16 total_blend_count;
    p3_u16 rows;
    p3_bam_transform_blend blends[P3_BAM_SCENE_MAX_BLENDS];
} p3_bam_transform_blend_table;

typedef struct p3_bam_joint_vertex_transform {
    p3_u32 joint_object_id;
} p3_bam_joint_vertex_transform;

typedef struct p3_bam_character_joint {
    char name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u16 child_count;
    p3_u16 total_child_count;
    p3_u32 child_object_ids[P3_BAM_SPECIAL_MAX_CHILDREN];
    p3_u32 forced_channel_object_id;
    p3_u32 character_object_id;
    p3_u16 net_node_count;
    p3_u16 total_net_node_count;
    p3_u32 net_node_object_ids[P3_BAM_SPECIAL_MAX_CHILDREN];
    p3_u16 local_node_count;
    p3_u16 total_local_node_count;
    p3_u32 local_node_object_ids[P3_BAM_SPECIAL_MAX_CHILDREN];
    p3_fixed initial_net_inverse[16];
} p3_bam_character_joint;

typedef struct p3_bam_scene_object {
    p3_u8 class_kind;
    p3_u8 record_kind;
    p3_u16 record_index;
    p3_u16 type_id;
    p3_u32 object_id;
    char type_name[P3_BAM_SPECIAL_TEXT_CAP];
    union {
        p3_bam_render_state render_state;
        p3_bam_transform_state transform_state;
        p3_bam_texture_stage texture_stage;
        p3_bam_texture texture;
        p3_bam_material material;
        p3_bam_texture_attrib texture_attrib;
        p3_bam_material_attrib material_attrib;
        p3_bam_transform_table transform_table;
        p3_bam_transform_blend transform_blend;
        p3_bam_transform_blend_table transform_blend_table;
        p3_bam_joint_vertex_transform joint_vertex_transform;
        p3_bam_character_joint character_joint;
        p3_bam_color_attrib color_attrib;
        p3_bam_color_scale_attrib color_scale_attrib;
        p3_bam_tex_matrix_attrib tex_matrix_attrib;
        p3_bam_tex_gen_attrib tex_gen_attrib;
    } u;
} p3_bam_scene_object;

void p3_bam_scene_object_init(p3_bam_scene_object *obj);
const char *p3_bam_scene_class_name(p3_u8 class_kind);
int p3_bam_scene_parse_object(const p3_bam_archive *archive, p3_u16 record_index, p3_bam_scene_object *out_obj);

int p3_bam_build_sampler_state_payload(const p3_bam_sampler_state *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_render_state_payload(const p3_bam_render_state *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_transform_state_payload(const p3_bam_transform_state *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_texture_stage_payload(const p3_bam_texture_stage *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor);
int p3_bam_build_texture_payload(const p3_bam_texture *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor);
int p3_bam_build_material_payload(const p3_bam_material *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor);
int p3_bam_build_texture_attrib_payload(const p3_bam_texture_attrib *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor);
int p3_bam_build_material_attrib_payload(const p3_bam_material_attrib *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_color_attrib_payload(const p3_bam_color_attrib *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_color_scale_attrib_payload(const p3_bam_color_scale_attrib *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_tex_matrix_attrib_payload(const p3_bam_tex_matrix_attrib *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor);
int p3_bam_build_tex_gen_attrib_payload(const p3_bam_tex_gen_attrib *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_transform_table_payload(const p3_bam_transform_table *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_transform_blend_payload(const p3_bam_transform_blend *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_transform_blend_table_payload(const p3_bam_transform_blend_table *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_joint_vertex_transform_payload(const p3_bam_joint_vertex_transform *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_character_joint_payload(const p3_bam_character_joint *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);

#endif
