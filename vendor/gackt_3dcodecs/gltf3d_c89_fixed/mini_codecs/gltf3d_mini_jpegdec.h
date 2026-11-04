#ifndef GLTF3D_MINI_JPEGDEC_H
#define GLTF3D_MINI_JPEGDEC_H

#include "gltf3d_mini_common.h"

#define GLTF3D_MINI_JPEGDEC_WORK_MAGIC 0x4a504744UL
#define GLTF3D_MINI_JPEGDEC_MIN_WORK 4096UL

/*
   Baseline sequential JPEG decoder, intended for glTF image/jpeg payloads.
   - C89 API, no malloc/free.
   - 8-bit precision only.
   - 1-component grayscale and 3-component YCbCr/RGB-ish files.
   - SOF0 + Huffman entropy + dequant + fixed-point IDCT.
   - Output is tightly packed RGBA8.
   Progressive/arithmetic/lossless/CMYK are rejected.
*/

typedef struct gltf3d_mini_jpegdec_result_s {
    unsigned long width;
    unsigned long height;
    unsigned long mcu_count;
    unsigned long restart_interval;
    int component_count;
    int saw_jfif;
    int saw_exif;
    int saw_adobe;
} gltf3d_mini_jpegdec_result;

int gltf3d_mini_jpeg_decode_rgba8(const unsigned char *data,
                                  unsigned long size,
                                  unsigned char *dst_rgba,
                                  unsigned long dst_size,
                                  void *work,
                                  unsigned long work_size,
                                  gltf3d_mini_jpegdec_result *result);

#endif
