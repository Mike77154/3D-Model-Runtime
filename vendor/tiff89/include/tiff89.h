#ifndef TIFF89_H
#define TIFF89_H
#include <stddef.h>

typedef struct tiff89_info_s {
    unsigned int width;
    unsigned int height;
    unsigned int page_count;
    unsigned int subresource_count;
    unsigned int flags;
} tiff89_info;

typedef struct tiff89_page_info_s {
    unsigned int page_index;
    unsigned int width;
    unsigned int height;
    unsigned int samples_per_pixel;
    unsigned int compression;
    unsigned int photometric;
    unsigned int planar_configuration;
    unsigned int tiled;
    unsigned int tile_width;
    unsigned int tile_height;
} tiff89_page_info;

#define TIFF89_FLAG_MULTIPAGE 0x00000001u
#define TIFF89_FLAG_TILED     0x00000002u
#define TIFF89_FLAG_PLANAR    0x00000004u

int tiff89_probe(const unsigned char *src, size_t size);
int tiff89_get_info(const unsigned char *src, size_t size, tiff89_info *out_info);
int tiff89_get_page_info(const unsigned char *src, size_t size, unsigned int page_index,
                         tiff89_page_info *out_info);
int tiff89_decode_page_rgba(const unsigned char *src, size_t size, unsigned int page_index,
                            unsigned char *dst, size_t cap,
                            unsigned int *w, unsigned int *h, size_t *used);
int tiff89_decode_rgba(const unsigned char *src, size_t size, unsigned char *dst,
                       size_t cap, unsigned int *w, unsigned int *h, size_t *used);
#endif
