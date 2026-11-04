#include "gltf3d_mini_images.h"
#include "gltf3d_mini_pngdec.h"
#include "gltf3d_mini_jpegparse.h"
#include "gltf3d_mini_jpegdec.h"

static unsigned long be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | (unsigned long)p[3];
}


int gltf3d_mini_png_probe(const unsigned char *data, unsigned long size, gltf3d_mini_image_info *info)
{
    static const unsigned char sig[8] = {137,80,78,71,13,10,26,10};
    int i;
    unsigned char ct;
    if (!data || !info) return GLTF3D_MINI_ERR_BAD_ARG;
    if (size < 33UL) return GLTF3D_MINI_ERR_BAD_DATA;
    for (i = 0; i < 8; ++i) if (data[i] != sig[i]) return GLTF3D_MINI_ERR_BAD_DATA;
    if (be32(data + 8) != 13UL) return GLTF3D_MINI_ERR_BAD_DATA;
    if (data[12] != 'I' || data[13] != 'H' || data[14] != 'D' || data[15] != 'R') return GLTF3D_MINI_ERR_BAD_DATA;
    info->width = be32(data + 16);
    info->height = be32(data + 20);
    info->bits_per_channel = (int)data[24];
    ct = data[25];
    info->channels = 0;
    if (ct == 0) info->channels = 1;
    else if (ct == 2) info->channels = 3;
    else if (ct == 3) info->channels = 1;
    else if (ct == 4) info->channels = 2;
    else if (ct == 6) info->channels = 4;
    info->format = (info->channels == 4) ? GLTF3D_MINI_RGBA8 : ((info->channels == 3) ? GLTF3D_MINI_RGB8 : GLTF3D_MINI_GRAY8);
    info->is_png = 1;
    info->is_jpeg = 0;
    return GLTF3D_MINI_OK;
}

int gltf3d_mini_jpeg_probe(const unsigned char *data, unsigned long size, gltf3d_mini_image_info *info)
{
    gltf3d_mini_jpeg_info ji;
    int r;
    if (!data || !info) return GLTF3D_MINI_ERR_BAD_ARG;
    r = gltf3d_mini_jpeg_parse(data, size, &ji);
    if (r != GLTF3D_MINI_OK) return r;
    info->bits_per_channel = ji.precision;
    info->height = ji.height;
    info->width = ji.width;
    info->channels = ji.component_count;
    info->format = (info->channels == 3) ? GLTF3D_MINI_RGB8 : GLTF3D_MINI_GRAY8;
    info->is_png = 0;
    info->is_jpeg = 1;
    return GLTF3D_MINI_OK;
}

int gltf3d_mini_image_probe(const unsigned char *data, unsigned long size, gltf3d_mini_image_info *info)
{
    int r;
    r = gltf3d_mini_png_probe(data, size, info);
    if (r == GLTF3D_MINI_OK) return r;
    return gltf3d_mini_jpeg_probe(data, size, info);
}

int gltf3d_mini_image_decode_rgba8(const unsigned char *data, unsigned long size, unsigned char *dst_rgba, unsigned long dst_size, unsigned long *w, unsigned long *h, const gltf3d_mini_image_callbacks *cb)
{
    gltf3d_mini_image_info info;
    int r;
    if (!data || !dst_rgba || !w || !h) return GLTF3D_MINI_ERR_BAD_ARG;
    r = gltf3d_mini_image_probe(data, size, &info);
    if (r != GLTF3D_MINI_OK) return r;
    if (info.is_png) {
        if (cb && cb->decode_png_rgba8) return cb->decode_png_rgba8(cb->user, data, size, dst_rgba, dst_size, w, h);
        *w = info.width; *h = info.height;
        return GLTF3D_MINI_ERR_NEED_EXTERNAL;
    }
    if (info.is_jpeg) {
        if (cb && cb->decode_jpeg_rgba8) return cb->decode_jpeg_rgba8(cb->user, data, size, dst_rgba, dst_size, w, h);
        *w = info.width; *h = info.height;
        return GLTF3D_MINI_ERR_NEED_EXTERNAL;
    }
    return GLTF3D_MINI_ERR_UNSUPPORTED;
}

int gltf3d_mini_image_decode_rgba8_work(const unsigned char *data, unsigned long size, unsigned char *dst_rgba, unsigned long dst_size, unsigned char *work, unsigned long work_size, unsigned long *w, unsigned long *h)
{
    gltf3d_mini_image_info info;
    int r;
    if (!data || !dst_rgba || !work || !w || !h) return GLTF3D_MINI_ERR_BAD_ARG;
    r = gltf3d_mini_image_probe(data, size, &info);
    if (r != GLTF3D_MINI_OK) return r;
    if (info.is_png) return gltf3d_mini_png_decode_rgba8_work(data, size, dst_rgba, dst_size, work, work_size, w, h);
    if (info.is_jpeg) {
        gltf3d_mini_jpegdec_result jr;
        r = gltf3d_mini_jpeg_decode_rgba8(data, size, dst_rgba, dst_size, work, work_size, &jr);
        if (r == GLTF3D_MINI_OK) { *w = jr.width; *h = jr.height; }
        return r;
    }
    *w = info.width;
    *h = info.height;
    return GLTF3D_MINI_ERR_NEED_EXTERNAL;
}
