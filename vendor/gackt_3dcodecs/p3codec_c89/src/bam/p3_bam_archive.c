#include <string.h>
#include "p3_error.h"
#include "p3_bam_archive.h"

static int p3_bam_copy_raw(p3_bam_archive *ar, const p3_u8 *src, p3_u32 len, p3_u32 *out_off) {
    if (ar->raw_len + len > P3_BAM_RAW_CAP) {
        return P3_ERR_CAPACITY;
    }
    *out_off = ar->raw_len;
    if (src != 0 && len > 0UL) {
        memcpy(ar->raw + ar->raw_len, src, (size_t)len);
    }
    ar->raw_len += len;
    return P3_OK;
}

void p3_bam_archive_init(p3_bam_archive *ar) {
    memset(ar, 0, sizeof(*ar));
    p3_string_pool_init(&ar->pool);
    ar->major = 6;
    ar->minor = 46;
    ar->endian = P3_BAM_BE_LITTLE;
    ar->stdfloat_double = 0;
}

int p3_bam_find_type_index(const p3_bam_archive *ar, p3_u16 type_id) {
    p3_u16 i;
    for (i = 0; i < ar->type_count; ++i) {
        if (ar->types[i].id == type_id) {
            return (int)i;
        }
    }
    return -1;
}


const char *p3_bam_type_name_ptr(const p3_bam_archive *ar, p3_u16 type_id) {
    int idx;
    if (ar == 0) {
        return "";
    }
    idx = p3_bam_find_type_index(ar, type_id);
    if (idx < 0) {
        return "";
    }
    return p3_string_pool_ptr(&ar->pool, ar->types[idx].name_off);
}

int p3_bam_type_name_copy(const p3_bam_archive *ar, p3_u16 type_id, char *dst, p3_u16 dst_cap) {
    int idx;
    if (ar == 0 || dst == 0 || dst_cap == 0U) {
        return P3_ERR_INVALID;
    }
    dst[0] = 0;
    idx = p3_bam_find_type_index(ar, type_id);
    if (idx < 0) {
        return P3_ERR_RANGE;
    }
    return p3_string_pool_copy_out(&ar->pool, ar->types[idx].name_off, ar->types[idx].name_len, dst, dst_cap);
}

int p3_bam_type_id_is_name(const p3_bam_archive *ar, p3_u16 type_id, const char *name) {
    int idx;
    if (ar == 0 || name == 0) {
        return P3_FALSE;
    }
    idx = p3_bam_find_type_index(ar, type_id);
    if (idx < 0) {
        return P3_FALSE;
    }
    return p3_string_pool_equals_cstr(&ar->pool, ar->types[idx].name_off, ar->types[idx].name_len, name);
}

static int p3_bam_type_index_derives_from_name(const p3_bam_archive *ar, p3_u16 type_index, const char *base_name, p3_u16 depth) {
    const p3_bam_type *type;
    p3_u8 i;
    if (depth > P3_BAM_MAX_TYPES) {
        return P3_FALSE;
    }
    if (type_index >= ar->type_count) {
        return P3_FALSE;
    }
    type = &ar->types[type_index];
    if (p3_string_pool_equals_cstr(&ar->pool, type->name_off, type->name_len, base_name)) {
        return P3_TRUE;
    }
    for (i = 0U; i < type->parent_count; ++i) {
        int parent_index;
        parent_index = p3_bam_find_type_index(ar, ar->type_parents[type->first_parent + i]);
        if (parent_index >= 0 && p3_bam_type_index_derives_from_name(ar, (p3_u16)parent_index, base_name, (p3_u16)(depth + 1U))) {
            return P3_TRUE;
        }
    }
    return P3_FALSE;
}

int p3_bam_type_id_derives_from_name(const p3_bam_archive *ar, p3_u16 type_id, const char *base_name) {
    int idx;
    if (ar == 0 || base_name == 0) {
        return P3_FALSE;
    }
    idx = p3_bam_find_type_index(ar, type_id);
    if (idx < 0) {
        return P3_FALSE;
    }
    return p3_bam_type_index_derives_from_name(ar, (p3_u16)idx, base_name, 0U);
}

int p3_bam_record_is_type_name(const p3_bam_archive *ar, p3_u16 record_index, const char *name) {
    if (ar == 0 || record_index >= ar->record_count) {
        return P3_FALSE;
    }
    if (ar->records[record_index].kind != P3_BAM_BOC_PUSH && ar->records[record_index].kind != P3_BAM_BOC_ADJUNCT) {
        return P3_FALSE;
    }
    return p3_bam_type_id_derives_from_name(ar, ar->records[record_index].type_id, name);
}

int p3_bam_add_type(p3_bam_archive *ar, p3_u16 type_id, const char *name, const p3_u16 *parents, p3_u8 parent_count) {
    p3_u16 off;
    p3_u16 i;
    int rc;
    if (ar->type_count >= P3_BAM_MAX_TYPES) {
        return P3_ERR_CAPACITY;
    }
    if (p3_bam_find_type_index(ar, type_id) >= 0) {
        return P3_OK;
    }
    if ((p3_u32)ar->type_parent_count + (p3_u32)parent_count > (p3_u32)P3_BAM_MAX_TYPE_PARENTS) {
        return P3_ERR_CAPACITY;
    }
    rc = p3_string_pool_add_cstr(&ar->pool, name, &off);
    if (rc != P3_OK) {
        return rc;
    }
    ar->types[ar->type_count].id = type_id;
    ar->types[ar->type_count].name_off = off;
    ar->types[ar->type_count].name_len = (p3_u16)strlen(name);
    ar->types[ar->type_count].first_parent = ar->type_parent_count;
    ar->types[ar->type_count].parent_count = parent_count;
    for (i = 0; i < parent_count; ++i) {
        ar->type_parents[ar->type_parent_count++] = parents[i];
    }
    ar->type_count = (p3_u16)(ar->type_count + 1U);
    return P3_OK;
}

static int p3_bam_add_object_like(p3_bam_archive *ar, p3_u8 kind, p3_u16 type_id, p3_u32 object_id, const p3_u8 *payload, p3_u32 payload_len) {
    p3_bam_record *rec;
    int rc;
    if (ar->record_count >= P3_BAM_MAX_RECORDS) {
        return P3_ERR_CAPACITY;
    }
    rec = &ar->records[ar->record_count];
    memset(rec, 0, sizeof(*rec));
    rec->kind = kind;
    rec->type_id = type_id;
    rec->object_id = object_id;
    rc = p3_bam_copy_raw(ar, payload, payload_len, &rec->payload_off);
    if (rc != P3_OK) {
        return rc;
    }
    rec->payload_len = payload_len;
    ar->record_count = (p3_u16)(ar->record_count + 1U);
    return P3_OK;
}

int p3_bam_add_push(p3_bam_archive *ar, p3_u16 type_id, p3_u32 object_id, const p3_u8 *payload, p3_u32 payload_len) {
    return p3_bam_add_object_like(ar, P3_BAM_BOC_PUSH, type_id, object_id, payload, payload_len);
}

int p3_bam_add_adjunct(p3_bam_archive *ar, p3_u16 type_id, p3_u32 object_id, const p3_u8 *payload, p3_u32 payload_len) {
    return p3_bam_add_object_like(ar, P3_BAM_BOC_ADJUNCT, type_id, object_id, payload, payload_len);
}

int p3_bam_add_reference(p3_bam_archive *ar, p3_u8 kind, p3_u32 object_id) {
    return p3_bam_add_object_like(ar, kind, 0, object_id, 0, 0UL);
}

int p3_bam_add_pop(p3_bam_archive *ar) {
    if (ar->record_count >= P3_BAM_MAX_RECORDS) {
        return P3_ERR_CAPACITY;
    }
    memset(&ar->records[ar->record_count], 0, sizeof(ar->records[ar->record_count]));
    ar->records[ar->record_count].kind = P3_BAM_BOC_POP;
    ar->record_count = (p3_u16)(ar->record_count + 1U);
    return P3_OK;
}

int p3_bam_add_remove(p3_bam_archive *ar, const p3_u32 *object_ids, p3_u16 count) {
    p3_bam_record *rec;
    p3_u16 i;
    if (ar->record_count >= P3_BAM_MAX_RECORDS) {
        return P3_ERR_CAPACITY;
    }
    if ((p3_u32)ar->remove_id_count + (p3_u32)count > (p3_u32)P3_BAM_MAX_REMOVE_IDS) {
        return P3_ERR_CAPACITY;
    }
    rec = &ar->records[ar->record_count];
    memset(rec, 0, sizeof(*rec));
    rec->kind = P3_BAM_BOC_REMOVE;
    rec->first_remove = ar->remove_id_count;
    rec->remove_count = count;
    for (i = 0; i < count; ++i) {
        ar->remove_ids[ar->remove_id_count++] = object_ids[i];
    }
    ar->record_count = (p3_u16)(ar->record_count + 1U);
    return P3_OK;
}

int p3_bam_add_file_data(p3_bam_archive *ar, const p3_u8 *data, p3_u32 size) {
    p3_bam_record *rec;
    int rc;
    if (ar->record_count >= P3_BAM_MAX_RECORDS) {
        return P3_ERR_CAPACITY;
    }
    rec = &ar->records[ar->record_count];
    memset(rec, 0, sizeof(*rec));
    rec->kind = P3_BAM_BOC_FILE_DATA;
    rc = p3_bam_copy_raw(ar, data, size, &rec->data_off);
    if (rc != P3_OK) {
        return rc;
    }
    rec->data_len = size;
    ar->record_count = (p3_u16)(ar->record_count + 1U);
    return P3_OK;
}
