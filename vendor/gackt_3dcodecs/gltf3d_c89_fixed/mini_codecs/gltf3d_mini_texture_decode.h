#ifndef GLTF3D_MINI_TEXTURE_DECODE_H
#define GLTF3D_MINI_TEXTURE_DECODE_H

#include "gltf3d_mini_common.h"
#include "gltf3d_mini_ktx2_basisu.h"

#define GLTF3D_MINI_TEX_SRC_NONE 0
#define GLTF3D_MINI_TEX_SRC_BUFFER_VIEW 1
#define GLTF3D_MINI_TEX_SRC_URI 2

#define GLTF3D_MINI_TEX_FMT_UNKNOWN 0
#define GLTF3D_MINI_TEX_FMT_PNG 1
#define GLTF3D_MINI_TEX_FMT_JPEG 2
#define GLTF3D_MINI_TEX_FMT_KTX2 3

typedef struct gltf3d_mini_texture_result_s {
    unsigned long width;
    unsigned long height;
    int source_kind;
    int format;
    int used_basisu_callback;
    int used_external_uri;
} gltf3d_mini_texture_result;

typedef struct gltf3d_mini_texture_callbacks_s {
    int (*load_uri)(void *user, const char *uri, const unsigned char **data, unsigned long *size);
    int (*basisu_transcode_rgba8)(void *user, const unsigned char *ktx2, unsigned long ktx2_size, unsigned char *dst_rgba, unsigned long dst_size, unsigned long *w, unsigned long *h);
    void *user;
} gltf3d_mini_texture_callbacks;

int gltf3d_mini_texture_get_image_bytes(const gltf3d_asset *asset, int image_index,
                                        const unsigned char **data, unsigned long *size,
                                        int *source_kind,
                                        const gltf3d_mini_texture_callbacks *cb);
int gltf3d_mini_texture_guess_format(const unsigned char *data, unsigned long size, const char *mime_type);
int gltf3d_mini_decode_image_rgba8(const unsigned char *data, unsigned long size, const char *mime_type,
                                   unsigned char *dst_rgba, unsigned long dst_size,
                                   unsigned char *work, unsigned long work_size,
                                   unsigned long *w, unsigned long *h,
                                   int *format,
                                   const gltf3d_mini_texture_callbacks *cb);
int gltf3d_mini_decode_texture_rgba8(const gltf3d_asset *asset, int texture_index,
                                     unsigned char *dst_rgba, unsigned long dst_size,
                                     unsigned char *work, unsigned long work_size,
                                     gltf3d_mini_texture_result *result,
                                     const gltf3d_mini_texture_callbacks *cb);

#endif
