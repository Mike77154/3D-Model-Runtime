#ifndef GLTF3D_MINI_DRACO_H
#define GLTF3D_MINI_DRACO_H

#include "gltf3d_mini_common.h"

typedef struct gltf3d_mini_draco_probe_s {
    unsigned long byte_count;
    int likely_draco;
    int bitstream_version_major;
    int bitstream_version_minor;
    int encoder_type;
    int geometry_type;
} gltf3d_mini_draco_probe;

typedef struct gltf3d_mini_draco_out_s {
    unsigned char *vertex_bytes;
    unsigned long vertex_capacity;
    unsigned long vertex_size;
    unsigned char *index_bytes;
    unsigned long index_capacity;
    unsigned long index_size;
} gltf3d_mini_draco_out;

typedef struct gltf3d_mini_draco_callbacks_s {
    int (*decode_mesh)(void *user, const unsigned char *src, unsigned long src_size, gltf3d_mini_draco_out *out);
    void *user;
} gltf3d_mini_draco_callbacks;

int gltf3d_mini_draco_probe_stream(const unsigned char *data, unsigned long size, gltf3d_mini_draco_probe *probe);
int gltf3d_mini_draco_require_supported_probe(const gltf3d_mini_draco_probe *probe);
int gltf3d_mini_draco_validate_primitive(const gltf3d_asset *asset, int primitive_index, const unsigned char **src, unsigned long *src_size);
int gltf3d_mini_draco_decode_mesh(const unsigned char *data, unsigned long size, gltf3d_mini_draco_out *out, const gltf3d_mini_draco_callbacks *cb);
int gltf3d_mini_draco_decode_primitive(const gltf3d_asset *asset, int primitive_index, gltf3d_mini_draco_out *out, const gltf3d_mini_draco_callbacks *cb);
int gltf3d_mini_draco_attribute_id(const gltf3d_asset *asset, int primitive_index, int semantic);
unsigned long gltf3d_mini_draco_attribute_mask(const gltf3d_asset *asset, int primitive_index);
const char *gltf3d_mini_draco_semantic_name(int semantic);

#endif
