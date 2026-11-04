#ifndef GLTF3D_MINI_IMAGES_H
#define GLTF3D_MINI_IMAGES_H

#include "gltf3d_mini_common.h"

typedef struct gltf3d_mini_image_info_s {
    unsigned long width;
    unsigned long height;
    int channels;
    int bits_per_channel;
    int format;
    int is_png;
    int is_jpeg;
} gltf3d_mini_image_info;

typedef struct gltf3d_mini_image_callbacks_s {
    int (*decode_png_rgba8)(void *user, const unsigned char *src, unsigned long src_size, unsigned char *dst_rgba, unsigned long dst_size, unsigned long *w, unsigned long *h);
    int (*decode_jpeg_rgba8)(void *user, const unsigned char *src, unsigned long src_size, unsigned char *dst_rgba, unsigned long dst_size, unsigned long *w, unsigned long *h);
    void *user;
} gltf3d_mini_image_callbacks;

int gltf3d_mini_png_probe(const unsigned char *data, unsigned long size, gltf3d_mini_image_info *info);
int gltf3d_mini_jpeg_probe(const unsigned char *data, unsigned long size, gltf3d_mini_image_info *info);
int gltf3d_mini_image_probe(const unsigned char *data, unsigned long size, gltf3d_mini_image_info *info);
int gltf3d_mini_image_decode_rgba8(const unsigned char *data, unsigned long size, unsigned char *dst_rgba, unsigned long dst_size, unsigned long *w, unsigned long *h, const gltf3d_mini_image_callbacks *cb);

/* Built-in no-heap PNG path: caller supplies scratch workspace. */
int gltf3d_mini_image_decode_rgba8_work(const unsigned char *data, unsigned long size, unsigned char *dst_rgba, unsigned long dst_size, unsigned char *work, unsigned long work_size, unsigned long *w, unsigned long *h);

#endif
