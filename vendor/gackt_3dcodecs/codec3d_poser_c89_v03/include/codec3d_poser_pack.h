#ifndef CODEC3D_POSER_PACK_H
#define CODEC3D_POSER_PACK_H

#include "codec3d_poser.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef P3D_PACK_OUT_MAX
#define P3D_PACK_OUT_MAX 1048576UL
#endif
#ifndef P3D_ZLIB_ARENA_SIZE
#define P3D_ZLIB_ARENA_SIZE 262144UL
#endif

/*
   Optional PPZ/PZZ/PP2Z/P2Z packed-text helper.
   Build with -DP3D_USE_ZLIB and link -lz to decode real gzip/zlib streams.
   Without P3D_USE_ZLIB it still detects gzip/zlib and returns P3D_ERR_UNSUPPORTED.
*/
int p3d_decode_packed_memory(P3D_Context *ctx,
                             const unsigned char *packed,
                             unsigned long packed_size,
                             unsigned char *scratch_out,
                             unsigned long scratch_out_size,
                             unsigned long *decoded_size);

#ifdef __cplusplus
}
#endif

#endif
