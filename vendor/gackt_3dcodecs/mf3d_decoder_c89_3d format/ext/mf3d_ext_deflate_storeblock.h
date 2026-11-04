#ifndef MF3D_EXT_DEFLATE_STOREBLOCK_H
#define MF3D_EXT_DEFLATE_STOREBLOCK_H

#include "../src/mf3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Raw Deflate callback for ZIP method 8 streams that contain only uncompressed
   Deflate blocks (BTYPE=00). It is intentionally small and no-heap; it is NOT
   a full Huffman Deflate implementation. */
int mf3d_inflate_raw_stored_blocks(void *user, const mf3d_u8 *src, mf3d_u32 src_len,
                                   mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len);

#ifdef __cplusplus
}
#endif
#endif
