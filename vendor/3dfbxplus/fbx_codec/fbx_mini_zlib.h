#ifndef FBX_MINI_ZLIB_H
#define FBX_MINI_ZLIB_H

#include <stddef.h>
#include "fbx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FbxMiniZlibResultTag {
    FBX_MINIZ_OK = 0,
    FBX_MINIZ_BAD_ARGUMENT = 1,
    FBX_MINIZ_BAD_HEADER = 2,
    FBX_MINIZ_UNSUPPORTED = 3,
    FBX_MINIZ_INPUT_OVERFLOW = 4,
    FBX_MINIZ_OUTPUT_OVERFLOW = 5,
    FBX_MINIZ_BAD_BLOCK = 6,
    FBX_MINIZ_BAD_HUFFMAN = 7,
    FBX_MINIZ_BAD_DISTANCE = 8,
    FBX_MINIZ_BAD_CHECKSUM = 9
} FbxMiniZlibResult;

int fbx_mini_zlib_decode(const fbx_u8 *src,
                         size_t src_size,
                         fbx_u8 *dst,
                         size_t dst_capacity,
                         size_t *out_written);

int fbx_mini_deflate_decode(const fbx_u8 *src,
                            size_t src_size,
                            fbx_u8 *dst,
                            size_t dst_capacity,
                            size_t *out_written);

size_t fbx_mini_zlib_store_bound(size_t src_size);

int fbx_mini_zlib_store_encode(const fbx_u8 *src,
                               size_t src_size,
                               fbx_u8 *dst,
                               size_t dst_capacity,
                               size_t *out_written);

const char *fbx_mini_zlib_result_string(int code);
fbx_u32     fbx_mini_adler32(const fbx_u8 *data, size_t size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FBX_MINI_ZLIB_H */
