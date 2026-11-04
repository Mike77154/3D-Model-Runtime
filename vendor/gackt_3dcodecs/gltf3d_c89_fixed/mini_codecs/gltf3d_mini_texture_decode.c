#include "gltf3d_mini_texture_decode.h"
#include "gltf3d_mini_pngdec.h"
#include "gltf3d_mini_jpegdec.h"
#include "gltf3d_mini_ktx2_basisu.h"

static int streq_c(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *b) { if (*a != *b) return 0; ++a; ++b; }
    return *a == 0 && *b == 0;
}

static void zero_result(gltf3d_mini_texture_result *r)
{
    unsigned long i;
    if (!r) return;
    for (i = 0UL; i < sizeof(*r); ++i) ((unsigned char *)r)[i] = 0U;
}

int gltf3d_mini_texture_get_image_bytes(const gltf3d_asset *asset, int image_index,
                                        const unsigned char **data, unsigned long *size,
                                        int *source_kind,
                                        const gltf3d_mini_texture_callbacks *cb)
{
    const gltf3d_image *im;
    const gltf3d_bufferview *bv;
    const gltf3d_buffer *b;
    unsigned long off;
    if (!asset || !data || !size || !source_kind) return GLTF3D_MINI_ERR_BAD_ARG;
    if (image_index < 0 || image_index >= asset->image_count) return GLTF3D_MINI_ERR_RANGE;
    im = &asset->images[image_index];
    if (im->buffer_view >= 0) {
        if (im->buffer_view >= asset->buffer_view_count) return GLTF3D_MINI_ERR_RANGE;
        bv = &asset->buffer_views[im->buffer_view];
        if (bv->has_meshopt) return GLTF3D_MINI_ERR_UNSUPPORTED;
        if (bv->buffer < 0 || bv->buffer >= asset->buffer_count) return GLTF3D_MINI_ERR_RANGE;
        b = &asset->buffers[bv->buffer];
        off = bv->byte_offset;
        if (!b->data || off > b->data_length || bv->byte_length > b->data_length - off) return GLTF3D_MINI_ERR_RANGE;
        *data = b->data + off;
        *size = bv->byte_length;
        *source_kind = GLTF3D_MINI_TEX_SRC_BUFFER_VIEW;
        return GLTF3D_MINI_OK;
    }
    if (im->uri[0]) {
        if (cb && cb->load_uri) {
            int r;
            r = cb->load_uri(cb->user, im->uri, data, size);
            if (r != 0) return GLTF3D_MINI_ERR_BAD_DATA;
            *source_kind = GLTF3D_MINI_TEX_SRC_URI;
            return GLTF3D_MINI_OK;
        }
        return GLTF3D_MINI_ERR_NEED_EXTERNAL;
    }
    return GLTF3D_MINI_ERR_BAD_DATA;
}

int gltf3d_mini_texture_guess_format(const unsigned char *data, unsigned long size, const char *mime_type)
{
    if (mime_type && mime_type[0]) {
        if (streq_c(mime_type, "image/png")) return GLTF3D_MINI_TEX_FMT_PNG;
        if (streq_c(mime_type, "image/jpeg")) return GLTF3D_MINI_TEX_FMT_JPEG;
        if (streq_c(mime_type, "image/ktx2") || streq_c(mime_type, "image/ktx")) return GLTF3D_MINI_TEX_FMT_KTX2;
    }
    if (data && size >= 8UL && data[0] == 0x89U && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') return GLTF3D_MINI_TEX_FMT_PNG;
    if (data && size >= 2UL && data[0] == 0xffU && data[1] == 0xd8U) return GLTF3D_MINI_TEX_FMT_JPEG;
    if (data && size >= 12UL && data[0] == 0xabU && data[1] == 'K' && data[2] == 'T' && data[3] == 'X' && data[4] == ' ' && data[5] == '2') return GLTF3D_MINI_TEX_FMT_KTX2;
    return GLTF3D_MINI_TEX_FMT_UNKNOWN;
}

int gltf3d_mini_decode_image_rgba8(const unsigned char *data, unsigned long size, const char *mime_type,
                                   unsigned char *dst_rgba, unsigned long dst_size,
                                   unsigned char *work, unsigned long work_size,
                                   unsigned long *w, unsigned long *h,
                                   int *format,
                                   const gltf3d_mini_texture_callbacks *cb)
{
    int fmt;
    if (!data || !dst_rgba || !w || !h || !format) return GLTF3D_MINI_ERR_BAD_ARG;
    fmt = gltf3d_mini_texture_guess_format(data, size, mime_type);
    *format = fmt;
    if (fmt == GLTF3D_MINI_TEX_FMT_PNG) {
        return gltf3d_mini_png_decode_rgba8_work(data, size, dst_rgba, dst_size, work, work_size, w, h);
    }
    if (fmt == GLTF3D_MINI_TEX_FMT_JPEG) {
        gltf3d_mini_jpegdec_result jr;
        int r;
        r = gltf3d_mini_jpeg_decode_rgba8(data, size, dst_rgba, dst_size, work, work_size, &jr);
        *w = jr.width;
        *h = jr.height;
        return r;
    }
    if (fmt == GLTF3D_MINI_TEX_FMT_KTX2) {
        int r;
        r = gltf3d_mini_ktx2_decode_rgba8_uncompressed(data, size, dst_rgba, dst_size, w, h);
        if (r == GLTF3D_MINI_OK) return r;
        if (cb && cb->basisu_transcode_rgba8) {
            gltf3d_mini_basisu_callbacks bc;
            bc.user = cb->user;
            bc.transcode_rgba8 = cb->basisu_transcode_rgba8;
            return gltf3d_mini_ktx2_basisu_transcode_rgba8(data, size, dst_rgba, dst_size, w, h, &bc);
        }
        return r;
    }
    return GLTF3D_MINI_ERR_UNSUPPORTED;
}

int gltf3d_mini_decode_texture_rgba8(const gltf3d_asset *asset, int texture_index,
                                     unsigned char *dst_rgba, unsigned long dst_size,
                                     unsigned char *work, unsigned long work_size,
                                     gltf3d_mini_texture_result *result,
                                     const gltf3d_mini_texture_callbacks *cb)
{
    const gltf3d_texture *tx;
    const gltf3d_image *im;
    const unsigned char *data;
    unsigned long size;
    unsigned long w;
    unsigned long h;
    int source_kind;
    int image_index;
    int fmt;
    int r;
    if (!asset || !dst_rgba || !result || texture_index < 0 || texture_index >= asset->texture_count) return GLTF3D_MINI_ERR_BAD_ARG;
    zero_result(result);
    tx = &asset->textures[texture_index];
    image_index = (tx->basisu_source >= 0) ? tx->basisu_source : tx->source;
    if (image_index < 0 || image_index >= asset->image_count) return GLTF3D_MINI_ERR_RANGE;
    im = &asset->images[image_index];
    r = gltf3d_mini_texture_get_image_bytes(asset, image_index, &data, &size, &source_kind, cb);
    if (r != GLTF3D_MINI_OK) return r;
    r = gltf3d_mini_decode_image_rgba8(data, size, im->mime_type, dst_rgba, dst_size, work, work_size, &w, &h, &fmt, cb);
    if (r != GLTF3D_MINI_OK) return r;
    result->width = w;
    result->height = h;
    result->source_kind = source_kind;
    result->format = fmt;
    result->used_external_uri = (source_kind == GLTF3D_MINI_TEX_SRC_URI) ? 1 : 0;
    result->used_basisu_callback = (tx->basisu_source >= 0 && fmt == GLTF3D_MINI_TEX_FMT_KTX2) ? 1 : 0;
    return GLTF3D_MINI_OK;
}
