#ifndef P3_BAM_SPECIAL_H
#define P3_BAM_SPECIAL_H

#include "p3_bam_archive.h"
#include "p3_config.h"
#include "p3_types.h"

enum {
    P3_BAM_CLASS_UNKNOWN = 0,
    P3_BAM_CLASS_INTERNAL_NAME,
    P3_BAM_CLASS_GEOM_VERTEX_COLUMN,
    P3_BAM_CLASS_GEOM_VERTEX_ARRAY_FORMAT,
    P3_BAM_CLASS_GEOM_VERTEX_FORMAT,
    P3_BAM_CLASS_GEOM_VERTEX_ARRAY_DATA,
    P3_BAM_CLASS_GEOM_VERTEX_DATA,
    P3_BAM_CLASS_GEOM_PRIMITIVE,
    P3_BAM_CLASS_GEOM,
    P3_BAM_CLASS_PANDA_NODE,
    P3_BAM_CLASS_MODEL_NODE,
    P3_BAM_CLASS_MODEL_ROOT,
    P3_BAM_CLASS_GEOM_NODE
};

typedef struct p3_bam_tag {
    char key[P3_BAM_SPECIAL_TEXT_CAP];
    char value[P3_BAM_SPECIAL_VALUE_CAP];
} p3_bam_tag;

typedef struct p3_bam_sorted_ref {
    p3_u32 object_id;
    p3_s32 sort;
} p3_bam_sorted_ref;

typedef struct p3_bam_geom_entry {
    p3_u32 geom_object_id;
    p3_u32 state_object_id;
} p3_bam_geom_entry;

typedef struct p3_bam_geom_vertex_column {
    p3_u32 name_object_id;
    char name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u8 num_components;
    p3_u8 numeric_type;
    p3_u8 contents;
    p3_u16 start;
    p3_u8 column_alignment;
} p3_bam_geom_vertex_column;

typedef struct p3_bam_geom_vertex_array_format {
    p3_u16 stride;
    p3_u16 total_bytes;
    p3_u8 pad_to;
    p3_u16 divisor;
    p3_u16 column_count;
    p3_u16 total_column_count;
    p3_bam_geom_vertex_column columns[P3_BAM_SPECIAL_MAX_COLUMNS];
} p3_bam_geom_vertex_array_format;

typedef struct p3_bam_geom_vertex_format {
    p3_u8 animation_type;
    p3_u16 num_transforms;
    p3_u8 indexed_transforms;
    p3_u16 array_count;
    p3_u16 total_array_count;
    p3_u32 array_format_object_ids[P3_BAM_SPECIAL_MAX_ARRAYS];
} p3_bam_geom_vertex_format;

typedef struct p3_bam_geom_vertex_array_data {
    p3_u32 array_format_object_id;
    p3_u8 usage_hint;
    p3_u32 data_size;
    p3_u8 preview_size;
    p3_u8 preview[P3_BAM_SPECIAL_DATA_PREVIEW];
} p3_bam_geom_vertex_array_data;

typedef struct p3_bam_geom_vertex_data {
    char name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u32 format_object_id;
    p3_u8 usage_hint;
    p3_u16 array_count;
    p3_u16 total_array_count;
    p3_u32 array_object_ids[P3_BAM_SPECIAL_MAX_ARRAYS];
    p3_u32 transform_table_object_id;
    p3_u32 transform_blend_table_object_id;
    p3_u32 slider_table_object_id;
} p3_bam_geom_vertex_data;

typedef struct p3_bam_geom_primitive {
    char primitive_name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u8 shade_model;
    p3_s32 first_vertex;
    p3_s32 num_vertices;
    p3_u8 index_type;
    p3_u8 usage_hint;
    p3_u32 vertices_object_id;
    p3_u16 patch_vertices_per_patch;
    p3_u32 trailing_bytes;
    p3_u32 ends_pta_id;
    p3_u16 end_count;
    p3_u16 total_end_count;
    p3_s32 ends[P3_BAM_SPECIAL_MAX_PRIMITIVE_ENDS];
    p3_u8 ends_available;
} p3_bam_geom_primitive;

typedef struct p3_bam_geom {
    p3_u32 vertex_data_object_id;
    p3_u16 primitive_count;
    p3_u16 total_primitive_count;
    p3_u32 primitive_object_ids[P3_BAM_SPECIAL_MAX_PRIMITIVES];
    p3_u8 primitive_type;
    p3_u8 shade_model;
    p3_u16 geom_rendering;
    p3_u8 bounds_type;
} p3_bam_geom;

typedef struct p3_bam_panda_node {
    char name[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u32 state_object_id;
    p3_u32 transform_object_id;
    p3_u32 effects_object_id;
    p3_u32 draw_control_mask;
    p3_u32 draw_show_mask;
    p3_u32 into_collide_mask;
    p3_u8 bounds_type;
    p3_u16 tag_count;
    p3_u16 total_tag_count;
    p3_bam_tag tags[P3_BAM_SPECIAL_MAX_TAGS];
    p3_u16 parent_count;
    p3_u16 total_parent_count;
    p3_u32 parent_object_ids[P3_BAM_SPECIAL_MAX_CHILDREN];
    p3_u16 child_count;
    p3_u16 total_child_count;
    p3_bam_sorted_ref children[P3_BAM_SPECIAL_MAX_CHILDREN];
    p3_u16 stashed_count;
    p3_u16 total_stashed_count;
    p3_bam_sorted_ref stashed[P3_BAM_SPECIAL_MAX_CHILDREN];
} p3_bam_panda_node;

typedef struct p3_bam_model_node {
    p3_bam_panda_node base;
    p3_u8 preserve_transform;
    p3_u16 preserve_attributes;
} p3_bam_model_node;

typedef struct p3_bam_model_root {
    p3_bam_model_node base;
    p3_u16 loader_type_id;
    char loader_type_name[P3_BAM_SPECIAL_TEXT_CAP];
} p3_bam_model_root;

typedef struct p3_bam_geom_node {
    p3_bam_panda_node base;
    p3_u16 geom_count;
    p3_u16 total_geom_count;
    p3_bam_geom_entry geoms[P3_BAM_SPECIAL_MAX_GEOMS];
} p3_bam_geom_node;

typedef struct p3_bam_object {
    p3_u8 class_kind;
    p3_u8 record_kind;
    p3_u16 record_index;
    p3_u16 type_id;
    p3_u32 object_id;
    char type_name[P3_BAM_SPECIAL_TEXT_CAP];
    union {
        char internal_name[P3_BAM_SPECIAL_TEXT_CAP];
        p3_bam_geom_vertex_column geom_vertex_column;
        p3_bam_geom_vertex_array_format geom_vertex_array_format;
        p3_bam_geom_vertex_format geom_vertex_format;
        p3_bam_geom_vertex_array_data geom_vertex_array_data;
        p3_bam_geom_vertex_data geom_vertex_data;
        p3_bam_geom_primitive geom_primitive;
        p3_bam_geom geom;
        p3_bam_panda_node panda_node;
        p3_bam_model_node model_node;
        p3_bam_model_root model_root;
        p3_bam_geom_node geom_node;
    } u;
} p3_bam_object;

void p3_bam_object_init(p3_bam_object *obj);
const char *p3_bam_class_name(p3_u8 class_kind);
int p3_bam_find_record_by_object_id(const p3_bam_archive *archive, p3_u32 object_id);
int p3_bam_parse_object(const p3_bam_archive *archive, p3_u16 record_index, p3_bam_object *out_obj);
int p3_bam_resolve_internal_name(const p3_bam_archive *archive, p3_u32 object_id, char *dst, p3_u16 dst_cap);

int p3_bam_build_internal_name_payload(const char *name, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_geom_vertex_column_payload(const p3_bam_geom_vertex_column *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_geom_vertex_array_format_payload(const p3_bam_geom_vertex_array_format *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor);
int p3_bam_build_geom_vertex_format_payload(const p3_bam_geom_vertex_format *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_geom_vertex_array_data_payload(const p3_bam_geom_vertex_array_data *src, const p3_u8 *data_bytes, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_geom_vertex_data_payload(const p3_bam_geom_vertex_data *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_geom_primitive_payload(const p3_bam_geom_primitive *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_geom_payload(const p3_bam_geom *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor);
int p3_bam_build_panda_node_payload(const p3_bam_panda_node *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_model_node_payload(const p3_bam_model_node *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);
int p3_bam_build_model_root_payload(const p3_bam_model_root *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor);
int p3_bam_build_geom_node_payload(const p3_bam_geom_node *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len);

#endif
