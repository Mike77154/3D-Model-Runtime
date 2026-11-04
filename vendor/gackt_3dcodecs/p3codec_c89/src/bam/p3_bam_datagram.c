#include <string.h>
#include "p3_bytes.h"
#include "p3_error.h"
#include "p3_bam_datagram.h"

void p3_bam_cursor_init(p3_bam_cursor *cur, const p3_u8 *data, p3_u32 size) {
    cur->data = data;
    cur->size = size;
    cur->pos = 0;
}

p3_u32 p3_bam_cursor_remaining(const p3_bam_cursor *cur) {
    if (cur == 0 || cur->pos > cur->size) {
        return 0UL;
    }
    return cur->size - cur->pos;
}

int p3_bam_skip_bytes(p3_bam_cursor *cur, p3_u32 count) {
    if (cur == 0) {
        return P3_ERR_INVALID;
    }
    if (count > p3_bam_cursor_remaining(cur)) {
        return P3_ERR_TRUNCATED;
    }
    cur->pos += count;
    return P3_OK;
}

int p3_bam_get_u8(p3_bam_cursor *cur, p3_u8 *out_value) {
    if (cur == 0 || out_value == 0) {
        return P3_ERR_INVALID;
    }
    if (1UL > p3_bam_cursor_remaining(cur)) {
        return P3_ERR_TRUNCATED;
    }
    *out_value = cur->data[cur->pos];
    cur->pos += 1UL;
    return P3_OK;
}

int p3_bam_get_u16(p3_bam_cursor *cur, p3_u16 *out_value) {
    if (cur == 0 || out_value == 0) {
        return P3_ERR_INVALID;
    }
    if (2UL > p3_bam_cursor_remaining(cur)) {
        return P3_ERR_TRUNCATED;
    }
    *out_value = p3_read_u16le(cur->data + cur->pos);
    cur->pos += 2UL;
    return P3_OK;
}

int p3_bam_get_u32(p3_bam_cursor *cur, p3_u32 *out_value) {
    if (cur == 0 || out_value == 0) {
        return P3_ERR_INVALID;
    }
    if (4UL > p3_bam_cursor_remaining(cur)) {
        return P3_ERR_TRUNCATED;
    }
    *out_value = p3_read_u32le(cur->data + cur->pos);
    cur->pos += 4UL;
    return P3_OK;
}

int p3_bam_get_string(p3_bam_cursor *cur, char *dst, p3_u16 dst_cap, p3_u16 *out_len) {
    p3_u16 len;
    int rc;
    if (cur == 0 || dst == 0 || dst_cap == 0U) {
        return P3_ERR_INVALID;
    }
    rc = p3_bam_get_u16(cur, &len);
    if (rc != P3_OK) {
        return rc;
    }
    if ((p3_u32)len > p3_bam_cursor_remaining(cur)) {
        return P3_ERR_TRUNCATED;
    }
    if ((p3_u32)len + 1UL > (p3_u32)dst_cap) {
        return P3_ERR_CAPACITY;
    }
    memcpy(dst, cur->data + cur->pos, (size_t)len);
    dst[len] = 0;
    cur->pos += (p3_u32)len;
    if (out_len != 0) {
        *out_len = len;
    }
    return P3_OK;
}

int p3_bam_get_bytes(p3_bam_cursor *cur, p3_u8 *dst, p3_u32 count) {
    if (cur == 0 || (dst == 0 && count > 0UL)) {
        return P3_ERR_INVALID;
    }
    if (count > p3_bam_cursor_remaining(cur)) {
        return P3_ERR_TRUNCATED;
    }
    if (count > 0UL) {
        memcpy(dst, cur->data + cur->pos, (size_t)count);
    }
    cur->pos += count;
    return P3_OK;
}

void p3_bam_builder_init(p3_bam_builder *b, p3_u8 *data, p3_u32 cap) {
    b->data = data;
    b->cap = cap;
    b->size = 0;
}

int p3_bam_put_u8(p3_bam_builder *b, p3_u8 value) {
    if (b == 0) {
        return P3_ERR_INVALID;
    }
    if (b->size > b->cap || 1UL > b->cap - b->size) {
        return P3_ERR_CAPACITY;
    }
    b->data[b->size++] = value;
    return P3_OK;
}

int p3_bam_put_u16(p3_bam_builder *b, p3_u16 value) {
    if (b == 0) {
        return P3_ERR_INVALID;
    }
    if (b->size > b->cap || 2UL > b->cap - b->size) {
        return P3_ERR_CAPACITY;
    }
    p3_write_u16le(b->data + b->size, value);
    b->size += 2UL;
    return P3_OK;
}

int p3_bam_put_u32(p3_bam_builder *b, p3_u32 value) {
    if (b == 0) {
        return P3_ERR_INVALID;
    }
    if (b->size > b->cap || 4UL > b->cap - b->size) {
        return P3_ERR_CAPACITY;
    }
    p3_write_u32le(b->data + b->size, value);
    b->size += 4UL;
    return P3_OK;
}

int p3_bam_put_string(p3_bam_builder *b, const char *text, p3_u16 len) {
    int rc;
    if (text == 0 && len > 0U) {
        return P3_ERR_INVALID;
    }
    rc = p3_bam_put_u16(b, len);
    if (rc != P3_OK) {
        return rc;
    }
    return p3_bam_put_bytes(b, (const p3_u8 *)text, (p3_u32)len);
}

int p3_bam_put_bytes(p3_bam_builder *b, const p3_u8 *src, p3_u32 count) {
    if (b == 0 || (src == 0 && count > 0UL)) {
        return P3_ERR_INVALID;
    }
    if (b->size > b->cap || count > b->cap - b->size) {
        return P3_ERR_CAPACITY;
    }
    if (count > 0UL) {
        memcpy(b->data + b->size, src, (size_t)count);
    }
    b->size += count;
    return P3_OK;
}
