#ifndef JPG89_H
#define JPG89_H
#include <stddef.h>

typedef struct jpg89_info_s {
    unsigned int width;
    unsigned int height;
    unsigned int progressive;
    unsigned int scan_count;
    unsigned int components;
} jpg89_info;

int jpg89_probe(const unsigned char *src, size_t size);
int jpg89_get_info(const unsigned char *src, size_t size, jpg89_info *out_info);
int jpg89_decode_rgba(const unsigned char *src, size_t size, unsigned char *dst,
                      size_t cap, unsigned int *w, unsigned int *h, size_t *used);
#endif
