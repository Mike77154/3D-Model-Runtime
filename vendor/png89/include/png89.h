#ifndef PNG89_H
#define PNG89_H
#include <stddef.h>
typedef struct png89_info_s {
    unsigned int width;
    unsigned int height;
    unsigned int bit_depth;
    unsigned int color_type;
    unsigned int interlace_method;
} png89_info;
int png89_probe(const unsigned char *src,size_t size);
int png89_get_info(const unsigned char *src,size_t size,png89_info *out_info);
int png89_decode_rgba(const unsigned char *src,size_t size,unsigned char *dst,size_t cap,unsigned int *w,unsigned int *h,size_t *used);
#endif
