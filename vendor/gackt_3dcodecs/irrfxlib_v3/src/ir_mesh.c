#include "ir_mesh.h"
#include "ir_fx.h"

#include <stdio.h>
#include <string.h>

static void ir_mesh_seterr(char* err, size_t err_cap, const char* msg)
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

static void ir_mesh_clear_buffer_ranges(ir_mesh_buffer* buf)
{
    if (!buf) {
        return;
    }
    ir_raw_range_clear(&buf->raw_block);
    ir_raw_range_clear(&buf->raw_bbox);
    ir_raw_range_clear(&buf->raw_material);
    ir_raw_range_clear(&buf->raw_vertices);
    ir_raw_range_clear(&buf->raw_vertices_text);
    ir_raw_range_clear(&buf->raw_indices);
    ir_raw_range_clear(&buf->raw_indices_text);
}

void ir_mesh_doc_init(ir_mesh_doc* doc, const ir_mesh_storage* storage)
{
    if (!doc || !storage) {
        return;
    }
    memset(doc, 0, sizeof(*doc));
    doc->buffers = storage->buffers;
    doc->buffer_cap = storage->buffer_cap;
    doc->materials = storage->materials;
    doc->material_cap = storage->material_cap;
    doc->attrs = storage->attrs;
    doc->attr_cap = storage->attr_cap;
    doc->vertices = storage->vertices;
    doc->vertex_cap = storage->vertex_cap;
    doc->indices = storage->indices;
    doc->index_cap = storage->index_cap;
    ir_strpool_init(&doc->strings, storage->string_pool, storage->string_cap);
    ir_box3fx_make_empty(&doc->bbox);
    doc->source_encoding = IR_XML_UTF8;
    ir_raw_source_clear(&doc->raw_source);
    ir_raw_range_clear(&doc->raw_document);
    ir_raw_range_clear(&doc->raw_root_block);
    ir_raw_range_clear(&doc->raw_root_bbox);
}

static int ir_mesh_ascii_equal_ci(const char* a, const char* b)
{
    unsigned char ca;
    unsigned char cb;
    if (!a || !b) {
        return 0;
    }
    while (*a && *b) {
        ca = (unsigned char)*a;
        cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb - 'A' + 'a');
        if (ca != cb) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

int ir_mesh_vertex_type_from_cstr(const char* s, int* out)
{
    if (!s || !out) {
        return IR_ERR_ARGUMENT;
    }
    if (ir_mesh_ascii_equal_ci(s, "standard") ||
        ir_mesh_ascii_equal_ci(s, "evt_standard") ||
        ir_mesh_ascii_equal_ci(s, "s3dvertex")) {
        *out = IR_MESH_VERTEX_STANDARD;
        return IR_OK;
    }
    if (ir_mesh_ascii_equal_ci(s, "2tcoords") ||
        ir_mesh_ascii_equal_ci(s, "2tcoord") ||
        ir_mesh_ascii_equal_ci(s, "evt_2tcoords") ||
        ir_mesh_ascii_equal_ci(s, "s3dvertex2tcoords")) {
        *out = IR_MESH_VERTEX_2TCOORDS;
        return IR_OK;
    }
    if (ir_mesh_ascii_equal_ci(s, "tangents") ||
        ir_mesh_ascii_equal_ci(s, "tangent") ||
        ir_mesh_ascii_equal_ci(s, "evt_tangents") ||
        ir_mesh_ascii_equal_ci(s, "s3dvertextangents")) {
        *out = IR_MESH_VERTEX_TANGENTS;
        return IR_OK;
    }
    return IR_ERR_UNSUPPORTED;
}

const char* ir_mesh_vertex_type_cstr(int t)
{
    switch (t) {
    case IR_MESH_VERTEX_STANDARD: return "standard";
    case IR_MESH_VERTEX_2TCOORDS: return "2tcoords";
    case IR_MESH_VERTEX_TANGENTS: return "tangents";
    default: return "standard";
    }
}

int ir_mesh_vertex_component_count(int type)
{
    switch (type) {
    case IR_MESH_VERTEX_STANDARD: return 9;
    case IR_MESH_VERTEX_2TCOORDS: return 11;
    case IR_MESH_VERTEX_TANGENTS: return 15;
    default: return 0;
    }
}


static int ir_mesh_material_range_ok(const ir_mesh_doc* doc, const ir_material_block* material)
{
    if (!doc || !material) {
        return 0;
    }
    return (size_t)material->first_attr + (size_t)material->attr_count <= doc->attr_count;
}

const ir_attr* ir_mesh_material_attr_at(const ir_mesh_doc* doc, const ir_material_block* material, size_t ordinal)
{
    if (!ir_mesh_material_range_ok(doc, material)) {
        return IR_NULL;
    }
    if (ordinal >= (size_t)material->attr_count) {
        return IR_NULL;
    }
    return &doc->attrs[material->first_attr + ordinal];
}

int ir_mesh_material_find_attr(const ir_mesh_doc* doc, const ir_material_block* material, const char* name, ir_u32* out_attr_index)
{
    size_t i;
    if (!doc || !material || !name || !out_attr_index) {
        return IR_ERR_ARGUMENT;
    }
    if (!ir_mesh_material_range_ok(doc, material)) {
        return IR_ERR_RANGE;
    }
    for (i = 0U; i < (size_t)material->attr_count; ++i) {
        const ir_attr* attr;
        attr = &doc->attrs[material->first_attr + i];
        if (strcmp(ir_attr_name_cstr(attr, &doc->strings), name) == 0) {
            *out_attr_index = (ir_u32)(material->first_attr + i);
            return IR_OK;
        }
    }
    return IR_EOF;
}

const ir_attr* ir_mesh_material_find_attr_ptr(const ir_mesh_doc* doc, const ir_material_block* material, const char* name)
{
    ir_u32 index;
    if (ir_mesh_material_find_attr(doc, material, name, &index) != IR_OK) {
        return IR_NULL;
    }
    return &doc->attrs[index];
}

const ir_attr* ir_mesh_buffer_find_material_attr(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer, const char* name)
{
    if (!doc || !buffer || !name) {
        return IR_NULL;
    }
    if (buffer->material_index == IR_MESH_NO_MATERIAL || buffer->material_index >= doc->material_count) {
        return IR_NULL;
    }
    return ir_mesh_material_find_attr_ptr(doc, &doc->materials[buffer->material_index], name);
}

static int ir_mesh_buffer_ranges_ok(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer)
{
    if (!doc || !buffer) {
        return 0;
    }
    if ((size_t)buffer->first_vertex + (size_t)buffer->vertex_count > doc->vertex_count) {
        return 0;
    }
    if ((size_t)buffer->first_index + (size_t)buffer->index_count > doc->index_count) {
        return 0;
    }
    return 1;
}

int ir_mesh_buffer_max_index(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer, ir_u32* out_max_index)
{
    size_t i;
    ir_u32 max_index;
    if (!doc || !buffer || !out_max_index) {
        return IR_ERR_ARGUMENT;
    }
    if (!ir_mesh_buffer_ranges_ok(doc, buffer)) {
        return IR_ERR_RANGE;
    }
    if (buffer->index_count == 0UL) {
        *out_max_index = 0UL;
        return IR_EOF;
    }
    max_index = 0UL;
    for (i = 0U; i < (size_t)buffer->index_count; ++i) {
        ir_u32 idx;
        idx = doc->indices[buffer->first_index + i];
        if (idx > max_index) {
            max_index = idx;
        }
    }
    *out_max_index = max_index;
    return IR_OK;
}

int ir_mesh_buffer_triangle_count(const ir_mesh_buffer* buffer, ir_u32* out_triangle_count)
{
    if (!buffer || !out_triangle_count) {
        return IR_ERR_ARGUMENT;
    }
    *out_triangle_count = (ir_u32)(buffer->index_count / 3UL);
    return IR_OK;
}

int ir_mesh_buffer_indices_in_range(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer)
{
    size_t i;
    if (!doc || !buffer) {
        return IR_ERR_ARGUMENT;
    }
    if (!ir_mesh_buffer_ranges_ok(doc, buffer)) {
        return IR_ERR_RANGE;
    }
    for (i = 0U; i < (size_t)buffer->index_count; ++i) {
        if (doc->indices[buffer->first_index + i] >= buffer->vertex_count) {
            return IR_ERR_FORMAT;
        }
    }
    return IR_OK;
}


const ir_vertex* ir_mesh_buffer_vertex_at(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer, size_t ordinal)
{
    if (!doc || !buffer) {
        return IR_NULL;
    }
    if (!ir_mesh_buffer_ranges_ok(doc, buffer)) {
        return IR_NULL;
    }
    if (ordinal >= (size_t)buffer->vertex_count) {
        return IR_NULL;
    }
    return &doc->vertices[buffer->first_vertex + ordinal];
}

int ir_mesh_buffer_index_at(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer, size_t ordinal, ir_u32* out_index)
{
    if (!doc || !buffer || !out_index) {
        return IR_ERR_ARGUMENT;
    }
    if (!ir_mesh_buffer_ranges_ok(doc, buffer)) {
        return IR_ERR_RANGE;
    }
    if (ordinal >= (size_t)buffer->index_count) {
        return IR_EOF;
    }
    *out_index = doc->indices[buffer->first_index + ordinal];
    return IR_OK;
}

int ir_mesh_buffer_compute_bounds(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer, ir_box3fx* out_bbox)
{
    size_t i;
    if (!doc || !buffer || !out_bbox) {
        return IR_ERR_ARGUMENT;
    }
    if (!ir_mesh_buffer_ranges_ok(doc, buffer)) {
        return IR_ERR_RANGE;
    }
    ir_box3fx_make_empty(out_bbox);
    for (i = 0U; i < (size_t)buffer->vertex_count; ++i) {
        ir_box3fx_expand(out_bbox, doc->vertices[buffer->first_vertex + i].pos);
    }
    if (ir_box3fx_is_empty(out_bbox)) {
        return IR_EOF;
    }
    return IR_OK;
}

int ir_mesh_buffer_used_vertex_count(const ir_mesh_doc* doc, const ir_mesh_buffer* buffer, ir_u32* out_count)
{
    ir_u32 max_index;
    int rc;
    if (!doc || !buffer || !out_count) {
        return IR_ERR_ARGUMENT;
    }
    rc = ir_mesh_buffer_max_index(doc, buffer, &max_index);
    if (rc == IR_EOF) {
        *out_count = 0UL;
        return IR_OK;
    }
    if (rc != IR_OK) {
        return rc;
    }
    *out_count = max_index + 1UL;
    return IR_OK;
}

int ir_mesh_doc_total_triangle_count(const ir_mesh_doc* doc, ir_u32* out_triangle_count)
{
    size_t i;
    ir_u32 total;
    if (!doc || !out_triangle_count) {
        return IR_ERR_ARGUMENT;
    }
    total = 0UL;
    for (i = 0U; i < doc->buffer_count; ++i) {
        ir_u32 tris;
        int rc;
        rc = ir_mesh_buffer_triangle_count(&doc->buffers[i], &tris);
        if (rc != IR_OK) {
            return rc;
        }
        if (4294967295UL - total < tris) {
            return IR_ERR_OVERFLOW;
        }
        total += tris;
    }
    *out_triangle_count = total;
    return IR_OK;
}

static int ir_mesh_add_material(ir_mesh_doc* doc, ir_u32* index)
{
    if (doc->material_count >= doc->material_cap) {
        return IR_ERR_CAPACITY;
    }
    *index = (ir_u32)doc->material_count;
    doc->materials[doc->material_count].first_attr = (ir_u32)doc->attr_count;
    doc->materials[doc->material_count].attr_count = 0UL;
    ir_raw_range_clear(&doc->materials[doc->material_count].raw_block);
    doc->material_count += 1U;
    return IR_OK;
}

static int ir_mesh_add_buffer(ir_mesh_doc* doc, ir_u32* index)
{
    if (doc->buffer_count >= doc->buffer_cap) {
        return IR_ERR_CAPACITY;
    }
    *index = (ir_u32)doc->buffer_count;
    memset(&doc->buffers[doc->buffer_count], 0, sizeof(doc->buffers[doc->buffer_count]));
    doc->buffers[doc->buffer_count].material_index = IR_MESH_NO_MATERIAL;
    ir_box3fx_make_empty(&doc->buffers[doc->buffer_count].bbox);
    ir_mesh_clear_buffer_ranges(&doc->buffers[doc->buffer_count]);
    doc->buffer_count += 1U;
    return IR_OK;
}

static int ir_mesh_add_attr_to_material(ir_mesh_doc* doc, ir_u32 mat_index, const ir_xml_tag* tag, char* err, size_t err_cap)
{
    int rc;
    ir_attr* attr;
    if (doc->attr_count >= doc->attr_cap) {
        return IR_ERR_CAPACITY;
    }
    attr = &doc->attrs[doc->attr_count];
    rc = ir_attr_from_xml_tag(tag, attr, &doc->strings, err, err_cap);
    if (rc != IR_OK) {
        return rc;
    }
    doc->attr_count += 1U;
    doc->materials[mat_index].attr_count += 1UL;
    return IR_OK;
}

static int ir_mesh_add_vertex(ir_mesh_doc* doc, const ir_vertex* v)
{
    if (doc->vertex_count >= doc->vertex_cap) {
        return IR_ERR_CAPACITY;
    }
    doc->vertices[doc->vertex_count] = *v;
    doc->vertex_count += 1U;
    return IR_OK;
}

static int ir_mesh_add_index(ir_mesh_doc* doc, ir_u32 idx)
{
    if (doc->index_count >= doc->index_cap) {
        return IR_ERR_CAPACITY;
    }
    doc->indices[doc->index_count] = idx;
    doc->index_count += 1U;
    return IR_OK;
}

static int ir_mesh_read_bbox_tag(const ir_xml_tag* tag, ir_box3fx* box)
{
    const char* min_edge;
    const char* max_edge;
    min_edge = ir_xml_tag_attr_local(tag, "minEdge");
    max_edge = ir_xml_tag_attr_local(tag, "maxEdge");
    if (!min_edge || !max_edge) {
        return IR_ERR_FORMAT;
    }
    return ir_box3fx_from_attrs(min_edge, max_edge, box);
}

static int ir_mesh_read_count_attr(const ir_xml_tag* tag, const char* attr_name, ir_u32* out, int* has_attr)
{
    const char* s;
    if (!tag || !attr_name || !out || !has_attr) {
        return IR_ERR_ARGUMENT;
    }
    s = ir_xml_tag_attr_local(tag, attr_name);
    if (!s) {
        *out = 0UL;
        *has_attr = 0;
        return IR_OK;
    }
    *has_attr = 1;
    return ir_u32_from_irr_index_str(s, out);
}

static void ir_mesh_recompute_one_buffer_bounds(ir_mesh_doc* doc, ir_mesh_buffer* buf)
{
    size_t vi;
    ir_box3fx box;
    if (!doc || !buf) {
        return;
    }
    ir_box3fx_make_empty(&box);
    for (vi = 0U; vi < (size_t)buf->vertex_count; ++vi) {
        ir_vertex* v;
        v = &doc->vertices[buf->first_vertex + vi];
        ir_box3fx_expand(&box, v->pos);
    }
    buf->bbox = box;
}

static int ir_mesh_read_material_attr_wrapper(ir_mesh_doc* doc, ir_xml_reader* xr, ir_u32 mat_index, const ir_xml_tag* start_tag, char* err, size_t err_cap)
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
            if (!ir_xml_name_eq(tag.name, "attributes")) {
                ir_mesh_seterr(err, err_cap, "unexpected closing tag inside material attributes");
                return IR_ERR_FORMAT;
            }
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            rc = ir_mesh_read_material_attr_wrapper(doc, xr, mat_index, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        } else {
            rc = ir_mesh_add_attr_to_material(doc, mat_index, &tag, err, err_cap);
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
}

static int ir_mesh_read_material(ir_mesh_doc* doc, ir_xml_reader* xr, ir_u32 mat_index, const ir_xml_tag* start_tag, char* err, size_t err_cap)
{
    ir_xml_tag tag;
    int rc;
    ir_material_block* mat;
    mat = &doc->materials[mat_index];
    mat->raw_block.begin = start_tag->raw_begin;
    mat->raw_block.end = start_tag->raw_end;
    if (start_tag->is_empty) {
        return IR_OK;
    }
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "material")) {
                ir_mesh_seterr(err, err_cap, "unexpected closing tag inside material");
                return IR_ERR_FORMAT;
            }
            mat->raw_block.end = tag.raw_end;
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            rc = ir_mesh_read_material_attr_wrapper(doc, xr, mat_index, &tag, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        } else {
            rc = ir_mesh_add_attr_to_material(doc, mat_index, &tag, err, err_cap);
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
}

static int ir_mesh_read_number_token(ir_xml_reader* xr, char* token, size_t cap, char* err, size_t err_cap)
{
    int rc;
    int hit_lt;
    rc = ir_xml_read_token(xr, token, cap, 1, &hit_lt, err, err_cap);
    if (rc == IR_EOF) {
        ir_mesh_seterr(err, err_cap, "unexpected end of numeric content");
        return IR_ERR_FORMAT;
    }
    (void)hit_lt;
    return rc;
}

static int ir_mesh_skip_numeric_remainder(ir_xml_reader* xr, char* err, size_t err_cap)
{
    char tok[128];
    int rc;
    for (;;) {
        rc = ir_xml_read_token(xr, tok, sizeof(tok), 1, IR_NULL, err, err_cap);
        if (rc == IR_EOF) {
            return IR_OK;
        }
        if (rc != IR_OK) {
            return rc;
        }
    }
}

static int ir_mesh_read_token_or_end(ir_xml_reader* xr, char* token, size_t cap, int* hit_end, char* err, size_t err_cap)
{
    int rc;
    int hit_lt;
    if (!hit_end) {
        return IR_ERR_ARGUMENT;
    }
    *hit_end = 0;
    rc = ir_xml_read_token(xr, token, cap, 1, &hit_lt, err, err_cap);
    if (rc == IR_EOF) {
        if (hit_lt) {
            *hit_end = 1;
            return IR_OK;
        }
        ir_mesh_seterr(err, err_cap, "unexpected end of numeric content");
        return IR_ERR_FORMAT;
    }
    return rc;
}

static int ir_mesh_read_fx_token(ir_xml_reader* xr, ir_fx16* out, char* err, size_t err_cap)
{
    char tok[128];
    int rc;
    rc = ir_mesh_read_number_token(xr, tok, sizeof(tok), err, err_cap);
    if (rc != IR_OK) {
        return rc;
    }
    rc = ir_fx_from_str(tok, out);
    if (rc != IR_OK) {
        ir_mesh_seterr(err, err_cap, "invalid vertex float token");
        return rc;
    }
    return IR_OK;
}

static int ir_mesh_parse_one_vertex(ir_xml_reader* xr, int vtype, const char* first_token, ir_vertex* v, char* err, size_t err_cap)
{
    char tok[128];
    int rc;
    if (!xr || !first_token || !v) {
        return IR_ERR_ARGUMENT;
    }
    memset(v, 0, sizeof(*v));
    rc = ir_fx_from_str(first_token, &v->pos.x);
    if (rc != IR_OK) {
        ir_mesh_seterr(err, err_cap, "invalid vertex position token");
        return rc;
    }
    rc = ir_mesh_read_fx_token(xr, &v->pos.y, err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_mesh_read_fx_token(xr, &v->pos.z, err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_mesh_read_fx_token(xr, &v->normal.x, err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_mesh_read_fx_token(xr, &v->normal.y, err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_mesh_read_fx_token(xr, &v->normal.z, err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_mesh_read_number_token(xr, tok, sizeof(tok), err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_color_argb_from_hex(tok, &v->color_argb);
    if (rc != IR_OK) {
        ir_mesh_seterr(err, err_cap, "invalid vertex color token");
        return rc;
    }
    rc = ir_mesh_read_fx_token(xr, &v->uv1.x, err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_mesh_read_fx_token(xr, &v->uv1.y, err, err_cap); if (rc != IR_OK) return rc;
    if (vtype == IR_MESH_VERTEX_2TCOORDS) {
        rc = ir_mesh_read_fx_token(xr, &v->uv2.x, err, err_cap); if (rc != IR_OK) return rc;
        rc = ir_mesh_read_fx_token(xr, &v->uv2.y, err, err_cap); if (rc != IR_OK) return rc;
    } else if (vtype == IR_MESH_VERTEX_TANGENTS) {
        rc = ir_mesh_read_fx_token(xr, &v->tangent.x, err, err_cap); if (rc != IR_OK) return rc;
        rc = ir_mesh_read_fx_token(xr, &v->tangent.y, err, err_cap); if (rc != IR_OK) return rc;
        rc = ir_mesh_read_fx_token(xr, &v->tangent.z, err, err_cap); if (rc != IR_OK) return rc;
        rc = ir_mesh_read_fx_token(xr, &v->binormal.x, err, err_cap); if (rc != IR_OK) return rc;
        rc = ir_mesh_read_fx_token(xr, &v->binormal.y, err, err_cap); if (rc != IR_OK) return rc;
        rc = ir_mesh_read_fx_token(xr, &v->binormal.z, err, err_cap); if (rc != IR_OK) return rc;
    }
    return IR_OK;
}

static int ir_mesh_parse_vertices(ir_mesh_doc* doc, ir_xml_reader* xr, ir_mesh_buffer* buf, const ir_xml_tag* start_tag, char* err, size_t err_cap)
{
    const char* type_s;
    ir_u32 count;
    ir_u32 i;
    ir_u32 parsed;
    ir_vertex v;
    char tok[128];
    int rc;
    int vtype;
    int has_count;
    int hit_end;
    ir_xml_tag end_tag;
    type_s = ir_xml_tag_attr_local(start_tag, "type");
    if (!type_s) {
        type_s = "standard";
    }
    rc = ir_mesh_vertex_type_from_cstr(type_s, &vtype);
    if (rc != IR_OK) {
        ir_mesh_seterr(err, err_cap, "unsupported vertex type");
        return rc;
    }
    rc = ir_mesh_read_count_attr(start_tag, "vertexCount", &count, &has_count);
    if (rc != IR_OK) {
        ir_mesh_seterr(err, err_cap, "invalid vertex count");
        return rc;
    }
    buf->vertex_type = (ir_u8)vtype;
    buf->first_vertex = (ir_u32)doc->vertex_count;
    buf->vertex_count = has_count ? count : 0UL;
    buf->raw_vertices.begin = start_tag->raw_begin;
    buf->raw_vertices.end = start_tag->raw_end;
    buf->raw_vertices_text.begin = start_tag->raw_end;
    buf->raw_vertices_text.end = start_tag->raw_end;
    if (has_count && (size_t)count > doc->vertex_cap - doc->vertex_count) {
        ir_mesh_seterr(err, err_cap, "vertex storage capacity exceeded by declared vertexCount");
        return IR_ERR_CAPACITY;
    }
    if (start_tag->is_empty) {
        if (has_count && count != 0UL) {
            ir_mesh_seterr(err, err_cap, "self-closing vertices tag requires vertexCount=0");
            return IR_ERR_FORMAT;
        }
        buf->vertex_count = 0UL;
        return IR_OK;
    }
    if (has_count) {
        for (i = 0UL; i < count; ++i) {
            rc = ir_mesh_read_number_token(xr, tok, sizeof(tok), err, err_cap);
            if (rc != IR_OK) return rc;
            rc = ir_mesh_parse_one_vertex(xr, vtype, tok, &v, err, err_cap);
            if (rc != IR_OK) return rc;
            rc = ir_mesh_add_vertex(doc, &v);
            if (rc != IR_OK) return rc;
        }
        rc = ir_mesh_skip_numeric_remainder(xr, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
    } else {
        parsed = 0UL;
        for (;;) {
            rc = ir_mesh_read_token_or_end(xr, tok, sizeof(tok), &hit_end, err, err_cap);
            if (rc != IR_OK) return rc;
            if (hit_end) {
                break;
            }
            if (doc->vertex_count >= doc->vertex_cap) {
                ir_mesh_seterr(err, err_cap, "vertex storage capacity exceeded while inferring vertexCount");
                return IR_ERR_CAPACITY;
            }
            rc = ir_mesh_parse_one_vertex(xr, vtype, tok, &v, err, err_cap);
            if (rc != IR_OK) return rc;
            rc = ir_mesh_add_vertex(doc, &v);
            if (rc != IR_OK) return rc;
            ++parsed;
        }
        buf->vertex_count = parsed;
    }
    rc = ir_xml_read_expected_end_tag(xr, "vertices", &end_tag, err, err_cap);
    if (rc != IR_OK) {
        return rc;
    }
    buf->raw_vertices_text.end = end_tag.raw_begin;
    buf->raw_vertices.end = end_tag.raw_end;
    return IR_OK;
}

static int ir_mesh_parse_indices(ir_mesh_doc* doc, ir_xml_reader* xr, ir_mesh_buffer* buf, const ir_xml_tag* start_tag, char* err, size_t err_cap)
{
    ir_u32 count;
    ir_u32 i;
    ir_u32 parsed;
    char tok[128];
    int rc;
    int has_count;
    int hit_end;
    ir_xml_tag end_tag;
    rc = ir_mesh_read_count_attr(start_tag, "indexCount", &count, &has_count);
    if (rc != IR_OK) {
        ir_mesh_seterr(err, err_cap, "invalid index count");
        return rc;
    }
    buf->first_index = (ir_u32)doc->index_count;
    buf->index_count = has_count ? count : 0UL;
    buf->raw_indices.begin = start_tag->raw_begin;
    buf->raw_indices.end = start_tag->raw_end;
    buf->raw_indices_text.begin = start_tag->raw_end;
    buf->raw_indices_text.end = start_tag->raw_end;
    if (has_count && (size_t)count > doc->index_cap - doc->index_count) {
        ir_mesh_seterr(err, err_cap, "index storage capacity exceeded by declared indexCount");
        return IR_ERR_CAPACITY;
    }
    if (start_tag->is_empty) {
        if (has_count && count != 0UL) {
            ir_mesh_seterr(err, err_cap, "self-closing indices tag requires indexCount=0");
            return IR_ERR_FORMAT;
        }
        buf->index_count = 0UL;
        return IR_OK;
    }
    if (has_count) {
        for (i = 0UL; i < count; ++i) {
            ir_u32 idx;
            rc = ir_mesh_read_number_token(xr, tok, sizeof(tok), err, err_cap);
            if (rc != IR_OK) return rc;
            rc = ir_u32_from_irr_index_str(tok, &idx);
            if (rc != IR_OK) return rc;
            rc = ir_mesh_add_index(doc, idx);
            if (rc != IR_OK) return rc;
        }
        rc = ir_mesh_skip_numeric_remainder(xr, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
    } else {
        parsed = 0UL;
        for (;;) {
            ir_u32 idx;
            rc = ir_mesh_read_token_or_end(xr, tok, sizeof(tok), &hit_end, err, err_cap);
            if (rc != IR_OK) return rc;
            if (hit_end) {
                break;
            }
            if (doc->index_count >= doc->index_cap) {
                ir_mesh_seterr(err, err_cap, "index storage capacity exceeded while inferring indexCount");
                return IR_ERR_CAPACITY;
            }
            rc = ir_u32_from_irr_index_str(tok, &idx);
            if (rc != IR_OK) return rc;
            rc = ir_mesh_add_index(doc, idx);
            if (rc != IR_OK) return rc;
            ++parsed;
        }
        buf->index_count = parsed;
    }
    rc = ir_xml_read_expected_end_tag(xr, "indices", &end_tag, err, err_cap);
    if (rc != IR_OK) {
        return rc;
    }
    buf->raw_indices_text.end = end_tag.raw_begin;
    buf->raw_indices.end = end_tag.raw_end;
    return IR_OK;
}

static int ir_mesh_parse_buffer(ir_mesh_doc* doc, ir_xml_reader* xr, ir_mesh_buffer* buf, const ir_xml_tag* start_tag, char* err, size_t err_cap)
{
    ir_xml_tag tag;
    int rc;
    int have_bbox;
    ir_u32 mat_index;
    buf->raw_block.begin = start_tag->raw_begin;
    buf->raw_block.end = start_tag->raw_end;
    have_bbox = 0;
    mat_index = IR_MESH_NO_MATERIAL;
    if (start_tag->is_empty) {
        ir_box3fx_make_empty(&buf->bbox);
        return IR_OK;
    }
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "buffer")) {
                ir_mesh_seterr(err, err_cap, "unexpected closing tag inside buffer");
                return IR_ERR_FORMAT;
            }
            if (!have_bbox) {
                ir_mesh_recompute_one_buffer_bounds(doc, buf);
            }
            buf->raw_block.end = tag.raw_end;
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "boundingBox")) {
            rc = ir_mesh_read_bbox_tag(&tag, &buf->bbox);
            if (rc == IR_OK) {
                buf->raw_bbox.begin = tag.raw_begin;
                buf->raw_bbox.end = tag.raw_end;
                have_bbox = 1;
            }
            if (!tag.is_empty) {
                size_t end_pos;
                rc = ir_xml_skip_element(xr, &tag, err, err_cap);
                if (rc != IR_OK) return rc;
                end_pos = ir_xml_reader_raw_offset(xr);
                if (have_bbox) {
                    buf->raw_bbox.end = end_pos;
                }
            }
        } else if (ir_xml_name_eq(tag.name, "material")) {
            rc = ir_mesh_add_material(doc, &mat_index);
            if (rc != IR_OK) return rc;
            buf->material_index = mat_index;
            buf->raw_material.begin = tag.raw_begin;
            buf->raw_material.end = tag.raw_end;
            rc = ir_mesh_read_material(doc, xr, mat_index, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
            buf->raw_material = doc->materials[mat_index].raw_block;
        } else if (ir_xml_name_eq(tag.name, "vertices")) {
            rc = ir_mesh_parse_vertices(doc, xr, buf, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        } else if (ir_xml_name_eq(tag.name, "indices")) {
            rc = ir_mesh_parse_indices(doc, xr, buf, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        } else {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        }
    }
}

void ir_mesh_recompute_bounds(ir_mesh_doc* doc)
{
    size_t bi;
    size_t vi;
    ir_box3fx mesh_box;
    ir_box3fx_make_empty(&mesh_box);
    if (!doc) {
        return;
    }
    for (bi = 0U; bi < doc->buffer_count; ++bi) {
        ir_mesh_buffer* buf;
        ir_box3fx box;
        buf = &doc->buffers[bi];
        ir_box3fx_make_empty(&box);
        for (vi = 0U; vi < (size_t)buf->vertex_count; ++vi) {
            ir_vertex* v;
            v = &doc->vertices[buf->first_vertex + vi];
            ir_box3fx_expand(&box, v->pos);
            ir_box3fx_expand(&mesh_box, v->pos);
        }
        if (!ir_box3fx_is_empty(&box)) {
            buf->bbox = box;
        }
    }
    if (!ir_box3fx_is_empty(&mesh_box)) {
        doc->bbox = mesh_box;
    }
}

int ir_mesh_parse_memory(ir_mesh_doc* doc, const void* data, size_t size, char* err, size_t err_cap)
{
    ir_xml_reader xr;
    ir_xml_tag tag;
    int rc;
    int have_bbox;
    if (!doc || !data || size == 0U) {
        return IR_ERR_ARGUMENT;
    }
    doc->buffer_count = 0U;
    doc->material_count = 0U;
    doc->attr_count = 0U;
    doc->vertex_count = 0U;
    doc->index_count = 0U;
    doc->strings.len = 0U;
    doc->has_xmlns = 0;
    doc->has_version = 0;
    ir_box3fx_make_empty(&doc->bbox);
    ir_raw_range_clear(&doc->raw_document);
    ir_raw_range_clear(&doc->raw_root_block);
    ir_raw_range_clear(&doc->raw_root_bbox);
    ir_raw_source_clear(&doc->raw_source);
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
                ir_mesh_seterr(err, err_cap, "missing <mesh> root element");
                return IR_ERR_FORMAT;
            }
            return rc;
        }
        if (!tag.is_end && ir_xml_name_eq(tag.name, "mesh")) {
            break;
        }
        if (!tag.is_end) {
            rc = ir_xml_skip_element(&xr, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        }
    }
    doc->raw_root_block.begin = tag.raw_begin;
    doc->raw_root_block.end = tag.raw_end;
    if (ir_xml_tag_attr_local(&tag, "xmlns")) {
        rc = ir_strpool_put(&doc->strings, ir_xml_tag_attr_local(&tag, "xmlns"), strlen(ir_xml_tag_attr_local(&tag, "xmlns")), &doc->xmlns_off, &doc->xmlns_len);
        if (rc != IR_OK) return rc;
        doc->has_xmlns = 1;
    }
    if (ir_xml_tag_attr_local(&tag, "version")) {
        rc = ir_strpool_put(&doc->strings, ir_xml_tag_attr_local(&tag, "version"), strlen(ir_xml_tag_attr_local(&tag, "version")), &doc->version_off, &doc->version_len);
        if (rc != IR_OK) return rc;
        doc->has_version = 1;
    }
    if (tag.is_empty) {
        return IR_OK;
    }
    have_bbox = 0;
    for (;;) {
        ir_u32 bindex;
        ir_mesh_buffer* buf;
        rc = ir_xml_next_tag(&xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "mesh")) {
                ir_mesh_seterr(err, err_cap, "unexpected closing tag at mesh level");
                return IR_ERR_FORMAT;
            }
            doc->raw_root_block.end = tag.raw_end;
            if (!have_bbox) {
                ir_mesh_recompute_bounds(doc);
            }
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "boundingBox")) {
            rc = ir_mesh_read_bbox_tag(&tag, &doc->bbox);
            if (rc == IR_OK) {
                doc->raw_root_bbox.begin = tag.raw_begin;
                doc->raw_root_bbox.end = tag.raw_end;
                have_bbox = 1;
            }
            if (!tag.is_empty) {
                size_t end_pos;
                rc = ir_xml_skip_element(&xr, &tag, err, err_cap);
                if (rc != IR_OK) return rc;
                end_pos = ir_xml_reader_raw_offset(&xr);
                if (have_bbox) {
                    doc->raw_root_bbox.end = end_pos;
                }
            }
        } else if (ir_xml_name_eq(tag.name, "buffer")) {
            rc = ir_mesh_add_buffer(doc, &bindex);
            if (rc != IR_OK) return rc;
            buf = &doc->buffers[bindex];
            buf->line = tag.line;
            buf->col = tag.col;
            rc = ir_mesh_parse_buffer(doc, &xr, buf, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        } else {
            rc = ir_xml_skip_element(&xr, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        }
    }
}

static int ir_mesh_write_box(ir_outbuf* out, const ir_box3fx* box, size_t indent, char* err, size_t err_cap)
{
    char a[32], b[32], c[32], d[32], e[32], f[32];
    int rc;
    rc = ir_fx_to_str(box->min_edge.x, a, sizeof(a)); if (rc != IR_OK) return rc;
    rc = ir_fx_to_str(box->min_edge.y, b, sizeof(b)); if (rc != IR_OK) return rc;
    rc = ir_fx_to_str(box->min_edge.z, c, sizeof(c)); if (rc != IR_OK) return rc;
    rc = ir_fx_to_str(box->max_edge.x, d, sizeof(d)); if (rc != IR_OK) return rc;
    rc = ir_fx_to_str(box->max_edge.y, e, sizeof(e)); if (rc != IR_OK) return rc;
    rc = ir_fx_to_str(box->max_edge.z, f, sizeof(f)); if (rc != IR_OK) return rc;
    rc = ir_outbuf_pad(out, ' ', indent); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "<boundingBox minEdge=\""); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, a); if (rc != IR_OK) return rc;
    rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, b); if (rc != IR_OK) return rc;
    rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, c); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "\" maxEdge=\""); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, d); if (rc != IR_OK) return rc;
    rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, e); if (rc != IR_OK) return rc;
    rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, f); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "\" />\n"); if (rc != IR_OK) return rc;
    (void)err; (void)err_cap;
    return IR_OK;
}

static int ir_mesh_write_vertices(const ir_mesh_doc* doc, const ir_mesh_buffer* buf, ir_outbuf* out)
{
    size_t i;
    int rc;
    char t0[32], t1[32], t2[32], t3[32], t4[32], t5[32];
    char t6[32], t7[32], t8[32], t9[32], t10[32], t11[32];
    char t12[32], t13[32], t14[32];
    rc = ir_outbuf_pad(out, ' ', 4U); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "<vertices type=\""); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, ir_mesh_vertex_type_cstr((int)buf->vertex_type)); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "\" vertexCount=\""); if (rc != IR_OK) return rc;
    sprintf(t0, "%lu", (unsigned long)buf->vertex_count);
    rc = ir_outbuf_puts(out, t0); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "\">\n"); if (rc != IR_OK) return rc;
    for (i = 0U; i < (size_t)buf->vertex_count; ++i) {
        const ir_vertex* v;
        v = &doc->vertices[buf->first_vertex + i];
        rc = ir_outbuf_pad(out, ' ', 6U); if (rc != IR_OK) return rc;
        rc = ir_fx_to_str(v->pos.x, t0, sizeof(t0)); if (rc != IR_OK) return rc;
        rc = ir_fx_to_str(v->pos.y, t1, sizeof(t1)); if (rc != IR_OK) return rc;
        rc = ir_fx_to_str(v->pos.z, t2, sizeof(t2)); if (rc != IR_OK) return rc;
        rc = ir_fx_to_str(v->normal.x, t3, sizeof(t3)); if (rc != IR_OK) return rc;
        rc = ir_fx_to_str(v->normal.y, t4, sizeof(t4)); if (rc != IR_OK) return rc;
        rc = ir_fx_to_str(v->normal.z, t5, sizeof(t5)); if (rc != IR_OK) return rc;
        rc = ir_fx_to_str(v->uv1.x, t6, sizeof(t6)); if (rc != IR_OK) return rc;
        rc = ir_fx_to_str(v->uv1.y, t7, sizeof(t7)); if (rc != IR_OK) return rc;
        sprintf(t8, "%08lx", (unsigned long)v->color_argb);
        rc = ir_outbuf_puts(out, t0); if (rc != IR_OK) return rc;
        rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
        rc = ir_outbuf_puts(out, t1); if (rc != IR_OK) return rc;
        rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
        rc = ir_outbuf_puts(out, t2); if (rc != IR_OK) return rc;
        rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
        rc = ir_outbuf_puts(out, t3); if (rc != IR_OK) return rc;
        rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
        rc = ir_outbuf_puts(out, t4); if (rc != IR_OK) return rc;
        rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
        rc = ir_outbuf_puts(out, t5); if (rc != IR_OK) return rc;
        rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
        rc = ir_outbuf_puts(out, t8); if (rc != IR_OK) return rc;
        rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
        rc = ir_outbuf_puts(out, t6); if (rc != IR_OK) return rc;
        rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
        rc = ir_outbuf_puts(out, t7); if (rc != IR_OK) return rc;
        if (buf->vertex_type == IR_MESH_VERTEX_2TCOORDS) {
            rc = ir_fx_to_str(v->uv2.x, t9, sizeof(t9)); if (rc != IR_OK) return rc;
            rc = ir_fx_to_str(v->uv2.y, t10, sizeof(t10)); if (rc != IR_OK) return rc;
            rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
            rc = ir_outbuf_puts(out, t9); if (rc != IR_OK) return rc;
            rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
            rc = ir_outbuf_puts(out, t10); if (rc != IR_OK) return rc;
        } else if (buf->vertex_type == IR_MESH_VERTEX_TANGENTS) {
            rc = ir_fx_to_str(v->tangent.x, t9, sizeof(t9)); if (rc != IR_OK) return rc;
            rc = ir_fx_to_str(v->tangent.y, t10, sizeof(t10)); if (rc != IR_OK) return rc;
            rc = ir_fx_to_str(v->tangent.z, t11, sizeof(t11)); if (rc != IR_OK) return rc;
            rc = ir_fx_to_str(v->binormal.x, t12, sizeof(t12)); if (rc != IR_OK) return rc;
            rc = ir_fx_to_str(v->binormal.y, t13, sizeof(t13)); if (rc != IR_OK) return rc;
            rc = ir_fx_to_str(v->binormal.z, t14, sizeof(t14)); if (rc != IR_OK) return rc;
            rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
            rc = ir_outbuf_puts(out, t9); if (rc != IR_OK) return rc;
            rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
            rc = ir_outbuf_puts(out, t10); if (rc != IR_OK) return rc;
            rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
            rc = ir_outbuf_puts(out, t11); if (rc != IR_OK) return rc;
            rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
            rc = ir_outbuf_puts(out, t12); if (rc != IR_OK) return rc;
            rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
            rc = ir_outbuf_puts(out, t13); if (rc != IR_OK) return rc;
            rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
            rc = ir_outbuf_puts(out, t14); if (rc != IR_OK) return rc;
        }
        rc = ir_outbuf_putc(out, '\n'); if (rc != IR_OK) return rc;
    }
    rc = ir_outbuf_pad(out, ' ', 4U); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "</vertices>\n"); if (rc != IR_OK) return rc;
    return IR_OK;
}

static int ir_mesh_write_indices(const ir_mesh_doc* doc, const ir_mesh_buffer* buf, ir_outbuf* out)
{
    size_t i;
    int rc;
    char nb[32];
    rc = ir_outbuf_pad(out, ' ', 4U); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "<indices indexCount=\""); if (rc != IR_OK) return rc;
    sprintf(nb, "%lu", (unsigned long)buf->index_count);
    rc = ir_outbuf_puts(out, nb); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "\">\n"); if (rc != IR_OK) return rc;
    for (i = 0U; i < (size_t)buf->index_count; ++i) {
        if ((i % 25U) == 0U) {
            rc = ir_outbuf_pad(out, ' ', 6U); if (rc != IR_OK) return rc;
        }
        sprintf(nb, "%lu", (unsigned long)doc->indices[buf->first_index + i]);
        rc = ir_outbuf_puts(out, nb); if (rc != IR_OK) return rc;
        if (((i + 1U) % 25U) == 0U || i + 1U == (size_t)buf->index_count) {
            rc = ir_outbuf_putc(out, '\n'); if (rc != IR_OK) return rc;
        } else {
            rc = ir_outbuf_putc(out, ' '); if (rc != IR_OK) return rc;
        }
    }
    rc = ir_outbuf_pad(out, ' ', 4U); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(out, "</indices>\n"); if (rc != IR_OK) return rc;
    return IR_OK;
}

int ir_mesh_write_memory(const ir_mesh_doc* doc, char* out_mem, size_t out_cap, size_t* out_len, char* err, size_t err_cap)
{
    ir_outbuf out;
    size_t bi;
    int rc;
    const char* xmlns;
    const char* version;
    if (!doc || !out_mem || out_cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    ir_outbuf_init(&out, out_mem, out_cap);
    xmlns = doc->has_xmlns ? ir_strpool_get(&doc->strings, doc->xmlns_off) : "http://irrlicht.sourceforge.net/IRRMESH_09_2007";
    version = doc->has_version ? ir_strpool_get(&doc->strings, doc->version_off) : "1.0";
    rc = ir_outbuf_puts(&out, "<?xml version=\"1.0\"?>\n"); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(&out, "<mesh xmlns=\""); if (rc != IR_OK) return rc;
    rc = ir_xml_escape_to_out(&out, xmlns); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(&out, "\" version=\""); if (rc != IR_OK) return rc;
    rc = ir_xml_escape_to_out(&out, version); if (rc != IR_OK) return rc;
    rc = ir_outbuf_puts(&out, "\">\n"); if (rc != IR_OK) return rc;
    rc = ir_mesh_write_box(&out, &doc->bbox, 2U, err, err_cap); if (rc != IR_OK) return rc;
    for (bi = 0U; bi < doc->buffer_count; ++bi) {
        const ir_mesh_buffer* buf;
        size_t ai;
        buf = &doc->buffers[bi];
        rc = ir_outbuf_puts(&out, "  <buffer>\n"); if (rc != IR_OK) return rc;
        rc = ir_mesh_write_box(&out, &buf->bbox, 4U, err, err_cap); if (rc != IR_OK) return rc;
        if (buf->material_index == IR_MESH_NO_MATERIAL || buf->material_index >= doc->material_count) {
            rc = ir_outbuf_puts(&out, "    <material />\n"); if (rc != IR_OK) return rc;
        } else {
            const ir_material_block* mb;
            rc = ir_outbuf_puts(&out, "    <material>\n"); if (rc != IR_OK) return rc;
            mb = &doc->materials[buf->material_index];
            for (ai = 0U; ai < (size_t)mb->attr_count; ++ai) {
                rc = ir_attr_write_xml(&doc->attrs[mb->first_attr + ai], &doc->strings, &out, 6U, err, err_cap);
                if (rc != IR_OK) return rc;
            }
            rc = ir_outbuf_puts(&out, "    </material>\n"); if (rc != IR_OK) return rc;
        }
        rc = ir_mesh_write_vertices(doc, buf, &out); if (rc != IR_OK) return rc;
        rc = ir_mesh_write_indices(doc, buf, &out); if (rc != IR_OK) return rc;
        rc = ir_outbuf_puts(&out, "  </buffer>\n"); if (rc != IR_OK) return rc;
    }
    rc = ir_outbuf_puts(&out, "</mesh>\n"); if (rc != IR_OK) return rc;
    return ir_outbuf_finalize(&out, out_len);
}
