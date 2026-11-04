#ifndef IR_MESH_H
#define IR_MESH_H

#include "ir_attr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IR_MESH_NO_MATERIAL ((ir_u32)0xFFFFFFFFUL)

enum ir_mesh_vertex_type {
    IR_MESH_VERTEX_STANDARD = 0,
    IR_MESH_VERTEX_2TCOORDS = 1,
    IR_MESH_VERTEX_TANGENTS = 2
};


typedef struct ir_vertex_tag {
    ir_vec3fx pos;
    ir_vec3fx normal;
    ir_u32 color_argb;
    ir_vec2fx uv1;
    ir_vec2fx uv2;
    ir_vec3fx tangent;
    ir_vec3fx binormal;
} ir_vertex;

typedef struct ir_material_block_tag {
    ir_u32 first_attr;
    ir_u32 attr_count;
    ir_raw_range raw_block;
} ir_material_block;

typedef struct ir_mesh_buffer_tag {
    ir_u8 vertex_type;
    unsigned long line;
    unsigned long col;
    ir_box3fx bbox;
    ir_u32 first_vertex;
    ir_u32 vertex_count;
    ir_u32 first_index;
    ir_u32 index_count;
    ir_u32 material_index;
    ir_raw_range raw_block;
    ir_raw_range raw_bbox;
    ir_raw_range raw_material;
    ir_raw_range raw_vertices;
    ir_raw_range raw_vertices_text;
    ir_raw_range raw_indices;
    ir_raw_range raw_indices_text;
} ir_mesh_buffer;

typedef struct ir_mesh_storage_tag {
    ir_mesh_buffer* buffers;
    size_t buffer_cap;
    ir_material_block* materials;
    size_t material_cap;
    ir_attr* attrs;
    size_t attr_cap;
    ir_vertex* vertices;
    size_t vertex_cap;
    ir_u32* indices;
    size_t index_cap;
    char* string_pool;
    size_t string_cap;
} ir_mesh_storage;

typedef struct ir_mesh_doc_tag {
    ir_box3fx bbox;
    int source_encoding;
    ir_raw_source raw_source;
    ir_raw_range raw_document;
    ir_raw_range raw_root_block;
    ir_raw_range raw_root_bbox;
    ir_mesh_buffer* buffers;
    size_t buffer_count;
    size_t buffer_cap;
    ir_material_block* materials;
    size_t material_count;
    size_t material_cap;
    ir_attr* attrs;
    size_t attr_count;
    size_t attr_cap;
    ir_vertex* vertices;
    size_t vertex_count;
    size_t vertex_cap;
    ir_u32* indices;
    size_t index_count;
    size_t index_cap;
    ir_strpool strings;
    ir_u32 xmlns_off;
    ir_u16 xmlns_len;
    ir_u32 version_off;
    ir_u16 version_len;
    int has_xmlns;
    int has_version;
} ir_mesh_doc;

void ir_mesh_doc_init(ir_mesh_doc* doc, const ir_mesh_storage* storage);
int ir_mesh_parse_memory(ir_mesh_doc* doc, const void* data, size_t size, char* err, size_t err_cap);
int ir_mesh_write_memory(const ir_mesh_doc* doc, char* out, size_t out_cap, size_t* out_len, char* err, size_t err_cap);
void ir_mesh_recompute_bounds(ir_mesh_doc* doc);
int ir_mesh_vertex_type_from_cstr(const char* s, int* out_type);
const char* ir_mesh_vertex_type_cstr(int type);
int ir_mesh_vertex_component_count(int type);
const ir_attr* ir_mesh_material_attr_at(const ir_mesh_doc* doc, const ir_material_block* material, size_t ordinal);
int ir_mesh_material_find_attr(const ir_mesh_doc* doc, const ir_material_block* material, const char* name, ir_u32* out_attr_index);
const ir_attr* ir_mesh_material_find_attr_ptr(const ir_mesh_doc* doc, const ir_material_block* material, const char* name);
const ir_attr* ir_mesh_buffer_find_material_attr(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer, const char* name);
int ir_mesh_buffer_max_index(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer, ir_u32* out_max_index);
int ir_mesh_buffer_triangle_count(const ir_mesh_buffer* buffer, ir_u32* out_triangle_count);
int ir_mesh_buffer_indices_in_range(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer);
const ir_vertex* ir_mesh_buffer_vertex_at(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer, size_t ordinal);
int ir_mesh_buffer_index_at(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer, size_t ordinal, ir_u32* out_index);
int ir_mesh_buffer_compute_bounds(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer, ir_box3fx* out_bbox);
int ir_mesh_buffer_used_vertex_count(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer, ir_u32* out_count);
int ir_mesh_doc_total_triangle_count(const ir_mesh_doc* doc, ir_u32* out_triangle_count);

#ifdef __cplusplus
}
#endif

#endif
