#ifndef P3_BYTES_H
#define P3_BYTES_H

#include "p3_types.h"

void p3_mem_copy(void *dst, const void *src, p3_u32 size);
void p3_mem_set(void *dst, int c, p3_u32 size);
int p3_mem_cmp(const void *a, const void *b, p3_u32 size);

p3_u16 p3_read_u16le(const p3_u8 *src);
p3_u32 p3_read_u32le(const p3_u8 *src);

void p3_write_u16le(p3_u8 *dst, p3_u16 value);
void p3_write_u32le(p3_u8 *dst, p3_u32 value);

#endif
