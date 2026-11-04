#ifndef P3_BAM_IR_H
#define P3_BAM_IR_H

#include "p3_bam_scene.h"
#include "p3_config.h"
#include "p3_fixed.h"
#include "p3_types.h"

typedef struct p3_ir_texture {
    p3_u32 object_id;
    char name[P3_BAM_SPECIAL_TEXT_CAP];
    char filename[P3_BAM_SPECIAL_TEXT_CAP];
    char alpha_filename[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u16 width;
    p3_u16 height;
    p3_u16 depth;
    p3_u16 num_views;
    p3_u8 texture_type;
    p3_u8 format;
    p3_u8 num_components;
    p3_u8 component_type;
    p3_u8 component_width;
    p3_u8 compression;
    p3_u8 quality_level;
    p3_u8 usage_hint;
    p3_u8 auto_texture_scale;
    p3_u8 ram_image_compression;
    p3_u8 has_clear_color;
    p3_fixed clear_color[4];
    p3_bam_sampler_state sampler;
} p3_ir_texture;

typedef struct p3_ir_material {
    p3_u32 object_id;
    char name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u32 flags;
    p3_fixed base_color[4];
    p3_fixed diffuse[4];
    p3_fixed emission[4];
    p3_fixed metallic;
    p3_fixed roughness_or_shininess;
} p3_ir_material;

typedef struct p3_ir_joint {
    p3_u32 object_id;
    char name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_s16 parent_index;
    p3_fixed bind_inverse[16];
} p3_ir_joint;

typedef struct p3_ir_vertex {
    p3_fixed position[3];
    p3_fixed normal[3];
    p3_fixed uv[2];
    p3_u8 has_color;
    p3_fixed color[4];
    p3_u8 has_tangent;
    p3_fixed tangent[4];
    p3_u8 has_binormal;
    p3_fixed binormal[3];
    p3_u8 joint_count;
    p3_u16 joint_index[4];
    p3_fixed joint_weight[4];
} p3_ir_vertex;

typedef struct p3_ir_submesh_texture {
    p3_s16 texture_index;
    p3_u32 texture_stage_object_id;
    char texcoord_name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_s32 stage_priority;
    p3_s32 stage_sort;
    p3_s32 implicit_sort;
    p3_s32 override_value;
    p3_u16 render_order;
    p3_u8 is_fixed_function;
    p3_s16 fixed_function_texcoord_index;
    p3_u8 has_sampler;
    p3_u8 sampler_source;
    p3_bam_sampler_state sampler;
    char stage_name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u8 stage_mode;
    p3_fixed stage_color[4];
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
    p3_u16 combine_rgb_pack;
    p3_u16 combine_alpha_pack;
    p3_u8 has_tex_matrix;
    p3_s32 tex_matrix_override;
    p3_fixed tex_matrix[16];
    p3_u8 has_tex_gen;
    p3_u8 tex_gen_mode;
} p3_ir_submesh_texture;

typedef struct p3_ir_submesh {
    char name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u32 geom_object_id;
    p3_u32 node_object_id;
    p3_u16 first_vertex;
    p3_u16 vertex_count;
    p3_u16 first_index;
    p3_u16 index_count;
    p3_s16 material_index;
    p3_s16 texture_index;
    p3_u32 texture_stage_object_id;
    char texcoord_name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u16 texture_count;
    p3_ir_submesh_texture textures[P3_BAM_IR_MAX_SUBMESH_TEXTURES];
    p3_u8 has_color_attrib;
    p3_u8 color_type;
    p3_fixed flat_color[4];
    p3_u8 has_color_scale;
    p3_u8 color_scale_off;
    p3_fixed color_scale[4];
    p3_u8 color_scale_has_scale;
    p3_u8 color_scale_has_rgb_scale;
    p3_u8 color_scale_has_alpha_scale;
} p3_ir_submesh;

typedef struct p3_bam_ir {
    p3_u16 texture_count;
    p3_ir_texture textures[P3_BAM_IR_MAX_TEXTURES];
    p3_u16 material_count;
    p3_ir_material materials[P3_BAM_IR_MAX_MATERIALS];
    p3_u16 joint_count;
    p3_ir_joint joints[P3_BAM_IR_MAX_JOINTS];
    p3_u16 vertex_count;
    p3_ir_vertex vertices[P3_BAM_IR_MAX_VERTICES];
    p3_u16 index_count;
    p3_u16 indices[P3_BAM_IR_MAX_INDICES];
    p3_u16 submesh_count;
    p3_ir_submesh submeshes[P3_BAM_IR_MAX_SUBMESHES];
} p3_bam_ir;

void p3_bam_ir_init(p3_bam_ir *ir);
int p3_bam_extract_ir(const p3_bam_archive *archive, p3_bam_ir *out_ir);

#endif
