#ifndef MD4_TYPES_H
#define MD4_TYPES_H

#include "md4_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef signed int md4_i32;
typedef unsigned int md4_u32;
typedef signed short md4_i16;
typedef unsigned short md4_u16;
typedef unsigned char md4_u8;
typedef signed int md4_fx_t;

typedef enum md4_rc_e {
    MD4_RC_OK = 0,
    MD4_RC_BAD_ARGUMENT,
    MD4_RC_BAD_IDENT,
    MD4_RC_BAD_VERSION,
    MD4_RC_TRUNCATED,
    MD4_RC_BAD_OFFSETS,
    MD4_RC_BAD_COUNTS,
    MD4_RC_BAD_LAYOUT,
    MD4_RC_CAPACITY,
    MD4_RC_BUFFER_TOO_SMALL,
    MD4_RC_UNSUPPORTED,
    MD4_RC_INTERNAL
} md4_rc_t;

typedef enum md4_header_mode_e {
    MD4_HEADER_AUTO = 0,
    MD4_HEADER_LEGACY = 1,
    MD4_HEADER_EXTENDED = 2
} md4_header_mode_t;

typedef enum md4_vertex_mode_e {
    MD4_VERT_AUTO = 0,
    MD4_VERT_RUNTIME = 1,
    MD4_VERT_PADDED = 2,
    MD4_VERT_DRAFT = 3
} md4_vertex_mode_t;

typedef struct md4_layout_s {
    md4_header_mode_t header_mode;
    md4_vertex_mode_t vertex_mode;
    md4_i32 frame_has_name;
    md4_i32 has_bone_names;
    md4_i32 vertex_has_rest_pos;
    md4_i32 weights_have_offset;
} md4_layout_t;

typedef struct md4_bone_s {
    md4_fx_t matrix[3][4];
} md4_bone_t;

typedef struct md4_frame_s {
    md4_fx_t bounds[2][3];
    md4_fx_t local_origin[3];
    md4_fx_t radius;
    char name[MD4_CFG_FRAME_NAME_LEN];
    md4_i32 first_bone;
} md4_frame_t;

typedef struct md4_lod_s {
    md4_i32 first_surface;
    md4_i32 num_surfaces;
} md4_lod_t;

typedef struct md4_triangle_s {
    md4_i32 indexes[3];
} md4_triangle_t;

typedef struct md4_weight_s {
    md4_i32 bone_index;
    md4_fx_t bone_weight;
    md4_fx_t offset[3];
} md4_weight_t;

typedef struct md4_vertex_s {
    md4_fx_t rest_pos[3];
    md4_fx_t normal[3];
    md4_fx_t uv[2];
    md4_i32 first_weight;
    md4_i32 num_weights;
} md4_vertex_t;

typedef struct md4_surface_s {
    char name[MD4_CFG_MAX_QPATH];
    char shader[MD4_CFG_MAX_QPATH];
    md4_i32 shader_index;
    md4_i32 first_vertex;
    md4_i32 num_vertices;
    md4_i32 first_triangle;
    md4_i32 num_triangles;
    md4_i32 first_bone_ref;
    md4_i32 num_bone_refs;
    md4_i32 ofs_header_raw;
} md4_surface_t;

typedef struct md4_model_s {
    char name[MD4_CFG_MAX_QPATH];
    md4_layout_t layout;

    md4_i32 num_frames;
    md4_i32 num_bones;
    md4_i32 num_lods;
    md4_i32 num_surfaces;
    md4_i32 num_vertices;
    md4_i32 num_triangles;
    md4_i32 num_weights;
    md4_i32 num_bone_refs;
    md4_i32 num_frame_bones;

    char bone_names[MD4_CFG_MAX_BONES][MD4_CFG_MAX_QPATH];
    md4_frame_t frames[MD4_CFG_MAX_FRAMES];
    md4_bone_t frame_bones[MD4_CFG_MAX_FRAME_BONES];
    md4_lod_t lods[MD4_CFG_MAX_LODS];
    md4_surface_t surfaces[MD4_CFG_MAX_SURFACES];
    md4_vertex_t vertices[MD4_CFG_MAX_VERTICES];
    md4_triangle_t triangles[MD4_CFG_MAX_TRIANGLES];
    md4_weight_t weights[MD4_CFG_MAX_WEIGHTS];
    md4_i32 bone_refs[MD4_CFG_MAX_BONE_REFS];
} md4_model_t;

typedef struct md4_pose_s {
    md4_i32 num_bones;
    md4_bone_t bones[MD4_CFG_MAX_BONES];
} md4_pose_t;

typedef struct md4_decode_opts_s {
    md4_header_mode_t header_mode;
    md4_vertex_mode_t vertex_mode;
    md4_i32 strict;
} md4_decode_opts_t;

typedef struct md4_encode_opts_s {
    md4_header_mode_t header_mode;
    md4_vertex_mode_t vertex_mode;
    md4_i32 write_frame_names;
    md4_i32 write_bone_names;
} md4_encode_opts_t;

typedef struct md4_ir_bone_s {
    char name[MD4_CFG_MAX_QPATH];
} md4_ir_bone_t;

typedef struct md4_ir_frame_s {
    md4_fx_t bounds[2][3];
    md4_fx_t local_origin[3];
    md4_fx_t radius;
    char name[MD4_CFG_FRAME_NAME_LEN];
    md4_i32 first_pose_bone;
} md4_ir_frame_t;

typedef struct md4_ir_pose_bone_s {
    md4_fx_t matrix[3][4];
} md4_ir_pose_bone_t;

typedef struct md4_ir_lod_s {
    md4_i32 first_mesh;
    md4_i32 num_meshes;
} md4_ir_lod_t;

typedef struct md4_ir_triangle_s {
    md4_i32 indexes[3];
} md4_ir_triangle_t;

typedef struct md4_ir_weight_s {
    md4_i32 bone_index;
    md4_fx_t bone_weight;
    md4_fx_t offset[3];
} md4_ir_weight_t;

typedef struct md4_ir_vertex_s {
    md4_fx_t rest_pos[3];
    md4_fx_t normal[3];
    md4_fx_t uv[2];
    md4_i32 first_weight;
    md4_i32 num_weights;
} md4_ir_vertex_t;

typedef struct md4_ir_mesh_s {
    char name[MD4_CFG_MAX_QPATH];
    char material[MD4_CFG_MAX_QPATH];
    md4_i32 material_index;
    md4_i32 first_vertex;
    md4_i32 num_vertices;
    md4_i32 first_triangle;
    md4_i32 num_triangles;
} md4_ir_mesh_t;

typedef struct md4_ir_model_s {
    char name[MD4_CFG_MAX_QPATH];
    md4_i32 has_frame_names;
    md4_i32 has_bone_names;
    md4_i32 has_rest_positions;
    md4_i32 has_weight_offsets;

    md4_i32 num_frames;
    md4_i32 num_bones;
    md4_i32 num_lods;
    md4_i32 num_meshes;
    md4_i32 num_vertices;
    md4_i32 num_triangles;
    md4_i32 num_weights;
    md4_i32 num_pose_bones;

    md4_ir_bone_t bones[MD4_CFG_MAX_BONES];
    md4_ir_frame_t frames[MD4_CFG_MAX_FRAMES];
    md4_ir_pose_bone_t pose_bones[MD4_CFG_MAX_FRAME_BONES];
    md4_ir_lod_t lods[MD4_CFG_MAX_LODS];
    md4_ir_mesh_t meshes[MD4_CFG_MAX_SURFACES];
    md4_ir_vertex_t vertices[MD4_CFG_MAX_VERTICES];
    md4_ir_triangle_t triangles[MD4_CFG_MAX_TRIANGLES];
    md4_ir_weight_t weights[MD4_CFG_MAX_WEIGHTS];
} md4_ir_model_t;

typedef struct md4_ir_export_opts_s {
    md4_header_mode_t header_mode;
    md4_vertex_mode_t vertex_mode;
    md4_i32 write_frame_names;
    md4_i32 write_bone_names;
} md4_ir_export_opts_t;

typedef struct md4_asset_bone_s {
    char name[MD4_CFG_MAX_QPATH];
    md4_i32 parent_index;
} md4_asset_bone_t;

typedef struct md4_asset_skeleton_s {
    md4_i32 num_bones;
    md4_asset_bone_t bones[MD4_CFG_MAX_BONES];
} md4_asset_skeleton_t;

typedef struct md4_asset_mesh_lod_s {
    md4_i32 first_surface;
    md4_i32 num_surfaces;
} md4_asset_mesh_lod_t;

typedef struct md4_asset_mesh_triangle_s {
    md4_i32 indexes[3];
} md4_asset_mesh_triangle_t;

typedef struct md4_asset_mesh_weight_s {
    md4_i32 bone_index;
    md4_fx_t bone_weight;
    md4_fx_t offset[3];
} md4_asset_mesh_weight_t;

typedef struct md4_asset_mesh_vertex_s {
    md4_fx_t rest_pos[3];
    md4_fx_t normal[3];
    md4_fx_t uv[2];
    md4_i32 first_weight;
    md4_i32 num_weights;
} md4_asset_mesh_vertex_t;

typedef struct md4_asset_mesh_surface_s {
    char name[MD4_CFG_MAX_QPATH];
    char material[MD4_CFG_MAX_QPATH];
    md4_i32 material_index;
    md4_i32 first_vertex;
    md4_i32 num_vertices;
    md4_i32 first_triangle;
    md4_i32 num_triangles;
    md4_i32 first_bone_palette;
    md4_i32 num_bone_palette;
} md4_asset_mesh_surface_t;

typedef struct md4_asset_mesh_s {
    md4_i32 num_lods;
    md4_i32 num_surfaces;
    md4_i32 num_vertices;
    md4_i32 num_triangles;
    md4_i32 num_weights;
    md4_i32 num_bone_palette;
    md4_asset_mesh_lod_t lods[MD4_CFG_MAX_LODS];
    md4_asset_mesh_surface_t surfaces[MD4_CFG_MAX_SURFACES];
    md4_asset_mesh_vertex_t vertices[MD4_CFG_MAX_VERTICES];
    md4_asset_mesh_triangle_t triangles[MD4_CFG_MAX_TRIANGLES];
    md4_asset_mesh_weight_t weights[MD4_CFG_MAX_WEIGHTS];
    md4_i32 bone_palette[MD4_CFG_MAX_BONE_REFS];
} md4_asset_mesh_t;

typedef struct md4_asset_anim_pose_bone_s {
    md4_fx_t matrix[3][4];
} md4_asset_anim_pose_bone_t;

typedef struct md4_asset_anim_frame_s {
    md4_fx_t bounds[2][3];
    md4_fx_t local_origin[3];
    md4_fx_t radius;
    char name[MD4_CFG_FRAME_NAME_LEN];
    md4_i32 first_pose_bone;
} md4_asset_anim_frame_t;

typedef struct md4_asset_anim_clip_s {
    char name[MD4_CFG_MAX_QPATH];
    md4_i32 first_frame;
    md4_i32 num_frames;
    md4_i32 frame_rate;
} md4_asset_anim_clip_t;

typedef struct md4_asset_animation_bank_s {
    md4_i32 num_clips;
    md4_i32 num_frames;
    md4_i32 num_pose_bones;
    md4_asset_anim_clip_t clips[MD4_CFG_MAX_ANIM_CLIPS];
    md4_asset_anim_frame_t frames[MD4_CFG_MAX_FRAMES];
    md4_asset_anim_pose_bone_t pose_bones[MD4_CFG_MAX_FRAME_BONES];
} md4_asset_animation_bank_t;

typedef struct md4_asset_ir_model_s {
    char name[MD4_CFG_MAX_QPATH];
    md4_i32 has_frame_names;
    md4_i32 has_bone_names;
    md4_i32 has_rest_positions;
    md4_i32 has_weight_offsets;
    md4_asset_skeleton_t skeleton;
    md4_asset_mesh_t mesh;
    md4_asset_animation_bank_t animations;
} md4_asset_ir_model_t;

typedef struct md4_asset_export_opts_s {
    md4_header_mode_t header_mode;
    md4_vertex_mode_t vertex_mode;
    md4_i32 write_frame_names;
    md4_i32 write_bone_names;
    md4_i32 clip_index;
} md4_asset_export_opts_t;

typedef struct md4_stream_header_info_s {
    char name[MD4_CFG_MAX_QPATH];
    md4_layout_t layout;
    md4_i32 num_frames;
    md4_i32 num_bones;
    md4_i32 num_lods;
    md4_i32 ofs_bone_names;
    md4_i32 ofs_frames;
    md4_i32 ofs_lods;
    md4_i32 ofs_end;
} md4_stream_header_info_t;

typedef struct md4_stream_bone_name_info_s {
    md4_i32 bone_index;
    char name[MD4_CFG_MAX_QPATH];
} md4_stream_bone_name_info_t;

typedef struct md4_stream_frame_info_s {
    md4_i32 frame_index;
    md4_fx_t bounds[2][3];
    md4_fx_t local_origin[3];
    md4_fx_t radius;
    char name[MD4_CFG_FRAME_NAME_LEN];
} md4_stream_frame_info_t;

typedef struct md4_stream_bone_info_s {
    md4_i32 frame_index;
    md4_i32 bone_index;
    md4_fx_t matrix[3][4];
} md4_stream_bone_info_t;

typedef struct md4_stream_lod_info_s {
    md4_i32 lod_index;
    md4_i32 num_surfaces;
} md4_stream_lod_info_t;

typedef struct md4_stream_surface_info_s {
    md4_i32 lod_index;
    md4_i32 surface_index_in_lod;
    md4_i32 surface_index_global;
    char name[MD4_CFG_MAX_QPATH];
    char shader[MD4_CFG_MAX_QPATH];
    md4_i32 shader_index;
    md4_i32 num_vertices;
    md4_i32 num_triangles;
    md4_i32 num_bone_refs;
    md4_i32 ofs_header_raw;
} md4_stream_surface_info_t;

typedef struct md4_stream_triangle_info_s {
    md4_i32 lod_index;
    md4_i32 surface_index_in_lod;
    md4_i32 surface_index_global;
    md4_i32 triangle_index;
    md4_i32 indexes[3];
} md4_stream_triangle_info_t;

typedef struct md4_stream_vertex_info_s {
    md4_i32 lod_index;
    md4_i32 surface_index_in_lod;
    md4_i32 surface_index_global;
    md4_i32 vertex_index_in_surface;
    md4_i32 num_weights;
    md4_fx_t rest_pos[3];
    md4_fx_t normal[3];
    md4_fx_t uv[2];
} md4_stream_vertex_info_t;

typedef struct md4_stream_weight_info_s {
    md4_i32 lod_index;
    md4_i32 surface_index_in_lod;
    md4_i32 surface_index_global;
    md4_i32 vertex_index_in_surface;
    md4_i32 weight_index_in_vertex;
    md4_i32 bone_index;
    md4_fx_t bone_weight;
    md4_fx_t offset[3];
} md4_stream_weight_info_t;

typedef struct md4_stream_bone_ref_info_s {
    md4_i32 lod_index;
    md4_i32 surface_index_in_lod;
    md4_i32 surface_index_global;
    md4_i32 bone_ref_index;
    md4_i32 bone_index;
} md4_stream_bone_ref_info_t;

typedef struct md4_stream_callbacks_s {
    md4_rc_t (*on_header)(void *user, const md4_stream_header_info_t *info);
    md4_rc_t (*on_bone_name)(void *user, const md4_stream_bone_name_info_t *info);
    md4_rc_t (*on_frame)(void *user, const md4_stream_frame_info_t *info);
    md4_rc_t (*on_bone)(void *user, const md4_stream_bone_info_t *info);
    md4_rc_t (*on_lod)(void *user, const md4_stream_lod_info_t *info);
    md4_rc_t (*on_surface)(void *user, const md4_stream_surface_info_t *info);
    md4_rc_t (*on_triangle)(void *user, const md4_stream_triangle_info_t *info);
    md4_rc_t (*on_vertex)(void *user, const md4_stream_vertex_info_t *info);
    md4_rc_t (*on_weight)(void *user, const md4_stream_weight_info_t *info);
    md4_rc_t (*on_bone_ref)(void *user, const md4_stream_bone_ref_info_t *info);
    md4_rc_t (*on_end)(void *user);
} md4_stream_callbacks_t;

typedef md4_rc_t (*md4_stream_sink_fn)(void *user, const md4_u8 *data, md4_u32 size);

typedef struct md4_decoder_s {
    md4_decode_opts_t opts;
    md4_u8 *storage;
    md4_u32 storage_capacity;
    md4_u32 bytes_received;
    md4_u32 expected_size;
    md4_header_mode_t detected_header_mode;
    md4_i32 detected_frame_has_name;
    md4_i32 header_ready;
    md4_i32 complete;
} md4_decoder_t;

typedef struct md4_stream_parser_s {
    md4_decode_opts_t opts;
    md4_stream_callbacks_t callbacks;
    void *user;
    md4_header_mode_t detected_header_mode;
    md4_vertex_mode_t selected_vertex_mode;
    md4_i32 detected_frame_has_name;
    md4_i32 header_ready;
    md4_i32 complete;
    md4_i32 stage;
    md4_i32 num_frames;
    md4_i32 num_bones;
    md4_i32 num_lods;
    md4_i32 ofs_bone_names;
    md4_i32 ofs_frames;
    md4_i32 ofs_lods;
    md4_i32 ofs_end;
    md4_i32 frame_stride;
    md4_i32 frame_has_name;
    md4_i32 frame_base_size;
    md4_i32 vertex_base_size;
    md4_i32 weight_size;
    md4_u32 bytes_consumed;
    md4_u32 expected_size;
    md4_u32 scratch_used;
    md4_u32 scratch_need;
    md4_u8 scratch[MD4_CFG_STREAM_CHUNK];
    md4_i32 bone_name_index;
    md4_i32 frame_index;
    md4_i32 bone_index_in_frame;
    md4_i32 lod_index;
    md4_u32 lod_abs_start;
    md4_u32 lod_abs_end;
    md4_i32 lod_num_surfaces;
    md4_i32 surface_index_in_lod;
    md4_i32 surface_index_global;
    md4_u32 surface_abs_start;
    md4_u32 surface_abs_end;
    md4_u32 tri_abs_start;
    md4_u32 vert_abs_start;
    md4_u32 vertex_block_end_abs;
    md4_u32 bone_ref_abs_start;
    md4_i32 surface_num_vertices;
    md4_i32 surface_num_triangles;
    md4_i32 surface_num_bone_refs;
    md4_i32 triangle_index;
    md4_i32 vertex_index_in_surface;
    md4_u32 vertex_abs_start;
    md4_i32 vertex_num_weights;
    md4_i32 weight_index_in_vertex;
    md4_i32 bone_ref_index;
} md4_stream_parser_t;

typedef struct md4_stream_decode_model_s {
    md4_stream_parser_t parser;
    md4_model_t *model;
} md4_stream_decode_model_t;

typedef struct md4_engine_asset_info_s {
    char name[MD4_CFG_MAX_QPATH];
    md4_layout_t layout;
    md4_i32 num_frames;
    md4_i32 num_bones;
    md4_i32 num_lods;
} md4_engine_asset_info_t;

typedef struct md4_engine_skeleton_info_s {
    md4_i32 num_bones;
    md4_i32 has_bone_names;
} md4_engine_skeleton_info_t;

typedef struct md4_engine_bone_def_info_s {
    md4_i32 bone_index;
    char name[MD4_CFG_MAX_QPATH];
    md4_i32 parent_index;
} md4_engine_bone_def_info_t;

typedef struct md4_engine_animation_info_s {
    md4_i32 num_clips;
    md4_i32 num_frames;
    md4_i32 num_bones;
} md4_engine_animation_info_t;

typedef struct md4_engine_anim_clip_info_s {
    md4_i32 clip_index;
    char name[MD4_CFG_MAX_QPATH];
    md4_i32 first_frame;
    md4_i32 num_frames;
    md4_i32 frame_rate;
} md4_engine_anim_clip_info_t;

typedef struct md4_engine_mesh_info_s {
    md4_i32 num_lods;
} md4_engine_mesh_info_t;

typedef struct md4_engine_stream_callbacks_s {
    md4_rc_t (*on_asset_begin)(void *user, const md4_engine_asset_info_t *info);
    md4_rc_t (*on_skeleton_begin)(void *user, const md4_engine_skeleton_info_t *info);
    md4_rc_t (*on_bone_def)(void *user, const md4_engine_bone_def_info_t *info);
    md4_rc_t (*on_skeleton_end)(void *user, const md4_engine_skeleton_info_t *info);
    md4_rc_t (*on_animation_begin)(void *user, const md4_engine_animation_info_t *info);
    md4_rc_t (*on_anim_clip)(void *user, const md4_engine_anim_clip_info_t *info);
    md4_rc_t (*on_anim_frame_begin)(void *user, const md4_stream_frame_info_t *info);
    md4_rc_t (*on_anim_frame_bone)(void *user, const md4_stream_bone_info_t *info);
    md4_rc_t (*on_anim_frame_end)(void *user, const md4_stream_frame_info_t *info);
    md4_rc_t (*on_animation_end)(void *user, const md4_engine_animation_info_t *info);
    md4_rc_t (*on_mesh_begin)(void *user, const md4_engine_mesh_info_t *info);
    md4_rc_t (*on_lod_begin)(void *user, const md4_stream_lod_info_t *info);
    md4_rc_t (*on_surface_begin)(void *user, const md4_stream_surface_info_t *info);
    md4_rc_t (*on_surface_triangle)(void *user, const md4_stream_triangle_info_t *info);
    md4_rc_t (*on_surface_vertex)(void *user, const md4_stream_vertex_info_t *info);
    md4_rc_t (*on_surface_weight)(void *user, const md4_stream_weight_info_t *info);
    md4_rc_t (*on_surface_bone_ref)(void *user, const md4_stream_bone_ref_info_t *info);
    md4_rc_t (*on_surface_end)(void *user, const md4_stream_surface_info_t *info);
    md4_rc_t (*on_lod_end)(void *user, const md4_stream_lod_info_t *info);
    md4_rc_t (*on_mesh_end)(void *user, const md4_engine_mesh_info_t *info);
    md4_rc_t (*on_asset_end)(void *user, const md4_engine_asset_info_t *info);
} md4_engine_stream_callbacks_t;

typedef struct md4_engine_stream_parser_s {
    md4_stream_parser_t parser;
    md4_engine_stream_callbacks_t callbacks;
    void *user;
    md4_engine_asset_info_t asset_info;
    md4_engine_skeleton_info_t skeleton_info;
    md4_engine_animation_info_t animation_info;
    md4_engine_anim_clip_info_t clip_info;
    md4_engine_mesh_info_t mesh_info;
    md4_stream_frame_info_t current_frame;
    md4_stream_lod_info_t current_lod;
    md4_stream_surface_info_t current_surface;
    md4_i32 skeleton_open;
    md4_i32 skeleton_closed;
    md4_i32 animation_open;
    md4_i32 animation_closed;
    md4_i32 frame_open;
    md4_i32 mesh_open;
    md4_i32 mesh_closed;
    md4_i32 lod_open;
    md4_i32 surface_open;
    md4_i32 next_bone_def_to_emit;
    md4_i32 asset_closed;
} md4_engine_stream_parser_t;

typedef enum md4_writer_phase_e {
    MD4_WR_PHASE_IDLE = 0,
    MD4_WR_PHASE_HEADER,
    MD4_WR_PHASE_BONE_NAME,
    MD4_WR_PHASE_FRAME_BASE,
    MD4_WR_PHASE_FRAME_BONE,
    MD4_WR_PHASE_LOD_HEADER,
    MD4_WR_PHASE_SURFACE_HEADER,
    MD4_WR_PHASE_SURFACE_TRIANGLE,
    MD4_WR_PHASE_SURFACE_VERTEX_BASE,
    MD4_WR_PHASE_SURFACE_VERTEX_WEIGHT,
    MD4_WR_PHASE_SURFACE_BONE_REF,
    MD4_WR_PHASE_DONE
} md4_writer_phase_t;

typedef struct md4_writer_layout_s {
    md4_header_mode_t header_mode;
    md4_vertex_mode_t vertex_mode;
    md4_i32 write_frame_names;
    md4_i32 write_bone_names;
    md4_u32 total_size;
    md4_u32 frames_off;
    md4_u32 lods_off;
    md4_u32 lod_starts[MD4_CFG_MAX_LODS];
    md4_u32 lod_ends[MD4_CFG_MAX_LODS];
    md4_u32 surface_starts[MD4_CFG_MAX_SURFACES];
    md4_u32 surface_triangles[MD4_CFG_MAX_SURFACES];
    md4_u32 surface_vertices[MD4_CFG_MAX_SURFACES];
    md4_u32 surface_bone_refs[MD4_CFG_MAX_SURFACES];
    md4_u32 surface_ends[MD4_CFG_MAX_SURFACES];
} md4_writer_layout_t;

typedef struct md4_writer_s {
    const md4_model_t *model;
    md4_writer_layout_t layout;
    md4_writer_phase_t phase;
    md4_i32 bone_name_index;
    md4_i32 frame_index;
    md4_i32 bone_index_in_frame;
    md4_i32 lod_index;
    md4_i32 surface_index_in_lod;
    md4_i32 current_surface_global;
    md4_i32 triangle_index;
    md4_i32 vertex_index;
    md4_i32 weight_index_in_vertex;
    md4_i32 bone_ref_index;
    md4_u32 bytes_written;
    md4_u32 pending_offset;
    md4_u32 pending_size;
    md4_u8 pending[MD4_CFG_STREAM_CHUNK];
} md4_writer_t;

#ifdef __cplusplus
}
#endif

#endif
