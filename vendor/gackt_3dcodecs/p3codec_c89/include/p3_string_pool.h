#ifndef P3_STRING_POOL_H
#define P3_STRING_POOL_H

#include "p3_config.h"
#include "p3_types.h"

typedef struct p3_string_pool {
    char data[P3_STRING_POOL_CAP];
    p3_u16 len;
} p3_string_pool;

void p3_string_pool_init(p3_string_pool *pool);
int p3_string_pool_add(p3_string_pool *pool, const char *src, p3_u16 src_len, p3_u16 *out_off);
int p3_string_pool_add_cstr(p3_string_pool *pool, const char *src, p3_u16 *out_off);
const char *p3_string_pool_ptr(const p3_string_pool *pool, p3_u16 off);
int p3_string_pool_copy_out(const p3_string_pool *pool, p3_u16 off, p3_u16 len, char *dst, p3_u16 dst_cap);
int p3_string_pool_equals_cstr(const p3_string_pool *pool, p3_u16 off, p3_u16 len, const char *text);
int p3_string_pool_equals_range(const p3_string_pool *pool, p3_u16 off, p3_u16 len, const char *text, p3_u16 text_len);

#endif
