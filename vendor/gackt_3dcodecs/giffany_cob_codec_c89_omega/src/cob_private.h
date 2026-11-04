#ifndef COB_PRIVATE_H
#define COB_PRIVATE_H

#include "cob_chiqui3d.h"

#include <string.h>
#include <stdio.h>

#define COB_ARRAY_COUNT(a) ((unsigned long)(sizeof(a) / sizeof((a)[0])))

void cob_diag_add(cob_scene *scene, int code, cob_i32 chunk_id, const char *type, const char *text);
void cob_copy_name(char *dst, unsigned long dst_size, const char *src, unsigned long src_len);
void cob_set_identity(cob_fixed m[16]);
void cob_zero_axes(cob_fixed a[12]);
int cob_starts_with(const char *s, const char *prefix);
const char *cob_skip_spaces(const char *p, const char *end);
const char *cob_next_token(const char *p, const char *end, const char **tok, unsigned long *len);
long cob_parse_i32_token(const char *p, const char *end, int *ok);
unsigned long cob_parse_u32_token(const char *p, const char *end, int *ok);
int cob_find_token_i32(const char *line, const char *token, long *out);
int cob_parse_chunk_info_ascii(cob_chunk_info *ci, const char *line);
int cob_match_chunk_header(const char *line, char type_out[5]);

unsigned long cob_strlen_ul(const char *s);
int cob_emit(cob_write_fn fn, void *user, const char *s);
int cob_emit_n(cob_write_fn fn, void *user, const void *bytes, unsigned long count);
int cob_emit_i(cob_write_fn fn, void *user, long v);
int cob_emit_u(cob_write_fn fn, void *user, unsigned long v);
int cob_emit_fixed(cob_write_fn fn, void *user, cob_fixed v, int decimals);

cob_u16 cob_get_u16_le(const unsigned char *p);
cob_i16 cob_get_i16_le(const unsigned char *p);
cob_u32 cob_get_u32_le(const unsigned char *p);
cob_i32 cob_get_i32_le(const unsigned char *p);
void cob_put_u16_le(unsigned char *p, cob_u16 v);
void cob_put_i16_le(unsigned char *p, cob_i16 v);
void cob_put_u32_le(unsigned char *p, cob_u32 v);
void cob_put_i32_le(unsigned char *p, cob_i32 v);

#endif
