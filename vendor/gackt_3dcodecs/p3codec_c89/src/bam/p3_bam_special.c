#include <string.h>
#include "p3_bam_archive.h"
#include "p3_bytes.h"
#include "p3_bam_datagram.h"
#include "p3_bam_special.h"
#include "p3_error.h"

#define P3_BAM_LOCAL_HANDLE_CAP 16U

typedef struct p3_bam_payload_cursor {
    p3_bam_cursor cur;
    int long_object_id;
} p3_bam_payload_cursor;

typedef struct p3_bam_local_handle {
    p3_u16 id;
    char name[P3_BAM_SPECIAL_TEXT_CAP];
} p3_bam_local_handle;

typedef struct p3_bam_handle_cache {
    p3_bam_local_handle items[P3_BAM_LOCAL_HANDLE_CAP];
    p3_u8 count;
} p3_bam_handle_cache;

static void p3_special_copy_text(char *dst, p3_u16 dst_cap, const char *src) {
    p3_u16 i;
    if (dst == 0 || dst_cap == 0U) {
        return;
    }
    if (src == 0) {
        dst[0] = 0;
        return;
    }
    i = 0U;
    while (src[i] != 0 && (p3_u32)i + 1UL < (p3_u32)dst_cap) {
        dst[i] = src[i];
        i = (p3_u16)(i + 1U);
    }
    dst[i] = 0;
}

static void p3_special_copy_pool_text(const p3_bam_archive *archive, p3_u16 off, p3_u16 len, char *dst, p3_u16 dst_cap) {
    if (dst == 0 || dst_cap == 0U) {
        return;
    }
    if (archive == 0) {
        dst[0] = 0;
        return;
    }
    if (p3_string_pool_copy_out(&archive->pool, off, len, dst, dst_cap) != P3_OK) {
        dst[0] = 0;
    }
}

static int p3_special_find_type(const p3_bam_archive *archive, p3_u16 type_id, char *dst, p3_u16 dst_cap) {
    int idx;
    if (dst != 0 && dst_cap > 0U) {
        dst[0] = 0;
    }
    idx = p3_bam_find_type_index(archive, type_id);
    if (idx < 0) {
        return P3_ERR_RANGE;
    }
    p3_special_copy_pool_text(archive,
        archive->types[idx].name_off,
        archive->types[idx].name_len,
        dst,
        dst_cap);
    return P3_OK;
}

static int p3_special_is_geom_primitive_name(const char *name) {
    if (name == 0) {
        return P3_FALSE;
    }
    if (strcmp(name, "GeomTriangles") == 0) {
        return P3_TRUE;
    }
    if (strcmp(name, "GeomTrianglesAdjacency") == 0) {
        return P3_TRUE;
    }
    if (strcmp(name, "GeomTristrips") == 0) {
        return P3_TRUE;
    }
    if (strcmp(name, "GeomTristripsAdjacency") == 0) {
        return P3_TRUE;
    }
    if (strcmp(name, "GeomTrifans") == 0) {
        return P3_TRUE;
    }
    if (strcmp(name, "GeomLines") == 0) {
        return P3_TRUE;
    }
    if (strcmp(name, "GeomLinestrips") == 0) {
        return P3_TRUE;
    }
    if (strcmp(name, "GeomPoints") == 0) {
        return P3_TRUE;
    }
    if (strcmp(name, "GeomPatches") == 0) {
        return P3_TRUE;
    }
    return P3_FALSE;
}

static void p3_special_payload_cursor_init(p3_bam_payload_cursor *pc, const p3_bam_record *rec, const p3_bam_archive *archive) {
    p3_bam_cursor_init(&pc->cur, archive->raw + rec->payload_off, rec->payload_len);
    pc->long_object_id = rec->payload_long_object_id ? 1 : 0;
}

static int p3_special_get_s32(p3_bam_payload_cursor *pc, p3_s32 *out_value) {
    p3_u32 tmp;
    int rc;
    rc = p3_bam_get_u32(&pc->cur, &tmp);
    if (rc != P3_OK) {
        return rc;
    }
    *out_value = (p3_s32)tmp;
    return P3_OK;
}

static int p3_special_get_pointer(p3_bam_payload_cursor *pc, p3_u32 *out_id) {
    int rc;
    p3_u16 short_id;
    if (pc->long_object_id) {
        return p3_bam_get_u32(&pc->cur, out_id);
    }
    rc = p3_bam_get_u16(&pc->cur, &short_id);
    if (rc != P3_OK) {
        return rc;
    }
    *out_id = (p3_u32)short_id;
    if (*out_id == 0xffffUL) {
        pc->long_object_id = 1;
    }
    return P3_OK;
}

static int p3_special_put_s32(p3_bam_builder *b, p3_s32 value) {
    return p3_bam_put_u32(b, (p3_u32)value);
}

static int p3_special_put_pointer(p3_bam_builder *b, int *io_long_object_id, p3_u32 object_id) {
    int rc;
    if (*io_long_object_id) {
        return p3_bam_put_u32(b, object_id);
    }
    if (object_id > 0xffffUL) {
        return P3_ERR_RANGE;
    }
    rc = p3_bam_put_u16(b, (p3_u16)object_id);
    if (rc != P3_OK) {
        return rc;
    }
    if (object_id == 0xffffUL) {
        *io_long_object_id = 1;
    }
    return P3_OK;
}

static int p3_special_get_pta_id(p3_bam_payload_cursor *pc, int *io_long_pta_id, p3_u32 *out_id) {
    int rc;
    p3_u16 short_id;
    if (pc == 0 || io_long_pta_id == 0 || out_id == 0) {
        return P3_ERR_INVALID;
    }
    if (*io_long_pta_id) {
        return p3_bam_get_u32(&pc->cur, out_id);
    }
    rc = p3_bam_get_u16(&pc->cur, &short_id);
    if (rc != P3_OK) {
        return rc;
    }
    *out_id = (p3_u32)short_id;
    if (*out_id == 0xffffUL) {
        *io_long_pta_id = 1;
    }
    return P3_OK;
}

static int p3_special_parse_pta_int(p3_bam_payload_cursor *pc, p3_bam_geom_primitive *prim) {
    int rc;
    int long_pta_id;
    p3_u32 pta_id;
    p3_u32 count32;
    p3_u32 i;
    if (pc == 0 || prim == 0) {
        return P3_ERR_INVALID;
    }
    prim->ends_pta_id = 0UL;
    prim->end_count = 0U;
    prim->total_end_count = 0U;
    prim->ends_available = 0U;
    if (p3_bam_cursor_remaining(&pc->cur) == 0UL) {
        return P3_OK;
    }
    long_pta_id = 0;
    rc = p3_special_get_pta_id(pc, &long_pta_id, &pta_id);
    if (rc != P3_OK) {
        return rc;
    }
    prim->ends_pta_id = pta_id;

    if (p3_bam_cursor_remaining(&pc->cur) == 0UL) {
        return P3_OK;
    }
    if (p3_bam_cursor_remaining(&pc->cur) < 4UL) {
        return P3_ERR_TRUNCATED;
    }
    rc = p3_bam_get_u32(&pc->cur, &count32);
    if (rc != P3_OK) {
        return rc;
    }
    prim->ends_available = 1U;
    prim->total_end_count = (count32 > 0xffffUL) ? 0xffffU : (p3_u16)count32;
    for (i = 0UL; i < count32; ++i) {
        p3_u32 raw_end;
        rc = p3_bam_get_u32(&pc->cur, &raw_end);
        if (rc != P3_OK) {
            return rc;
        }
        if (prim->end_count < P3_BAM_SPECIAL_MAX_PRIMITIVE_ENDS) {
            prim->ends[prim->end_count] = (p3_s32)raw_end;
            prim->end_count = (p3_u16)(prim->end_count + 1U);
        }
    }
    return P3_OK;
}

static int p3_special_handle_cache_find(const p3_bam_handle_cache *cache, p3_u16 id, char *dst, p3_u16 dst_cap) {
    p3_u8 i;
    for (i = 0U; i < cache->count; ++i) {
        if (cache->items[i].id == id) {
            p3_special_copy_text(dst, dst_cap, cache->items[i].name);
            return P3_OK;
        }
    }
    return P3_ERR_RANGE;
}

static int p3_special_handle_cache_add(p3_bam_handle_cache *cache, p3_u16 id, const char *name) {
    if (cache->count >= P3_BAM_LOCAL_HANDLE_CAP) {
        return P3_ERR_CAPACITY;
    }
    cache->items[cache->count].id = id;
    p3_special_copy_text(cache->items[cache->count].name, (p3_u16)sizeof(cache->items[cache->count].name), name);
    cache->count = (p3_u8)(cache->count + 1U);
    return P3_OK;
}

static int p3_special_get_handle_name(const p3_bam_archive *archive, p3_bam_payload_cursor *pc, p3_bam_handle_cache *cache, p3_u16 *out_id, char *dst, p3_u16 dst_cap) {
    p3_u16 id;
    int rc;
    p3_u8 parent_count;
    char name_buf[P3_BAM_SPECIAL_TEXT_CAP];
    p3_u16 i;

    rc = p3_bam_get_u16(&pc->cur, &id);
    if (rc != P3_OK) {
        return rc;
    }
    *out_id = id;
    if (id == 0U) {
        if (dst != 0 && dst_cap > 0U) {
            dst[0] = 0;
        }
        return P3_OK;
    }
    if (p3_special_find_type(archive, id, dst, dst_cap) == P3_OK) {
        return P3_OK;
    }
    if (p3_special_handle_cache_find(cache, id, dst, dst_cap) == P3_OK) {
        return P3_OK;
    }

    rc = p3_bam_get_string(&pc->cur, name_buf, (p3_u16)sizeof(name_buf), (p3_u16 *)0);
    if (rc != P3_OK) {
        return rc;
    }
    rc = p3_bam_get_u8(&pc->cur, &parent_count);
    if (rc != P3_OK) {
        return rc;
    }
    for (i = 0U; i < (p3_u16)parent_count; ++i) {
        p3_u16 dummy_id;
        char dummy_name[P3_BAM_SPECIAL_TEXT_CAP];
        rc = p3_special_get_handle_name(archive, pc, cache, &dummy_id, dummy_name, (p3_u16)sizeof(dummy_name));
        if (rc != P3_OK) {
            return rc;
        }
    }
    rc = p3_special_handle_cache_add(cache, id, name_buf);
    if (rc != P3_OK) {
        return rc;
    }
    p3_special_copy_text(dst, dst_cap, name_buf);
    return P3_OK;
}

static int p3_special_parse_inline_column(const p3_bam_archive *archive, p3_bam_payload_cursor *pc, p3_bam_geom_vertex_column *out_col) {
    int rc;
    p3_mem_set(out_col, 0, (p3_u32)sizeof(*out_col));
    rc = p3_special_get_pointer(pc, &out_col->name_object_id);
    if (rc != P3_OK) {
        return rc;
    }
    if (out_col->name_object_id != 0UL) {
        p3_bam_resolve_internal_name(archive, out_col->name_object_id, out_col->name, (p3_u16)sizeof(out_col->name));
    }
    rc = p3_bam_get_u8(&pc->cur, &out_col->num_components);
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &out_col->numeric_type);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &out_col->contents);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u16(&pc->cur, &out_col->start);
    }
    if (rc == P3_OK && archive->minor >= 29U) {
        rc = p3_bam_get_u8(&pc->cur, &out_col->column_alignment);
    }
    return rc;
}

static int p3_special_parse_panda_node_body(const p3_bam_archive *archive, p3_bam_payload_cursor *pc, p3_bam_panda_node *node) {
    p3_u32 tag_total;
    p3_u16 parent_total;
    p3_u16 child_total;
    p3_u16 stashed_total;
    p3_u16 i;
    int rc;
    char key_buf[P3_BAM_SPECIAL_TEXT_CAP];
    char value_buf[P3_BAM_SPECIAL_VALUE_CAP];

    (void)archive;
    p3_mem_set(node, 0, (p3_u32)sizeof(*node));
    rc = p3_bam_get_string(&pc->cur, node->name, (p3_u16)sizeof(node->name), (p3_u16 *)0);
    if (rc != P3_OK) {
        return rc;
    }
    rc = p3_special_get_pointer(pc, &node->state_object_id);
    if (rc == P3_OK) {
        rc = p3_special_get_pointer(pc, &node->transform_object_id);
    }
    if (rc == P3_OK) {
        rc = p3_special_get_pointer(pc, &node->effects_object_id);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u32(&pc->cur, &node->draw_control_mask);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u32(&pc->cur, &node->draw_show_mask);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u32(&pc->cur, &node->into_collide_mask);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &node->bounds_type);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u32(&pc->cur, &tag_total);
    }
    if (rc != P3_OK) {
        return rc;
    }
    node->total_tag_count = (tag_total > 0xffffUL) ? 0xffffU : (p3_u16)tag_total;
    for (i = 0U; i < node->total_tag_count; ++i) {
        rc = p3_bam_get_string(&pc->cur, key_buf, (p3_u16)sizeof(key_buf), (p3_u16 *)0);
        if (rc == P3_OK) {
            rc = p3_bam_get_string(&pc->cur, value_buf, (p3_u16)sizeof(value_buf), (p3_u16 *)0);
        }
        if (rc != P3_OK) {
            return rc;
        }
        if (node->tag_count < P3_BAM_SPECIAL_MAX_TAGS) {
            p3_special_copy_text(node->tags[node->tag_count].key, (p3_u16)sizeof(node->tags[node->tag_count].key), key_buf);
            p3_special_copy_text(node->tags[node->tag_count].value, (p3_u16)sizeof(node->tags[node->tag_count].value), value_buf);
            node->tag_count = (p3_u16)(node->tag_count + 1U);
        }
    }

    rc = p3_bam_get_u16(&pc->cur, &parent_total);
    if (rc != P3_OK) {
        return rc;
    }
    node->total_parent_count = parent_total;
    for (i = 0U; i < parent_total; ++i) {
        p3_u32 parent_id;
        rc = p3_special_get_pointer(pc, &parent_id);
        if (rc != P3_OK) {
            return rc;
        }
        if (node->parent_count < P3_BAM_SPECIAL_MAX_CHILDREN) {
            node->parent_object_ids[node->parent_count] = parent_id;
            node->parent_count = (p3_u16)(node->parent_count + 1U);
        }
    }

    rc = p3_bam_get_u16(&pc->cur, &child_total);
    if (rc != P3_OK) {
        return rc;
    }
    node->total_child_count = child_total;
    for (i = 0U; i < child_total; ++i) {
        p3_u32 child_id;
        p3_s32 sort;
        rc = p3_special_get_pointer(pc, &child_id);
        if (rc == P3_OK) {
            rc = p3_special_get_s32(pc, &sort);
        }
        if (rc != P3_OK) {
            return rc;
        }
        if (node->child_count < P3_BAM_SPECIAL_MAX_CHILDREN) {
            node->children[node->child_count].object_id = child_id;
            node->children[node->child_count].sort = sort;
            node->child_count = (p3_u16)(node->child_count + 1U);
        }
    }

    rc = p3_bam_get_u16(&pc->cur, &stashed_total);
    if (rc != P3_OK) {
        return rc;
    }
    node->total_stashed_count = stashed_total;
    for (i = 0U; i < stashed_total; ++i) {
        p3_u32 child_id2;
        p3_s32 sort2;
        rc = p3_special_get_pointer(pc, &child_id2);
        if (rc == P3_OK) {
            rc = p3_special_get_s32(pc, &sort2);
        }
        if (rc != P3_OK) {
            return rc;
        }
        if (node->stashed_count < P3_BAM_SPECIAL_MAX_CHILDREN) {
            node->stashed[node->stashed_count].object_id = child_id2;
            node->stashed[node->stashed_count].sort = sort2;
            node->stashed_count = (p3_u16)(node->stashed_count + 1U);
        }
    }

    return P3_OK;
}

void p3_bam_object_init(p3_bam_object *obj) {
    if (obj != 0) {
        p3_mem_set(obj, 0, (p3_u32)sizeof(*obj));
    }
}

const char *p3_bam_class_name(p3_u8 class_kind) {
    switch (class_kind) {
    case P3_BAM_CLASS_INTERNAL_NAME:
        return "InternalName";
    case P3_BAM_CLASS_GEOM_VERTEX_COLUMN:
        return "GeomVertexColumn";
    case P3_BAM_CLASS_GEOM_VERTEX_ARRAY_FORMAT:
        return "GeomVertexArrayFormat";
    case P3_BAM_CLASS_GEOM_VERTEX_FORMAT:
        return "GeomVertexFormat";
    case P3_BAM_CLASS_GEOM_VERTEX_ARRAY_DATA:
        return "GeomVertexArrayData";
    case P3_BAM_CLASS_GEOM_VERTEX_DATA:
        return "GeomVertexData";
    case P3_BAM_CLASS_GEOM_PRIMITIVE:
        return "GeomPrimitive";
    case P3_BAM_CLASS_GEOM:
        return "Geom";
    case P3_BAM_CLASS_PANDA_NODE:
        return "PandaNode";
    case P3_BAM_CLASS_MODEL_NODE:
        return "ModelNode";
    case P3_BAM_CLASS_MODEL_ROOT:
        return "ModelRoot";
    case P3_BAM_CLASS_GEOM_NODE:
        return "GeomNode";
    default:
        return "Unknown";
    }
}

int p3_bam_find_record_by_object_id(const p3_bam_archive *archive, p3_u32 object_id) {
    p3_u16 i;
    if (archive == 0) {
        return -1;
    }
    for (i = 0U; i < archive->record_count; ++i) {
        if ((archive->records[i].kind == P3_BAM_BOC_PUSH || archive->records[i].kind == P3_BAM_BOC_ADJUNCT) && archive->records[i].object_id == object_id) {
            return (int)i;
        }
    }
    return -1;
}

int p3_bam_resolve_internal_name(const p3_bam_archive *archive, p3_u32 object_id, char *dst, p3_u16 dst_cap) {
    int rec_index;
    p3_bam_object obj;
    int rc;
    if (dst != 0 && dst_cap > 0U) {
        dst[0] = 0;
    }
    rec_index = p3_bam_find_record_by_object_id(archive, object_id);
    if (rec_index < 0) {
        return P3_ERR_RANGE;
    }
    rc = p3_bam_parse_object(archive, (p3_u16)rec_index, &obj);
    if (rc != P3_OK) {
        return rc;
    }
    if (obj.class_kind != P3_BAM_CLASS_INTERNAL_NAME) {
        return P3_ERR_RANGE;
    }
    p3_special_copy_text(dst, dst_cap, obj.u.internal_name);
    return P3_OK;
}

int p3_bam_parse_object(const p3_bam_archive *archive, p3_u16 record_index, p3_bam_object *out_obj) {
    const p3_bam_record *rec;
    p3_bam_payload_cursor pc;
    int rc;
    p3_u16 i;
    p3_bam_handle_cache handle_cache;

    if (archive == 0 || out_obj == 0) {
        return P3_ERR_INVALID;
    }
    if (record_index >= archive->record_count) {
        return P3_ERR_RANGE;
    }

    rec = &archive->records[record_index];
    if (rec->kind != P3_BAM_BOC_PUSH && rec->kind != P3_BAM_BOC_ADJUNCT) {
        return P3_ERR_INVALID;
    }

    p3_bam_object_init(out_obj);
    out_obj->record_kind = rec->kind;
    out_obj->record_index = record_index;
    out_obj->type_id = rec->type_id;
    out_obj->object_id = rec->object_id;
    p3_special_find_type(archive, rec->type_id, out_obj->type_name, (p3_u16)sizeof(out_obj->type_name));

    p3_special_payload_cursor_init(&pc, rec, archive);
    p3_mem_set(&handle_cache, 0, (p3_u32)sizeof(handle_cache));

    if (strcmp(out_obj->type_name, "InternalName") == 0) {
        out_obj->class_kind = P3_BAM_CLASS_INTERNAL_NAME;
        return p3_bam_get_string(&pc.cur, out_obj->u.internal_name, (p3_u16)sizeof(out_obj->u.internal_name), (p3_u16 *)0);
    }

    if (strcmp(out_obj->type_name, "GeomVertexColumn") == 0) {
        out_obj->class_kind = P3_BAM_CLASS_GEOM_VERTEX_COLUMN;
        return p3_special_parse_inline_column(archive, &pc, &out_obj->u.geom_vertex_column);
    }

    if (strcmp(out_obj->type_name, "GeomVertexArrayFormat") == 0) {
        p3_bam_geom_vertex_array_format *fmt;
        p3_u16 count;
        out_obj->class_kind = P3_BAM_CLASS_GEOM_VERTEX_ARRAY_FORMAT;
        fmt = &out_obj->u.geom_vertex_array_format;
        rc = p3_bam_get_u16(&pc.cur, &fmt->stride);
        if (rc == P3_OK) {
            rc = p3_bam_get_u16(&pc.cur, &fmt->total_bytes);
        }
        if (rc == P3_OK) {
            rc = p3_bam_get_u8(&pc.cur, &fmt->pad_to);
        }
        if (rc == P3_OK && archive->minor > 36U) {
            rc = p3_bam_get_u16(&pc.cur, &fmt->divisor);
        }
        if (rc == P3_OK) {
            rc = p3_bam_get_u16(&pc.cur, &count);
        }
        if (rc != P3_OK) {
            return rc;
        }
        fmt->total_column_count = count;
        for (i = 0U; i < count; ++i) {
            p3_bam_geom_vertex_column col;
            rc = p3_special_parse_inline_column(archive, &pc, &col);
            if (rc != P3_OK) {
                return rc;
            }
            if (fmt->column_count < P3_BAM_SPECIAL_MAX_COLUMNS) {
                fmt->columns[fmt->column_count] = col;
                fmt->column_count = (p3_u16)(fmt->column_count + 1U);
            }
        }
        return P3_OK;
    }

    if (strcmp(out_obj->type_name, "GeomVertexFormat") == 0) {
        p3_bam_geom_vertex_format *fmt2;
        p3_u16 count2;
        out_obj->class_kind = P3_BAM_CLASS_GEOM_VERTEX_FORMAT;
        fmt2 = &out_obj->u.geom_vertex_format;
        rc = p3_bam_get_u8(&pc.cur, &fmt2->animation_type);
        if (rc == P3_OK) {
            rc = p3_bam_get_u16(&pc.cur, &fmt2->num_transforms);
        }
        if (rc == P3_OK) {
            rc = p3_bam_get_u8(&pc.cur, &fmt2->indexed_transforms);
        }
        if (rc == P3_OK) {
            rc = p3_bam_get_u16(&pc.cur, &count2);
        }
        if (rc != P3_OK) {
            return rc;
        }
        fmt2->total_array_count = count2;
        for (i = 0U; i < count2; ++i) {
            p3_u32 ref;
            rc = p3_special_get_pointer(&pc, &ref);
            if (rc != P3_OK) {
                return rc;
            }
            if (fmt2->array_count < P3_BAM_SPECIAL_MAX_ARRAYS) {
                fmt2->array_format_object_ids[fmt2->array_count] = ref;
                fmt2->array_count = (p3_u16)(fmt2->array_count + 1U);
            }
        }
        return P3_OK;
    }

    if (strcmp(out_obj->type_name, "GeomVertexArrayData") == 0) {
        p3_bam_geom_vertex_array_data *arr;
        p3_u32 remain;
        out_obj->class_kind = P3_BAM_CLASS_GEOM_VERTEX_ARRAY_DATA;
        arr = &out_obj->u.geom_vertex_array_data;
        rc = p3_special_get_pointer(&pc, &arr->array_format_object_id);
        if (rc == P3_OK) {
            rc = p3_bam_get_u8(&pc.cur, &arr->usage_hint);
        }
        if (rc == P3_OK) {
            rc = p3_bam_get_u32(&pc.cur, &arr->data_size);
        }
        if (rc != P3_OK) {
            return rc;
        }
        remain = p3_bam_cursor_remaining(&pc.cur);
        if (arr->data_size > remain) {
            return P3_ERR_TRUNCATED;
        }
        arr->preview_size = (arr->data_size > P3_BAM_SPECIAL_DATA_PREVIEW) ? P3_BAM_SPECIAL_DATA_PREVIEW : (p3_u8)arr->data_size;
        if (arr->preview_size > 0U) {
            p3_mem_copy(arr->preview, pc.cur.data + pc.cur.pos, arr->preview_size);
        }
        rc = p3_bam_skip_bytes(&pc.cur, arr->data_size);
        return rc;
    }

    if (strcmp(out_obj->type_name, "GeomVertexData") == 0) {
        p3_bam_geom_vertex_data *vdata;
        p3_u16 count3;
        out_obj->class_kind = P3_BAM_CLASS_GEOM_VERTEX_DATA;
        vdata = &out_obj->u.geom_vertex_data;
        rc = p3_bam_get_string(&pc.cur, vdata->name, (p3_u16)sizeof(vdata->name), (p3_u16 *)0);
        if (rc == P3_OK) {
            rc = p3_special_get_pointer(&pc, &vdata->format_object_id);
        }
        if (rc == P3_OK) {
            rc = p3_bam_get_u8(&pc.cur, &vdata->usage_hint);
        }
        if (rc == P3_OK) {
            rc = p3_bam_get_u16(&pc.cur, &count3);
        }
        if (rc != P3_OK) {
            return rc;
        }
        vdata->total_array_count = count3;
        for (i = 0U; i < count3; ++i) {
            p3_u32 ref2;
            rc = p3_special_get_pointer(&pc, &ref2);
            if (rc != P3_OK) {
                return rc;
            }
            if (vdata->array_count < P3_BAM_SPECIAL_MAX_ARRAYS) {
                vdata->array_object_ids[vdata->array_count] = ref2;
                vdata->array_count = (p3_u16)(vdata->array_count + 1U);
            }
        }
        rc = p3_special_get_pointer(&pc, &vdata->transform_table_object_id);
        if (rc == P3_OK) {
            rc = p3_special_get_pointer(&pc, &vdata->transform_blend_table_object_id);
        }
        if (rc == P3_OK) {
            rc = p3_special_get_pointer(&pc, &vdata->slider_table_object_id);
        }
        return rc;
    }

    if (p3_special_is_geom_primitive_name(out_obj->type_name)) {
        p3_bam_geom_primitive *prim;
        out_obj->class_kind = P3_BAM_CLASS_GEOM_PRIMITIVE;
        prim = &out_obj->u.geom_primitive;
        p3_special_copy_text(prim->primitive_name, (p3_u16)sizeof(prim->primitive_name), out_obj->type_name);
        rc = p3_bam_get_u8(&pc.cur, &prim->shade_model);
        if (rc == P3_OK) {
            rc = p3_special_get_s32(&pc, &prim->first_vertex);
        }
        if (rc == P3_OK) {
            rc = p3_special_get_s32(&pc, &prim->num_vertices);
        }
        if (rc == P3_OK) {
            rc = p3_bam_get_u8(&pc.cur, &prim->index_type);
        }
        if (rc == P3_OK) {
            rc = p3_bam_get_u8(&pc.cur, &prim->usage_hint);
        }
        if (rc == P3_OK) {
            rc = p3_special_get_pointer(&pc, &prim->vertices_object_id);
        }
        if (rc == P3_OK) {
            rc = p3_special_parse_pta_int(&pc, prim);
        }
        if (rc == P3_OK && strcmp(out_obj->type_name, "GeomPatches") == 0) {
            rc = p3_bam_get_u16(&pc.cur, &prim->patch_vertices_per_patch);
        }
        if (rc != P3_OK) {
            return rc;
        }
        prim->trailing_bytes = p3_bam_cursor_remaining(&pc.cur);
        pc.cur.pos = pc.cur.size;
        return P3_OK;
    }

    if (strcmp(out_obj->type_name, "Geom") == 0) {
        p3_bam_geom *geom;
        p3_u16 count4;
        out_obj->class_kind = P3_BAM_CLASS_GEOM;
        geom = &out_obj->u.geom;
        rc = p3_special_get_pointer(&pc, &geom->vertex_data_object_id);
        if (rc == P3_OK) {
            rc = p3_bam_get_u16(&pc.cur, &count4);
        }
        if (rc != P3_OK) {
            return rc;
        }
        geom->total_primitive_count = count4;
        for (i = 0U; i < count4; ++i) {
            p3_u32 ref3;
            rc = p3_special_get_pointer(&pc, &ref3);
            if (rc != P3_OK) {
                return rc;
            }
            if (geom->primitive_count < P3_BAM_SPECIAL_MAX_PRIMITIVES) {
                geom->primitive_object_ids[geom->primitive_count] = ref3;
                geom->primitive_count = (p3_u16)(geom->primitive_count + 1U);
            }
        }
        rc = p3_bam_get_u8(&pc.cur, &geom->primitive_type);
        if (rc == P3_OK) {
            rc = p3_bam_get_u8(&pc.cur, &geom->shade_model);
        }
        if (rc == P3_OK) {
            rc = p3_bam_get_u16(&pc.cur, &geom->geom_rendering);
        }
        if (rc == P3_OK && archive->minor >= 19U) {
            rc = p3_bam_get_u8(&pc.cur, &geom->bounds_type);
        }
        return rc;
    }

    if (strcmp(out_obj->type_name, "PandaNode") == 0) {
        out_obj->class_kind = P3_BAM_CLASS_PANDA_NODE;
        return p3_special_parse_panda_node_body(archive, &pc, &out_obj->u.panda_node);
    }

    if (strcmp(out_obj->type_name, "ModelNode") == 0) {
        out_obj->class_kind = P3_BAM_CLASS_MODEL_NODE;
        rc = p3_special_parse_panda_node_body(archive, &pc, &out_obj->u.model_node.base);
        if (rc == P3_OK) {
            rc = p3_bam_get_u8(&pc.cur, &out_obj->u.model_node.preserve_transform);
        }
        if (rc == P3_OK) {
            rc = p3_bam_get_u16(&pc.cur, &out_obj->u.model_node.preserve_attributes);
        }
        return rc;
    }

    if (strcmp(out_obj->type_name, "ModelRoot") == 0) {
        out_obj->class_kind = P3_BAM_CLASS_MODEL_ROOT;
        rc = p3_special_parse_panda_node_body(archive, &pc, &out_obj->u.model_root.base.base);
        if (rc == P3_OK) {
            rc = p3_bam_get_u8(&pc.cur, &out_obj->u.model_root.base.preserve_transform);
        }
        if (rc == P3_OK) {
            rc = p3_bam_get_u16(&pc.cur, &out_obj->u.model_root.base.preserve_attributes);
        }
        if (rc == P3_OK && archive->minor >= 46U) {
            rc = p3_special_get_handle_name(archive, &pc, &handle_cache,
                &out_obj->u.model_root.loader_type_id,
                out_obj->u.model_root.loader_type_name,
                (p3_u16)sizeof(out_obj->u.model_root.loader_type_name));
        }
        return rc;
    }

    if (strcmp(out_obj->type_name, "GeomNode") == 0) {
        p3_bam_geom_node *gnode;
        p3_u16 count5;
        out_obj->class_kind = P3_BAM_CLASS_GEOM_NODE;
        gnode = &out_obj->u.geom_node;
        rc = p3_special_parse_panda_node_body(archive, &pc, &gnode->base);
        if (rc == P3_OK) {
            rc = p3_bam_get_u16(&pc.cur, &count5);
        }
        if (rc != P3_OK) {
            return rc;
        }
        gnode->total_geom_count = count5;
        for (i = 0U; i < count5; ++i) {
            p3_u32 geom_ref;
            p3_u32 state_ref;
            rc = p3_special_get_pointer(&pc, &geom_ref);
            if (rc == P3_OK) {
                rc = p3_special_get_pointer(&pc, &state_ref);
            }
            if (rc != P3_OK) {
                return rc;
            }
            if (gnode->geom_count < P3_BAM_SPECIAL_MAX_GEOMS) {
                gnode->geoms[gnode->geom_count].geom_object_id = geom_ref;
                gnode->geoms[gnode->geom_count].state_object_id = state_ref;
                gnode->geom_count = (p3_u16)(gnode->geom_count + 1U);
            }
        }
        return P3_OK;
    }

    out_obj->class_kind = P3_BAM_CLASS_UNKNOWN;
    return P3_OK;
}

int p3_bam_build_internal_name_payload(const char *name, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    p3_u16 len;
    if (dst == 0 || out_len == 0 || name == 0) {
        return P3_ERR_INVALID;
    }
    len = (p3_u16)strlen(name);
    p3_bam_builder_init(&b, dst, cap);
    if (p3_bam_put_string(&b, name, len) != P3_OK) {
        return P3_ERR_CAPACITY;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_geom_vertex_column_payload(const p3_bam_geom_vertex_column *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int long_ids;
    int rc;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    p3_bam_builder_init(&b, dst, cap);
    long_ids = 0;
    rc = p3_special_put_pointer(&b, &long_ids, src->name_object_id);
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->num_components);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->numeric_type);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->contents);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->start);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->column_alignment);
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_geom_vertex_array_format_payload(const p3_bam_geom_vertex_array_format *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor) {
    p3_bam_builder b;
    p3_u16 i;
    int rc;
    int long_ids;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->column_count > P3_BAM_SPECIAL_MAX_COLUMNS) {
        return P3_ERR_RANGE;
    }
    p3_bam_builder_init(&b, dst, cap);
    long_ids = 0;
    rc = p3_bam_put_u16(&b, src->stride);
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->total_bytes);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->pad_to);
    }
    if (rc == P3_OK && file_minor > 36U) {
        rc = p3_bam_put_u16(&b, src->divisor);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->column_count);
    }
    for (i = 0U; rc == P3_OK && i < src->column_count; ++i) {
        rc = p3_special_put_pointer(&b, &long_ids, src->columns[i].name_object_id);
        if (rc == P3_OK) {
            rc = p3_bam_put_u8(&b, src->columns[i].num_components);
        }
        if (rc == P3_OK) {
            rc = p3_bam_put_u8(&b, src->columns[i].numeric_type);
        }
        if (rc == P3_OK) {
            rc = p3_bam_put_u8(&b, src->columns[i].contents);
        }
        if (rc == P3_OK) {
            rc = p3_bam_put_u16(&b, src->columns[i].start);
        }
        if (rc == P3_OK && file_minor >= 29U) {
            rc = p3_bam_put_u8(&b, src->columns[i].column_alignment);
        }
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_geom_vertex_format_payload(const p3_bam_geom_vertex_format *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    p3_u16 i;
    int rc;
    int long_ids;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->array_count > P3_BAM_SPECIAL_MAX_ARRAYS) {
        return P3_ERR_RANGE;
    }
    p3_bam_builder_init(&b, dst, cap);
    long_ids = 0;
    rc = p3_bam_put_u8(&b, src->animation_type);
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->num_transforms);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->indexed_transforms);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->array_count);
    }
    for (i = 0U; rc == P3_OK && i < src->array_count; ++i) {
        rc = p3_special_put_pointer(&b, &long_ids, src->array_format_object_ids[i]);
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_geom_vertex_array_data_payload(const p3_bam_geom_vertex_array_data *src, const p3_u8 *data_bytes, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->data_size > 0UL && data_bytes == 0) {
        return P3_ERR_INVALID;
    }
    p3_bam_builder_init(&b, dst, cap);
    long_ids = 0;
    rc = p3_special_put_pointer(&b, &long_ids, src->array_format_object_id);
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->usage_hint);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u32(&b, src->data_size);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_bytes(&b, data_bytes, src->data_size);
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_geom_vertex_data_payload(const p3_bam_geom_vertex_data *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    p3_u16 i;
    p3_u16 name_len;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->array_count > P3_BAM_SPECIAL_MAX_ARRAYS) {
        return P3_ERR_RANGE;
    }
    name_len = (p3_u16)strlen(src->name);
    p3_bam_builder_init(&b, dst, cap);
    long_ids = 0;
    rc = p3_bam_put_string(&b, src->name, name_len);
    if (rc == P3_OK) {
        rc = p3_special_put_pointer(&b, &long_ids, src->format_object_id);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->usage_hint);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->array_count);
    }
    for (i = 0U; rc == P3_OK && i < src->array_count; ++i) {
        rc = p3_special_put_pointer(&b, &long_ids, src->array_object_ids[i]);
    }
    if (rc == P3_OK) {
        rc = p3_special_put_pointer(&b, &long_ids, src->transform_table_object_id);
    }
    if (rc == P3_OK) {
        rc = p3_special_put_pointer(&b, &long_ids, src->transform_blend_table_object_id);
    }
    if (rc == P3_OK) {
        rc = p3_special_put_pointer(&b, &long_ids, src->slider_table_object_id);
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_geom_primitive_payload(const p3_bam_geom_primitive *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    p3_bam_builder_init(&b, dst, cap);
    long_ids = 0;
    rc = p3_bam_put_u8(&b, src->shade_model);
    if (rc == P3_OK) {
        rc = p3_special_put_s32(&b, src->first_vertex);
    }
    if (rc == P3_OK) {
        rc = p3_special_put_s32(&b, src->num_vertices);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->index_type);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->usage_hint);
    }
    if (src->end_count > P3_BAM_SPECIAL_MAX_PRIMITIVE_ENDS) {
        return P3_ERR_RANGE;
    }
    if (rc == P3_OK) {
        rc = p3_special_put_pointer(&b, &long_ids, src->vertices_object_id);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, 0U);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u32(&b, (p3_u32)src->end_count);
    }
    if (rc == P3_OK) {
        p3_u16 i;
        for (i = 0U; rc == P3_OK && i < src->end_count; ++i) {
            rc = p3_special_put_s32(&b, src->ends[i]);
        }
    }
    if (rc == P3_OK && strcmp(src->primitive_name, "GeomPatches") == 0) {
        rc = p3_bam_put_u16(&b, src->patch_vertices_per_patch);
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_geom_payload(const p3_bam_geom *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    p3_u16 i;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->primitive_count > P3_BAM_SPECIAL_MAX_PRIMITIVES) {
        return P3_ERR_RANGE;
    }
    p3_bam_builder_init(&b, dst, cap);
    long_ids = 0;
    rc = p3_special_put_pointer(&b, &long_ids, src->vertex_data_object_id);
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->primitive_count);
    }
    for (i = 0U; rc == P3_OK && i < src->primitive_count; ++i) {
        rc = p3_special_put_pointer(&b, &long_ids, src->primitive_object_ids[i]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->primitive_type);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->shade_model);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->geom_rendering);
    }
    if (rc == P3_OK && file_minor >= 19U) {
        rc = p3_bam_put_u8(&b, src->bounds_type);
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_panda_node_payload(const p3_bam_panda_node *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    p3_u16 i;
    p3_u16 name_len;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->tag_count > P3_BAM_SPECIAL_MAX_TAGS || src->parent_count > P3_BAM_SPECIAL_MAX_CHILDREN || src->child_count > P3_BAM_SPECIAL_MAX_CHILDREN || src->stashed_count > P3_BAM_SPECIAL_MAX_CHILDREN) {
        return P3_ERR_RANGE;
    }
    name_len = (p3_u16)strlen(src->name);
    p3_bam_builder_init(&b, dst, cap);
    long_ids = 0;
    rc = p3_bam_put_string(&b, src->name, name_len);
    if (rc == P3_OK) {
        rc = p3_special_put_pointer(&b, &long_ids, src->state_object_id);
    }
    if (rc == P3_OK) {
        rc = p3_special_put_pointer(&b, &long_ids, src->transform_object_id);
    }
    if (rc == P3_OK) {
        rc = p3_special_put_pointer(&b, &long_ids, src->effects_object_id);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u32(&b, src->draw_control_mask);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u32(&b, src->draw_show_mask);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u32(&b, src->into_collide_mask);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->bounds_type);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u32(&b, src->tag_count);
    }
    for (i = 0U; rc == P3_OK && i < src->tag_count; ++i) {
        rc = p3_bam_put_string(&b, src->tags[i].key, (p3_u16)strlen(src->tags[i].key));
        if (rc == P3_OK) {
            rc = p3_bam_put_string(&b, src->tags[i].value, (p3_u16)strlen(src->tags[i].value));
        }
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->parent_count);
    }
    for (i = 0U; rc == P3_OK && i < src->parent_count; ++i) {
        rc = p3_special_put_pointer(&b, &long_ids, src->parent_object_ids[i]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->child_count);
    }
    for (i = 0U; rc == P3_OK && i < src->child_count; ++i) {
        rc = p3_special_put_pointer(&b, &long_ids, src->children[i].object_id);
        if (rc == P3_OK) {
            rc = p3_special_put_s32(&b, src->children[i].sort);
        }
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->stashed_count);
    }
    for (i = 0U; rc == P3_OK && i < src->stashed_count; ++i) {
        rc = p3_special_put_pointer(&b, &long_ids, src->stashed[i].object_id);
        if (rc == P3_OK) {
            rc = p3_special_put_s32(&b, src->stashed[i].sort);
        }
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_model_node_payload(const p3_bam_model_node *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_u8 tmp[P3_TEXT_IO_CAP];
    p3_u32 base_len;
    p3_bam_builder b;
    int rc;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    rc = p3_bam_build_panda_node_payload(&src->base, tmp, P3_TEXT_IO_CAP, &base_len);
    if (rc != P3_OK) {
        return rc;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_bytes(&b, tmp, base_len);
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->preserve_transform);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->preserve_attributes);
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_model_root_payload(const p3_bam_model_root *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor) {
    p3_u8 tmp[P3_TEXT_IO_CAP];
    p3_u32 base_len;
    p3_bam_builder b;
    int rc;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    rc = p3_bam_build_model_node_payload(&src->base, tmp, P3_TEXT_IO_CAP, &base_len);
    if (rc != P3_OK) {
        return rc;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_bytes(&b, tmp, base_len);
    if (rc == P3_OK && file_minor >= 46U) {
        rc = p3_bam_put_u16(&b, src->loader_type_id);
        if (rc == P3_OK && src->loader_type_id != 0U && src->loader_type_name[0] != 0) {
            rc = p3_bam_put_string(&b, src->loader_type_name, (p3_u16)strlen(src->loader_type_name));
            if (rc == P3_OK) {
                rc = p3_bam_put_u8(&b, 0U);
            }
        }
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_geom_node_payload(const p3_bam_geom_node *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_u8 tmp[P3_TEXT_IO_CAP];
    p3_u32 base_len;
    p3_bam_builder b;
    int rc;
    int long_ids;
    p3_u16 i;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->geom_count > P3_BAM_SPECIAL_MAX_GEOMS) {
        return P3_ERR_RANGE;
    }
    rc = p3_bam_build_panda_node_payload(&src->base, tmp, P3_TEXT_IO_CAP, &base_len);
    if (rc != P3_OK) {
        return rc;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_bytes(&b, tmp, base_len);
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->geom_count);
    }
    long_ids = 0;
    for (i = 0U; rc == P3_OK && i < src->geom_count; ++i) {
        rc = p3_special_put_pointer(&b, &long_ids, src->geoms[i].geom_object_id);
        if (rc == P3_OK) {
            rc = p3_special_put_pointer(&b, &long_ids, src->geoms[i].state_object_id);
        }
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}
