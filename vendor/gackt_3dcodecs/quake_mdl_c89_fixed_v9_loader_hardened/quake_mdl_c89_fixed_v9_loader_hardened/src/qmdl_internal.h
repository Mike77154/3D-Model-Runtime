
#ifndef QMDL_INTERNAL_H
#define QMDL_INTERNAL_H

#include "../include/qmdl/qmdl.h"
#include <string.h>

typedef struct qmdl_cursor_s {
    const qmdl_u8 *ptr;
    const qmdl_u8 *end;
} qmdl_cursor;

typedef struct qmdl_writer_s {
    qmdl_u8 *ptr;
    qmdl_u8 *end;
} qmdl_writer;

int qmdl_read_u8(qmdl_cursor *cur, qmdl_u8 *out_value);
int qmdl_read_u32_le(qmdl_cursor *cur, qmdl_u32 *out_value);
int qmdl_read_i32_le(qmdl_cursor *cur, qmdl_i32 *out_value);
int qmdl_read_fx16_f32_le(qmdl_cursor *cur, qmdl_fx16 *out_value);
int qmdl_read_bytes(qmdl_cursor *cur, void *dst, size_t count);

int qmdl_write_u8(qmdl_writer *wr, qmdl_u8 value);
int qmdl_write_u32_le(qmdl_writer *wr, qmdl_u32 value);
int qmdl_write_i32_le(qmdl_writer *wr, qmdl_i32 value);
int qmdl_write_fx16_f32_le(qmdl_writer *wr, qmdl_fx16 value);
int qmdl_write_bytes(qmdl_writer *wr, const void *src, size_t count);

int qmdl_size_add(size_t *value, size_t delta);
int qmdl_size_mul(size_t a, size_t b, size_t *out_value);
int qmdl_size_add_mul(size_t *acc, size_t a, size_t b);
int qmdl_u32_add(qmdl_u32 *value, qmdl_u32 delta);
int qmdl_u32_mul(qmdl_u32 a, qmdl_u32 b, qmdl_u32 *out_value);

void qmdl_copy_name16(char dst[16], const char src[16]);

#endif
