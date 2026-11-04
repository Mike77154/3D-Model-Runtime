#include "ir_scene.h"

#include <string.h>

static void ir_scene_seterr(char* err, size_t err_cap, const char* msg)
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

static void ir_scene_clear_node_ranges(ir_scene_node* node)
{
    if (!node) {
        return;
    }
    ir_raw_range_clear(&node->raw_block);
    ir_raw_range_clear(&node->raw_materials);
    ir_raw_range_clear(&node->raw_animators);
    ir_raw_range_clear(&node->raw_userdata);
    ir_raw_range_clear(&node->attrs.raw_block);
    ir_raw_range_clear(&node->user_attrs.raw_block);
}

void ir_scene_doc_init(ir_scene_doc* doc, const ir_scene_storage* storage)
{
    if (!doc || !storage) {
        return;
    }
    memset(doc, 0, sizeof(*doc));
    doc->nodes = storage->nodes;
    doc->node_cap = storage->node_cap;
    doc->materials = storage->materials;
    doc->material_cap = storage->material_cap;
    doc->animators = storage->animators;
    doc->animator_cap = storage->animator_cap;
    doc->attrs = storage->attrs;
    doc->attr_cap = storage->attr_cap;
    ir_strpool_init(&doc->strings, storage->string_pool, storage->string_cap);
    doc->source_encoding = IR_XML_UTF8;
    ir_raw_source_clear(&doc->raw_source);
    ir_raw_range_clear(&doc->raw_document);
    ir_raw_range_clear(&doc->raw_root_block);
}

static int ir_scene_add_attr(ir_scene_doc* doc, ir_attr_block* block, const ir_xml_tag* tag, char* err, size_t err_cap)
{
    int rc;
    if (doc->attr_count >= doc->attr_cap) {
        return IR_ERR_CAPACITY;
    }
    rc = ir_attr_from_xml_tag(tag, &doc->attrs[doc->attr_count], &doc->strings, err, err_cap);
    if (rc != IR_OK) {
        return rc;
    }
    doc->attr_count += 1U;
    block->attr_count += 1UL;
    return IR_OK;
}

static int ir_scene_parse_attr_block_impl(ir_scene_doc* doc, ir_xml_reader* xr, ir_attr_block* block, const ir_xml_tag* start_tag, int append, char* err, size_t err_cap)
{
    ir_xml_tag tag;
    int rc;
    size_t expected;
    ir_attr_block sink;
    if (!append || block->attr_count == 0UL) {
        block->first_attr = (ir_u32)doc->attr_count;
        block->attr_count = 0UL;
        block->raw_block.begin = start_tag->raw_begin;
    } else {
        expected = (size_t)block->first_attr + (size_t)block->attr_count;
        if (doc->attr_count != expected) {
            /* The public attr block is a contiguous range.  Irrlicht-style nested
             * scene containers may legally apply another <attributes> block after
             * materials/animators/userData; consume and preserve those attrs in
             * doc->attrs without corrupting the earlier contiguous block.
             */
            return ir_scene_parse_attr_block_impl(doc, xr, &sink, start_tag, 0, err, err_cap);
        }
    }
    block->raw_block.end = start_tag->raw_end;
    if (start_tag->is_empty) {
        return IR_OK;
    }
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "attributes")) {
                ir_scene_seterr(err, err_cap, "unexpected closing tag in attributes block");
                return IR_ERR_FORMAT;
            }
            block->raw_block.end = tag.raw_end;
            return IR_OK;
        }
        rc = ir_scene_add_attr(doc, block, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (!tag.is_empty) {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
    }
}

static int ir_scene_parse_attr_block(ir_scene_doc* doc, ir_xml_reader* xr, ir_attr_block* block, const ir_xml_tag* start_tag, char* err, size_t err_cap)
{
    return ir_scene_parse_attr_block_impl(doc, xr, block, start_tag, 0, err, err_cap);
}

static int ir_scene_parse_attr_block_append(ir_scene_doc* doc, ir_xml_reader* xr, ir_attr_block* block, const ir_xml_tag* start_tag, char* err, size_t err_cap)
{
    return ir_scene_parse_attr_block_impl(doc, xr, block, start_tag, 1, err, err_cap);
}

static int ir_scene_add_material(ir_scene_doc* doc, ir_u32* index)
{
    if (doc->material_count >= doc->material_cap) {
        return IR_ERR_CAPACITY;
    }
    *index = (ir_u32)doc->material_count;
    doc->materials[doc->material_count].attrs.first_attr = (ir_u32)doc->attr_count;
    doc->materials[doc->material_count].attrs.attr_count = 0UL;
    ir_raw_range_clear(&doc->materials[doc->material_count].attrs.raw_block);
    doc->material_count += 1U;
    return IR_OK;
}

static int ir_scene_add_animator(ir_scene_doc* doc, ir_u32* index)
{
    if (doc->animator_count >= doc->animator_cap) {
        return IR_ERR_CAPACITY;
    }
    *index = (ir_u32)doc->animator_count;
    doc->animators[doc->animator_count].attrs.first_attr = (ir_u32)doc->attr_count;
    doc->animators[doc->animator_count].attrs.attr_count = 0UL;
    ir_raw_range_clear(&doc->animators[doc->animator_count].attrs.raw_block);
    doc->animator_count += 1U;
    return IR_OK;
}

static int ir_scene_append_child(ir_scene_doc* doc, ir_i32 parent, ir_i32 child)
{
    ir_scene_node* pnode;
    ir_i32 cur;
    if (parent < 0) {
        return IR_OK;
    }
    pnode = &doc->nodes[parent];
    if (pnode->first_child < 0) {
        pnode->first_child = child;
        return IR_OK;
    }
    cur = pnode->first_child;
    while (doc->nodes[cur].next_sibling >= 0) {
        cur = doc->nodes[cur].next_sibling;
    }
    doc->nodes[cur].next_sibling = child;
    return IR_OK;
}

static int ir_scene_add_node(ir_scene_doc* doc, ir_i32 parent, const char* type_name, ir_u32 type_len, ir_u32* out_index, unsigned long line, unsigned long col)
{
    ir_scene_node* node;
    int rc;
    if (doc->node_count >= doc->node_cap) {
        return IR_ERR_CAPACITY;
    }
    *out_index = (ir_u32)doc->node_count;
    node = &doc->nodes[doc->node_count];
    memset(node, 0, sizeof(*node));
    node->parent = parent;
    node->line = line;
    node->col = col;
    node->first_child = -1;
    node->next_sibling = -1;
    node->attrs.first_attr = (ir_u32)doc->attr_count;
    node->attrs.attr_count = 0UL;
    node->user_attrs.first_attr = (ir_u32)doc->attr_count;
    node->user_attrs.attr_count = 0UL;
    node->first_material = (ir_u32)doc->material_count;
    node->material_count = 0UL;
    node->first_animator = (ir_u32)doc->animator_count;
    node->animator_count = 0UL;
    ir_scene_clear_node_ranges(node);
    rc = ir_strpool_put(&doc->strings, type_name, type_len, &node->type_off, &node->type_len);
    if (rc != IR_OK) {
        return rc;
    }
    doc->node_count += 1U;
    return ir_scene_append_child(doc, parent, (ir_i32)(*out_index));
}

static int ir_scene_parse_userdata(ir_scene_doc* doc, ir_xml_reader* xr, ir_attr_block* block, const ir_xml_tag* start_tag, char* err, size_t err_cap)
{
    ir_xml_tag tag;
    int rc;
    block->first_attr = (ir_u32)doc->attr_count;
    block->attr_count = 0UL;
    block->raw_block.begin = start_tag->raw_begin;
    block->raw_block.end = start_tag->raw_end;
    if (start_tag->is_empty) {
        return IR_OK;
    }
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "userData")) {
                ir_scene_seterr(err, err_cap, "unexpected closing tag in userData");
                return IR_ERR_FORMAT;
            }
            block->raw_block.end = tag.raw_end;
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            rc = ir_scene_parse_attr_block(doc, xr, block, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        } else {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
    }
}

static int ir_scene_parse_materials(ir_scene_doc* doc, ir_xml_reader* xr, ir_scene_node* node, const ir_xml_tag* start_tag, char* err, size_t err_cap)
{
    ir_xml_tag tag;
    int rc;
    node->raw_materials.begin = start_tag->raw_begin;
    node->raw_materials.end = start_tag->raw_end;
    if (start_tag->is_empty) {
        return IR_OK;
    }
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "materials")) {
                ir_scene_seterr(err, err_cap, "unexpected closing tag in materials");
                return IR_ERR_FORMAT;
            }
            node->raw_materials.end = tag.raw_end;
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            ir_u32 idx;
            rc = ir_scene_add_material(doc, &idx);
            if (rc != IR_OK) return rc;
            rc = ir_scene_parse_attr_block(doc, xr, &doc->materials[idx].attrs, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
            node->material_count += 1UL;
        } else {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        }
    }
}

static int ir_scene_parse_animators(ir_scene_doc* doc, ir_xml_reader* xr, ir_scene_node* node, const ir_xml_tag* start_tag, char* err, size_t err_cap)
{
    ir_xml_tag tag;
    int rc;
    node->raw_animators.begin = start_tag->raw_begin;
    node->raw_animators.end = start_tag->raw_end;
    if (start_tag->is_empty) {
        return IR_OK;
    }
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "animators")) {
                ir_scene_seterr(err, err_cap, "unexpected closing tag in animators");
                return IR_ERR_FORMAT;
            }
            node->raw_animators.end = tag.raw_end;
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            ir_u32 idx;
            rc = ir_scene_add_animator(doc, &idx);
            if (rc != IR_OK) return rc;
            rc = ir_scene_parse_attr_block(doc, xr, &doc->animators[idx].attrs, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
            node->animator_count += 1UL;
        } else {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        }
    }
}

static int ir_scene_parse_node(ir_scene_doc* doc, ir_xml_reader* xr, const ir_xml_tag* start_tag, ir_i32 parent, char* err, size_t err_cap);
static int ir_scene_parse_scene_container(ir_scene_doc* doc, ir_xml_reader* xr, const ir_xml_tag* start_tag, ir_i32 parent, ir_scene_node* context_node, char* err, size_t err_cap);

static int ir_scene_parse_node(ir_scene_doc* doc, ir_xml_reader* xr, const ir_xml_tag* start_tag, ir_i32 parent, char* err, size_t err_cap)
{
    ir_u32 index;
    ir_scene_node* node;
    ir_xml_tag tag;
    const char* type_name;
    int rc;
    type_name = ir_xml_tag_attr_local(start_tag, "type");
    if (!type_name) {
        type_name = "";
    }
    rc = ir_scene_add_node(doc, parent, type_name, (ir_u32)strlen(type_name), &index, start_tag->line, start_tag->col);
    if (rc != IR_OK) {
        return rc;
    }
    node = &doc->nodes[index];
    node->raw_block.begin = start_tag->raw_begin;
    node->raw_block.end = start_tag->raw_end;
    if (start_tag->is_empty) {
        return IR_OK;
    }
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "node")) {
                ir_scene_seterr(err, err_cap, "unexpected closing tag in node");
                return IR_ERR_FORMAT;
            }
            node->raw_block.end = tag.raw_end;
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            rc = ir_scene_parse_attr_block(doc, xr, &node->attrs, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        } else if (ir_xml_name_eq(tag.name, "materials")) {
            node->first_material = (ir_u32)doc->material_count;
            node->material_count = 0UL;
            rc = ir_scene_parse_materials(doc, xr, node, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        } else if (ir_xml_name_eq(tag.name, "animators")) {
            node->first_animator = (ir_u32)doc->animator_count;
            node->animator_count = 0UL;
            rc = ir_scene_parse_animators(doc, xr, node, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        } else if (ir_xml_name_eq(tag.name, "userData")) {
            node->raw_userdata.begin = tag.raw_begin;
            node->raw_userdata.end = tag.raw_end;
            rc = ir_scene_parse_userdata(doc, xr, &node->user_attrs, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
            node->raw_userdata = node->user_attrs.raw_block;
        } else if (ir_xml_name_eq(tag.name, "node")) {
            rc = ir_scene_parse_node(doc, xr, &tag, (ir_i32)index, err, err_cap);
            if (rc != IR_OK) return rc;
        } else if (ir_xml_name_eq(tag.name, "irr_scene")) {
            rc = ir_scene_parse_scene_container(doc, xr, &tag, (ir_i32)index, node, err, err_cap);
            if (rc != IR_OK) return rc;
        } else {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        }
    }
}

static int ir_scene_parse_scene_container(ir_scene_doc* doc, ir_xml_reader* xr, const ir_xml_tag* start_tag, ir_i32 parent, ir_scene_node* context_node, char* err, size_t err_cap)
{
    ir_xml_tag tag;
    int rc;
    if (!doc || !xr || !start_tag) {
        return IR_ERR_ARGUMENT;
    }
    if (start_tag->is_empty) {
        return IR_OK;
    }
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "irr_scene")) {
                ir_scene_seterr(err, err_cap, "unexpected closing tag in nested irr_scene");
                return IR_ERR_FORMAT;
            }
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            if (context_node) {
                rc = ir_scene_parse_attr_block_append(doc, xr, &context_node->attrs, &tag, err, err_cap);
            } else {
                rc = ir_scene_parse_attr_block_append(doc, xr, &doc->root_attrs, &tag, err, err_cap);
            }
            if (rc != IR_OK) return rc;
        } else if (ir_xml_name_eq(tag.name, "userData")) {
            if (context_node) {
                context_node->raw_userdata.begin = tag.raw_begin;
                context_node->raw_userdata.end = tag.raw_end;
                rc = ir_scene_parse_userdata(doc, xr, &context_node->user_attrs, &tag, err, err_cap);
                if (rc != IR_OK) return rc;
                context_node->raw_userdata = context_node->user_attrs.raw_block;
            } else {
                rc = ir_scene_parse_userdata(doc, xr, &doc->root_user_attrs, &tag, err, err_cap);
                if (rc != IR_OK) return rc;
            }
        } else if (context_node && ir_xml_name_eq(tag.name, "materials")) {
            context_node->first_material = (ir_u32)doc->material_count;
            context_node->material_count = 0UL;
            rc = ir_scene_parse_materials(doc, xr, context_node, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        } else if (context_node && ir_xml_name_eq(tag.name, "animators")) {
            context_node->first_animator = (ir_u32)doc->animator_count;
            context_node->animator_count = 0UL;
            rc = ir_scene_parse_animators(doc, xr, context_node, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        } else if (ir_xml_name_eq(tag.name, "node")) {
            rc = ir_scene_parse_node(doc, xr, &tag, parent, err, err_cap);
            if (rc != IR_OK) return rc;
        } else if (ir_xml_name_eq(tag.name, "irr_scene")) {
            rc = ir_scene_parse_scene_container(doc, xr, &tag, parent, context_node, err, err_cap);
            if (rc != IR_OK) return rc;
        } else {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        }
    }
}

int ir_scene_parse_memory(ir_scene_doc* doc, const void* data, size_t size, char* err, size_t err_cap)
{
    ir_xml_reader xr;
    ir_xml_tag tag;
    int rc;
    if (!doc || !data || size == 0U) {
        return IR_ERR_ARGUMENT;
    }
    doc->node_count = 0U;
    doc->material_count = 0U;
    doc->animator_count = 0U;
    doc->attr_count = 0U;
    doc->strings.len = 0U;
    doc->root_attrs.first_attr = 0UL;
    doc->root_attrs.attr_count = 0UL;
    ir_raw_range_clear(&doc->root_attrs.raw_block);
    doc->root_user_attrs.first_attr = 0UL;
    doc->root_user_attrs.attr_count = 0UL;
    ir_raw_range_clear(&doc->root_user_attrs.raw_block);
    ir_raw_source_clear(&doc->raw_source);
    ir_raw_range_clear(&doc->raw_document);
    ir_raw_range_clear(&doc->raw_root_block);
    ir_xml_reader_init(&xr, data, size);
    doc->source_encoding = xr.encoding;
    rc = ir_raw_source_bind(&doc->raw_source, data, size, xr.encoding);
    if (rc != IR_OK) {
        return rc;
    }
    doc->raw_document.begin = 0U;
    doc->raw_document.end = size;
    for (;;) {
        rc = ir_xml_next_tag(&xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            if (rc == IR_EOF) {
                ir_scene_seterr(err, err_cap, "missing <irr_scene> root element");
                return IR_ERR_FORMAT;
            }
            return rc;
        }
        if (!tag.is_end && ir_xml_name_eq(tag.name, "irr_scene")) {
            break;
        }
        if (!tag.is_end) {
            rc = ir_xml_skip_element(&xr, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        }
    }
    doc->raw_root_block.begin = tag.raw_begin;
    doc->raw_root_block.end = tag.raw_end;
    if (tag.is_empty) {
        return IR_OK;
    }
    for (;;) {
        rc = ir_xml_next_tag(&xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "irr_scene")) {
                ir_scene_seterr(err, err_cap, "unexpected closing tag at scene root");
                return IR_ERR_FORMAT;
            }
            doc->raw_root_block.end = tag.raw_end;
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            rc = ir_scene_parse_attr_block(doc, &xr, &doc->root_attrs, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        } else if (ir_xml_name_eq(tag.name, "userData")) {
            rc = ir_scene_parse_userdata(doc, &xr, &doc->root_user_attrs, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        } else if (ir_xml_name_eq(tag.name, "node")) {
            rc = ir_scene_parse_node(doc, &xr, &tag, -1, err, err_cap);
            if (rc != IR_OK) return rc;
        } else if (ir_xml_name_eq(tag.name, "irr_scene")) {
            rc = ir_scene_parse_scene_container(doc, &xr, &tag, -1, IR_NULL, err, err_cap);
            if (rc != IR_OK) return rc;
        } else {
            rc = ir_xml_skip_element(&xr, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        }
    }
}

static int ir_scene_write_attr_block(const ir_scene_doc* doc, const ir_attr_block* block, ir_outbuf* out, size_t indent, char* err, size_t err_cap)
{
    size_t i;
    int rc;
    rc = ir_outbuf_pad(out, ' ', indent); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "<attributes>\n"); if (rc != IR_OK) return rc;
    for (i = 0U; i < (size_t)block->attr_count; ++i) {
        rc = ir_attr_write_xml(&doc->attrs[block->first_attr + i], &doc->strings, out, indent + 2U, err, err_cap);
        if (rc != IR_OK) return rc;
    }
    rc = ir_outbuf_pad(out, ' ', indent); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "</attributes>\n"); if (rc != IR_OK) return rc;
    return IR_OK;
}

static int ir_scene_write_userdata(const ir_scene_doc* doc, const ir_attr_block* block, ir_outbuf* out, size_t indent, char* err, size_t err_cap)
{
    int rc;
    rc = ir_outbuf_pad(out, ' ', indent); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "<userData>\n"); if (rc != IR_OK) return rc;
    rc = ir_scene_write_attr_block(doc, block, out, indent + 2U, err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_outbuf_pad(out, ' ', indent); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "</userData>\n"); if (rc != IR_OK) return rc;
    return IR_OK;
}

static int ir_scene_write_node(const ir_scene_doc* doc, ir_u32 idx, ir_outbuf* out, size_t indent, char* err, size_t err_cap)
{
    const ir_scene_node* node;
    ir_i32 child;
    size_t i;
    int rc;
    node = &doc->nodes[idx];
    rc = ir_outbuf_pad(out, ' ', indent); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "<node type=\""); if (rc != IR_OK) return rc;
    rc = ir_xml_escape_to_out(out, ir_strpool_get(&doc->strings, node->type_off)); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "\">\n"); if (rc != IR_OK) return rc;
    if (node->attrs.attr_count > 0UL) {
        rc = ir_scene_write_attr_block(doc, &node->attrs, out, indent + 2U, err, err_cap); if (rc != IR_OK) return rc;
    }
    if (node->material_count > 0UL) {
        rc = ir_outbuf_pad(out, ' ', indent + 2U); if (rc != IR_OK) return rc;
        rc = ir_outbuf_puts(out, "<materials>\n"); if (rc != IR_OK) return rc;
        for (i = 0U; i < (size_t)node->material_count; ++i) {
            rc = ir_scene_write_attr_block(doc, &doc->materials[node->first_material + i].attrs, out, indent + 4U, err, err_cap);
            if (rc != IR_OK) return rc;
        }
        rc = ir_outbuf_pad(out, ' ', indent + 2U); if (rc != IR_OK) return rc;
        rc = ir_outbuf_puts(out, "</materials>\n"); if (rc != IR_OK) return rc;
    }
    if (node->animator_count > 0UL) {
        rc = ir_outbuf_pad(out, ' ', indent + 2U); if (rc != IR_OK) return rc;
        rc = ir_outbuf_puts(out, "<animators>\n"); if (rc != IR_OK) return rc;
        for (i = 0U; i < (size_t)node->animator_count; ++i) {
            rc = ir_scene_write_attr_block(doc, &doc->animators[node->first_animator + i].attrs, out, indent + 4U, err, err_cap);
            if (rc != IR_OK) return rc;
        }
        rc = ir_outbuf_pad(out, ' ', indent + 2U); if (rc != IR_OK) return rc;
        rc = ir_outbuf_puts(out, "</animators>\n"); if (rc != IR_OK) return rc;
    }
    if (node->user_attrs.attr_count > 0UL) {
        rc = ir_scene_write_userdata(doc, &node->user_attrs, out, indent + 2U, err, err_cap); if (rc != IR_OK) return rc;
    }
    child = node->first_child;
    while (child >= 0) {
        rc = ir_scene_write_node(doc, (ir_u32)child, out, indent + 2U, err, err_cap);
        if (rc != IR_OK) return rc;
        child = doc->nodes[child].next_sibling;
    }
    rc = ir_outbuf_pad(out, ' ', indent); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "</node>\n"); if (rc != IR_OK) return rc;
    return IR_OK;
}

int ir_scene_write_memory(const ir_scene_doc* doc, char* out_mem, size_t out_cap, size_t* out_len, char* err, size_t err_cap)
{
    ir_outbuf out;
    size_t i;
    int rc;
    if (!doc || !out_mem || out_cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    ir_outbuf_init(&out, out_mem, out_cap);
    rc = ir_outbuf_puts(&out, "<?xml version=\"1.0\"?>\n"); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(&out, "<irr_scene>\n"); if (rc != IR_OK) return rc;
    if (doc->root_attrs.attr_count > 0UL) {
        rc = ir_scene_write_attr_block(doc, &doc->root_attrs, &out, 2U, err, err_cap); if (rc != IR_OK) return rc;
    }
    if (doc->root_user_attrs.attr_count > 0UL) {
        rc = ir_scene_write_userdata(doc, &doc->root_user_attrs, &out, 2U, err, err_cap); if (rc != IR_OK) return rc;
    }
    for (i = 0U; i < doc->node_count; ++i) {
        if (doc->nodes[i].parent < 0) {
            rc = ir_scene_write_node(doc, (ir_u32)i, &out, 2U, err, err_cap);
            if (rc != IR_OK) return rc;
        }
    }
    rc = ir_outbuf_puts(&out, "</irr_scene>\n"); if (rc != IR_OK) return rc;
    return ir_outbuf_finalize(&out, out_len);
}


const char* ir_scene_node_type_cstr(const ir_scene_doc* doc, const ir_scene_node* node)
{
    if (!doc || !node) {
        return "";
    }
    return ir_strpool_get(&doc->strings, node->type_off);
}

static int ir_scene_attr_block_range_ok(const ir_scene_doc* doc, const ir_attr_block* block)
{
    if (!doc || !block) {
        return 0;
    }
    return (size_t)block->first_attr + (size_t)block->attr_count <= doc->attr_count;
}

const ir_attr* ir_scene_attr_block_attr_at(const ir_scene_doc* doc, const ir_attr_block* block, size_t ordinal)
{
    if (!ir_scene_attr_block_range_ok(doc, block)) {
        return IR_NULL;
    }
    if (ordinal >= (size_t)block->attr_count) {
        return IR_NULL;
    }
    return &doc->attrs[block->first_attr + ordinal];
}

int ir_scene_attr_block_find(const ir_scene_doc* doc, const ir_attr_block* block, const char* name, ir_u32* out_attr_index)
{
    size_t i;
    if (!doc || !block || !name || !out_attr_index) {
        return IR_ERR_ARGUMENT;
    }
    if (!ir_scene_attr_block_range_ok(doc, block)) {
        return IR_ERR_RANGE;
    }
    for (i = 0U; i < (size_t)block->attr_count; ++i) {
        const ir_attr* attr;
        attr = &doc->attrs[block->first_attr + i];
        if (strcmp(ir_attr_name_cstr(attr, &doc->strings), name) == 0) {
            *out_attr_index = (ir_u32)(block->first_attr + i);
            return IR_OK;
        }
    }
    return IR_EOF;
}

const ir_attr* ir_scene_attr_block_find_attr(const ir_scene_doc* doc, const ir_attr_block* block, const char* name)
{
    ir_u32 index;
    if (ir_scene_attr_block_find(doc, block, name, &index) != IR_OK) {
        return IR_NULL;
    }
    return &doc->attrs[index];
}

const ir_attr* ir_scene_node_find_attr(const ir_scene_doc* doc, const ir_scene_node* node, const char* name)
{
    if (!node) {
        return IR_NULL;
    }
    return ir_scene_attr_block_find_attr(doc, &node->attrs, name);
}

const ir_attr* ir_scene_node_find_user_attr(const ir_scene_doc* doc, const ir_scene_node* node, const char* name)
{
    if (!node) {
        return IR_NULL;
    }
    return ir_scene_attr_block_find_attr(doc, &node->user_attrs, name);
}

const ir_scene_material* ir_scene_node_material_at(const ir_scene_doc* doc, const ir_scene_node* node, size_t ordinal)
{
    if (!doc || !node) {
        return IR_NULL;
    }
    if ((size_t)node->first_material + (size_t)node->material_count > doc->material_count) {
        return IR_NULL;
    }
    if (ordinal >= (size_t)node->material_count) {
        return IR_NULL;
    }
    return &doc->materials[node->first_material + ordinal];
}

const ir_scene_animator* ir_scene_node_animator_at(const ir_scene_doc* doc, const ir_scene_node* node, size_t ordinal)
{
    if (!doc || !node) {
        return IR_NULL;
    }
    if ((size_t)node->first_animator + (size_t)node->animator_count > doc->animator_count) {
        return IR_NULL;
    }
    if (ordinal >= (size_t)node->animator_count) {
        return IR_NULL;
    }
    return &doc->animators[node->first_animator + ordinal];
}

const ir_attr* ir_scene_node_find_material_attr(const ir_scene_doc* doc, const ir_scene_node* node, size_t material_ordinal, const char* name)
{
    const ir_scene_material* mat;
    mat = ir_scene_node_material_at(doc, node, material_ordinal);
    if (!mat) {
        return IR_NULL;
    }
    return ir_scene_attr_block_find_attr(doc, &mat->attrs, name);
}

const ir_attr* ir_scene_node_find_animator_attr(const ir_scene_doc* doc, const ir_scene_node* node, size_t animator_ordinal, const char* name)
{
    const ir_scene_animator* anim;
    anim = ir_scene_node_animator_at(doc, node, animator_ordinal);
    if (!anim) {
        return IR_NULL;
    }
    return ir_scene_attr_block_find_attr(doc, &anim->attrs, name);
}

int ir_scene_node_child_count(const ir_scene_doc* doc, const ir_scene_node* node, ir_u32* out_count)
{
    ir_i32 cur;
    size_t steps;
    ir_u32 count;
    if (!doc || !node || !out_count) {
        return IR_ERR_ARGUMENT;
    }
    cur = node->first_child;
    steps = 0U;
    count = 0UL;
    while (cur >= 0) {
        if ((size_t)cur >= doc->node_count) {
            return IR_ERR_RANGE;
        }
        if (steps > doc->node_count) {
            return IR_ERR_FORMAT;
        }
        ++count;
        cur = doc->nodes[cur].next_sibling;
        ++steps;
    }
    *out_count = count;
    return IR_OK;
}

const ir_scene_node* ir_scene_node_child_at(const ir_scene_doc* doc, const ir_scene_node* node, size_t ordinal)
{
    ir_i32 cur;
    size_t steps;
    size_t index;
    if (!doc || !node) {
        return IR_NULL;
    }
    cur = node->first_child;
    steps = 0U;
    index = 0U;
    while (cur >= 0) {
        if ((size_t)cur >= doc->node_count || steps > doc->node_count) {
            return IR_NULL;
        }
        if (index == ordinal) {
            return &doc->nodes[cur];
        }
        cur = doc->nodes[cur].next_sibling;
        ++steps;
        ++index;
    }
    return IR_NULL;
}

const ir_scene_node* ir_scene_find_first_node_by_type(const ir_scene_doc* doc, const char* type)
{
    size_t i;
    if (!doc || !type) {
        return IR_NULL;
    }
    for (i = 0U; i < doc->node_count; ++i) {
        if (strcmp(ir_scene_node_type_cstr(doc, &doc->nodes[i]), type) == 0) {
            return &doc->nodes[i];
        }
    }
    return IR_NULL;
}

int ir_scene_count_nodes_by_type(const ir_scene_doc* doc, const char* type, ir_u32* out_count)
{
    size_t i;
    ir_u32 count;
    if (!doc || !type || !out_count) {
        return IR_ERR_ARGUMENT;
    }
    count = 0UL;
    for (i = 0U; i < doc->node_count; ++i) {
        if (strcmp(ir_scene_node_type_cstr(doc, &doc->nodes[i]), type) == 0) {
            ++count;
        }
    }
    *out_count = count;
    return IR_OK;
}
