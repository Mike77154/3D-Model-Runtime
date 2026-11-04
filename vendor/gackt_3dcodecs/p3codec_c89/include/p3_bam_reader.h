#ifndef P3_BAM_READER_H
#define P3_BAM_READER_H

#include "p3_bam_archive.h"
#include "p3_types.h"

int p3_bam_read_file(p3_bam_archive *archive, const p3_u8 *src, p3_u32 size, char *err, p3_u32 err_cap);

#endif
