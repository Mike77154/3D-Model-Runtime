#ifndef G3D_KIB_H
#define G3D_KIB_H

#include "g3d_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

#define G3D_KIB_MAGIC 0x3042494Bu /* 'KIB0' little endian */
#define G3D_KIB_VERSION 1u

#define G3D_KIB_METHOD_STORE       0u
#define G3D_KIB_METHOD_PACKBITS    1u
#define G3D_KIB_METHOD_LZSS12      2u
#define G3D_KIB_METHOD_WAVELET8    3u
#define G3D_KIB_METHOD_RAW_FALLBACK 255u

#define G3D_KIB_DECODE_ALLOW_RAW_FALLBACK 0x00000001u
#define G3D_KIB_DECODE_REQUIRE_CONTAINER  0x00000002u

const char *g3d_kib_method_name(g3d_u32 method);

g3d_u32 g3d_kib_container_header_size(void);
int g3d_kib_wrap_container(g3d_u32 method,
                           const g3d_u8 *payload, g3d_u32 payload_size,
                           g3d_u32 unpacked_size,
                           g3d_u8 *dst, g3d_u32 dst_size,
                           g3d_u32 *used_bytes);

int g3d_kib_decode_auto(const g3d_u8 *src, g3d_u32 src_size,
                        g3d_u8 *dst, g3d_u32 dst_size,
                        g3d_u32 flags,
                        g3d_u32 *src_used,
                        g3d_u32 *method_out);
int g3d_kib_decode_auto_arena(const g3d_u8 *src, g3d_u32 src_size,
                              g3d_u8 *dst, g3d_u32 dst_size,
                              g3d_u32 flags,
                              g3d_u32 *src_used,
                              g3d_u32 *method_out,
                              g3d_arena *scratch_arena);

int g3d_kib_packbits_encode(const g3d_u8 *src, g3d_u32 src_size,
                            g3d_u8 *dst, g3d_u32 dst_size,
                            g3d_u32 *used_bytes);
int g3d_kib_packbits_decode(const g3d_u8 *src, g3d_u32 src_size,
                            g3d_u8 *dst, g3d_u32 dst_size,
                            g3d_u32 *src_used);

int g3d_kib_lzss12_decode(const g3d_u8 *src, g3d_u32 src_size,
                          g3d_u8 *dst, g3d_u32 dst_size,
                          g3d_u32 *src_used);

int g3d_kib_paeth_filter(g3d_u8 *data, g3d_u32 width, g3d_u32 height, g3d_u32 bytes_per_pixel);
int g3d_kib_paeth_unfilter(g3d_u8 *data, g3d_u32 width, g3d_u32 height, g3d_u32 bytes_per_pixel);

int g3d_kib_wavelet8_decode(const g3d_u8 *payload, g3d_u32 payload_size,
                            g3d_u8 *dst, g3d_u32 dst_size,
                            g3d_u32 *width_out, g3d_u32 *height_out,
                            g3d_u32 *levels_out);
int g3d_kib_wavelet8_decode_arena(const g3d_u8 *payload, g3d_u32 payload_size,
                                  g3d_u8 *dst, g3d_u32 dst_size,
                                  g3d_u32 *width_out, g3d_u32 *height_out,
                                  g3d_u32 *levels_out,
                                  g3d_arena *scratch_arena);

#ifdef __cplusplus
}
#endif

#endif
