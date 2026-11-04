#include "re4c3d/re4_texture.h"
#include "re4c3d/re4_base.h"
#include <string.h>

static void re4_ps2_put_rgba(re4_u8 *dst, re4_u32 stride, re4_u32 x, re4_u32 y,
                             const re4_u8 rgba[4])
{
    re4_u8 *p;
    p = dst + ((re4_size)y * (re4_size)stride) + ((re4_size)x * 4UL);
    p[0] = rgba[0];
    p[1] = rgba[1];
    p[2] = rgba[2];
    p[3] = rgba[3];
}

re4_size re4_ps2_tpl_index_payload_size_for_header(const re4_ps2_tpl_image_header *h)
{
    re4_size pixels;
    if (h == 0) return 0UL;
    pixels = (re4_size)h->width * (re4_size)h->height;
    if (h->bitdepth == 0x0008U) return (pixels + 1UL) / 2UL;
    if (h->bitdepth == 0x0009U) return pixels;
    if (h->bitdepth == 0x0006U) return pixels * 4UL;
    return 0UL;
}

re4_size re4_ps2_tpl_palette_payload_size_for_header(const re4_ps2_tpl_image_header *h)
{
    if (h == 0) return 0UL;
    if (h->bitdepth == 0x0008U) return 0x80UL;
    if (h->bitdepth == 0x0009U) return 256UL * 4UL;
    return 0UL;
}

re4_u32 re4_ps2_tpl_palette_source_index(re4_u32 bitdepth, re4_u32 logical_index)
{
    static const re4_u8 order256[4] = { 0U, 2U, 1U, 3U };
    re4_u32 block;
    re4_u32 in_block;
    re4_u32 group;
    re4_u32 rem;
    if (bitdepth == 0x0008U) {
        if (logical_index >= 16U) return RE4_U32_MAX;
        if (logical_index < 8U) return logical_index;
        return 16U + (logical_index - 8U);
    }
    if (bitdepth == 0x0009U) {
        if (logical_index >= 256U) return RE4_U32_MAX;
        block = logical_index & ~31U;
        in_block = logical_index & 31U;
        group = in_block >> 3;
        rem = in_block & 7U;
        return block + ((re4_u32)order256[group] * 8U) + rem;
    }
    return logical_index;
}

re4_err re4_ps2_tpl_palette_entry_rgba32(re4_span palette, re4_u32 bitdepth,
                                         re4_u32 logical_index, re4_u8 out_rgba[4])
{
    re4_u32 src_index;
    re4_size off;
    re4_u32 a;
    if (out_rgba == 0) return RE4_ERR_NULL;
    src_index = re4_ps2_tpl_palette_source_index(bitdepth, logical_index);
    if (src_index == RE4_U32_MAX) return RE4_ERR_RANGE;
    off = (re4_size)src_index * 4UL;
    if (!re4_span_ok(palette, off, 4UL)) return RE4_ERR_RANGE;
    a = ((re4_u32)palette.data[off + 3UL] * 255U) / 128U;
    if (a > 255U) a = 255U;
    out_rgba[0] = palette.data[off + 0UL];
    out_rgba[1] = palette.data[off + 1UL];
    out_rgba[2] = palette.data[off + 2UL];
    out_rgba[3] = (re4_u8)a;
    return RE4_OK;
}

static re4_err re4_ps2_decode_check_output(const re4_ps2_tpl_image_header *h,
                                           re4_u8 *dst, re4_size dst_size,
                                           re4_u32 dst_stride)
{
    re4_size need;
    if (h == 0 || dst == 0) return RE4_ERR_NULL;
    if (h->width == 0U || h->height == 0U) return RE4_ERR_BAD_FORMAT;
    if (dst_stride < ((re4_u32)h->width * 4U)) return RE4_ERR_RANGE;
    need = ((re4_size)(h->height - 1U) * (re4_size)dst_stride) + ((re4_size)h->width * 4UL);
    if (dst_size < need) return RE4_ERR_NO_SPACE;
    return RE4_OK;
}

static re4_err re4_ps2_decode_bpp4(re4_span indexes, re4_span palette,
                                   const re4_ps2_tpl_image_header *h,
                                   re4_u8 *dst, re4_u32 stride)
{
    re4_u32 x;
    re4_u32 y;
    re4_size off;
    re4_err er;
    re4_u8 rgba[4];
    off = 0UL;
    for (y = 0U; y < (re4_u32)h->height; ++y) {
        for (x = 0U; x < (re4_u32)h->width; x += 2U) {
            re4_u8 b;
            re4_u32 left;
            re4_u32 right;
            if (!re4_span_ok(indexes, off, 1UL)) return RE4_ERR_RANGE;
            b = indexes.data[off++];
            left = (re4_u32)(b & 15U);
            right = (re4_u32)(b >> 4);
            er = re4_ps2_tpl_palette_entry_rgba32(palette, 0x0008U, left, rgba);
            if (er != RE4_OK) return er;
            re4_ps2_put_rgba(dst, stride, x, y, rgba);
            if (x + 1U < (re4_u32)h->width) {
                er = re4_ps2_tpl_palette_entry_rgba32(palette, 0x0008U, right, rgba);
                if (er != RE4_OK) return er;
                re4_ps2_put_rgba(dst, stride, x + 1U, y, rgba);
            }
        }
    }
    return RE4_OK;
}

static re4_err re4_ps2_decode_bpp8(re4_span indexes, re4_span palette,
                                   const re4_ps2_tpl_image_header *h,
                                   re4_u8 *dst, re4_u32 stride)
{
    re4_u32 x;
    re4_u32 y;
    re4_size off;
    re4_err er;
    re4_u8 rgba[4];
    off = 0UL;
    for (y = 0U; y < (re4_u32)h->height; ++y) {
        for (x = 0U; x < (re4_u32)h->width; ++x) {
            re4_u32 idx;
            if (!re4_span_ok(indexes, off, 1UL)) return RE4_ERR_RANGE;
            idx = (re4_u32)indexes.data[off++];
            er = re4_ps2_tpl_palette_entry_rgba32(palette, 0x0009U, idx, rgba);
            if (er != RE4_OK) return er;
            re4_ps2_put_rgba(dst, stride, x, y, rgba);
        }
    }
    return RE4_OK;
}

static re4_err re4_ps2_decode_rgba_raw(re4_span indexes,
                                       const re4_ps2_tpl_image_header *h,
                                       re4_u8 *dst, re4_u32 stride)
{
    re4_u32 x;
    re4_u32 y;
    re4_size off;
    re4_u8 rgba[4];
    off = 0UL;
    for (y = 0U; y < (re4_u32)h->height; ++y) {
        for (x = 0U; x < (re4_u32)h->width; ++x) {
            if (!re4_span_ok(indexes, off, 4UL)) return RE4_ERR_RANGE;
            rgba[0] = indexes.data[off + 0UL];
            rgba[1] = indexes.data[off + 1UL];
            rgba[2] = indexes.data[off + 2UL];
            rgba[3] = indexes.data[off + 3UL];
            re4_ps2_put_rgba(dst, stride, x, y, rgba);
            off += 4UL;
        }
    }
    return RE4_OK;
}

re4_err re4_ps2_tpl_decode_header_rgba32(re4_span src, const re4_ps2_tpl_image_header *h,
                                         re4_u8 *dst, re4_size dst_size, re4_u32 dst_stride)
{
    re4_err er;
    re4_span indexes;
    re4_span palette;
    re4_size need_indexes;
    re4_size need_palette;
    er = re4_ps2_decode_check_output(h, dst, dst_size, dst_stride);
    if (er != RE4_OK) return er;
    if ((h->interlace & RE4_PS2TPL_INTERLACE_SWIZZLE) != 0U) return RE4_ERR_UNSUPPORTED;
    er = re4_ps2_tpl_indexes_span(src, h, &indexes);
    if (er != RE4_OK) return er;
    need_indexes = re4_ps2_tpl_index_payload_size_for_header(h);
    if (need_indexes == 0UL || indexes.size < need_indexes) return RE4_ERR_BAD_FORMAT;
    if (h->bitdepth == 0x0008U || h->bitdepth == 0x0009U) {
        er = re4_ps2_tpl_palette_span(src, h, &palette);
        if (er != RE4_OK) return er;
        need_palette = re4_ps2_tpl_palette_payload_size_for_header(h);
        if (palette.size < need_palette) return RE4_ERR_BAD_FORMAT;
    } else {
        palette.data = 0;
        palette.size = 0;
    }
    if (h->bitdepth == 0x0008U) return re4_ps2_decode_bpp4(indexes, palette, h, dst, dst_stride);
    if (h->bitdepth == 0x0009U) return re4_ps2_decode_bpp8(indexes, palette, h, dst, dst_stride);
    if (h->bitdepth == 0x0006U) return re4_ps2_decode_rgba_raw(indexes, h, dst, dst_stride);
    return RE4_ERR_UNSUPPORTED;
}

re4_err re4_ps2_tpl_decode_image_rgba32(re4_span src, const re4_ps2_tpl *tpl, re4_u32 image_index,
                                        re4_u8 *dst, re4_size dst_size, re4_u32 dst_stride)
{
    if (tpl == 0) return RE4_ERR_NULL;
    if (image_index >= tpl->image_count) return RE4_ERR_RANGE;
    return re4_ps2_tpl_decode_header_rgba32(src, &tpl->images[image_index].main_header,
                                            dst, dst_size, dst_stride);
}
