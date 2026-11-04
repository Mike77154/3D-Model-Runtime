#include <string.h>
#include "p3_error.h"
#include "p3_string_pool.h"

void p3_string_pool_init(p3_string_pool *pool) {
    pool->len = 0;
}

int p3_string_pool_add(p3_string_pool *pool, const char *src, p3_u16 src_len, p3_u16 *out_off) {
    p3_u32 need;
    if (pool == 0 || src == 0 || out_off == 0) {
        return P3_ERR_INVALID;
    }
    need = (p3_u32)pool->len + (p3_u32)src_len + 1UL;
    if (need > (p3_u32)P3_STRING_POOL_CAP) {
        return P3_ERR_CAPACITY;
    }
    *out_off = pool->len;
    memcpy(pool->data + pool->len, src, (size_t)src_len);
    pool->len = (p3_u16)(pool->len + src_len);
    pool->data[pool->len] = 0;
    pool->len = (p3_u16)(pool->len + 1U);
    return P3_OK;
}

int p3_string_pool_add_cstr(p3_string_pool *pool, const char *src, p3_u16 *out_off) {
    return p3_string_pool_add(pool, src, (p3_u16)strlen(src), out_off);
}

const char *p3_string_pool_ptr(const p3_string_pool *pool, p3_u16 off) {
    if (pool == 0 || off >= pool->len) {
        return "";
    }
    return pool->data + off;
}

int p3_string_pool_copy_out(const p3_string_pool *pool, p3_u16 off, p3_u16 len, char *dst, p3_u16 dst_cap) {
    if (pool == 0 || dst == 0) {
        return P3_ERR_INVALID;
    }
    if ((p3_u32)off + (p3_u32)len > (p3_u32)pool->len) {
        return P3_ERR_RANGE;
    }
    if ((p3_u32)len + 1UL > (p3_u32)dst_cap) {
        return P3_ERR_CAPACITY;
    }
    memcpy(dst, pool->data + off, (size_t)len);
    dst[len] = 0;
    return P3_OK;
}

int p3_string_pool_equals_range(const p3_string_pool *pool, p3_u16 off, p3_u16 len, const char *text, p3_u16 text_len) {
    if (pool == 0 || text == 0) {
        return P3_FALSE;
    }
    if (len != text_len) {
        return P3_FALSE;
    }
    if ((p3_u32)off + (p3_u32)len > (p3_u32)pool->len) {
        return P3_FALSE;
    }
    if (memcmp(pool->data + off, text, (size_t)len) == 0) {
        return P3_TRUE;
    }
    return P3_FALSE;
}

int p3_string_pool_equals_cstr(const p3_string_pool *pool, p3_u16 off, p3_u16 len, const char *text) {
    return p3_string_pool_equals_range(pool, off, len, text, (p3_u16)strlen(text));
}
