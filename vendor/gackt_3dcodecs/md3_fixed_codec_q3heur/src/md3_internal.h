#ifndef MD3_INTERNAL_H
#define MD3_INTERNAL_H

#include <string.h>
#include "../include/md3/md3.h"

int md3_io_seek(FILE *fp, long offset);
int md3_io_read_exact(FILE *fp, void *dst, unsigned int size_bytes);
int md3_io_write_exact(FILE *fp, const void *src, unsigned int size_bytes);
int md3_io_zero_fill(FILE *fp, long total_size);

int md3_io_read_u16_le(FILE *fp, md3_u16 *out_value);
int md3_io_read_i16_le(FILE *fp, md3_i16 *out_value);
int md3_io_read_u32_le(FILE *fp, md3_u32 *out_value);
int md3_io_read_i32_le(FILE *fp, md3_i32 *out_value);

int md3_io_write_u16_le(FILE *fp, md3_u16 value);
int md3_io_write_i16_le(FILE *fp, md3_i16 value);
int md3_io_write_u32_le(FILE *fp, md3_u32 value);
int md3_io_write_i32_le(FILE *fp, md3_i32 value);

int md3_fixed_from_ieee754_u32(md3_u32 bits, md3_fixed_t *out_value);
md3_u32 md3_fixed_to_ieee754_u32(md3_fixed_t value);

void md3_zero_memory(void *ptr, unsigned int size_bytes);
void md3_copy_cstr_to_field(char *dst, unsigned int dst_size, const char *src);
void md3_read_field_to_cstr(char *dst, unsigned int dst_size,
    const char *src, unsigned int src_size);

int md3_read_fixed_f32(FILE *fp, md3_fixed_t *out_value);
int md3_write_fixed_f32(FILE *fp, md3_fixed_t value);
int md3_read_fixed_f32_raw(FILE *fp, md3_fixed_t *out_value,
    md3_u32 *out_bits);
int md3_write_fixed_f32_raw(FILE *fp, md3_fixed_t value,
    int use_raw_bits, md3_u32 raw_bits);

const md3_fixed_t *md3_sin_table_q16(void);
md3_fixed_t md3_sin_step_q16(md3_u8 angle_index);
md3_fixed_t md3_cos_step_q16(md3_u8 angle_index);

md3_u32 md3_isqrt_u32(md3_u32 value);

int md3_read_surface_header(FILE *fp, long base_offset,
    md3_surface_info_t *out_info);
int md3_write_surface_header(FILE *fp, const md3_surface_info_t *surface_info);

#endif
