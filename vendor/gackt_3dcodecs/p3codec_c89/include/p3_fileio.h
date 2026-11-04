#ifndef P3_FILEIO_H
#define P3_FILEIO_H

#include "p3_types.h"

int p3_file_read_all(const char *path, p3_u8 *dst, p3_u32 cap, p3_u32 *out_size);
int p3_file_write_all(const char *path, const p3_u8 *src, p3_u32 size);

#endif
