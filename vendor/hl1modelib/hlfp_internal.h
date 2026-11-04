#ifndef HLFP_INTERNAL_H
#define HLFP_INTERNAL_H

#include "hlfp.h"

#include <limits.h>
#include <string.h>

#define HLFP_UNUSED(x) ((void)(x))

typedef struct hlfp_text_cursor_s {
    const char *data;
    size_t size;
    size_t pos;
    int line;
} hlfp_text_cursor;

int hlfp__set_error(hlfp_error *err, int code, int line, int column, const char *msg);
int hlfp__set_errorf(hlfp_error *err, int code, int line, int column, const char *prefix, int value);
void hlfp__zero(void *ptr, size_t size);
void hlfp__copy_cstr(char *dst, int dst_cap, const char *src, int src_len);
int hlfp__is_space(int c);
int hlfp__is_digit(int c);
const hlfp_u8 *hlfp__checked_range(const hlfp_buffer *buf, size_t off, size_t bytes, hlfp_error *err, const char *what);
int hlfp__read_line(hlfp_text_cursor *cur, const char **line_start, int *line_len);
void hlfp__trim_line(const char **start, int *len);
int hlfp__parse_i32_token(const char *s, int len, int *out);
int hlfp__parse_fixed_token(const char *s, int len, hlfp_fixed *out);
int hlfp__parse_quoted_string(const char *s, int len, int *used, char *dst, int dst_cap);
int hlfp__float_bits_to_fixed(hlfp_u32 bits, hlfp_fixed *out);
int hlfp__fixed_mul_i16(hlfp_fixed scale, hlfp_i16 sample, hlfp_fixed *out);
hlfp_u16 hlfp__read_u16le(const hlfp_u8 *p);
hlfp_i16 hlfp__read_i16le(const hlfp_u8 *p);
hlfp_u32 hlfp__read_u32le(const hlfp_u8 *p);
hlfp_i32 hlfp__read_i32le(const hlfp_u8 *p);

#endif
