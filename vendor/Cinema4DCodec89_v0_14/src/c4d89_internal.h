#ifndef C4D89_INTERNAL_H
#define C4D89_INTERNAL_H
#include "c4d89.h"
int c4d89_magic(FILE *f);
int c4d89_jpeg_at(FILE *f, long offset, long filesize, C4D89_Image *out);
int c4d89_scan_images(FILE *f, long filesize, C4D89_Report *rep);
int c4d89_scan_names(FILE *f, long filesize, C4D89_Report *rep);
unsigned long c4d89_be32(const unsigned char *p);
#endif
