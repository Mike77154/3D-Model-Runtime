#ifndef ZLIB89_H
#define ZLIB89_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZLIB89_OK 0
#define ZLIB89_ERR_ARG (-1)
#define ZLIB89_ERR_FORMAT (-2)
#define ZLIB89_ERR_SPACE (-3)
#define ZLIB89_ERR_CHECKSUM (-4)
#define ZLIB89_ERR_UNSUPPORTED (-5)

typedef int (*zlib89_getbyte_fn)(void *user, unsigned char *out_byte);

int zlib89_inflate_reader(zlib89_getbyte_fn get_byte, void *user,
                          unsigned char *dst, size_t dst_capacity,
                          size_t *out_size);
int zlib89_inflate_memory(const unsigned char *src, size_t src_size,
                          unsigned char *dst, size_t dst_capacity,
                          size_t *out_size);
int zlib89_inflate_raw_memory(const unsigned char *src, size_t src_size,
                              unsigned char *dst, size_t dst_capacity,
                              size_t *out_size);
unsigned int zlib89_adler32(const unsigned char *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif
