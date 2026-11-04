#ifndef P3_BAM_ARCHIVE_H
#define P3_BAM_ARCHIVE_H

#include "p3_config.h"
#include "p3_string_pool.h"
#include "p3_types.h"

#define P3_BAM_MAGIC_SIZE 6

enum {
    P3_BAM_BE_BIG = 0,
    P3_BAM_BE_LITTLE = 1
};

enum {
    P3_BAM_BOC_PUSH = 0,
    P3_BAM_BOC_POP = 1,
    P3_BAM_BOC_ADJUNCT = 2,
    P3_BAM_BOC_REMOVE = 3,
    P3_BAM_BOC_FILE_DATA = 4
};

typedef struct p3_bam_type {
    p3_u16 id;
    p3_u16 name_off;
    p3_u16 name_len;
    p3_u16 first_parent;
    p3_u8 parent_count;
} p3_bam_type;

typedef struct p3_bam_record {
    p3_u8 kind;
    p3_u16 type_id;
    p3_u32 object_id;
    p3_u32 payload_off;
    p3_u32 payload_len;
    p3_u32 data_off;
    p3_u32 data_len;
    p3_u16 first_remove;
    p3_u16 remove_count;
    p3_u8 payload_long_object_id;
} p3_bam_record;

typedef struct p3_bam_archive {
    p3_u16 major;
    p3_u16 minor;
    p3_u8 endian;
    p3_u8 stdfloat_double;

    p3_string_pool pool;

    p3_bam_type types[P3_BAM_MAX_TYPES];
    p3_u16 type_count;

    p3_u16 type_parents[P3_BAM_MAX_TYPE_PARENTS];
    p3_u16 type_parent_count;

    p3_bam_record records[P3_BAM_MAX_RECORDS];
    p3_u16 record_count;

    p3_u32 remove_ids[P3_BAM_MAX_REMOVE_IDS];
    p3_u16 remove_id_count;

    p3_u8 raw[P3_BAM_RAW_CAP];
    p3_u32 raw_len;
} p3_bam_archive;

void p3_bam_archive_init(p3_bam_archive *ar);
int p3_bam_find_type_index(const p3_bam_archive *ar, p3_u16 type_id);
const char *p3_bam_type_name_ptr(const p3_bam_archive *ar, p3_u16 type_id);
int p3_bam_type_name_copy(const p3_bam_archive *ar, p3_u16 type_id, char *dst, p3_u16 dst_cap);
int p3_bam_type_id_is_name(const p3_bam_archive *ar, p3_u16 type_id, const char *name);
int p3_bam_type_id_derives_from_name(const p3_bam_archive *ar, p3_u16 type_id, const char *base_name);
int p3_bam_record_is_type_name(const p3_bam_archive *ar, p3_u16 record_index, const char *name);
int p3_bam_add_type(p3_bam_archive *ar, p3_u16 type_id, const char *name, const p3_u16 *parents, p3_u8 parent_count);
int p3_bam_add_push(p3_bam_archive *ar, p3_u16 type_id, p3_u32 object_id, const p3_u8 *payload, p3_u32 payload_len);
int p3_bam_add_adjunct(p3_bam_archive *ar, p3_u16 type_id, p3_u32 object_id, const p3_u8 *payload, p3_u32 payload_len);
int p3_bam_add_reference(p3_bam_archive *ar, p3_u8 kind, p3_u32 object_id);
int p3_bam_add_pop(p3_bam_archive *ar);
int p3_bam_add_remove(p3_bam_archive *ar, const p3_u32 *object_ids, p3_u16 count);
int p3_bam_add_file_data(p3_bam_archive *ar, const p3_u8 *data, p3_u32 size);

#endif
