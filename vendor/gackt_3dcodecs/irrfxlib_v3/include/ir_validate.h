#ifndef IR_VALIDATE_H
#define IR_VALIDATE_H

#include "ir_mesh.h"
#include "ir_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ir_mesh_validation_report_tag {
    unsigned long errors;
    unsigned long warnings;
    unsigned long empty_buffers;
    unsigned long missing_mesh_bbox;
    unsigned long missing_buffer_bbox;
    unsigned long invalid_material_refs;
    unsigned long invalid_vertex_ranges;
    unsigned long invalid_index_ranges;
    unsigned long out_of_range_indices;
    unsigned long invalid_attr_ranges;
} ir_mesh_validation_report;

typedef struct ir_scene_validation_report_tag {
    unsigned long errors;
    unsigned long warnings;
    unsigned long invalid_parent_links;
    unsigned long invalid_child_links;
    unsigned long invalid_sibling_links;
    unsigned long invalid_attr_ranges;
    unsigned long invalid_material_ranges;
    unsigned long invalid_animator_ranges;
    unsigned long empty_type_names;
    unsigned long broken_parent_chains;
    unsigned long broken_child_chains;
    unsigned long orphaned_nodes;
    unsigned long root_nodes;
} ir_scene_validation_report;

int ir_mesh_validate(const ir_mesh_doc* doc, ir_mesh_validation_report* report, char* err, size_t err_cap);
int ir_scene_validate(const ir_scene_doc* doc, ir_scene_validation_report* report, char* err, size_t err_cap);

#ifdef __cplusplus
}
#endif

#endif
