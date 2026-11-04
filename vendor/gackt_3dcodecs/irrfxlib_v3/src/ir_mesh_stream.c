#include "ir_mesh_stream.h"
#include "ir_fx.h"

#include <string.h>

#define IR_STREAM_ATTR_POOL_CAP (IR_XML_MAX_ATTRS * (IR_XML_NAME_CAP + IR_XML_VALUE_CAP + 16) + 256)

static void ir_mesh_stream_seterr(char* err, size_t err_cap, const char* msg)
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

static int ir_mesh_stream_emit_attr(const ir_xml_tag* tag, const ir_mesh_stream_events* events, void* user, ir_u32 buffer_index, char* err, size_t err_cap)
{
    ir_attr attr;
    ir_strpool pool;
    char pool_mem[IR_STREAM_ATTR_POOL_CAP];
    int rc;
    ir_strpool_init(&pool, pool_mem, sizeof(pool_mem));
    rc = ir_attr_from_xml_tag(tag, &attr, &pool, err, err_cap);
    if (rc != IR_OK) {
        return rc;
    }
    if (events && events->on_material_attr) {
        rc = events->on_material_attr(user, buffer_index, &attr);
        if (rc != IR_OK) {
            ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
            return rc;
        }
    }
    if (events && events->on_material_attr_ex) {
        rc = events->on_material_attr_ex(user, buffer_index, &attr, &pool);
        if (rc != IR_OK) {
            ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
            return rc;
        }
    }
    return IR_OK;
}

static int ir_mesh_stream_read_bbox(const ir_xml_tag* tag, ir_box3fx* box)
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

static int ir_mesh_stream_read_count_attr(const ir_xml_tag* tag, const char* attr_name, ir_u32* out, int* has_attr)
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

static int ir_mesh_stream_read_number_token(ir_xml_reader* xr, char* token, size_t cap, char* err, size_t err_cap)
{
    int rc;
    int hit_lt;
    rc = ir_xml_read_token(xr, token, cap, 1, &hit_lt, err, err_cap);
    if (rc == IR_EOF) {
        ir_mesh_stream_seterr(err, err_cap, "unexpected end of numeric content");
        return IR_ERR_FORMAT;
    }
    (void)hit_lt;
    return rc;
}

static int ir_mesh_stream_skip_numeric_remainder(ir_xml_reader* xr, char* err, size_t err_cap)
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

static int ir_mesh_stream_read_token_or_end(ir_xml_reader* xr, char* token, size_t cap, int* hit_end, char* err, size_t err_cap)
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
        ir_mesh_stream_seterr(err, err_cap, "unexpected end of numeric content");
        return IR_ERR_FORMAT;
    }
    return rc;
}

static int ir_mesh_stream_read_fx_token(ir_xml_reader* xr, ir_fx16* out, char* err, size_t err_cap)
{
    char tok[128];
    int rc;
    rc = ir_mesh_stream_read_number_token(xr, tok, sizeof(tok), err, err_cap);
    if (rc != IR_OK) return rc;
    rc = ir_fx_from_str(tok, out);
    if (rc != IR_OK) {
        ir_mesh_stream_seterr(err, err_cap, "invalid vertex float token");
        return rc;
    }
    return IR_OK;
}

static int ir_mesh_stream_parse_one_vertex(ir_xml_reader* xr, int vtype, const char* first_token, ir_vertex* v, char* err, size_t err_cap)
{
    char tok[128];
    int rc;
    if (!xr || !first_token || !v) {
        return IR_ERR_ARGUMENT;
    }
    memset(v, 0, sizeof(*v));
    rc = ir_fx_from_str(first_token, &v->pos.x);
    if (rc != IR_OK) {
        ir_mesh_stream_seterr(err, err_cap, "invalid vertex position token");
        return rc;
    }
    rc = ir_mesh_stream_read_fx_token(xr, &v->pos.y, err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_mesh_stream_read_fx_token(xr, &v->pos.z, err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_mesh_stream_read_fx_token(xr, &v->normal.x, err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_mesh_stream_read_fx_token(xr, &v->normal.y, err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_mesh_stream_read_fx_token(xr, &v->normal.z, err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_mesh_stream_read_number_token(xr, tok, sizeof(tok), err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_color_argb_from_hex(tok, &v->color_argb);
    if (rc != IR_OK) {
        ir_mesh_stream_seterr(err, err_cap, "invalid vertex color token");
        return rc;
    }
    rc = ir_mesh_stream_read_fx_token(xr, &v->uv1.x, err, err_cap); if (rc != IR_OK) return rc;
    rc = ir_mesh_stream_read_fx_token(xr, &v->uv1.y, err, err_cap); if (rc != IR_OK) return rc;
    if (vtype == IR_MESH_VERTEX_2TCOORDS) {
        rc = ir_mesh_stream_read_fx_token(xr, &v->uv2.x, err, err_cap); if (rc != IR_OK) return rc;
        rc = ir_mesh_stream_read_fx_token(xr, &v->uv2.y, err, err_cap); if (rc != IR_OK) return rc;
    } else if (vtype == IR_MESH_VERTEX_TANGENTS) {
        rc = ir_mesh_stream_read_fx_token(xr, &v->tangent.x, err, err_cap); if (rc != IR_OK) return rc;
        rc = ir_mesh_stream_read_fx_token(xr, &v->tangent.y, err, err_cap); if (rc != IR_OK) return rc;
        rc = ir_mesh_stream_read_fx_token(xr, &v->tangent.z, err, err_cap); if (rc != IR_OK) return rc;
        rc = ir_mesh_stream_read_fx_token(xr, &v->binormal.x, err, err_cap); if (rc != IR_OK) return rc;
        rc = ir_mesh_stream_read_fx_token(xr, &v->binormal.y, err, err_cap); if (rc != IR_OK) return rc;
        rc = ir_mesh_stream_read_fx_token(xr, &v->binormal.z, err, err_cap); if (rc != IR_OK) return rc;
    }
    return IR_OK;
}

static int ir_mesh_stream_parse_vertices(ir_xml_reader* xr, const ir_xml_tag* start_tag, const ir_mesh_stream_events* events, void* user, ir_u32 buffer_index, char* err, size_t err_cap)
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
    type_s = ir_xml_tag_attr_local(start_tag, "type");
    if (!type_s) {
        type_s = "standard";
    }
    rc = ir_mesh_vertex_type_from_cstr(type_s, &vtype);
    if (rc != IR_OK) {
        return rc;
    }
    rc = ir_mesh_stream_read_count_attr(start_tag, "vertexCount", &count, &has_count);
    if (rc != IR_OK) {
        return rc;
    }
    if (events && events->on_vertices_begin) {
        rc = events->on_vertices_begin(user, buffer_index, vtype, count, start_tag->line, start_tag->col);
        if (rc != IR_OK) {
            ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
            return rc;
        }
    }
    if (start_tag->is_empty) {
        if (has_count && count != 0UL) {
            ir_mesh_stream_seterr(err, err_cap, "self-closing vertices tag requires vertexCount=0");
            return IR_ERR_FORMAT;
        }
        return IR_OK;
    }
    if (has_count) {
        for (i = 0UL; i < count; ++i) {
            rc = ir_mesh_stream_read_number_token(xr, tok, sizeof(tok), err, err_cap);
            if (rc != IR_OK) return rc;
            rc = ir_mesh_stream_parse_one_vertex(xr, vtype, tok, &v, err, err_cap);
            if (rc != IR_OK) return rc;
            if (events && events->on_vertex) {
                rc = events->on_vertex(user, buffer_index, i, &v);
                if (rc != IR_OK) {
                    ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
                    return rc;
                }
            }
        }
        rc = ir_mesh_stream_skip_numeric_remainder(xr, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
    } else {
        parsed = 0UL;
        for (;;) {
            rc = ir_mesh_stream_read_token_or_end(xr, tok, sizeof(tok), &hit_end, err, err_cap);
            if (rc != IR_OK) return rc;
            if (hit_end) {
                break;
            }
            rc = ir_mesh_stream_parse_one_vertex(xr, vtype, tok, &v, err, err_cap);
            if (rc != IR_OK) return rc;
            if (events && events->on_vertex) {
                rc = events->on_vertex(user, buffer_index, parsed, &v);
                if (rc != IR_OK) {
                    ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
                    return rc;
                }
            }
            ++parsed;
        }
    }
    return ir_xml_read_expected_end(xr, "vertices", err, err_cap);
}

static int ir_mesh_stream_parse_indices(ir_xml_reader* xr, const ir_xml_tag* start_tag, const ir_mesh_stream_events* events, void* user, ir_u32 buffer_index, char* err, size_t err_cap)
{
    ir_u32 count;
    ir_u32 i;
    ir_u32 parsed;
    char tok[128];
    int rc;
    int has_count;
    int hit_end;
    rc = ir_mesh_stream_read_count_attr(start_tag, "indexCount", &count, &has_count);
    if (rc != IR_OK) {
        return rc;
    }
    if (events && events->on_indices_begin) {
        rc = events->on_indices_begin(user, buffer_index, count, start_tag->line, start_tag->col);
        if (rc != IR_OK) {
            ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
            return rc;
        }
    }
    if (start_tag->is_empty) {
        if (has_count && count != 0UL) {
            ir_mesh_stream_seterr(err, err_cap, "self-closing indices tag requires indexCount=0");
            return IR_ERR_FORMAT;
        }
        return IR_OK;
    }
    if (has_count) {
        for (i = 0UL; i < count; ++i) {
            ir_u32 idx;
            rc = ir_mesh_stream_read_number_token(xr, tok, sizeof(tok), err, err_cap);
            if (rc != IR_OK) return rc;
            rc = ir_u32_from_irr_index_str(tok, &idx);
            if (rc != IR_OK) return rc;
            if (events && events->on_index) {
                rc = events->on_index(user, buffer_index, i, idx);
                if (rc != IR_OK) {
                    ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
                    return rc;
                }
            }
        }
        rc = ir_mesh_stream_skip_numeric_remainder(xr, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
    } else {
        parsed = 0UL;
        for (;;) {
            ir_u32 idx;
            rc = ir_mesh_stream_read_token_or_end(xr, tok, sizeof(tok), &hit_end, err, err_cap);
            if (rc != IR_OK) return rc;
            if (hit_end) {
                break;
            }
            rc = ir_u32_from_irr_index_str(tok, &idx);
            if (rc != IR_OK) return rc;
            if (events && events->on_index) {
                rc = events->on_index(user, buffer_index, parsed, idx);
                if (rc != IR_OK) {
                    ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
                    return rc;
                }
            }
            ++parsed;
        }
    }
    return ir_xml_read_expected_end(xr, "indices", err, err_cap);
}

static int ir_mesh_stream_parse_material_attr_wrapper(ir_xml_reader* xr, const ir_xml_tag* start_tag, const ir_mesh_stream_events* events, void* user, ir_u32 buffer_index, char* err, size_t err_cap)
{
    ir_xml_tag tag;
    int rc;
    if (!xr || !start_tag) {
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
                return IR_ERR_FORMAT;
            }
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            rc = ir_mesh_stream_parse_material_attr_wrapper(xr, &tag, events, user, buffer_index, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        } else {
            rc = ir_mesh_stream_emit_attr(&tag, events, user, buffer_index, err, err_cap);
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

static int ir_mesh_stream_parse_material(ir_xml_reader* xr, const ir_xml_tag* start_tag, const ir_mesh_stream_events* events, void* user, ir_u32 buffer_index, char* err, size_t err_cap)
{
    ir_xml_tag tag;
    int rc;
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
                return IR_ERR_FORMAT;
            }
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "attributes")) {
            rc = ir_mesh_stream_parse_material_attr_wrapper(xr, &tag, events, user, buffer_index, err, err_cap);
            if (rc != IR_OK) {
                return rc;
            }
        } else {
            rc = ir_mesh_stream_emit_attr(&tag, events, user, buffer_index, err, err_cap);
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

static int ir_mesh_stream_parse_buffer(ir_xml_reader* xr, const ir_xml_tag* start_tag, const ir_mesh_stream_events* events, void* user, ir_u32 buffer_index, char* err, size_t err_cap)
{
    ir_xml_tag tag;
    int rc;
    if (start_tag->is_empty) {
        if (events && events->on_buffer_end) {
            rc = events->on_buffer_end(user, buffer_index);
            if (rc != IR_OK) {
                ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
                return rc;
            }
        }
        return IR_OK;
    }
    for (;;) {
        rc = ir_xml_next_tag(xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "buffer")) {
                return IR_ERR_FORMAT;
            }
            if (events && events->on_buffer_end) {
                rc = events->on_buffer_end(user, buffer_index);
                if (rc != IR_OK) {
                    ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
                    return rc;
                }
            }
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "boundingBox")) {
            ir_box3fx box;
            rc = ir_mesh_stream_read_bbox(&tag, &box);
            if (rc == IR_OK && events && events->on_buffer_bbox) {
                rc = events->on_buffer_bbox(user, buffer_index, &box, tag.line, tag.col);
                if (rc != IR_OK) {
                    ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
                    return rc;
                }
            }
            if (!tag.is_empty) {
                rc = ir_xml_skip_element(xr, &tag, err, err_cap);
                if (rc != IR_OK) return rc;
            }
        } else if (ir_xml_name_eq(tag.name, "material")) {
            rc = ir_mesh_stream_parse_material(xr, &tag, events, user, buffer_index, err, err_cap);
            if (rc != IR_OK) return rc;
        } else if (ir_xml_name_eq(tag.name, "vertices")) {
            rc = ir_mesh_stream_parse_vertices(xr, &tag, events, user, buffer_index, err, err_cap);
            if (rc != IR_OK) return rc;
        } else if (ir_xml_name_eq(tag.name, "indices")) {
            rc = ir_mesh_stream_parse_indices(xr, &tag, events, user, buffer_index, err, err_cap);
            if (rc != IR_OK) return rc;
        } else {
            rc = ir_xml_skip_element(xr, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        }
    }
}

int ir_mesh_parse_memory_stream(const void* data, size_t size, const ir_mesh_stream_events* events, void* user, char* err, size_t err_cap)
{
    ir_xml_reader xr;
    ir_xml_tag tag;
    int rc;
    ir_u32 buffer_index;
    if (!data || size == 0U) {
        return IR_ERR_ARGUMENT;
    }
    buffer_index = 0UL;
    ir_xml_reader_init(&xr, data, size);
    for (;;) {
        rc = ir_xml_next_tag(&xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return (rc == IR_EOF) ? IR_ERR_FORMAT : rc;
        }
        if (!tag.is_end && ir_xml_name_eq(tag.name, "mesh")) {
            break;
        }
        if (!tag.is_end) {
            rc = ir_xml_skip_element(&xr, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        }
    }
    if (events && events->on_mesh_begin) {
        rc = events->on_mesh_begin(user, ir_xml_tag_attr_local(&tag, "xmlns"), ir_xml_tag_attr_local(&tag, "version"), tag.line, tag.col);
        if (rc != IR_OK) {
            ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
            return rc;
        }
    }
    if (tag.is_empty) {
        if (events && events->on_mesh_end) {
            rc = events->on_mesh_end(user);
            if (rc != IR_OK) {
                ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
                return rc;
            }
        }
        return IR_OK;
    }
    for (;;) {
        rc = ir_xml_next_tag(&xr, &tag, err, err_cap);
        if (rc != IR_OK) {
            return rc;
        }
        if (tag.is_end) {
            if (!ir_xml_name_eq(tag.name, "mesh")) {
                return IR_ERR_FORMAT;
            }
            if (events && events->on_mesh_end) {
                rc = events->on_mesh_end(user);
                if (rc != IR_OK) {
                    ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
                    return rc;
                }
            }
            return IR_OK;
        }
        if (ir_xml_name_eq(tag.name, "boundingBox")) {
            ir_box3fx box;
            rc = ir_mesh_stream_read_bbox(&tag, &box);
            if (rc == IR_OK && events && events->on_mesh_bbox) {
                rc = events->on_mesh_bbox(user, &box, tag.line, tag.col);
                if (rc != IR_OK) {
                    ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
                    return rc;
                }
            }
            if (!tag.is_empty) {
                rc = ir_xml_skip_element(&xr, &tag, err, err_cap);
                if (rc != IR_OK) return rc;
            }
        } else if (ir_xml_name_eq(tag.name, "buffer")) {
            if (events && events->on_buffer_begin) {
                rc = events->on_buffer_begin(user, buffer_index, tag.line, tag.col);
                if (rc != IR_OK) {
                    ir_mesh_stream_seterr(err, err_cap, "mesh callback aborted");
                    return rc;
                }
            }
            rc = ir_mesh_stream_parse_buffer(&xr, &tag, events, user, buffer_index, err, err_cap);
            if (rc != IR_OK) return rc;
            buffer_index += 1UL;
        } else {
            rc = ir_xml_skip_element(&xr, &tag, err, err_cap);
            if (rc != IR_OK) return rc;
        }
    }
}
