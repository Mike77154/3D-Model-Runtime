#ifndef GLTF3D_MINI_PNGDEC_H
#define GLTF3D_MINI_PNGDEC_H

#include "gltf3d_mini_common.h"
#include "gltf3d_mini_images.h"

#define GLTF3D_MINI_PNG_COLOR_GRAY 0
#define GLTF3D_MINI_PNG_COLOR_RGB 2
#define GLTF3D_MINI_PNG_COLOR_INDEXED 3
#define GLTF3D_MINI_PNG_COLOR_GRAY_ALPHA 4
#define GLTF3D_MINI_PNG_COLOR_RGBA 6

#define GLTF3D_MINI_PNG_MAX_PALETTE 256

#define GLTF3D_MINI_PNG_FLAG_HAS_PLTE 1
#define GLTF3D_MINI_PNG_FLAG_HAS_TRNS 2
#define GLTF3D_MINI_PNG_FLAG_INTERLACED 4
#define GLTF3D_MINI_PNG_FLAG_HAS_IDAT 8

/*
   Tiny no-heap PNG decoder for glTF textures.
   Scope: PNG signature/chunks, CRC checks, zlib wrapper, DEFLATE stored/fixed/dynamic,
   non-interlaced scanlines, filters 0..4, color types 0/2/3/4/6, bit depths 1/2/4/8/16.
   Output is RGBA8. 16-bit samples are downshifted to the high byte.
   Caller owns dst and work buffers. No malloc, no FILE, no float/double.
*/
typedef struct gltf3d_mini_png_info_s {
    unsigned long width;
    unsigned long height;
    int bit_depth;
    int color_type;
    int compression_method;
    int filter_method;
    int interlace_method;
    unsigned long idat_bytes;
    unsigned long chunk_count;
    unsigned long flags;
    unsigned char palette[GLTF3D_MINI_PNG_MAX_PALETTE * 4];
    unsigned long palette_count;
    unsigned char trns_gray[2];
    unsigned char trns_rgb[6];
    unsigned long required_raw_bytes;
    unsigned long required_work_bytes;
} gltf3d_mini_png_info;

int gltf3d_mini_png_parse(const unsigned char *data, unsigned long size, gltf3d_mini_png_info *info);
int gltf3d_mini_png_required_rgba8_work(const unsigned char *data, unsigned long size, unsigned long *work_bytes, unsigned long *dst_bytes, unsigned long *w, unsigned long *h);
int gltf3d_mini_png_decode_rgba8_work(const unsigned char *data, unsigned long size, unsigned char *dst_rgba, unsigned long dst_size, unsigned char *work, unsigned long work_size, unsigned long *w, unsigned long *h);

#endif
