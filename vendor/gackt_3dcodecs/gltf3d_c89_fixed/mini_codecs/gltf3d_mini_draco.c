#include "gltf3d_mini_draco.h"

int gltf3d_mini_draco_probe_stream(const unsigned char *data, unsigned long size, gltf3d_mini_draco_probe *probe)
{
    unsigned long i;
    if (!data || !probe) return GLTF3D_MINI_ERR_BAD_ARG;
    for (i = 0UL; i < sizeof(*probe); ++i) ((unsigned char *)probe)[i] = 0;
    probe->byte_count = size;
    if (size >= 8UL) {
        probe->likely_draco = 1;
        probe->encoder_type = (int)data[0];
        probe->geometry_type = (int)data[1];
        probe->bitstream_version_major = (int)data[2];
        probe->bitstream_version_minor = (int)data[3];
    } else if (size >= 2UL) {
        probe->likely_draco = 1;
    }
    return GLTF3D_MINI_OK;
}

int gltf3d_mini_draco_validate_primitive(const gltf3d_asset *asset, int primitive_index, const unsigned char **src, unsigned long *src_size)
{
    const gltf3d_primitive *p;
    const gltf3d_bufferview *bv;
    const gltf3d_buffer *b;
    unsigned long off;
    if (!asset || !src || !src_size) return GLTF3D_MINI_ERR_BAD_ARG;
    if (primitive_index < 0 || primitive_index >= asset->primitive_count) return GLTF3D_MINI_ERR_RANGE;
    p = &asset->primitives[primitive_index];
    if (!p->has_draco) return GLTF3D_MINI_ERR_UNSUPPORTED;
    if (p->draco_buffer_view < 0 || p->draco_buffer_view >= asset->buffer_view_count) return GLTF3D_MINI_ERR_RANGE;
    bv = &asset->buffer_views[p->draco_buffer_view];
    if (bv->buffer < 0 || bv->buffer >= asset->buffer_count) return GLTF3D_MINI_ERR_RANGE;
    b = &asset->buffers[bv->buffer];
    off = bv->byte_offset;
    if (!b->data || off > b->data_length || bv->byte_length > b->data_length - off) return GLTF3D_MINI_ERR_RANGE;
    *src = b->data + off;
    *src_size = bv->byte_length;
    return GLTF3D_MINI_OK;
}

int gltf3d_mini_draco_decode_mesh(const unsigned char *data, unsigned long size, gltf3d_mini_draco_out *out, const gltf3d_mini_draco_callbacks *cb)
{
    if (!data || !out) return GLTF3D_MINI_ERR_BAD_ARG;
    if (cb && cb->decode_mesh) return cb->decode_mesh(cb->user, data, size, out);
    return GLTF3D_MINI_ERR_NEED_EXTERNAL;
}

int gltf3d_mini_draco_decode_primitive(const gltf3d_asset *asset, int primitive_index, gltf3d_mini_draco_out *out, const gltf3d_mini_draco_callbacks *cb)
{
    const unsigned char *src;
    unsigned long src_size;
    int r;
    if (!out) return GLTF3D_MINI_ERR_BAD_ARG;
    r = gltf3d_mini_draco_validate_primitive(asset, primitive_index, &src, &src_size);
    if (r != GLTF3D_MINI_OK) return r;
    return gltf3d_mini_draco_decode_mesh(src, src_size, out, cb);
}

int gltf3d_mini_draco_require_supported_probe(const gltf3d_mini_draco_probe *probe)
{
    if (!probe) return GLTF3D_MINI_ERR_BAD_ARG;
    if (!probe->likely_draco || probe->byte_count < 8UL) return GLTF3D_MINI_ERR_BAD_DATA;
    /* glTF KHR_draco_mesh_compression is specified around Draco bitstream 2.2; other versions are left to external decoder policy. */
    if (probe->bitstream_version_major != 2) return GLTF3D_MINI_ERR_UNSUPPORTED;
    return GLTF3D_MINI_OK;
}


const char *gltf3d_mini_draco_semantic_name(int semantic)
{
    if (semantic == GLTF3D_ATTR_POSITION) return "POSITION";
    if (semantic == GLTF3D_ATTR_NORMAL) return "NORMAL";
    if (semantic == GLTF3D_ATTR_TANGENT) return "TANGENT";
    if (semantic == GLTF3D_ATTR_TEXCOORD_0) return "TEXCOORD_0";
    if (semantic == GLTF3D_ATTR_TEXCOORD_1) return "TEXCOORD_1";
    if (semantic == GLTF3D_ATTR_COLOR_0) return "COLOR_0";
    if (semantic == GLTF3D_ATTR_JOINTS_0) return "JOINTS_0";
    if (semantic == GLTF3D_ATTR_WEIGHTS_0) return "WEIGHTS_0";
    return "UNKNOWN";
}

int gltf3d_mini_draco_attribute_id(const gltf3d_asset *asset, int primitive_index, int semantic)
{
    const gltf3d_primitive *p;
    if (!asset || primitive_index < 0 || primitive_index >= asset->primitive_count) return -1;
    if (semantic < 0 || semantic >= GLTF3D_ATTR_COUNT) return -1;
    p = &asset->primitives[primitive_index];
    if (!p->has_draco) return -1;
    return p->draco_attributes[semantic];
}

unsigned long gltf3d_mini_draco_attribute_mask(const gltf3d_asset *asset, int primitive_index)
{
    const gltf3d_primitive *p;
    int i;
    unsigned long m;
    if (!asset || primitive_index < 0 || primitive_index >= asset->primitive_count) return 0UL;
    p = &asset->primitives[primitive_index];
    if (!p->has_draco) return 0UL;
    m = 0UL;
    for (i = 0; i < GLTF3D_ATTR_COUNT; ++i) if (p->draco_attributes[i] >= 0) m |= (1UL << i);
    return m;
}
