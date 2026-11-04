#ifndef GLTF3D_MINI_KTX2_BASISU_H
#define GLTF3D_MINI_KTX2_BASISU_H

#include "gltf3d_mini_common.h"

#define GLTF3D_MINI_KTX2_MAX_LEVELS 16
#define GLTF3D_MINI_KTX2_SUPER_NONE 0UL
#define GLTF3D_MINI_KTX2_SUPER_BASIS_LZ 1UL
#define GLTF3D_MINI_KTX2_SUPER_ZSTD 2UL
#define GLTF3D_MINI_KTX2_SUPER_ZLIB 3UL
#define GLTF3D_MINI_KTX2_FLAG_SGD_64BIT 1
#define GLTF3D_MINI_BASISU_KIND_NONE 0
#define GLTF3D_MINI_BASISU_KIND_ETC1S 1
#define GLTF3D_MINI_BASISU_KIND_UASTC 2
#define GLTF3D_MINI_BASISU_KIND_UNCOMPRESSED_RGBA8 3
#define GLTF3D_MINI_BASISU_TARGET_RGBA8 1

typedef struct gltf3d_mini_ktx2_level_s {
    unsigned long byte_offset;
    unsigned long byte_length;
    unsigned long uncompressed_byte_length;
    unsigned long byte_offset_hi;
    unsigned long byte_length_hi;
    unsigned long uncompressed_byte_length_hi;
} gltf3d_mini_ktx2_level;

typedef struct gltf3d_mini_ktx2_info_s {
    unsigned long vk_format;
    unsigned long type_size;
    unsigned long pixel_width;
    unsigned long pixel_height;
    unsigned long pixel_depth;
    unsigned long layer_count;
    unsigned long face_count;
    unsigned long level_count;
    unsigned long supercompression_scheme;
    unsigned long dfd_byte_offset;
    unsigned long dfd_byte_length;
    unsigned long kvd_byte_offset;
    unsigned long kvd_byte_length;
    unsigned long sgd_byte_offset;
    unsigned long sgd_byte_length;
    unsigned long sgd_byte_offset_hi;
    unsigned long sgd_byte_length_hi;
    int is_etc1s_basislz;
    int is_uastc_basislz;
    unsigned long flags;
    unsigned long level_index_count;
    gltf3d_mini_ktx2_level levels[GLTF3D_MINI_KTX2_MAX_LEVELS];
    unsigned long dfd_vendor_id;
    unsigned long dfd_descriptor_type;
    unsigned long dfd_version_number;
    unsigned long dfd_color_model;
    unsigned long dfd_color_primaries;
    unsigned long dfd_transfer_function;
    unsigned long dfd_flags;
    unsigned long dfd_texel_block_dimension[4];
    unsigned long dfd_bytes_plane[8];
    unsigned long kv_pair_count;
} gltf3d_mini_ktx2_info;

typedef struct gltf3d_mini_basisu_callbacks_s {
    int (*transcode_rgba8)(void *user, const unsigned char *ktx2, unsigned long ktx2_size, unsigned char *dst_rgba, unsigned long dst_size, unsigned long *w, unsigned long *h);
    void *user;
} gltf3d_mini_basisu_callbacks;

int gltf3d_mini_ktx2_probe(const unsigned char *data, unsigned long size, gltf3d_mini_ktx2_info *info);
int gltf3d_mini_ktx2_validate_ranges(const unsigned char *data, unsigned long size, const gltf3d_mini_ktx2_info *info);
int gltf3d_mini_ktx2_basisu_transcode_rgba8(const unsigned char *data, unsigned long size, unsigned char *dst_rgba, unsigned long dst_size, unsigned long *w, unsigned long *h, const gltf3d_mini_basisu_callbacks *cb);
int gltf3d_mini_ktx2_decode_rgba8_uncompressed(const unsigned char *data, unsigned long size, unsigned char *dst_rgba, unsigned long dst_size, unsigned long *w, unsigned long *h);
int gltf3d_mini_ktx2_basisu_kind(const gltf3d_mini_ktx2_info *info);
const char *gltf3d_mini_ktx2_basisu_kind_name(int kind);
int gltf3d_mini_ktx2_transcode_plan_rgba8(const unsigned char *data, unsigned long size, unsigned long *w, unsigned long *h, int *kind);

#endif
