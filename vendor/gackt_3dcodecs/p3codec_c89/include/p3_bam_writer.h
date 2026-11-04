#ifndef P3_BAM_WRITER_H
#define P3_BAM_WRITER_H

#include "p3_bam_archive.h"
#include "p3_types.h"

int p3_bam_write_file(const p3_bam_archive *archive, p3_u8 *dst, p3_u32 cap, p3_u32 *out_size, char *err, p3_u32 err_cap);

#endif
