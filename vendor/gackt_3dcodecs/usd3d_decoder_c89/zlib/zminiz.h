#ifndef ZMINIZ_H
#define ZMINIZ_H

/*
   zminiz: tiny C89, no-heap inflate helper for usd3d_decoder_c89.

   It implements enough of RFC1950/RFC1951 for importer-side decoding:
   - raw DEFLATE blocks: stored, fixed Huffman, dynamic Huffman
   - zlib wrapper validation + Adler-32 check
   - no malloc/free/realloc/calloc
   - caller-owned input/output buffers

   Not a full replacement for upstream zlib. No compressor, no gzip, no ZIP parser.
*/

#ifdef __cplusplus
extern "C" {
#endif

#define ZMINIZ_OK 0
#define ZMINIZ_ERR_BAD_ARG -1
#define ZMINIZ_ERR_INPUT -2
#define ZMINIZ_ERR_OUTPUT -3
#define ZMINIZ_ERR_FORMAT -4
#define ZMINIZ_ERR_UNSUPPORTED -5
#define ZMINIZ_ERR_CHECKSUM -6

typedef unsigned char zminiz_u8;
typedef unsigned long zminiz_u32;

int zminiz_inflate_raw(const zminiz_u8 *src,
                       zminiz_u32 src_len,
                       zminiz_u8 *dst,
                       zminiz_u32 dst_cap,
                       zminiz_u32 *dst_len);

int zminiz_inflate_zlib(const zminiz_u8 *src,
                        zminiz_u32 src_len,
                        zminiz_u8 *dst,
                        zminiz_u32 dst_cap,
                        zminiz_u32 *dst_len);

zminiz_u32 zminiz_adler32(const zminiz_u8 *buf, zminiz_u32 len);
const char *zminiz_result_name(int code);

#ifdef __cplusplus
}
#endif

#endif
