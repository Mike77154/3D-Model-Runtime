#include <stdio.h>
#include <string.h>
#include "p3_bam_archive.h"
#include "p3_bam_datagram.h"
#include "p3_bam_writer.h"
#include "p3_bytes.h"
#include "p3_error.h"

typedef struct p3_bam_write_state {
    p3_u16 written_type_ids[P3_BAM_MAX_TYPES];
    p3_u16 written_type_count;
    int long_object_id;
} p3_bam_write_state;

static void p3_bam_set_error(char *err, p3_u32 err_cap, const char *msg) {
    if (err != 0 && err_cap > 0UL) {
        sprintf(err, "%s", msg);
    }
}

static int p3_write_datagram(p3_u8 *dst, p3_u32 cap, p3_u32 *io_pos, const p3_u8 *data, p3_u32 len) {
    if (dst == 0 || io_pos == 0 || (data == 0 && len > 0UL)) {
        return P3_ERR_INVALID;
    }
    if (*io_pos > cap || 4UL > cap - *io_pos || len > cap - *io_pos - 4UL) {
        return P3_ERR_CAPACITY;
    }
    p3_write_u32le(dst + *io_pos, len);
    *io_pos += 4UL;
    if (len > 0UL) {
        memcpy(dst + *io_pos, data, (size_t)len);
        *io_pos += len;
    }
    return P3_OK;
}

static int p3_type_seen(const p3_bam_write_state *st, p3_u16 type_id) {
    p3_u16 i;
    for (i = 0; i < st->written_type_count; ++i) {
        if (st->written_type_ids[i] == type_id) {
            return P3_TRUE;
        }
    }
    return P3_FALSE;
}

static int p3_mark_type_seen(p3_bam_write_state *st, p3_u16 type_id) {
    if (st->written_type_count >= P3_BAM_MAX_TYPES) {
        return P3_ERR_CAPACITY;
    }
    st->written_type_ids[st->written_type_count++] = type_id;
    return P3_OK;
}

static const p3_bam_type *p3_find_type(const p3_bam_archive *archive, p3_u16 type_id) {
    int idx;
    idx = p3_bam_find_type_index(archive, type_id);
    if (idx < 0) {
        return 0;
    }
    return &archive->types[idx];
}

static int p3_write_handle(p3_bam_builder *b, p3_bam_write_state *st, const p3_bam_archive *archive, p3_u16 type_id) {
    const p3_bam_type *type;
    int rc;
    p3_u8 i;

    rc = p3_bam_put_u16(b, type_id);
    if (rc != P3_OK) {
        return rc;
    }
    if (type_id == 0U) {
        return P3_OK;
    }
    if (p3_type_seen(st, type_id)) {
        return P3_OK;
    }
    rc = p3_mark_type_seen(st, type_id);
    if (rc != P3_OK) {
        return rc;
    }

    type = p3_find_type(archive, type_id);
    if (type == 0) {
        return P3_ERR_RANGE;
    }

    rc = p3_bam_put_string(b, p3_string_pool_ptr(&archive->pool, type->name_off), type->name_len);
    if (rc != P3_OK) {
        return rc;
    }
    rc = p3_bam_put_u8(b, type->parent_count);
    if (rc != P3_OK) {
        return rc;
    }
    for (i = 0; i < type->parent_count; ++i) {
        rc = p3_write_handle(b, st, archive, archive->type_parents[type->first_parent + i]);
        if (rc != P3_OK) {
            return rc;
        }
    }
    return P3_OK;
}

static int p3_write_object_id(p3_bam_builder *b, p3_bam_write_state *st, p3_u32 object_id) {
    int rc;
    if (st->long_object_id) {
        rc = p3_bam_put_u32(b, object_id);
        return rc;
    }
    if (object_id > 0xffffUL) {
        return P3_ERR_RANGE;
    }
    rc = p3_bam_put_u16(b, (p3_u16)object_id);
    if (rc != P3_OK) {
        return rc;
    }
    if (object_id == 0xffffUL) {
        st->long_object_id = 1;
    }
    return P3_OK;
}

int p3_bam_write_file(const p3_bam_archive *archive, p3_u8 *dst, p3_u32 cap, p3_u32 *out_size, char *err, p3_u32 err_cap) {
    static const p3_u8 magic[P3_BAM_MAGIC_SIZE] = { 'p', 'b', 'j', 0, '\n', '\r' };
    p3_u8 dg_buf[P3_TEXT_IO_CAP];
    p3_bam_builder b;
    p3_bam_write_state st;
    p3_u32 pos;
    p3_u16 i;
    int rc;

    if (archive == 0 || dst == 0 || out_size == 0) {
        p3_bam_set_error(err, err_cap, "invalid args");
        return P3_ERR_INVALID;
    }

    if ((p3_u32)P3_BAM_MAGIC_SIZE > cap) {
        p3_bam_set_error(err, err_cap, "output too small");
        return P3_ERR_CAPACITY;
    }

    memcpy(dst, magic, P3_BAM_MAGIC_SIZE);
    pos = P3_BAM_MAGIC_SIZE;

    memset(&st, 0, sizeof(st));

    p3_bam_builder_init(&b, dg_buf, P3_TEXT_IO_CAP);
    rc = p3_bam_put_u16(&b, archive->major);
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, archive->minor);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, archive->endian);
    }
    if (rc == P3_OK && (archive->major > 6U || (archive->major == 6U && archive->minor >= 27U))) {
        rc = p3_bam_put_u8(&b, (p3_u8)(archive->stdfloat_double ? 1 : 0));
    }
    if (rc != P3_OK) {
        p3_bam_set_error(err, err_cap, "unable to build header");
        return rc;
    }
    rc = p3_write_datagram(dst, cap, &pos, dg_buf, b.size);
    if (rc != P3_OK) {
        p3_bam_set_error(err, err_cap, "unable to write header");
        return rc;
    }

    for (i = 0; i < archive->record_count; ++i) {
        const p3_bam_record *rec;
        rec = &archive->records[i];
        p3_bam_builder_init(&b, dg_buf, P3_TEXT_IO_CAP);
        rc = p3_bam_put_u8(&b, rec->kind);
        if (rc != P3_OK) {
            p3_bam_set_error(err, err_cap, "record buffer overflow");
            return rc;
        }

        if (rec->kind == P3_BAM_BOC_POP) {
            rc = p3_write_datagram(dst, cap, &pos, dg_buf, b.size);
            if (rc != P3_OK) {
                p3_bam_set_error(err, err_cap, "output too small");
                return rc;
            }
            continue;
        }

        if (rec->kind == P3_BAM_BOC_FILE_DATA) {
            rc = p3_write_datagram(dst, cap, &pos, dg_buf, b.size);
            if (rc != P3_OK) {
                p3_bam_set_error(err, err_cap, "output too small");
                return rc;
            }
            rc = p3_write_datagram(dst, cap, &pos, archive->raw + rec->data_off, rec->data_len);
            if (rc != P3_OK) {
                p3_bam_set_error(err, err_cap, "output too small");
                return rc;
            }
            continue;
        }

        if (rec->kind == P3_BAM_BOC_REMOVE) {
            p3_u16 j;
            for (j = 0; j < rec->remove_count; ++j) {
                rc = p3_write_object_id(&b, &st, archive->remove_ids[rec->first_remove + j]);
                if (rc != P3_OK) {
                    p3_bam_set_error(err, err_cap, "remove id overflow");
                    return rc;
                }
            }
            rc = p3_write_datagram(dst, cap, &pos, dg_buf, b.size);
            if (rc != P3_OK) {
                p3_bam_set_error(err, err_cap, "output too small");
                return rc;
            }
            continue;
        }

        if (rec->kind != P3_BAM_BOC_PUSH && rec->kind != P3_BAM_BOC_ADJUNCT) {
            p3_bam_set_error(err, err_cap, "unsupported record kind");
            return P3_ERR_UNSUPPORTED;
        }

        rc = p3_write_handle(&b, &st, archive, rec->type_id);
        if (rc != P3_OK) {
            p3_bam_set_error(err, err_cap, "unknown type id");
            return rc;
        }
        rc = p3_write_object_id(&b, &st, rec->object_id);
        if (rc != P3_OK) {
            p3_bam_set_error(err, err_cap, "object id overflow");
            return rc;
        }
        rc = p3_bam_put_bytes(&b, archive->raw + rec->payload_off, rec->payload_len);
        if (rc != P3_OK) {
            p3_bam_set_error(err, err_cap, "record payload overflow");
            return rc;
        }
        rc = p3_write_datagram(dst, cap, &pos, dg_buf, b.size);
        if (rc != P3_OK) {
            p3_bam_set_error(err, err_cap, "output too small");
            return rc;
        }
    }

    *out_size = pos;
    return P3_OK;
}
