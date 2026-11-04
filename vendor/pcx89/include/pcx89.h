#ifndef PCX89_H
#define PCX89_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int pcx89_probe(const unsigned char *src, size_t len);
/* 0=OK, -1=invalid, -2=insufficient RGBA output buffer. No heap. */
int pcx89_decode_rgba(const unsigned char *src, size_t len,
                      unsigned char *dst, size_t cap,
                      unsigned int *w, unsigned int *h, size_t *used);
#ifdef __cplusplus
}
#endif
#endif
