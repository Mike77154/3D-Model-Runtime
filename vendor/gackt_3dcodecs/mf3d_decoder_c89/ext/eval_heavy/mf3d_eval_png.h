#ifndef MF3D_EVAL_PNG_H
#define MF3D_EVAL_PNG_H

#include "../../src/mf3d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*mf3d_eval_png_inflate_fn)(void *user, const mf3d_u8 *src, mf3d_u32 src_len, mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len);

typedef struct mf3d_eval_image_u8_s {
    mf3d_u32 width;
    mf3d_u32 height;
    mf3d_u32 channels; /* 1 gray, 3 rgb, 4 rgba */
    mf3d_u8 *pixels;
    mf3d_u32 pixel_cap;
} mf3d_eval_image_u8;

typedef struct mf3d_eval_png_options_s {
    mf3d_eval_png_inflate_fn inflate_cb; /* raw deflate callback; zlib header is stripped here */
    void *inflate_user;
    mf3d_u8 *compressed_scratch;
    mf3d_u32 compressed_scratch_size;
    mf3d_u8 *scanline_scratch;
    mf3d_u32 scanline_scratch_size;
} mf3d_eval_png_options;

int mf3d_eval_png_decode(const mf3d_u8 *png, mf3d_u32 png_size, mf3d_eval_image_u8 *out_img, mf3d_eval_png_options *opt);
mf3d_fix mf3d_eval_image_sample_gray16(const mf3d_eval_image_u8 *img, mf3d_fix u, mf3d_fix v);

#ifdef __cplusplus
}
#endif
#endif
