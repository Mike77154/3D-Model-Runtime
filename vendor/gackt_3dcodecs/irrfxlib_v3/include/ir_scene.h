#ifndef IR_SCENE_H
#define IR_SCENE_H

#include "ir_attr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ir_attr_block_tag {
    ir_u32 first_attr;
    ir_u32 attr_count;
    ir_raw_range raw_block;
} ir_attr_block;

typedef struct ir_scene_material_tag {
    ir_attr_block attrs;
} ir_scene_material;

typedef struct ir_scene_animator_tag {
    ir_attr_block attrs;
} ir_scene_animator;

typedef struct ir_scene_node_tag {
    ir_i32 parent;
    unsigned long line;
    unsigned long col;
    ir_i32 first_child;
    ir_i32 next_sibling;
    ir_u32 type_off;
    ir_u16 type_len;
    ir_attr_block attrs;
    ir_attr_block user_attrs;
    ir_u32 first_material;
    ir_u32 material_count;
    ir_u32 first_animator;
    ir_u32 animator_count;
    ir_raw_range raw_block;
    ir_raw_range raw_materials;
    ir_raw_range raw_animators;
    ir_raw_range raw_userdata;
} ir_scene_node;

typedef struct ir_scene_storage_tag {
    ir_scene_node* nodes;
    size_t node_cap;
    ir_scene_material* materials;
    size_t material_cap;
    ir_scene_animator* animators;
    size_t animator_cap;
    ir_attr* attrs;
    size_t attr_cap;
    char* string_pool;
    size_t string_cap;
} ir_scene_storage;

typedef struct ir_scene_doc_tag {
    int source_encoding;
    ir_raw_source raw_source;
    ir_raw_range raw_document;
    ir_raw_range raw_root_block;
    ir_scene_node* nodes;
    size_t node_count;
    size_t node_cap;
    ir_scene_material* materials;
    size_t material_count;
    size_t material_cap;
    ir_scene_animator* animators;
    size_t animator_count;
    size_t animator_cap;
    ir_attr* attrs;
    size_t attr_count;
    size_t attr_cap;
    ir_attr_block root_attrs;
    ir_attr_block root_user_attrs;
    ir_strpool strings;
} ir_scene_doc;

void ir_scene_doc_init(ir_scene_doc* doc, const ir_scene_storage* storage);
int ir_scene_parse_memory(ir_scene_doc* doc, const void* data, size_t size, char* err, size_t err_cap);
int ir_scene_write_memory(const ir_scene_doc* doc, char* out, size_t out_cap, size_t* out_len, char* err, size_t err_cap);
const char* ir_scene_node_type_cstr(const ir_scene_doc* doc, const ir_scene_node* node);
const ir_attr* ir_scene_attr_block_attr_at(const ir_scene_doc* doc, const ir_attr_block* block, size_t ordinal);
int ir_scene_attr_block_find(const ir_scene_doc* doc, const ir_attr_block* block, const char* name, ir_u32* out_attr_index);
const ir_attr* ir_scene_attr_block_find_attr(const ir_scene_doc* doc, const ir_attr_block* block, const char* name);
const ir_attr* ir_scene_node_find_attr(const ir_scene_doc* doc, const ir_scene_node* node, const char* name);
const ir_attr* ir_scene_node_find_user_attr(const ir_scene_doc* doc, const ir_scene_node* node, const char* name);
const ir_scene_material* ir_scene_node_material_at(const ir_scene_doc* doc, const ir_scene_node* node, size_t ordinal);
const ir_scene_animator* ir_scene_node_animator_at(const ir_scene_doc* doc, const ir_scene_node* node, size_t ordinal);
const ir_attr* ir_scene_node_find_material_attr(const ir_scene_doc* doc, const ir_scene_node* node, size_t material_ordinal, const char* name);
const ir_attr* ir_scene_node_find_animator_attr(const ir_scene_doc* doc, const ir_scene_node* node, size_t animator_ordinal, const char* name);
int ir_scene_node_child_count(const ir_scene_doc* doc, const ir_scene_node* node, ir_u32* out_count);
const ir_scene_node* ir_scene_node_child_at(const ir_scene_doc* doc, const ir_scene_node* node, size_t ordinal);
const ir_scene_node* ir_scene_find_first_node_by_type(const ir_scene_doc* doc, const char* type);
int ir_scene_count_nodes_by_type(const ir_scene_doc* doc, const char* type, ir_u32* out_count);

#ifdef __cplusplus
}
#endif

#endif
