#include "model_image89.h"
#include "png89.h"
#include "bmp89.h"
#include "dds89.h"
#include "tga89.h"
#include "jpg89.h"
#include "tim89.h"
#include "tiff89.h"
#include "pcx89.h"

#include <stdio.h>
#include <string.h>

#ifndef MODELRT_IMAGE_ENABLE_PNG
#define MODELRT_IMAGE_ENABLE_PNG 1
#endif
#ifndef MODELRT_IMAGE_ENABLE_BMP
#define MODELRT_IMAGE_ENABLE_BMP 1
#endif
#ifndef MODELRT_IMAGE_ENABLE_DDS
#define MODELRT_IMAGE_ENABLE_DDS 1
#endif
#ifndef MODELRT_IMAGE_ENABLE_TGA
#define MODELRT_IMAGE_ENABLE_TGA 1
#endif
#ifndef MODELRT_IMAGE_ENABLE_JPG
#define MODELRT_IMAGE_ENABLE_JPG 1
#endif
#ifndef MODELRT_IMAGE_ENABLE_TIM
#define MODELRT_IMAGE_ENABLE_TIM 1
#endif
#ifndef MODELRT_IMAGE_ENABLE_TIFF
#define MODELRT_IMAGE_ENABLE_TIFF 1
#endif
#ifndef MODELRT_IMAGE_ENABLE_PCX
#define MODELRT_IMAGE_ENABLE_PCX 1
#endif

typedef int (*img89_probe_fn)(const unsigned char *, size_t);
typedef int (*img89_decode_fn)(const unsigned char *, size_t, unsigned char *, size_t,
                               unsigned int *, unsigned int *, size_t *);
typedef struct img89_decoder_s {
    modelrt_u32 bit;
    const char *name;
    img89_probe_fn probe;
    img89_decode_fn decode;
    int built;
    int space_error;
} img89_decoder;

static const img89_decoder g_img89[] = {
#if MODELRT_IMAGE_ENABLE_PCX
    { MODELRT_IMAGE_PCX, "PCX89", pcx89_probe, pcx89_decode_rgba, 1, -2 },
#endif
#if MODELRT_IMAGE_ENABLE_PNG
    { MODELRT_IMAGE_PNG, "PNG89", png89_probe, png89_decode_rgba, 1, -7 },
#endif
#if MODELRT_IMAGE_ENABLE_BMP
    { MODELRT_IMAGE_BMP, "BMP89", bmp89_probe, bmp89_decode_rgba, 1, -4 },
#endif
#if MODELRT_IMAGE_ENABLE_DDS
    { MODELRT_IMAGE_DDS, "DDS89", dds89_probe, dds89_decode_rgba, 1, -3 },
#endif
#if MODELRT_IMAGE_ENABLE_TGA
    { MODELRT_IMAGE_TGA, "TGA89", tga89_probe, tga89_decode_rgba, 1, -2 },
#endif
#if MODELRT_IMAGE_ENABLE_JPG
    { MODELRT_IMAGE_JPG, "JPG89", jpg89_probe, jpg89_decode_rgba, 1, -11 },
#endif
#if MODELRT_IMAGE_ENABLE_TIM
    { MODELRT_IMAGE_TIM, "TIM89", tim89_probe, tim89_decode_rgba, 1, -5 },
#endif
#if MODELRT_IMAGE_ENABLE_TIFF
    { MODELRT_IMAGE_TIFF, "TIFF89", tiff89_probe, tiff89_decode_rgba, 1, -7 },
#endif
    { 0u, "none", 0, 0, 0, 0 }
};

static modelrt_u32 img89_compiled_mask(void)
{
    modelrt_u32 m;
    unsigned int i;
    m = 0u;
    for (i = 0u; i < (unsigned int)(sizeof(g_img89) / sizeof(g_img89[0])); ++i)
        if (g_img89[i].built) m |= g_img89[i].bit;
    return m;
}

static const img89_decoder *img89_find(modelrt_u32 mask,
                                       const modelrt_u8 *src, size_t src_size)
{
    unsigned int i;
    for (i = 0u; i < (unsigned int)(sizeof(g_img89) / sizeof(g_img89[0])); ++i) {
        if (!g_img89[i].built || g_img89[i].probe == 0) continue;
        if ((mask & g_img89[i].bit) == 0u) continue;
        if (g_img89[i].probe(src, src_size)) return &g_img89[i];
    }
    return 0;
}

static void img89_info_base(modelrt_image_info *o, const img89_decoder *dec,
                            modelrt_u32 w, modelrt_u32 h, size_t used)
{
    memset(o, 0, sizeof(*o));
    o->decoder = dec->bit;
    strncpy(o->decoder_name, dec->name, sizeof(o->decoder_name) - 1u);
    o->decoder_name[sizeof(o->decoder_name) - 1u] = '\0';
    o->width = w;
    o->height = h;
    o->rgba_bytes = (modelrt_u32)used;
    o->subresource_count = 1u;
    o->mip_count = 1u;
    o->face_count = 1u;
}

static void img89_fill_native_info(const img89_decoder *dec,
                                   const modelrt_u8 *src, size_t src_size,
                                   modelrt_image_info *o)
{
#if MODELRT_IMAGE_ENABLE_PNG
    if (dec->bit == MODELRT_IMAGE_PNG) {
        png89_info pi;
        if (png89_get_info(src, src_size, &pi) == 0 && pi.interlace_method == 1u)
            o->flags |= MODELRT_IMAGE_FLAG_ADAM7;
        return;
    }
#endif
#if MODELRT_IMAGE_ENABLE_DDS
    if (dec->bit == MODELRT_IMAGE_DDS) {
        dds89_info di;
        if (dds89_get_info(src, src_size, &di) == 0) {
            o->mip_count = di.mip_count;
            o->face_count = di.face_count;
            o->subresource_count = di.subresource_count;
            if (di.flags & DDS89_FLAG_CUBEMAP) o->flags |= MODELRT_IMAGE_FLAG_CUBEMAP;
        }
        return;
    }
#endif
#if MODELRT_IMAGE_ENABLE_JPG
    if (dec->bit == MODELRT_IMAGE_JPG) {
        jpg89_info ji;
        if (jpg89_get_info(src, src_size, &ji) == 0 && ji.progressive)
            o->flags |= MODELRT_IMAGE_FLAG_PROGRESSIVE;
        return;
    }
#endif
#if MODELRT_IMAGE_ENABLE_TIM
    if (dec->bit == MODELRT_IMAGE_TIM) {
        tim89_info ti;
        if (tim89_get_info(src, src_size, &ti) == 0) {
            o->palette_count = ti.palette_count;
            o->subresource_count = ti.subresource_count;
            if (ti.flags & TIM89_FLAG_INDEXED) o->flags |= MODELRT_IMAGE_FLAG_INDEXED;
        }
        return;
    }
#endif
#if MODELRT_IMAGE_ENABLE_PCX
    if (dec->bit == MODELRT_IMAGE_PCX && src_size >= 128u && src[65] == 1u)
        o->flags |= MODELRT_IMAGE_FLAG_INDEXED;
#endif
#if MODELRT_IMAGE_ENABLE_TIFF
    if (dec->bit == MODELRT_IMAGE_TIFF) {
        tiff89_info ti;
        if (tiff89_get_info(src, src_size, &ti) == 0) {
            o->page_count = ti.page_count;
            o->subresource_count = ti.subresource_count;
            if (ti.flags & TIFF89_FLAG_MULTIPAGE) o->flags |= MODELRT_IMAGE_FLAG_MULTIPAGE;
            if (ti.flags & TIFF89_FLAG_TILED) o->flags |= MODELRT_IMAGE_FLAG_TILED;
            if (ti.flags & TIFF89_FLAG_PLANAR) o->flags |= MODELRT_IMAGE_FLAG_PLANAR;
        }
        return;
    }
#endif
}

void modelrt_runtime_set_image_decoder_mask(modelrt_runtime *rt, modelrt_u32 mask)
{
    if (rt == 0) return;
    rt->image_decoder_mask = mask & img89_compiled_mask();
}

modelrt_u32 modelrt_runtime_get_image_decoder_mask(const modelrt_runtime *rt)
{
    if (rt == 0) return 0u;
    return rt->image_decoder_mask;
}

int modelrt_image_decode_memory_mask(modelrt_u32 mask,
                                     const modelrt_u8 *src, size_t src_size,
                                     modelrt_u8 *dst, size_t dst_capacity,
                                     modelrt_image_info *out_info)
{
    const img89_decoder *dec;
    unsigned int w, h;
    size_t used;
    int rc;
    if (src == 0 || src_size == 0u || dst == 0 || out_info == 0)
        return MODELRT_ERR_BAD_ARG;
    dec = img89_find(mask, src, src_size);
    if (dec == 0) return MODELRT_ERR_UNSUPPORTED;
    w = h = 0u;
    used = 0u;
    rc = dec->decode(src, src_size, dst, dst_capacity, &w, &h, &used);
    if (rc != 0) return rc == dec->space_error ? MODELRT_ERR_NO_SPACE : MODELRT_ERR_CODEC;
    if (used > 0xffffffffu) return MODELRT_ERR_LIMIT;
    img89_info_base(out_info, dec, w, h, used);
    img89_fill_native_info(dec, src, src_size, out_info);
    return MODELRT_OK;
}

int modelrt_image_decode_memory(const modelrt_runtime *rt,
                                const modelrt_u8 *src, size_t src_size,
                                modelrt_u8 *dst, size_t dst_capacity,
                                modelrt_image_info *out_info)
{
    modelrt_u32 mask;
    mask = rt != 0 ? rt->image_decoder_mask : img89_compiled_mask();
    return modelrt_image_decode_memory_mask(mask, src, src_size, dst, dst_capacity, out_info);
}

static int img89_subinfo(const img89_decoder *dec,
                         const modelrt_u8 *src, size_t src_size,
                         modelrt_u32 sub, modelrt_image_subresource_info *o)
{
    memset(o, 0, sizeof(*o));
    o->index = sub;
#if MODELRT_IMAGE_ENABLE_DDS
    if (dec->bit == MODELRT_IMAGE_DDS) {
        dds89_subresource_info si;
        if (dds89_get_subresource_info(src, src_size, sub, &si) != 0)
            return MODELRT_ERR_BAD_ARG;
        o->kind = MODELRT_IMAGE_SUBRESOURCE_DDS_SURFACE;
        o->width = si.width; o->height = si.height;
        o->mip_level = si.mip_level; o->face_index = si.face_index;
        return MODELRT_OK;
    }
#endif
#if MODELRT_IMAGE_ENABLE_TIM
    if (dec->bit == MODELRT_IMAGE_TIM) {
        tim89_info ti;
        if (tim89_get_info(src, src_size, &ti) != 0 || sub >= ti.subresource_count)
            return MODELRT_ERR_BAD_ARG;
        o->kind = MODELRT_IMAGE_SUBRESOURCE_TIM_PALETTE;
        o->width = ti.width; o->height = ti.height; o->palette_index = sub;
        if (ti.flags & TIM89_FLAG_INDEXED) o->flags |= MODELRT_IMAGE_FLAG_INDEXED;
        return MODELRT_OK;
    }
#endif
#if MODELRT_IMAGE_ENABLE_TIFF
    if (dec->bit == MODELRT_IMAGE_TIFF) {
        tiff89_page_info pi;
        if (tiff89_get_page_info(src, src_size, sub, &pi) != 0)
            return MODELRT_ERR_BAD_ARG;
        o->kind = MODELRT_IMAGE_SUBRESOURCE_TIFF_PAGE;
        o->width = pi.width; o->height = pi.height; o->page_index = sub;
        if (pi.tiled) o->flags |= MODELRT_IMAGE_FLAG_TILED;
        if (pi.planar_configuration == 2u) o->flags |= MODELRT_IMAGE_FLAG_PLANAR;
        return MODELRT_OK;
    }
#endif
    if (sub != 0u) return MODELRT_ERR_BAD_ARG;
    o->kind = MODELRT_IMAGE_SUBRESOURCE_IMAGE;
#if MODELRT_IMAGE_ENABLE_PNG
    if (dec->bit == MODELRT_IMAGE_PNG) {
        png89_info pi;
        if (png89_get_info(src, src_size, &pi) == 0) {
            o->width = pi.width; o->height = pi.height;
            if (pi.interlace_method == 1u) o->flags |= MODELRT_IMAGE_FLAG_ADAM7;
        }
    }
#endif
#if MODELRT_IMAGE_ENABLE_JPG
    if (dec->bit == MODELRT_IMAGE_JPG) {
        jpg89_info ji;
        if (jpg89_get_info(src, src_size, &ji) == 0) {
            o->width = ji.width; o->height = ji.height;
            if (ji.progressive) o->flags |= MODELRT_IMAGE_FLAG_PROGRESSIVE;
        }
    }
#endif
    return MODELRT_OK;
}

int modelrt_image_subresource_info_memory(const modelrt_runtime *rt,
                                          const modelrt_u8 *src, size_t src_size,
                                          modelrt_u32 subresource_index,
                                          modelrt_image_subresource_info *out_info)
{
    modelrt_u32 mask;
    const img89_decoder *dec;
    if (src == 0 || src_size == 0u || out_info == 0) return MODELRT_ERR_BAD_ARG;
    mask = rt != 0 ? rt->image_decoder_mask : img89_compiled_mask();
    dec = img89_find(mask, src, src_size);
    if (dec == 0) return MODELRT_ERR_UNSUPPORTED;
    return img89_subinfo(dec, src, src_size, subresource_index, out_info);
}

int modelrt_image_decode_subresource_memory(const modelrt_runtime *rt,
                                            const modelrt_u8 *src, size_t src_size,
                                            modelrt_u32 subresource_index,
                                            modelrt_u8 *dst, size_t dst_capacity,
                                            modelrt_image_info *out_info,
                                            modelrt_image_subresource_info *out_subresource)
{
    modelrt_u32 mask;
    const img89_decoder *dec;
    unsigned int w, h;
    size_t used;
    int rc;
    if (src == 0 || src_size == 0u || dst == 0 || out_info == 0)
        return MODELRT_ERR_BAD_ARG;
    mask = rt != 0 ? rt->image_decoder_mask : img89_compiled_mask();
    dec = img89_find(mask, src, src_size);
    if (dec == 0) return MODELRT_ERR_UNSUPPORTED;
    w = h = 0u; used = 0u; rc = 0;
#if MODELRT_IMAGE_ENABLE_DDS
    if (dec->bit == MODELRT_IMAGE_DDS)
        rc = dds89_decode_subresource_rgba(src, src_size, subresource_index,
                                           dst, dst_capacity, &w, &h, &used);
    else
#endif
#if MODELRT_IMAGE_ENABLE_TIM
    if (dec->bit == MODELRT_IMAGE_TIM)
        rc = tim89_decode_subresource_rgba(src, src_size, subresource_index,
                                           dst, dst_capacity, &w, &h, &used);
    else
#endif
#if MODELRT_IMAGE_ENABLE_TIFF
    if (dec->bit == MODELRT_IMAGE_TIFF)
        rc = tiff89_decode_page_rgba(src, src_size, subresource_index,
                                     dst, dst_capacity, &w, &h, &used);
    else
#endif
    {
        if (subresource_index != 0u) return MODELRT_ERR_BAD_ARG;
        rc = dec->decode(src, src_size, dst, dst_capacity, &w, &h, &used);
    }
    if (rc != 0) return rc == dec->space_error ? MODELRT_ERR_NO_SPACE : MODELRT_ERR_CODEC;
    if (used > 0xffffffffu) return MODELRT_ERR_LIMIT;
    img89_info_base(out_info, dec, w, h, used);
    img89_fill_native_info(dec, src, src_size, out_info);
    if (out_subresource != 0) {
        rc = img89_subinfo(dec, src, src_size, subresource_index, out_subresource);
        if (rc != MODELRT_OK) return rc;
    }
    return MODELRT_OK;
}

static int img89_read_file(const char *path,
                           modelrt_u8 *encoded_scratch, size_t encoded_scratch_size,
                           size_t *out_size)
{
    FILE *f;
    long len;
    size_t got;
    if (path == 0 || encoded_scratch == 0 || encoded_scratch_size == 0u || out_size == 0)
        return MODELRT_ERR_BAD_ARG;
    f = fopen(path, "rb");
    if (f == 0) return MODELRT_ERR_IO;
    if (fseek(f, 0L, SEEK_END) != 0) { fclose(f); return MODELRT_ERR_IO; }
    len = ftell(f);
    if (len < 0L) { fclose(f); return MODELRT_ERR_IO; }
    if ((size_t)len > encoded_scratch_size) { fclose(f); return MODELRT_ERR_NO_SPACE; }
    if (fseek(f, 0L, SEEK_SET) != 0) { fclose(f); return MODELRT_ERR_IO; }
    got = fread(encoded_scratch, 1u, (size_t)len, f);
    fclose(f);
    if (got != (size_t)len) return MODELRT_ERR_IO;
    *out_size = got;
    return MODELRT_OK;
}

int modelrt_image_decode_file(const modelrt_runtime *rt, const char *path,
                              modelrt_u8 *encoded_scratch, size_t encoded_scratch_size,
                              modelrt_u8 *dst, size_t dst_capacity,
                              modelrt_image_info *out_info)
{
    size_t got;
    int rc;
    if (dst == 0 || out_info == 0) return MODELRT_ERR_BAD_ARG;
    rc = img89_read_file(path, encoded_scratch, encoded_scratch_size, &got);
    if (rc != MODELRT_OK) return rc;
    return modelrt_image_decode_memory(rt, encoded_scratch, got, dst, dst_capacity, out_info);
}

int modelrt_image_decode_subresource_file(const modelrt_runtime *rt, const char *path,
                                          modelrt_u32 subresource_index,
                                          modelrt_u8 *encoded_scratch, size_t encoded_scratch_size,
                                          modelrt_u8 *dst, size_t dst_capacity,
                                          modelrt_image_info *out_info,
                                          modelrt_image_subresource_info *out_subresource)
{
    size_t got;
    int rc;
    if (dst == 0 || out_info == 0) return MODELRT_ERR_BAD_ARG;
    rc = img89_read_file(path, encoded_scratch, encoded_scratch_size, &got);
    if (rc != MODELRT_OK) return rc;
    return modelrt_image_decode_subresource_memory(rt, encoded_scratch, got,
                                                   subresource_index, dst, dst_capacity,
                                                   out_info, out_subresource);
}
