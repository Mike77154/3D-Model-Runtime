#include "ir_validate.h"

#include <string.h>

static void ir_validate_seterr(char* err, size_t err_cap, const char* msg)
{
    size_t n;
    if (!err || err_cap == 0U) {
        return;
    }
    if (!msg) {
        msg = "error";
    }
    n = strlen(msg);
    if (n >= err_cap) {
        n = err_cap - 1U;
    }
    memcpy(err, msg, n);
    err[n] = '\0';
}

static int ir_validate_box_equal(const ir_box3fx* a, const ir_box3fx* b)
{
    return a->min_edge.x == b->min_edge.x && a->min_edge.y == b->min_edge.y && a->min_edge.z == b->min_edge.z &&
           a->max_edge.x == b->max_edge.x && a->max_edge.y == b->max_edge.y && a->max_edge.z == b->max_edge.z;
}

int ir_mesh_validate(const ir_mesh_doc* doc, ir_mesh_validation_report* report, char* err, size_t err_cap)
{
    size_t bi;
    ir_box3fx mesh_box;
    if (!doc || !report) {
        return IR_ERR_ARGUMENT;
    }
    memset(report, 0, sizeof(*report));
    ir_box3fx_make_empty(&mesh_box);
    for (bi = 0U; bi < doc->buffer_count; ++bi) {
        const ir_mesh_buffer* buf;
        ir_box3fx buf_box;
        size_t vi;
        size_t ii;
        int have_vertices;
        buf = &doc->buffers[bi];
        if (buf->material_index != IR_MESH_NO_MATERIAL) {
            if (buf->material_index >= doc->material_count) {
                report->invalid_material_refs += 1UL;
                report->errors += 1UL;
            } else {
                const ir_material_block* mb;
                mb = &doc->materials[buf->material_index];
                if ((size_t)mb->first_attr + (size_t)mb->attr_count > doc->attr_count) {
                    report->invalid_attr_ranges += 1UL;
                    report->errors += 1UL;
                }
            }
        }
        if ((size_t)buf->first_vertex + (size_t)buf->vertex_count > doc->vertex_count) {
            report->invalid_vertex_ranges += 1UL;
            report->errors += 1UL;
            continue;
        }
        if ((size_t)buf->first_index + (size_t)buf->index_count > doc->index_count) {
            report->invalid_index_ranges += 1UL;
            report->errors += 1UL;
            continue;
        }
        if (buf->vertex_count == 0UL || buf->index_count == 0UL) {
            report->empty_buffers += 1UL;
            report->warnings += 1UL;
        }
        ir_box3fx_make_empty(&buf_box);
        have_vertices = 0;
        for (vi = 0U; vi < (size_t)buf->vertex_count; ++vi) {
            const ir_vertex* v;
            v = &doc->vertices[buf->first_vertex + vi];
            ir_box3fx_expand(&buf_box, v->pos);
            ir_box3fx_expand(&mesh_box, v->pos);
            have_vertices = 1;
        }
        if (have_vertices) {
            if (ir_box3fx_is_empty(&buf->bbox)) {
                report->missing_buffer_bbox += 1UL;
                report->warnings += 1UL;
            } else if (!ir_validate_box_equal(&buf->bbox, &buf_box)) {
                report->missing_buffer_bbox += 1UL;
                report->warnings += 1UL;
            }
        }
        for (ii = 0U; ii < (size_t)buf->index_count; ++ii) {
            ir_u32 idx;
            idx = doc->indices[buf->first_index + ii];
            if (idx >= buf->vertex_count) {
                report->out_of_range_indices += 1UL;
                report->errors += 1UL;
            }
        }
    }
    if (!ir_box3fx_is_empty(&mesh_box)) {
        if (ir_box3fx_is_empty(&doc->bbox) || !ir_validate_box_equal(&doc->bbox, &mesh_box)) {
            report->missing_mesh_bbox += 1UL;
            report->warnings += 1UL;
        }
    }
    if (report->errors > 0UL) {
        ir_validate_seterr(err, err_cap, "mesh validation failed");
        return IR_ERR_FORMAT;
    }
    return IR_OK;
}

static int ir_scene_attr_block_ok(const ir_attr_block* block, size_t attr_count)
{
    return (size_t)block->first_attr + (size_t)block->attr_count <= attr_count;
}

static int ir_scene_chain_contains_child(const ir_scene_doc* doc, ir_i32 parent, ir_i32 child)
{
    ir_i32 cur;
    size_t steps;
    cur = doc->nodes[parent].first_child;
    steps = 0U;
    while (cur >= 0 && steps <= doc->node_count) {
        if (cur == child) {
            return 1;
        }
        cur = doc->nodes[cur].next_sibling;
        ++steps;
    }
    return 0;
}

int ir_scene_validate(const ir_scene_doc* doc, ir_scene_validation_report* report, char* err, size_t err_cap)
{
    size_t i;
    if (!doc || !report) {
        return IR_ERR_ARGUMENT;
    }
    memset(report, 0, sizeof(*report));
    if (!ir_scene_attr_block_ok(&doc->root_attrs, doc->attr_count) || !ir_scene_attr_block_ok(&doc->root_user_attrs, doc->attr_count)) {
        report->invalid_attr_ranges += 1UL;
        report->errors += 1UL;
    }
    for (i = 0U; i < doc->node_count; ++i) {
        const ir_scene_node* node;
        size_t mi;
        size_t ai;
        ir_i32 cur;
        size_t steps;
        node = &doc->nodes[i];
        if (node->parent < -1 || (node->parent >= 0 && (size_t)node->parent >= doc->node_count)) {
            report->invalid_parent_links += 1UL;
            report->errors += 1UL;
        }
        if (node->first_child < -1 || (node->first_child >= 0 && (size_t)node->first_child >= doc->node_count)) {
            report->invalid_child_links += 1UL;
            report->errors += 1UL;
        }
        if (node->next_sibling < -1 || (node->next_sibling >= 0 && (size_t)node->next_sibling >= doc->node_count)) {
            report->invalid_sibling_links += 1UL;
            report->errors += 1UL;
        }
        if (!ir_scene_attr_block_ok(&node->attrs, doc->attr_count) || !ir_scene_attr_block_ok(&node->user_attrs, doc->attr_count)) {
            report->invalid_attr_ranges += 1UL;
            report->errors += 1UL;
        }
        if ((size_t)node->first_material + (size_t)node->material_count > doc->material_count) {
            report->invalid_material_ranges += 1UL;
            report->errors += 1UL;
        }
        if ((size_t)node->first_animator + (size_t)node->animator_count > doc->animator_count) {
            report->invalid_animator_ranges += 1UL;
            report->errors += 1UL;
        }
        if (ir_strpool_get(&doc->strings, node->type_off)[0] == '\0') {
            report->empty_type_names += 1UL;
            report->warnings += 1UL;
        }
        if (node->parent < 0) {
            report->root_nodes += 1UL;
        } else if ((size_t)node->parent < doc->node_count) {
            if (!ir_scene_chain_contains_child(doc, node->parent, (ir_i32)i)) {
                report->orphaned_nodes += 1UL;
                report->warnings += 1UL;
            }
        }
        steps = 0U;
        cur = (ir_i32)i;
        while (cur >= 0 && steps <= doc->node_count) {
            cur = doc->nodes[cur].parent;
            ++steps;
        }
        if (steps > doc->node_count) {
            report->broken_parent_chains += 1UL;
            report->errors += 1UL;
        }
        steps = 0U;
        cur = node->first_child;
        while (cur >= 0 && steps <= doc->node_count) {
            if (doc->nodes[cur].parent != (ir_i32)i) {
                report->broken_child_chains += 1UL;
                report->warnings += 1UL;
                break;
            }
            cur = doc->nodes[cur].next_sibling;
            ++steps;
        }
        if (steps > doc->node_count) {
            report->broken_child_chains += 1UL;
            report->errors += 1UL;
        }
        for (mi = 0U; mi < (size_t)node->material_count && (size_t)node->first_material + mi < doc->material_count; ++mi) {
            const ir_scene_material* mat;
            mat = &doc->materials[node->first_material + mi];
            if (!ir_scene_attr_block_ok(&mat->attrs, doc->attr_count)) {
                report->invalid_material_ranges += 1UL;
                report->errors += 1UL;
            }
        }
        for (ai = 0U; ai < (size_t)node->animator_count && (size_t)node->first_animator + ai < doc->animator_count; ++ai) {
            const ir_scene_animator* anim;
            anim = &doc->animators[node->first_animator + ai];
            if (!ir_scene_attr_block_ok(&anim->attrs, doc->attr_count)) {
                report->invalid_animator_ranges += 1UL;
                report->errors += 1UL;
            }
        }
    }
    if (report->errors > 0UL) {
        ir_validate_seterr(err, err_cap, "scene validation failed");
        return IR_ERR_FORMAT;
    }
    return IR_OK;
}
