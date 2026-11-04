#ifndef GIF_PLY_BRIDGE_ADAPTER_H
#define GIF_PLY_BRIDGE_ADAPTER_H

#include "gif_ply_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GIF_PLY_CODEC_NAME "gif_ply_c89_fixed"
#define GIF_PLY_CODEC_EXT  "ply"

typedef struct GifPlyCodecBinding {
    const char *name;
    const char *extension;
    int version_major;
    int version_minor;
    int version_patch;
    int (*probe_memory)(const unsigned char *data,
                        unsigned long size,
                        GifPlyInfo *info);
    int (*import_memory)(const unsigned char *data,
                         unsigned long size,
                         GifPlyMesh *mesh,
                         GifPlyInfo *info);
} GifPlyCodecBinding;

void gif_ply_get_codec_binding(GifPlyCodecBinding *out_binding);

#ifdef __cplusplus
}
#endif

#endif
