#ifndef USD3D_H
#define USD3D_H

/*
   usd3d_decoder_c89
   Small OpenUSD ASCII / USDA mesh decoder for C89 projects.

   Design rules:
   - no malloc/free/realloc/calloc
   - no heap ownership in the library
   - no float/double arithmetic
   - fixed point Q16.16 values
   - callback-oriented output
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef USD3D_MAX_NAME
#define USD3D_MAX_NAME 96
#endif

#ifndef USD3D_MAX_FACE_COUNTS
#define USD3D_MAX_FACE_COUNTS 8192
#endif

#ifndef USD3D_MAX_FACE_INDICES
#define USD3D_MAX_FACE_INDICES 65536
#endif

#ifndef USD3D_MAX_INDEX_ARRAY
#define USD3D_MAX_INDEX_ARRAY 65536
#endif

#ifndef USD3D_MAX_NGON
#define USD3D_MAX_NGON 64
#endif

#ifndef USD3D_TOKEN_MAX
#define USD3D_TOKEN_MAX 192
#endif

#ifndef USD3D_PATH_MAX
#define USD3D_PATH_MAX 256
#endif

#ifndef USD3D_MAX_DEPTH
#define USD3D_MAX_DEPTH 32
#endif

#ifndef USD3D_MAX_XFORM_ORDER_TEXT
#define USD3D_MAX_XFORM_ORDER_TEXT 384
#endif

#ifndef USD3D_MAX_POINT_INSTANCES
#define USD3D_MAX_POINT_INSTANCES 4096
#endif

#ifndef USD3D_MAX_SKEL_INFLUENCE_VALUES
#define USD3D_MAX_SKEL_INFLUENCE_VALUES 65536
#endif

#ifndef USD3D_MAX_SKIN_POINTS
#define USD3D_MAX_SKIN_POINTS 8192
#endif

#ifndef USD3D_USDZ_ASSET_CHUNK
#define USD3D_USDZ_ASSET_CHUNK 32768UL
#endif

#ifndef USD3D_USDZ_DEFLATE_MAX_COMPRESSED
#define USD3D_USDZ_DEFLATE_MAX_COMPRESSED 1048576UL
#endif

#ifndef USD3D_USDZ_DEFLATE_MAX_OUTPUT
#define USD3D_USDZ_DEFLATE_MAX_OUTPUT 4194304UL
#endif

#define USD3D_FIXED_ONE 65536L
#define USD3D_FIXED_HALF 32768L

/* C89 has no stdint.h. This decoder assumes long is at least 32 bits. */
typedef long usd3d_i32;
typedef unsigned long usd3d_u32;
typedef short usd3d_i16;
typedef unsigned short usd3d_u16;
typedef signed char usd3d_i8;
typedef unsigned char usd3d_u8;
typedef long usd3d_fixed;

typedef struct usd3d_vec2_s {
    usd3d_fixed x;
    usd3d_fixed y;
} usd3d_vec2;

typedef struct usd3d_vec3_s {
    usd3d_fixed x;
    usd3d_fixed y;
    usd3d_fixed z;
} usd3d_vec3;

typedef enum usd3d_result_e {
    USD3D_OK = 0,
    USD3D_ERR_IO = -1,
    USD3D_ERR_BAD_ARG = -2,
    USD3D_ERR_UNSUPPORTED = -3,
    USD3D_ERR_PARSE = -4,
    USD3D_ERR_LIMIT = -5,
    USD3D_ERR_FORMAT = -6
} usd3d_result;

typedef enum usd3d_file_kind_e {
    USD3D_KIND_UNKNOWN = 0,
    USD3D_KIND_USDA_TEXT = 1,
    USD3D_KIND_USDC_BINARY = 2,
    USD3D_KIND_USDZ_PACKAGE = 3
} usd3d_file_kind;

typedef enum usd3d_index_kind_e {
    USD3D_INDEX_UV = 0,
    USD3D_INDEX_NORMAL = 1,
    USD3D_INDEX_COLOR = 2
} usd3d_index_kind;

typedef struct usd3d_stats_s {
    usd3d_u32 meshes;
    usd3d_u32 prims;
    usd3d_u32 materials;
    usd3d_u32 material_bindings;
    usd3d_u32 curves;
    usd3d_u32 curve_counts;
    usd3d_u32 widths;
    usd3d_u32 cameras;
    usd3d_u32 point_instancers;
    usd3d_u32 point_instance_prototypes;
    usd3d_u32 point_instance_positions;
    usd3d_u32 point_instance_orientations;
    usd3d_u32 point_instance_scales;
    usd3d_u32 point_instance_proto_indices;
    usd3d_u32 point_instance_matrices;
    usd3d_u32 geom_subsets;
    usd3d_u32 subset_indices;
    usd3d_u32 lights;
    usd3d_u32 light_attrs;
    usd3d_u32 volumes;
    usd3d_u32 volume_fields;
    usd3d_u32 collections;
    usd3d_u32 collection_targets;
    usd3d_u32 relationships;
    usd3d_u32 asset_paths;
    usd3d_u32 prim_metadata;
    usd3d_u32 list_ops;
    usd3d_u32 composition_arcs;
    usd3d_u32 layer_metadata;
    usd3d_u32 attr_metadata;
    usd3d_u32 matrix_ops;
    usd3d_u32 connections;
    usd3d_u32 variant_sets;
    usd3d_u32 variants;
    usd3d_u32 variant_selections;
    usd3d_u32 value_resolutions;
    usd3d_u32 resolved_layers;
    usd3d_u32 skel_roots;
    usd3d_u32 skeletons;
    usd3d_u32 skel_animations;
    usd3d_u32 blend_shapes;
    usd3d_u32 skin_bindings;
    usd3d_u32 skel_joints;
    usd3d_u32 skel_weights;
    usd3d_u32 skel_influences;
    usd3d_u32 skel_transforms;
    usd3d_u32 materialx_nodes;
    usd3d_u32 mdl_nodes;
    usd3d_u32 usdz_assets;
    usd3d_u32 usdz_asset_chunks;
    usd3d_u32 zip64_entries;
    usd3d_u32 data_descriptor_entries;
    usd3d_u32 usdc_sections;
    usd3d_u32 usdc_fields;
    usd3d_u32 usdc_values;
    usd3d_u32 flattened_values;
    usd3d_u32 variant_pruned;
    usd3d_u32 skinned_points;
    usd3d_u32 material_evals;
    usd3d_u32 volume_probes;
    usd3d_u32 points;
    usd3d_u32 normals;
    usd3d_u32 texcoords;
    usd3d_u32 colors;
    usd3d_u32 indices;
    usd3d_u32 faces;
    usd3d_u32 xform_ops;
    usd3d_u32 time_samples;
    usd3d_u32 warnings;
    usd3d_u32 unsupported;
    usd3d_file_kind kind;
} usd3d_stats;

typedef struct usd3d_callbacks_s {
    int (*begin_prim)(void *user, const char *type_name, const char *name, const char *path);
    int (*end_prim)(void *user, const char *type_name, const char *name, const char *path);

    int (*stage_info)(void *user, const char *key, const char *value);
    int (*stage_metric)(void *user, const char *key, usd3d_fixed value);
    int (*prim_metadata)(void *user, const char *prim_path, const char *key, const char *value);
    int (*relationship)(void *user, const char *prim_path, const char *rel_name, const char *target_path);
    int (*asset_path)(void *user, const char *prim_path, const char *attr_name, const char *asset_path);
    int (*composition_arc)(void *user, const char *prim_path, const char *arc_type, const char *asset_path, const char *target_path);
    int (*attr_metadata)(void *user, const char *prim_path, const char *attr_name, const char *key, const char *value);
    int (*connection)(void *user, const char *prim_path, const char *attr_name, const char *target_path);
    int (*value_resolution)(void *user, const char *prim_path, const char *attr_name, const char *source_layer, const char *rule);
    int (*variant_set_begin)(void *user, const char *prim_path, const char *set_name);
    int (*variant_begin)(void *user, const char *prim_path, const char *set_name, const char *variant_name);
    int (*variant_selection)(void *user, const char *prim_path, const char *set_name, const char *variant_name);
    int (*variant_end)(void *user, const char *prim_path, const char *set_name, const char *variant_name);
    int (*variant_set_end)(void *user, const char *prim_path, const char *set_name);
    int (*resolved_layer)(void *user, const char *requesting_layer, const char *asset_path, const char *resolved_path, const char *arc_type);

    int (*begin_mesh)(void *user, const char *name);
    int (*point)(void *user, usd3d_u32 index, usd3d_fixed x, usd3d_fixed y, usd3d_fixed z);
    int (*normal)(void *user, usd3d_u32 index, usd3d_fixed x, usd3d_fixed y, usd3d_fixed z);
    int (*texcoord)(void *user, usd3d_u32 index, usd3d_fixed s, usd3d_fixed t);
    int (*color)(void *user, usd3d_u32 index, usd3d_fixed r, usd3d_fixed g, usd3d_fixed b);
    int (*index)(void *user, int kind, usd3d_u32 index, usd3d_u32 value);
    int (*face)(void *user, usd3d_u32 face_index, const usd3d_u32 *indices, usd3d_u16 count);
    int (*mesh_info)(void *user, const char *path, const char *subdivision_scheme, int double_sided, const char *orientation);
    int (*extent)(void *user, const char *path, usd3d_fixed min_x, usd3d_fixed min_y, usd3d_fixed min_z, usd3d_fixed max_x, usd3d_fixed max_y, usd3d_fixed max_z);
    int (*end_mesh)(void *user, const char *name);

    int (*xform_translate)(void *user, usd3d_fixed x, usd3d_fixed y, usd3d_fixed z);
    int (*xform_scale)(void *user, usd3d_fixed x, usd3d_fixed y, usd3d_fixed z);
    int (*xform_rotate_xyz_deg)(void *user, usd3d_fixed x, usd3d_fixed y, usd3d_fixed z);
    int (*xform_op3)(void *user, const char *prim_path, const char *op_name, usd3d_fixed x, usd3d_fixed y, usd3d_fixed z);
    int (*xform_op1)(void *user, const char *prim_path, const char *op_name, usd3d_fixed v);
    int (*xform_op_order)(void *user, const char *prim_path, const char *order_text);
    int (*xform_matrix4)(void *user, const char *prim_path, const char *op_name, const usd3d_fixed *m16);

    int (*begin_curves)(void *user, const char *name, const char *path);
    int (*curve_count)(void *user, const char *path, usd3d_u32 curve_index, usd3d_u32 vertex_count);
    int (*width)(void *user, const char *path, usd3d_u32 index, usd3d_fixed width);
    int (*curve_info)(void *user, const char *path, const char *type, const char *basis, const char *wrap);
    int (*end_curves)(void *user, const char *name, const char *path);

    int (*begin_camera)(void *user, const char *name, const char *path);
    int (*camera_scalar)(void *user, const char *path, const char *attr_name, usd3d_fixed v);
    int (*camera_vec2)(void *user, const char *path, const char *attr_name, usd3d_fixed x, usd3d_fixed y);
    int (*end_camera)(void *user, const char *name, const char *path);

    int (*begin_point_instancer)(void *user, const char *name, const char *path);
    int (*point_instancer_prototype)(void *user, const char *path, usd3d_u32 index, const char *target_path);
    int (*point_instancer_proto_index)(void *user, const char *path, usd3d_u32 index, usd3d_u32 proto_index);
    int (*point_instancer_invisible_id)(void *user, const char *path, usd3d_u32 index, usd3d_u32 invisible_id);
    int (*point_instancer_position)(void *user, const char *path, usd3d_u32 index, usd3d_fixed x, usd3d_fixed y, usd3d_fixed z);
    int (*point_instancer_orientation)(void *user, const char *path, usd3d_u32 index, usd3d_fixed i, usd3d_fixed j, usd3d_fixed k, usd3d_fixed w);
    int (*point_instancer_scale)(void *user, const char *path, usd3d_u32 index, usd3d_fixed x, usd3d_fixed y, usd3d_fixed z);
    int (*point_instancer_matrix)(void *user, const char *path, usd3d_u32 index, usd3d_u32 proto_index, const usd3d_fixed *m16);
    int (*end_point_instancer)(void *user, const char *name, const char *path);

    int (*begin_geom_subset)(void *user, const char *name, const char *path);
    int (*subset_index)(void *user, const char *path, usd3d_u32 index, usd3d_u32 value);
    int (*subset_info)(void *user, const char *path, const char *key, const char *value);
    int (*end_geom_subset)(void *user, const char *name, const char *path);

    int (*begin_light)(void *user, const char *type_name, const char *name, const char *path);
    int (*light_scalar)(void *user, const char *path, const char *attr_name, usd3d_fixed value);
    int (*light_color)(void *user, const char *path, const char *attr_name, usd3d_fixed r, usd3d_fixed g, usd3d_fixed b);
    int (*light_asset)(void *user, const char *path, const char *attr_name, const char *asset_path);
    int (*end_light)(void *user, const char *type_name, const char *name, const char *path);

    int (*begin_volume)(void *user, const char *type_name, const char *name, const char *path);
    int (*volume_field_asset)(void *user, const char *path, const char *field_name, const char *asset_path);
    int (*end_volume)(void *user, const char *type_name, const char *name, const char *path);

    int (*collection_target)(void *user, const char *prim_path, const char *collection_name, const char *target_path, int include);

    int (*begin_skel)(void *user, const char *type_name, const char *name, const char *path);
    int (*skel_joint)(void *user, const char *path, const char *attr_name, usd3d_u32 index, const char *joint_token);
    int (*skel_matrix4)(void *user, const char *path, const char *attr_name, usd3d_u32 index, const usd3d_fixed *m16);
    int (*skel_binding)(void *user, const char *prim_path, const char *attr_name, const char *target_path);
    int (*skel_weight)(void *user, const char *prim_path, const char *attr_name, usd3d_u32 index, usd3d_fixed value);
    int (*skel_influence)(void *user, const char *prim_path, usd3d_u32 vertex_index, usd3d_u32 slot, usd3d_u32 joint_index, usd3d_fixed weight);
    int (*skel_index)(void *user, const char *prim_path, const char *attr_name, usd3d_u32 index, usd3d_u32 value);
    int (*blend_shape_offset)(void *user, const char *path, const char *attr_name, usd3d_u32 index, usd3d_fixed x, usd3d_fixed y, usd3d_fixed z);
    int (*end_skel)(void *user, const char *type_name, const char *name, const char *path);

    int (*begin_material)(void *user, const char *name, const char *path);
    int (*material_color)(void *user, const char *material_path, const char *input_name, usd3d_fixed r, usd3d_fixed g, usd3d_fixed b);
    int (*material_scalar)(void *user, const char *material_path, const char *input_name, usd3d_fixed v);
    int (*material_asset)(void *user, const char *material_path, const char *input_name, const char *asset_path);
    int (*material_binding)(void *user, const char *prim_path, const char *material_path);
    int (*shader_id)(void *user, const char *shader_path, const char *id_token);
    int (*shader_source_asset)(void *user, const char *shader_path, const char *source_type, const char *asset_path, const char *sub_identifier);
    int (*materialx_node)(void *user, const char *path, const char *node_name, const char *node_type);
    int (*mdl_node)(void *user, const char *path, const char *module_name, const char *symbol_name);
    int (*end_material)(void *user, const char *name, const char *path);

    int (*usdz_asset)(void *user, const char *asset_path, const char *mime_hint, unsigned long offset, unsigned long compressed_size, unsigned long uncompressed_size, int method);
    int (*usdz_asset_chunk)(void *user, const char *asset_path, const char *mime_hint, unsigned long chunk_offset, const unsigned char *data, unsigned long size, int final_chunk);
    int (*usdc_info)(void *user, const char *key, const char *value);
    int (*usdc_section)(void *user, const char *section_name, unsigned long offset, unsigned long size);
    int (*usdc_value)(void *user, const char *table_name, unsigned long index, const char *type_name, const char *value_text);
    int (*flattened_value)(void *user, const char *prim_path, const char *attr_name, const char *value_text, const char *source_layer, const char *strength_rule);
    int (*variant_pruned)(void *user, const char *prim_path, const char *set_name, const char *variant_name, int parsed);
    int (*skinned_point)(void *user, const char *prim_path, usd3d_u32 point_index, usd3d_fixed x, usd3d_fixed y, usd3d_fixed z);
    int (*material_eval)(void *user, const char *material_path, const char *evaluator, usd3d_fixed r, usd3d_fixed g, usd3d_fixed b, usd3d_fixed opacity, usd3d_fixed roughness, usd3d_fixed metallic);
    int (*volume_probe)(void *user, const char *asset_path, const char *format_name, const char *grid_name, unsigned long header_bytes);

    int (*time_sample_begin)(void *user, const char *prim_path, const char *attr_name, usd3d_fixed time_code);
    int (*time_sample_end)(void *user, const char *prim_path, const char *attr_name, usd3d_fixed time_code);

    int (*warning)(void *user, int code, const char *message, unsigned long line);
} usd3d_callbacks;

typedef struct usd3d_config_s {
    int parse_usdz_first_usda;
    int triangulate_ngons;
    int emit_points;
    int emit_normals;
    int emit_texcoords;
    int emit_colors;
    int emit_indices;
    int emit_prims;
    int emit_materials;
    int emit_time_samples;
    int emit_stage;
    int emit_composition;
    int emit_attr_metadata;
    int emit_curves;
    int emit_cameras;
    int emit_point_instancers;
    int emit_geom_subsets;
    int emit_lights;
    int emit_volumes;
    int emit_collections;
    int emit_relationships;
    int emit_prim_metadata;
    int emit_assets;
    int emit_connections;
    int emit_variants;
    int emit_resolved_layers;
    int resolve_references;
    int resolve_payloads;
    int emit_skel;
    int emit_materialx_mdl;
    int emit_usdz_assets;
    int emit_usdz_asset_data;
    int emit_instancer_matrices;
    int emit_skel_influences;
    int emit_usdc;
    int emit_usdc_values;
    int emit_flattened_values;
    int prune_variants;
    int emit_cpu_skinning;
    int emit_material_eval;
    int emit_volume_decode;
} usd3d_config;

void usd3d_default_config(usd3d_config *cfg);
void usd3d_zero_stats(usd3d_stats *stats);

int usd3d_decode_file(const char *path,
                      const usd3d_config *cfg,
                      const usd3d_callbacks *cb,
                      void *user,
                      usd3d_stats *stats);

/* Best-effort stage resolver: recursively decodes USDA/USDZ references, payloads,
   and subLayers using static depth limits. It does not implement full OpenUSD
   LIVRPS value merging, but emits value_resolution/resolved_layer callbacks. */
int usd3d_decode_stage_file(const char *path,
                            const usd3d_config *cfg,
                            const usd3d_callbacks *cb,
                            void *user,
                            usd3d_stats *stats);

int usd3d_probe_file(const char *path, usd3d_file_kind *kind);

/* Fixed-point helpers. All are integer-only. */
int usd3d_parse_fixed_text(const char *text, usd3d_fixed *out_value);
void usd3d_fixed_to_string(usd3d_fixed v, char *out_buf, unsigned int out_cap, unsigned int decimals);
usd3d_fixed usd3d_fixed_sin_deg(usd3d_fixed deg_q16);
usd3d_fixed usd3d_fixed_cos_deg(usd3d_fixed deg_q16);
usd3d_fixed usd3d_fixed_mul(usd3d_fixed a, usd3d_fixed b);
usd3d_fixed usd3d_fixed_div(usd3d_fixed a, usd3d_fixed b);

const char *usd3d_result_name(int code);
const char *usd3d_kind_name(usd3d_file_kind kind);
const char *usd3d_index_kind_name(int kind);

/* Optional module APIs. They compile into libusd3d.a but can be used directly. */
#include "usd3d_crate_values.h"
#include "usd3d_livrps.h"
#include "usd3d_variant_prune.h"
#include "usd3d_skinning_cpu.h"
#include "usd3d_material_eval.h"
#include "usd3d_volume_decode.h"

#ifdef __cplusplus
}
#endif

#endif
