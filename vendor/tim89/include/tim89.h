#ifndef TIM89_H
#define TIM89_H
#include <stddef.h>

typedef struct tim89_info_s {
    unsigned int width;
    unsigned int height;
    unsigned int bpp;
    unsigned int palette_count;
    unsigned int subresource_count;
    unsigned int image_x;
    unsigned int image_y;
    unsigned int clut_x;
    unsigned int clut_y;
    unsigned int flags;
} tim89_info;

#define TIM89_FLAG_INDEXED  0x00000001u
#define TIM89_FLAG_HAS_CLUT 0x00000002u
#define TIM89_FLAG_STP      0x00000004u

int tim89_probe(const unsigned char *src, size_t size);
int tim89_get_info(const unsigned char *src, size_t size, tim89_info *out_info);
int tim89_decode_subresource_rgba(const unsigned char *src, size_t size,
                                  unsigned int subresource_index,
                                  unsigned char *dst, size_t cap,
                                  unsigned int *w, unsigned int *h, size_t *used);
int tim89_decode_rgba(const unsigned char *src, size_t size, unsigned char *dst,
                      size_t cap, unsigned int *w, unsigned int *h, size_t *used);
#endif
