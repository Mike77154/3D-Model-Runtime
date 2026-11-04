#ifndef DDS89_H
#define DDS89_H
#include <stddef.h>

typedef struct dds89_info_s {
    unsigned int width;
    unsigned int height;
    unsigned int mip_count;
    unsigned int face_count;
    unsigned int subresource_count;
    unsigned int face_mask;
    unsigned int format;
    unsigned int flags;
} dds89_info;

typedef struct dds89_subresource_info_s {
    unsigned int index;
    unsigned int face_index;
    unsigned int mip_level;
    unsigned int width;
    unsigned int height;
    size_t encoded_offset;
    size_t encoded_bytes;
} dds89_subresource_info;

#define DDS89_FLAG_CUBEMAP 0x00000001u
#define DDS89_FLAG_DX10    0x00000002u

int dds89_probe(const unsigned char *src, size_t size);
int dds89_get_info(const unsigned char *src, size_t size, dds89_info *out_info);
int dds89_get_subresource_info(const unsigned char *src, size_t size,
                               unsigned int subresource_index,
                               dds89_subresource_info *out_info);
int dds89_decode_subresource_rgba(const unsigned char *src, size_t size,
                                  unsigned int subresource_index,
                                  unsigned char *dst, size_t cap,
                                  unsigned int *w, unsigned int *h, size_t *used);
int dds89_decode_rgba(const unsigned char *src, size_t size, unsigned char *dst,
                      size_t cap, unsigned int *w, unsigned int *h, size_t *used);
#endif
