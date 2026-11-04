#ifndef P3_BAM_DATAGRAM_H
#define P3_BAM_DATAGRAM_H

#include "p3_types.h"

typedef struct p3_bam_cursor {
    const p3_u8 *data;
    p3_u32 size;
    p3_u32 pos;
} p3_bam_cursor;

typedef struct p3_bam_builder {
    p3_u8 *data;
    p3_u32 cap;
    p3_u32 size;
} p3_bam_builder;

void p3_bam_cursor_init(p3_bam_cursor *cur, const p3_u8 *data, p3_u32 size);
p3_u32 p3_bam_cursor_remaining(const p3_bam_cursor *cur);
int p3_bam_skip_bytes(p3_bam_cursor *cur, p3_u32 count);
int p3_bam_get_u8(p3_bam_cursor *cur, p3_u8 *out_value);
int p3_bam_get_u16(p3_bam_cursor *cur, p3_u16 *out_value);
int p3_bam_get_u32(p3_bam_cursor *cur, p3_u32 *out_value);
int p3_bam_get_string(p3_bam_cursor *cur, char *dst, p3_u16 dst_cap, p3_u16 *out_len);
int p3_bam_get_bytes(p3_bam_cursor *cur, p3_u8 *dst, p3_u32 count);

void p3_bam_builder_init(p3_bam_builder *b, p3_u8 *data, p3_u32 cap);
int p3_bam_put_u8(p3_bam_builder *b, p3_u8 value);
int p3_bam_put_u16(p3_bam_builder *b, p3_u16 value);
int p3_bam_put_u32(p3_bam_builder *b, p3_u32 value);
int p3_bam_put_string(p3_bam_builder *b, const char *text, p3_u16 len);
int p3_bam_put_bytes(p3_bam_builder *b, const p3_u8 *src, p3_u32 count);

#endif
