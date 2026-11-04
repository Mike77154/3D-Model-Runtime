#ifndef MD2_IO_H
#define MD2_IO_H

#include <stdio.h>
#include "md2_types.h"

int md2_io_get_file_size(FILE *fp, md2_i32 *out_size);
int md2_io_read_u8(FILE *fp, md2_u8 *out_value);
int md2_io_read_i16_le(FILE *fp, md2_i16 *out_value);
int md2_io_read_u16_le(FILE *fp, md2_u16 *out_value);
int md2_io_read_i32_le(FILE *fp, md2_i32 *out_value);
int md2_io_read_u32_le(FILE *fp, md2_u32 *out_value);
int md2_io_write_u8(FILE *fp, md2_u8 value);
int md2_io_write_i16_le(FILE *fp, md2_i16 value);
int md2_io_write_u16_le(FILE *fp, md2_u16 value);
int md2_io_write_i32_le(FILE *fp, md2_i32 value);
int md2_io_write_u32_le(FILE *fp, md2_u32 value);

#endif /* MD2_IO_H */
