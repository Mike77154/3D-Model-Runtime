#include <stdio.h>
#include <string.h>
#include "p3_bam_archive.h"
#include "p3_bam_datagram.h"
#include "p3_bam_reader.h"
#include "p3_bytes.h"
#include "p3_error.h"

typedef struct p3_bam_read_state {
    int long_object_id;
} p3_bam_read_state;

static void p3_bam_set_error(char *err, p3_u32 err_cap, const char *msg) {
    if (err != 0 && err_cap > 0UL) {
        sprintf(err, "%s", msg);
    }
}

static int p3_read_datagram(const p3_u8 *src, p3_u32 size, p3_u32 *io_pos, const p3_u8 **out_ptr, p3_u32 *out_len) {
    p3_u32 len;
    if (*io_pos == size) {
        return 1;
    }
    if (*io_pos > size || 4UL > size - *io_pos) {
        return P3_ERR_TRUNCATED;
    }
    len = p3_read_u32le(src + *io_pos);
    *io_pos += 4UL;
    if (len == 0xffffffffUL) {
        p3_u32 lo;
        p3_u32 hi;
        if (*io_pos > size || 8UL > size - *io_pos) {
            return P3_ERR_TRUNCATED;
        }
        lo = p3_read_u32le(src + *io_pos);
        hi = p3_read_u32le(src + *io_pos + 4UL);
        *io_pos += 8UL;
        if (hi != 0UL) {
            return P3_ERR_UNSUPPORTED;
        }
        len = lo;
    }
    if (*io_pos > size || len > size - *io_pos) {
        return P3_ERR_TRUNCATED;
    }
    *out_ptr = src + *io_pos;
    *out_len = len;
    *io_pos += len;
    return P3_OK;
}

static int p3_read_object_id(p3_bam_cursor *cur, p3_bam_read_state *st, p3_u32 *out_id) {
    int rc;
    p3_u16 short_id;
    if (st->long_object_id) {
        return p3_bam_get_u32(cur, out_id);
    }
    rc = p3_bam_get_u16(cur, &short_id);
    if (rc != P3_OK) {
        return rc;
    }
    *out_id = (p3_u32)short_id;
    if (*out_id == 0xffffUL) {
        st->long_object_id = 1;
    }
    return P3_OK;
}

static int p3_read_handle(p3_bam_archive *archive, p3_bam_cursor *cur, p3_u16 *out_type_id) {
    p3_u16 id;
    int rc;
    rc = p3_bam_get_u16(cur, &id);
    if (rc != P3_OK) {
        return rc;
    }
    if (id == 0U) {
        *out_type_id = 0U;
        return P3_OK;
    }
    if (p3_bam_find_type_index(archive, id) >= 0) {
        *out_type_id = id;
        return P3_OK;
    }
    {
        char name[256];
        p3_u16 name_len;
        p3_u8 parent_count;
        p3_u16 parents[255];
        p3_u8 i;

        rc = p3_bam_get_string(cur, name, (p3_u16)sizeof(name), &name_len);
        if (rc != P3_OK) {
            return rc;
        }
        rc = p3_bam_get_u8(cur, &parent_count);
        if (rc != P3_OK) {
            return rc;
        }
        for (i = 0; i < parent_count; ++i) {
            rc = p3_read_handle(archive, cur, &parents[i]);
            if (rc != P3_OK) {
                return rc;
            }
        }
        rc = p3_bam_add_type(archive, id, name, parents, parent_count);
        if (rc != P3_OK) {
            return rc;
        }
    }
    *out_type_id = id;
    return P3_OK;
}

int p3_bam_read_file(p3_bam_archive *archive, const p3_u8 *src, p3_u32 size, char *err, p3_u32 err_cap) {
    static const p3_u8 magic[P3_BAM_MAGIC_SIZE] = { 'p', 'b', 'j', 0, '\n', '\r' };
    p3_u32 pos;
    const p3_u8 *dg;
    p3_u32 dg_len;
    p3_bam_cursor cur;
    p3_bam_read_state st;
    int rc;

    if (archive == 0 || src == 0) {
        p3_bam_set_error(err, err_cap, "invalid args");
        return P3_ERR_INVALID;
    }
    if (size < P3_BAM_MAGIC_SIZE) {
        p3_bam_set_error(err, err_cap, "file too small");
        return P3_ERR_TRUNCATED;
    }
    if (memcmp(src, magic, P3_BAM_MAGIC_SIZE) != 0) {
        p3_bam_set_error(err, err_cap, "bad bam magic");
        return P3_ERR_INVALID;
    }

    p3_bam_archive_init(archive);
    memset(&st, 0, sizeof(st));
    pos = P3_BAM_MAGIC_SIZE;

    rc = p3_read_datagram(src, size, &pos, &dg, &dg_len);
    if (rc != P3_OK) {
        p3_bam_set_error(err, err_cap, "missing header datagram");
        return rc;
    }

    p3_bam_cursor_init(&cur, dg, dg_len);
    rc = p3_bam_get_u16(&cur, &archive->major);
    if (rc == P3_OK) {
        rc = p3_bam_get_u16(&cur, &archive->minor);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&cur, &archive->endian);
    }
    if (rc == P3_OK && archive->minor >= 27U) {
        rc = p3_bam_get_u8(&cur, &archive->stdfloat_double);
    }
    if (rc != P3_OK) {
        p3_bam_set_error(err, err_cap, "bad header");
        return rc;
    }

    while (1) {
        p3_u8 kind;
        rc = p3_read_datagram(src, size, &pos, &dg, &dg_len);
        if (rc == 1) {
            break;
        }
        if (rc != P3_OK) {
            p3_bam_set_error(err, err_cap, "truncated record datagram");
            return rc;
        }

        p3_bam_cursor_init(&cur, dg, dg_len);
        if (archive->minor >= 21U) {
            rc = p3_bam_get_u8(&cur, &kind);
            if (rc != P3_OK) {
                p3_bam_set_error(err, err_cap, "empty record datagram");
                return rc;
            }
        } else {
            kind = P3_BAM_BOC_ADJUNCT;
        }

        if (kind == P3_BAM_BOC_POP) {
            rc = p3_bam_add_pop(archive);
            if (rc != P3_OK) {
                p3_bam_set_error(err, err_cap, "record capacity exceeded");
                return rc;
            }
            continue;
        }

        if (kind == P3_BAM_BOC_REMOVE) {
            p3_u32 ids[P3_BAM_MAX_REMOVE_IDS];
            p3_u16 count;
            count = 0;
            while (cur.pos < cur.size) {
                if (count >= P3_BAM_MAX_REMOVE_IDS) {
                    p3_bam_set_error(err, err_cap, "too many remove ids");
                    return P3_ERR_CAPACITY;
                }
                rc = p3_read_object_id(&cur, &st, &ids[count]);
                if (rc != P3_OK) {
                    p3_bam_set_error(err, err_cap, "bad remove record");
                    return rc;
                }
                count = (p3_u16)(count + 1U);
            }
            rc = p3_bam_add_remove(archive, ids, count);
            if (rc != P3_OK) {
                p3_bam_set_error(err, err_cap, "record capacity exceeded");
                return rc;
            }
            continue;
        }

        if (kind == P3_BAM_BOC_FILE_DATA) {
            const p3_u8 *data_dg;
            p3_u32 data_len;
            rc = p3_read_datagram(src, size, &pos, &data_dg, &data_len);
            if (rc != P3_OK) {
                p3_bam_set_error(err, err_cap, "missing file-data payload");
                return rc;
            }
            rc = p3_bam_add_file_data(archive, data_dg, data_len);
            if (rc != P3_OK) {
                p3_bam_set_error(err, err_cap, "file-data capacity exceeded");
                return rc;
            }
            continue;
        }

        if (kind == P3_BAM_BOC_PUSH || kind == P3_BAM_BOC_ADJUNCT) {
            p3_u16 type_id;
            p3_u32 object_id;
            const p3_u8 *payload_ptr;
            p3_u32 payload_len;

            rc = p3_read_handle(archive, &cur, &type_id);
            if (rc != P3_OK) {
                p3_bam_set_error(err, err_cap, "bad type handle");
                return rc;
            }
            rc = p3_read_object_id(&cur, &st, &object_id);
            if (rc != P3_OK) {
                p3_bam_set_error(err, err_cap, "bad object id");
                return rc;
            }

            payload_ptr = cur.data + cur.pos;
            payload_len = cur.size - cur.pos;

            if (kind == P3_BAM_BOC_PUSH) {
                rc = p3_bam_add_push(archive, type_id, object_id, payload_ptr, payload_len);
            } else {
                rc = p3_bam_add_adjunct(archive, type_id, object_id, payload_ptr, payload_len);
            }
            if (rc != P3_OK) {
                p3_bam_set_error(err, err_cap, "record capacity exceeded");
                return rc;
            }
            archive->records[archive->record_count - 1U].payload_long_object_id = (p3_u8)(st.long_object_id ? 1 : 0);
            continue;
        }

        p3_bam_set_error(err, err_cap, "invalid record kind");
        return P3_ERR_INVALID;
    }

    return P3_OK;
}
