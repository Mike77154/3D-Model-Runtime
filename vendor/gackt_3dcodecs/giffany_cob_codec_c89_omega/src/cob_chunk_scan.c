#include "cob_private.h"

#ifndef COB_SCAN_LINE_MAX
#define COB_SCAN_LINE_MAX 384
#endif

static int scan_store(cob_chunk_info *out_chunks, unsigned long cap, unsigned long *n, const cob_chunk_info *ci)
{
    if (n == 0 || ci == 0) {
        return COB_ERR_ARGUMENT;
    }
    if (out_chunks != 0 && *n < cap) {
        out_chunks[*n] = *ci;
    }
    ++(*n);
    if (out_chunks != 0 && *n > cap) {
        return COB_ERR_CAPACITY;
    }
    return COB_OK;
}

static int scan_ascii_chunks(const unsigned char *bytes, unsigned long count, unsigned long offset, cob_chunk_info *out_chunks, unsigned long cap, unsigned long *out_count)
{
    const char *p;
    const char *end;
    char line[COB_SCAN_LINE_MAX];
    unsigned long len;
    unsigned long n;
    cob_chunk_info ci;
    int st;

    p = (const char *)bytes + offset;
    end = (const char *)bytes + count;
    n = 0UL;
    while (p < end) {
        len = 0UL;
        while (p < end && *p != '\n' && *p != '\r') {
            if (len + 1UL < COB_SCAN_LINE_MAX) {
                line[len++] = *p;
            }
            ++p;
        }
        line[len] = '\0';
        while (p < end && (*p == '\n' || *p == '\r')) {
            ++p;
        }
        if (cob_parse_chunk_info_ascii(&ci, line)) {
            st = scan_store(out_chunks, cap, &n, &ci);
            if (st != COB_OK) {
                if (out_count != 0) *out_count = n;
                return st;
            }
            if (strcmp(ci.type, "END ") == 0) {
                break;
            }
        }
    }
    if (out_count != 0) {
        *out_count = n;
    }
    return COB_OK;
}

static int scan_binary_chunks(const unsigned char *bytes, unsigned long count, unsigned long offset, cob_chunk_info *out_chunks, unsigned long cap, unsigned long *out_count)
{
    unsigned long pos;
    unsigned long n;
    cob_chunk_info ci;
    int st;

    pos = offset;
    n = 0UL;
    while (pos + 20UL <= count) {
        memset(&ci, 0, sizeof(ci));
        ci.type[0] = (char)bytes[pos + 0UL];
        ci.type[1] = (char)bytes[pos + 1UL];
        ci.type[2] = (char)bytes[pos + 2UL];
        ci.type[3] = (char)bytes[pos + 3UL];
        ci.type[4] = '\0';
        ci.major = (cob_u16)cob_get_i16_le(bytes + pos + 4UL);
        ci.minor = (cob_u16)cob_get_i16_le(bytes + pos + 6UL);
        ci.id = cob_get_i32_le(bytes + pos + 8UL);
        ci.parent_id = cob_get_i32_le(bytes + pos + 12UL);
        ci.size = cob_get_i32_le(bytes + pos + 16UL);
        pos += 20UL;
        st = scan_store(out_chunks, cap, &n, &ci);
        if (st != COB_OK) {
            if (out_count != 0) *out_count = n;
            return st;
        }
        if (strcmp(ci.type, "END ") == 0) {
            break;
        }
        if (ci.size < 0) {
            if (out_count != 0) *out_count = n;
            return COB_ERR_PARSE;
        }
        if (pos + (unsigned long)ci.size > count) {
            if (out_count != 0) *out_count = n;
            return COB_ERR_TRUNCATED;
        }
        pos += (unsigned long)ci.size;
    }
    if (out_count != 0) {
        *out_count = n;
    }
    return COB_OK;
}

int cob_scan_chunks_memory(const void *bytes, unsigned long count, cob_chunk_info *out_chunks, unsigned long chunk_capacity, unsigned long *out_count)
{
    const unsigned char *p;
    char mode;
    int little;

    if (out_count != 0) {
        *out_count = 0UL;
    }
    if (bytes == 0 || count < 32UL) {
        return COB_ERR_ARGUMENT;
    }
    p = (const unsigned char *)bytes;
    if (memcmp(p, "Caligari ", 9) != 0) {
        return COB_ERR_BAD_MAGIC;
    }
    mode = (char)p[15];
    little = (p[16] == 'L' && p[17] == 'H') ? 1 : 0;
    if (!little) {
        return COB_ERR_UNSUPPORTED_ENDIAN;
    }
    if (mode == 'A') {
        return scan_ascii_chunks(p, count, 32UL, out_chunks, chunk_capacity, out_count);
    }
    if (mode == 'B') {
        return scan_binary_chunks(p, count, 32UL, out_chunks, chunk_capacity, out_count);
    }
    return COB_ERR_UNSUPPORTED_FORMAT;
}
