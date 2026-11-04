#ifndef BMP89_H
#define BMP89_H
#include <stddef.h>
int bmp89_probe(const unsigned char *src, size_t size);
int bmp89_decode_rgba(const unsigned char *src, size_t size, unsigned char *dst, size_t cap,
                      unsigned int *w, unsigned int *h, size_t *used);
#endif
