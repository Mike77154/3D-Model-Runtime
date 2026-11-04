#include "gltf3d_mini_ktx2_basisu.h"

static unsigned long ktx_le32(const unsigned char *p)
{
    return ((unsigned long)p[0]) | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static void ktx_le64_pair(const unsigned char *p, unsigned long *lo, unsigned long *hi)
{
    *lo = ktx_le32(p);
    *hi = ktx_le32(p + 4UL);
}

static int is_ktx2_id(const unsigned char *p)
{
    static const unsigned char id[12] = {0xab,0x4b,0x54,0x58,0x20,0x32,0x30,0xbb,0x0d,0x0a,0x1a,0x0a};
    int i;
    for (i = 0; i < 12; ++i) if (p[i] != id[i]) return 0;
    return 1;
}

static void ktx_zero(void *p, unsigned long n)
{
    unsigned long i;
    for (i = 0UL; i < n; ++i) ((unsigned char *)p)[i] = 0;
}

static int range_ok32(unsigned long off, unsigned long len, unsigned long size)
{
    if (off == 0UL && len == 0UL) return 1;
    if (off > size) return 0;
    if (len > size - off) return 0;
    return 1;
}

static int ktx_parse_dfd(const unsigned char *data, unsigned long size, gltf3d_mini_ktx2_info *info)
{
    const unsigned char *p;
    unsigned long off;
    unsigned long len;
    unsigned long total_size;
    int i;
    off = info->dfd_byte_offset;
    len = info->dfd_byte_length;
    if (len == 0UL) return GLTF3D_MINI_OK;
    if (!range_ok32(off, len, size)) return GLTF3D_MINI_ERR_RANGE;
    if (len < 24UL) return GLTF3D_MINI_ERR_BAD_DATA;
    p = data + off;
    total_size = ktx_le32(p);
    if (total_size > len || total_size < 24UL) return GLTF3D_MINI_ERR_BAD_DATA;
    info->dfd_vendor_id = (unsigned long)p[4] | ((unsigned long)p[5] << 8);
    info->dfd_descriptor_type = (unsigned long)p[6] | ((unsigned long)p[7] << 8);
    info->dfd_version_number = (unsigned long)p[8] | ((unsigned long)p[9] << 8);
    info->dfd_color_model = (unsigned long)p[12];
    info->dfd_color_primaries = (unsigned long)p[13];
    info->dfd_transfer_function = (unsigned long)p[14];
    info->dfd_flags = (unsigned long)p[15];
    for (i = 0; i < 4; ++i) info->dfd_texel_block_dimension[i] = (unsigned long)p[16 + i] + 1UL;
    for (i = 0; i < 8; ++i) info->dfd_bytes_plane[i] = (unsigned long)p[20 + i];
    if (info->dfd_color_model == 163UL) info->is_etc1s_basislz = 1;
    if (info->dfd_color_model == 166UL) info->is_uastc_basislz = 1;
    return GLTF3D_MINI_OK;
}

static int ktx_parse_kvd(const unsigned char *data, unsigned long size, gltf3d_mini_ktx2_info *info)
{
    unsigned long p;
    unsigned long end;
    unsigned long kvs;
    if (info->kvd_byte_length == 0UL) return GLTF3D_MINI_OK;
    if (!range_ok32(info->kvd_byte_offset, info->kvd_byte_length, size)) return GLTF3D_MINI_ERR_RANGE;
    p = info->kvd_byte_offset;
    end = info->kvd_byte_offset + info->kvd_byte_length;
    while (p + 4UL <= end) {
        kvs = ktx_le32(data + p);
        p += 4UL;
        if (kvs > end - p) return GLTF3D_MINI_ERR_BAD_DATA;
        info->kv_pair_count++;
        p += kvs;
        while ((p & 3UL) != 0UL && p < end) p++;
    }
    if (p != end) return GLTF3D_MINI_ERR_BAD_DATA;
    return GLTF3D_MINI_OK;
}

int gltf3d_mini_ktx2_validate_ranges(const unsigned char *data, unsigned long size, const gltf3d_mini_ktx2_info *info)
{
    unsigned long i;
    (void)data;
    if (!info) return GLTF3D_MINI_ERR_BAD_ARG;
    if (!range_ok32(info->dfd_byte_offset, info->dfd_byte_length, size)) return GLTF3D_MINI_ERR_RANGE;
    if (!range_ok32(info->kvd_byte_offset, info->kvd_byte_length, size)) return GLTF3D_MINI_ERR_RANGE;
    if (info->sgd_byte_offset_hi || info->sgd_byte_length_hi) return GLTF3D_MINI_ERR_RANGE;
    if (!range_ok32(info->sgd_byte_offset, info->sgd_byte_length, size)) return GLTF3D_MINI_ERR_RANGE;
    for (i = 0UL; i < info->level_index_count; ++i) {
        if (info->levels[i].byte_offset_hi || info->levels[i].byte_length_hi) return GLTF3D_MINI_ERR_RANGE;
        if (!range_ok32(info->levels[i].byte_offset, info->levels[i].byte_length, size)) return GLTF3D_MINI_ERR_RANGE;
    }
    return GLTF3D_MINI_OK;
}

int gltf3d_mini_ktx2_probe(const unsigned char *data, unsigned long size, gltf3d_mini_ktx2_info *info)
{
    unsigned long nlevels;
    unsigned long i;
    unsigned long p;
    int r;
    if (!data || !info) return GLTF3D_MINI_ERR_BAD_ARG;
    if (size < 80UL) return GLTF3D_MINI_ERR_BAD_DATA;
    if (!is_ktx2_id(data)) return GLTF3D_MINI_ERR_BAD_DATA;
    ktx_zero(info, (unsigned long)sizeof(*info));
    info->vk_format = ktx_le32(data + 12UL);
    info->type_size = ktx_le32(data + 16UL);
    info->pixel_width = ktx_le32(data + 20UL);
    info->pixel_height = ktx_le32(data + 24UL);
    info->pixel_depth = ktx_le32(data + 28UL);
    info->layer_count = ktx_le32(data + 32UL);
    info->face_count = ktx_le32(data + 36UL);
    info->level_count = ktx_le32(data + 40UL);
    info->supercompression_scheme = ktx_le32(data + 44UL);
    info->dfd_byte_offset = ktx_le32(data + 48UL);
    info->dfd_byte_length = ktx_le32(data + 52UL);
    info->kvd_byte_offset = ktx_le32(data + 56UL);
    info->kvd_byte_length = ktx_le32(data + 60UL);
    ktx_le64_pair(data + 64UL, &info->sgd_byte_offset, &info->sgd_byte_offset_hi);
    ktx_le64_pair(data + 72UL, &info->sgd_byte_length, &info->sgd_byte_length_hi);
    if (info->pixel_width == 0UL) return GLTF3D_MINI_ERR_BAD_DATA;
    if (info->pixel_height == 0UL) info->pixel_height = 1UL;
    if (info->pixel_depth == 0UL) info->pixel_depth = 1UL;
    if (info->face_count != 1UL && info->face_count != 6UL) return GLTF3D_MINI_ERR_BAD_DATA;
    if (info->level_count == 0UL) nlevels = 1UL;
    else nlevels = info->level_count;
    if (nlevels > GLTF3D_MINI_KTX2_MAX_LEVELS) info->level_index_count = GLTF3D_MINI_KTX2_MAX_LEVELS;
    else info->level_index_count = nlevels;
    if (80UL + nlevels * 24UL > size) return GLTF3D_MINI_ERR_BAD_DATA;
    p = 80UL;
    for (i = 0UL; i < info->level_index_count; ++i) {
        ktx_le64_pair(data + p + 0UL, &info->levels[i].byte_offset, &info->levels[i].byte_offset_hi);
        ktx_le64_pair(data + p + 8UL, &info->levels[i].byte_length, &info->levels[i].byte_length_hi);
        ktx_le64_pair(data + p + 16UL, &info->levels[i].uncompressed_byte_length, &info->levels[i].uncompressed_byte_length_hi);
        p += 24UL;
    }
    if (info->sgd_byte_offset_hi || info->sgd_byte_length_hi) info->flags |= GLTF3D_MINI_KTX2_FLAG_SGD_64BIT;
    if (info->supercompression_scheme == GLTF3D_MINI_KTX2_SUPER_BASIS_LZ && info->vk_format == 0UL) {
        info->is_etc1s_basislz = 1;
        info->is_uastc_basislz = 1;
    }
    r = gltf3d_mini_ktx2_validate_ranges(data, size, info);
    if (r != GLTF3D_MINI_OK) return r;
    r = ktx_parse_dfd(data, size, info);
    if (r != GLTF3D_MINI_OK) return r;
    r = ktx_parse_kvd(data, size, info);
    if (r != GLTF3D_MINI_OK) return r;
    return GLTF3D_MINI_OK;
}

int gltf3d_mini_ktx2_basisu_transcode_rgba8(const unsigned char *data, unsigned long size, unsigned char *dst_rgba, unsigned long dst_size, unsigned long *w, unsigned long *h, const gltf3d_mini_basisu_callbacks *cb)
{
    gltf3d_mini_ktx2_info info;
    int r;
    if (!data || !dst_rgba || !w || !h) return GLTF3D_MINI_ERR_BAD_ARG;
    r = gltf3d_mini_ktx2_probe(data, size, &info);
    if (r != GLTF3D_MINI_OK) return r;
    *w = info.pixel_width;
    *h = info.pixel_height;
    if (cb && cb->transcode_rgba8) return cb->transcode_rgba8(cb->user, data, size, dst_rgba, dst_size, w, h);
    return GLTF3D_MINI_ERR_NEED_EXTERNAL;
}

static int ktx_copy_rgba_swizzle(const unsigned char *src, unsigned long pixels, unsigned char *dst, int bgra)
{
    unsigned long i;
    for (i = 0UL; i < pixels; ++i) {
        if (bgra) {
            dst[i * 4UL + 0UL] = src[i * 4UL + 2UL];
            dst[i * 4UL + 1UL] = src[i * 4UL + 1UL];
            dst[i * 4UL + 2UL] = src[i * 4UL + 0UL];
            dst[i * 4UL + 3UL] = src[i * 4UL + 3UL];
        } else {
            dst[i * 4UL + 0UL] = src[i * 4UL + 0UL];
            dst[i * 4UL + 1UL] = src[i * 4UL + 1UL];
            dst[i * 4UL + 2UL] = src[i * 4UL + 2UL];
            dst[i * 4UL + 3UL] = src[i * 4UL + 3UL];
        }
    }
    return GLTF3D_MINI_OK;
}

int gltf3d_mini_ktx2_decode_rgba8_uncompressed(const unsigned char *data, unsigned long size, unsigned char *dst_rgba, unsigned long dst_size, unsigned long *w, unsigned long *h)
{
    gltf3d_mini_ktx2_info info;
    unsigned long pixels;
    unsigned long need;
    int bgra;
    int r;
    if (!data || !dst_rgba || !w || !h) return GLTF3D_MINI_ERR_BAD_ARG;
    r = gltf3d_mini_ktx2_probe(data, size, &info);
    if (r != GLTF3D_MINI_OK) return r;
    if (info.supercompression_scheme != GLTF3D_MINI_KTX2_SUPER_NONE) return GLTF3D_MINI_ERR_UNSUPPORTED;
    if (info.level_index_count < 1UL) return GLTF3D_MINI_ERR_BAD_DATA;
    if (info.pixel_depth != 1UL || info.layer_count > 1UL || info.face_count != 1UL) return GLTF3D_MINI_ERR_UNSUPPORTED;
    bgra = 0;
    /* Vulkan values: R8G8B8A8_UNORM=37, R8G8B8A8_SRGB=43, B8G8R8A8_UNORM=44, B8G8R8A8_SRGB=50. */
    if (info.vk_format == 37UL || info.vk_format == 43UL) bgra = 0;
    else if (info.vk_format == 44UL || info.vk_format == 50UL) bgra = 1;
    else return GLTF3D_MINI_ERR_UNSUPPORTED;
    pixels = info.pixel_width * info.pixel_height;
    if (info.pixel_width == 0UL || pixels / info.pixel_width != info.pixel_height) return GLTF3D_MINI_ERR_RANGE;
    need = pixels * 4UL;
    if (need / 4UL != pixels) return GLTF3D_MINI_ERR_RANGE;
    if (dst_size < need) return GLTF3D_MINI_ERR_RANGE;
    if (info.levels[0].byte_offset_hi || info.levels[0].byte_length_hi) return GLTF3D_MINI_ERR_RANGE;
    if (info.levels[0].byte_length < need) return GLTF3D_MINI_ERR_BAD_DATA;
    if (info.levels[0].byte_offset > size || need > size - info.levels[0].byte_offset) return GLTF3D_MINI_ERR_RANGE;
    r = ktx_copy_rgba_swizzle(data + info.levels[0].byte_offset, pixels, dst_rgba, bgra);
    *w = info.pixel_width;
    *h = info.pixel_height;
    return r;
}


int gltf3d_mini_ktx2_basisu_kind(const gltf3d_mini_ktx2_info *info)
{
    if (!info) return GLTF3D_MINI_BASISU_KIND_NONE;
    if (info->supercompression_scheme == GLTF3D_MINI_KTX2_SUPER_NONE) {
        if (info->vk_format == 37UL || info->vk_format == 43UL || info->vk_format == 44UL || info->vk_format == 50UL) return GLTF3D_MINI_BASISU_KIND_UNCOMPRESSED_RGBA8;
        return GLTF3D_MINI_BASISU_KIND_NONE;
    }
    if (info->supercompression_scheme == GLTF3D_MINI_KTX2_SUPER_BASIS_LZ) {
        if (info->is_uastc_basislz && !info->is_etc1s_basislz) return GLTF3D_MINI_BASISU_KIND_UASTC;
        if (info->is_etc1s_basislz && !info->is_uastc_basislz) return GLTF3D_MINI_BASISU_KIND_ETC1S;
        if (info->dfd_color_model == 166UL) return GLTF3D_MINI_BASISU_KIND_UASTC;
        return GLTF3D_MINI_BASISU_KIND_ETC1S;
    }
    return GLTF3D_MINI_BASISU_KIND_NONE;
}

const char *gltf3d_mini_ktx2_basisu_kind_name(int kind)
{
    if (kind == GLTF3D_MINI_BASISU_KIND_ETC1S) return "ETC1S/BasisLZ";
    if (kind == GLTF3D_MINI_BASISU_KIND_UASTC) return "UASTC";
    if (kind == GLTF3D_MINI_BASISU_KIND_UNCOMPRESSED_RGBA8) return "uncompressed RGBA8/BGRA8";
    return "none/unsupported";
}

int gltf3d_mini_ktx2_transcode_plan_rgba8(const unsigned char *data, unsigned long size, unsigned long *w, unsigned long *h, int *kind)
{
    gltf3d_mini_ktx2_info info;
    int r;
    if (!data || !w || !h || !kind) return GLTF3D_MINI_ERR_BAD_ARG;
    r = gltf3d_mini_ktx2_probe(data, size, &info);
    if (r != GLTF3D_MINI_OK) return r;
    *w = info.pixel_width;
    *h = info.pixel_height;
    *kind = gltf3d_mini_ktx2_basisu_kind(&info);
    if (*kind == GLTF3D_MINI_BASISU_KIND_NONE) return GLTF3D_MINI_ERR_UNSUPPORTED;
    return GLTF3D_MINI_OK;
}
