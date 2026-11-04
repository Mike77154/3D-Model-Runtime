#ifndef TGA89_H
#define TGA89_H
#include <stddef.h>
int tga89_probe(const unsigned char *src,size_t size);
int tga89_decode_rgba(const unsigned char *src,size_t size,unsigned char *dst,size_t cap,unsigned int *w,unsigned int *h,size_t *used);
#endif
