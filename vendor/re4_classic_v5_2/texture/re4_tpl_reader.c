#include "re4c3d/re4_texture.h"
#include "re4c3d/re4_base.h"
#include <string.h>
#include <stdio.h>

static re4_u8 re4_expand4(re4_u8 v)
{
    return (re4_u8)((v << 4) | v);
}

static re4_u8 re4_expand5(re4_u32 v)
{
    return (re4_u8)((v << 3) | (v >> 2));
}

static re4_u8 re4_expand6(re4_u32 v)
{
    return (re4_u8)((v << 2) | (v >> 4));
}

static void re4_put_rgba(re4_u8 *dst, re4_u32 stride, re4_u32 x, re4_u32 y,
                         re4_u8 r, re4_u8 g, re4_u8 b, re4_u8 a)
{
    re4_u8 *p;
    p = dst + ((re4_size)y * (re4_size)stride) + ((re4_size)x * 4UL);
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = a;
}

static void re4_rgb565_to_rgba(re4_u16 v, re4_u8 *rgba)
{
    rgba[0] = re4_expand5((v >> 11) & 31U);
    rgba[1] = re4_expand6((v >> 5) & 63U);
    rgba[2] = re4_expand5(v & 31U);
    rgba[3] = 255;
}

static void re4_rgb5a3_to_rgba(re4_u16 v, re4_u8 *rgba)
{
    if ((v & 0x8000U) != 0) {
        rgba[0] = re4_expand5((v >> 10) & 31U);
        rgba[1] = re4_expand5((v >> 5) & 31U);
        rgba[2] = re4_expand5(v & 31U);
        rgba[3] = 255;
    } else {
        rgba[3] = (re4_u8)((((v >> 12) & 7U) * 255U) / 7U);
        rgba[0] = re4_expand4((re4_u8)((v >> 8) & 15U));
        rgba[1] = re4_expand4((re4_u8)((v >> 4) & 15U));
        rgba[2] = re4_expand4((re4_u8)(v & 15U));
    }
}

static re4_err re4_palette_lookup(re4_span src, const re4_tpl_image *img, re4_u32 idx, re4_u8 *rgba)
{
    re4_u16 v;
    if (idx >= img->palette_items) return RE4_ERR_RANGE;
    if (!re4_span_ok(src, (re4_size)img->palette_data_offset + ((re4_size)idx * 2UL), 2)) return RE4_ERR_RANGE;
    v = re4_read_u16_be(src.data + img->palette_data_offset + ((re4_size)idx * 2UL));
    if (img->palette_format == RE4_TPL_PAL_IA8) {
        rgba[0] = (re4_u8)(v & 0xFFU);
        rgba[1] = (re4_u8)(v & 0xFFU);
        rgba[2] = (re4_u8)(v & 0xFFU);
        rgba[3] = (re4_u8)(v >> 8);
        return RE4_OK;
    }
    if (img->palette_format == RE4_TPL_PAL_RGB565) {
        re4_rgb565_to_rgba(v, rgba);
        return RE4_OK;
    }
    if (img->palette_format == RE4_TPL_PAL_RGB5A3) {
        re4_rgb5a3_to_rgba(v, rgba);
        return RE4_OK;
    }
    return RE4_ERR_UNSUPPORTED;
}

const char *re4_tpl_format_name(re4_u32 fmt)
{
    switch (fmt) {
    case RE4_TPL_I4: return "I4";
    case RE4_TPL_I8: return "I8";
    case RE4_TPL_IA4: return "IA4";
    case RE4_TPL_IA8: return "IA8";
    case RE4_TPL_RGB565: return "RGB565";
    case RE4_TPL_RGB5A3: return "RGB5A3";
    case RE4_TPL_RGBA8: return "RGBA8";
    case RE4_TPL_CI4: return "CI4";
    case RE4_TPL_CI8: return "CI8";
    case RE4_TPL_CI14X2: return "CI14X2";
    case RE4_TPL_CMPR: return "CMPR";
    default: return "unknown";
    }
}

re4_err re4_tpl_read_gc(re4_span src, re4_arena *arena, re4_tpl *out)
{
    re4_u32 magic;
    re4_u32 n;
    re4_u32 table_off;
    re4_u32 i;
    re4_tpl_image *images;
    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0, 12)) return RE4_ERR_RANGE;
    magic = re4_read_u32_be(src.data);
    if (magic != 0x0020AF30U) return RE4_ERR_BAD_MAGIC;
    n = re4_read_u32_be(src.data + 4);
    table_off = re4_read_u32_be(src.data + 8);
    if (n > 65535U) return RE4_ERR_BAD_FORMAT;
    if (!re4_span_ok(src, (re4_size)table_off, (re4_size)n * 8UL)) return RE4_ERR_RANGE;
    images = (re4_tpl_image *)re4_arena_push(arena, (re4_size)n * sizeof(re4_tpl_image), sizeof(re4_u32));
    if (n != 0 && images == 0) return RE4_ERR_NO_SPACE;
    for (i = 0; i < n; ++i) {
        re4_u32 th;
        re4_u32 ph;
        const re4_u8 *tp;
        th = re4_read_u32_be(src.data + table_off + ((re4_size)i * 8UL));
        ph = re4_read_u32_be(src.data + table_off + ((re4_size)i * 8UL) + 4UL);
        if (!re4_span_ok(src, (re4_size)th, 36)) return RE4_ERR_RANGE;
        tp = src.data + th;
        images[i].height = re4_read_u16_be(tp + 0);
        images[i].width = re4_read_u16_be(tp + 2);
        images[i].format = re4_read_u32_be(tp + 4);
        images[i].data_offset = re4_read_u32_be(tp + 8);
        images[i].wrap_s = re4_read_u32_be(tp + 12);
        images[i].wrap_t = re4_read_u32_be(tp + 16);
        images[i].min_filter = re4_read_u32_be(tp + 20);
        images[i].mag_filter = re4_read_u32_be(tp + 24);
        images[i].lod_bias_bits = re4_read_u32_be(tp + 28);
        images[i].edge_lod = tp[32];
        images[i].min_lod = tp[33];
        images[i].max_lod = tp[34];
        images[i].palette_offset = ph;
        if (ph != 0) {
            if (!re4_span_ok(src, (re4_size)ph, 12)) return RE4_ERR_RANGE;
            images[i].palette_items = re4_read_u16_be(src.data + ph + 0);
            images[i].palette_format = re4_read_u32_be(src.data + ph + 4);
            images[i].palette_data_offset = re4_read_u32_be(src.data + ph + 8);
        }
    }
    out->image_count = n;
    out->images = images;
    out->endian = RE4_ENDIAN_BIG;
    return RE4_OK;
}


static re4_u32 re4_round_up_u32(re4_u32 v, re4_u32 step)
{
    if (step == 0U) return v;
    return ((v + step - 1U) / step) * step;
}

re4_err re4_tpl_image_payload_size(const re4_tpl_image *img, re4_size *out_bytes)
{
    re4_u32 w;
    re4_u32 h;
    re4_size bytes;
    if (img == 0 || out_bytes == 0) return RE4_ERR_NULL;
    w = (re4_u32)img->width;
    h = (re4_u32)img->height;
    bytes = 0;
    switch (img->format) {
    case RE4_TPL_I4:
    case RE4_TPL_CI4:
        bytes = ((re4_size)re4_round_up_u32(w, 8U) * (re4_size)re4_round_up_u32(h, 8U)) / 2UL;
        break;
    case RE4_TPL_I8:
    case RE4_TPL_IA4:
    case RE4_TPL_CI8:
        bytes = (re4_size)re4_round_up_u32(w, 8U) * (re4_size)re4_round_up_u32(h, 4U);
        break;
    case RE4_TPL_IA8:
    case RE4_TPL_RGB565:
    case RE4_TPL_RGB5A3:
    case RE4_TPL_CI14X2:
        bytes = (re4_size)re4_round_up_u32(w, 4U) * (re4_size)re4_round_up_u32(h, 4U) * 2UL;
        break;
    case RE4_TPL_RGBA8:
        bytes = (re4_size)re4_round_up_u32(w, 4U) * (re4_size)re4_round_up_u32(h, 4U) * 4UL;
        break;
    case RE4_TPL_CMPR:
        bytes = ((re4_size)re4_round_up_u32(w, 8U) * (re4_size)re4_round_up_u32(h, 8U)) / 2UL;
        break;
    default:
        return RE4_ERR_UNSUPPORTED;
    }
    *out_bytes = bytes;
    return RE4_OK;
}

re4_err re4_tpl_image_span(re4_span src, const re4_tpl_image *img, re4_span *out)
{
    re4_size bytes;
    re4_err er;
    if (out == 0) return RE4_ERR_NULL;
    er = re4_tpl_image_payload_size(img, &bytes);
    if (er != RE4_OK) return er;
    return re4_span_sub(src, (re4_size)img->data_offset, bytes, out);
}

static re4_err re4_decode_i4(re4_span src, const re4_tpl_image *img, re4_u8 *dst, re4_u32 stride)
{
    re4_u32 bx;
    re4_u32 by;
    re4_size off;
    off = img->data_offset;
    for (by = 0; by < (re4_u32)img->height; by += 8U) {
        for (bx = 0; bx < (re4_u32)img->width; bx += 8U) {
            re4_u32 y;
            for (y = 0; y < 8U; ++y) {
                re4_u32 x;
                for (x = 0; x < 8U; x += 2U) {
                    re4_u8 b;
                    re4_u8 v0;
                    re4_u8 v1;
                    if (!re4_span_ok(src, off, 1)) return RE4_ERR_RANGE;
                    b = src.data[off++];
                    v0 = re4_expand4((re4_u8)(b >> 4));
                    v1 = re4_expand4((re4_u8)(b & 15U));
                    if (bx + x < img->width && by + y < img->height) re4_put_rgba(dst, stride, bx + x, by + y, v0, v0, v0, 255);
                    if (bx + x + 1U < img->width && by + y < img->height) re4_put_rgba(dst, stride, bx + x + 1U, by + y, v1, v1, v1, 255);
                }
            }
        }
    }
    return RE4_OK;
}

static re4_err re4_decode_i8_or_ia4(re4_span src, const re4_tpl_image *img, re4_u8 *dst, re4_u32 stride, re4_bool ia4)
{
    re4_u32 bx;
    re4_u32 by;
    re4_size off;
    off = img->data_offset;
    for (by = 0; by < (re4_u32)img->height; by += 4U) {
        for (bx = 0; bx < (re4_u32)img->width; bx += 8U) {
            re4_u32 y;
            for (y = 0; y < 4U; ++y) {
                re4_u32 x;
                for (x = 0; x < 8U; ++x) {
                    re4_u8 b;
                    re4_u8 i;
                    re4_u8 a;
                    if (!re4_span_ok(src, off, 1)) return RE4_ERR_RANGE;
                    b = src.data[off++];
                    if (ia4) {
                        a = re4_expand4((re4_u8)(b >> 4));
                        i = re4_expand4((re4_u8)(b & 15U));
                    } else {
                        i = b;
                        a = 255;
                    }
                    if (bx + x < img->width && by + y < img->height) re4_put_rgba(dst, stride, bx + x, by + y, i, i, i, a);
                }
            }
        }
    }
    return RE4_OK;
}

static re4_err re4_decode_ia8_rgb16(re4_span src, const re4_tpl_image *img, re4_u8 *dst, re4_u32 stride)
{
    re4_u32 bx;
    re4_u32 by;
    re4_size off;
    off = img->data_offset;
    for (by = 0; by < (re4_u32)img->height; by += 4U) {
        for (bx = 0; bx < (re4_u32)img->width; bx += 4U) {
            re4_u32 y;
            for (y = 0; y < 4U; ++y) {
                re4_u32 x;
                for (x = 0; x < 4U; ++x) {
                    re4_u16 v;
                    re4_u8 rgba[4];
                    if (!re4_span_ok(src, off, 2)) return RE4_ERR_RANGE;
                    v = re4_read_u16_be(src.data + off);
                    off += 2;
                    if (img->format == RE4_TPL_IA8) {
                        rgba[3] = (re4_u8)(v >> 8);
                        rgba[0] = (re4_u8)(v & 0xFFU);
                        rgba[1] = rgba[0];
                        rgba[2] = rgba[0];
                    } else if (img->format == RE4_TPL_RGB565) {
                        re4_rgb565_to_rgba(v, rgba);
                    } else {
                        re4_rgb5a3_to_rgba(v, rgba);
                    }
                    if (bx + x < img->width && by + y < img->height) re4_put_rgba(dst, stride, bx + x, by + y, rgba[0], rgba[1], rgba[2], rgba[3]);
                }
            }
        }
    }
    return RE4_OK;
}

static re4_err re4_decode_rgba8(re4_span src, const re4_tpl_image *img, re4_u8 *dst, re4_u32 stride)
{
    re4_u32 bx;
    re4_u32 by;
    re4_size off;
    off = img->data_offset;
    for (by = 0; by < (re4_u32)img->height; by += 4U) {
        for (bx = 0; bx < (re4_u32)img->width; bx += 4U) {
            re4_size block;
            re4_u32 y;
            block = off;
            if (!re4_span_ok(src, block, 64)) return RE4_ERR_RANGE;
            for (y = 0; y < 4U; ++y) {
                re4_u32 x;
                for (x = 0; x < 4U; ++x) {
                    re4_size i;
                    re4_u8 a;
                    re4_u8 r;
                    re4_u8 g;
                    re4_u8 b;
                    i = ((re4_size)y * 4UL + (re4_size)x) * 2UL;
                    a = src.data[block + i + 0UL];
                    r = src.data[block + i + 1UL];
                    g = src.data[block + 32UL + i + 0UL];
                    b = src.data[block + 32UL + i + 1UL];
                    if (bx + x < img->width && by + y < img->height) re4_put_rgba(dst, stride, bx + x, by + y, r, g, b, a);
                }
            }
            off += 64UL;
        }
    }
    return RE4_OK;
}

static re4_err re4_decode_ci(re4_span src, const re4_tpl_image *img, re4_u8 *dst, re4_u32 stride)
{
    re4_u32 bx;
    re4_u32 by;
    re4_size off;
    off = img->data_offset;
    if (img->palette_offset == 0) return RE4_ERR_BAD_FORMAT;
    if (img->format == RE4_TPL_CI4) {
        for (by = 0; by < (re4_u32)img->height; by += 8U) {
            for (bx = 0; bx < (re4_u32)img->width; bx += 8U) {
                re4_u32 y;
                for (y = 0; y < 8U; ++y) {
                    re4_u32 x;
                    for (x = 0; x < 8U; x += 2U) {
                        re4_u8 b;
                        re4_u8 rgba[4];
                        if (!re4_span_ok(src, off, 1)) return RE4_ERR_RANGE;
                        b = src.data[off++];
                        if (bx + x < img->width && by + y < img->height) {
                            re4_err er;
                            er = re4_palette_lookup(src, img, (b >> 4) & 15U, rgba);
                            if (er != RE4_OK) return er;
                            re4_put_rgba(dst, stride, bx + x, by + y, rgba[0], rgba[1], rgba[2], rgba[3]);
                        }
                        if (bx + x + 1U < img->width && by + y < img->height) {
                            re4_err er2;
                            er2 = re4_palette_lookup(src, img, b & 15U, rgba);
                            if (er2 != RE4_OK) return er2;
                            re4_put_rgba(dst, stride, bx + x + 1U, by + y, rgba[0], rgba[1], rgba[2], rgba[3]);
                        }
                    }
                }
            }
        }
        return RE4_OK;
    }
    if (img->format == RE4_TPL_CI8) {
        for (by = 0; by < (re4_u32)img->height; by += 4U) {
            for (bx = 0; bx < (re4_u32)img->width; bx += 8U) {
                re4_u32 y;
                for (y = 0; y < 4U; ++y) {
                    re4_u32 x;
                    for (x = 0; x < 8U; ++x) {
                        re4_u8 rgba[4];
                        re4_err er;
                        if (!re4_span_ok(src, off, 1)) return RE4_ERR_RANGE;
                        er = re4_palette_lookup(src, img, src.data[off++], rgba);
                        if (er != RE4_OK) return er;
                        if (bx + x < img->width && by + y < img->height) re4_put_rgba(dst, stride, bx + x, by + y, rgba[0], rgba[1], rgba[2], rgba[3]);
                    }
                }
            }
        }
        return RE4_OK;
    }
    if (img->format == RE4_TPL_CI14X2) {
        for (by = 0; by < (re4_u32)img->height; by += 4U) {
            for (bx = 0; bx < (re4_u32)img->width; bx += 4U) {
                re4_u32 y;
                for (y = 0; y < 4U; ++y) {
                    re4_u32 x;
                    for (x = 0; x < 4U; ++x) {
                        re4_u16 idx;
                        re4_u8 rgba[4];
                        re4_err er;
                        if (!re4_span_ok(src, off, 2)) return RE4_ERR_RANGE;
                        idx = (re4_u16)(re4_read_u16_be(src.data + off) & 0x3FFFU);
                        off += 2;
                        er = re4_palette_lookup(src, img, idx, rgba);
                        if (er != RE4_OK) return er;
                        if (bx + x < img->width && by + y < img->height) re4_put_rgba(dst, stride, bx + x, by + y, rgba[0], rgba[1], rgba[2], rgba[3]);
                    }
                }
            }
        }
        return RE4_OK;
    }
    return RE4_ERR_UNSUPPORTED;
}

static void re4_cmpr_subblock(re4_span src, re4_size off, re4_u8 *dst, re4_u32 stride,
                              re4_u32 ox, re4_u32 oy, re4_u32 w, re4_u32 h)
{
    re4_u16 c0;
    re4_u16 c1;
    re4_u8 col[4][4];
    re4_u32 row;
    c0 = re4_read_u16_be(src.data + off + 0UL);
    c1 = re4_read_u16_be(src.data + off + 2UL);
    re4_rgb565_to_rgba(c0, col[0]);
    re4_rgb565_to_rgba(c1, col[1]);
    if (c0 > c1) {
        col[2][0] = (re4_u8)(((re4_u32)col[0][0] * 2U + col[1][0]) / 3U);
        col[2][1] = (re4_u8)(((re4_u32)col[0][1] * 2U + col[1][1]) / 3U);
        col[2][2] = (re4_u8)(((re4_u32)col[0][2] * 2U + col[1][2]) / 3U);
        col[2][3] = 255;
        col[3][0] = (re4_u8)(((re4_u32)col[0][0] + (re4_u32)col[1][0] * 2U) / 3U);
        col[3][1] = (re4_u8)(((re4_u32)col[0][1] + (re4_u32)col[1][1] * 2U) / 3U);
        col[3][2] = (re4_u8)(((re4_u32)col[0][2] + (re4_u32)col[1][2] * 2U) / 3U);
        col[3][3] = 255;
    } else {
        col[2][0] = (re4_u8)(((re4_u32)col[0][0] + col[1][0]) / 2U);
        col[2][1] = (re4_u8)(((re4_u32)col[0][1] + col[1][1]) / 2U);
        col[2][2] = (re4_u8)(((re4_u32)col[0][2] + col[1][2]) / 2U);
        col[2][3] = 255;
        col[3][0] = 0;
        col[3][1] = 0;
        col[3][2] = 0;
        col[3][3] = 0;
    }
    for (row = 0; row < 4U; ++row) {
        re4_u8 bits;
        re4_u32 x;
        bits = src.data[off + 4UL + row];
        for (x = 0; x < 4U; ++x) {
            re4_u32 idx;
            idx = (bits >> (6U - (x * 2U))) & 3U;
            if (ox + x < w && oy + row < h) re4_put_rgba(dst, stride, ox + x, oy + row, col[idx][0], col[idx][1], col[idx][2], col[idx][3]);
        }
    }
}

static re4_err re4_decode_cmpr(re4_span src, const re4_tpl_image *img, re4_u8 *dst, re4_u32 stride)
{
    re4_u32 bx;
    re4_u32 by;
    re4_size off;
    off = img->data_offset;
    for (by = 0; by < (re4_u32)img->height; by += 8U) {
        for (bx = 0; bx < (re4_u32)img->width; bx += 8U) {
            if (!re4_span_ok(src, off, 32)) return RE4_ERR_RANGE;
            re4_cmpr_subblock(src, off + 0UL, dst, stride, bx + 0U, by + 0U, img->width, img->height);
            re4_cmpr_subblock(src, off + 8UL, dst, stride, bx + 4U, by + 0U, img->width, img->height);
            re4_cmpr_subblock(src, off + 16UL, dst, stride, bx + 0U, by + 4U, img->width, img->height);
            re4_cmpr_subblock(src, off + 24UL, dst, stride, bx + 4U, by + 4U, img->width, img->height);
            off += 32UL;
        }
    }
    return RE4_OK;
}

re4_err re4_tpl_decode_image_rgba32(re4_span src, const re4_tpl *tpl, re4_u32 image_index,
                                    re4_u8 *dst, re4_size dst_size, re4_u32 dst_stride)
{
    const re4_tpl_image *img;
    re4_size min_size;
    if (tpl == 0 || dst == 0) return RE4_ERR_NULL;
    if (image_index >= tpl->image_count) return RE4_ERR_RANGE;
    img = &tpl->images[image_index];
    if (dst_stride < (re4_u32)img->width * 4U) return RE4_ERR_NO_SPACE;
    min_size = (re4_size)dst_stride * (re4_size)img->height;
    if (dst_size < min_size) return RE4_ERR_NO_SPACE;
    {
        re4_span payload_check;
        re4_err payload_er;
        payload_er = re4_tpl_image_span(src, img, &payload_check);
        if (payload_er != RE4_OK) return payload_er;
    }
    switch (img->format) {
    case RE4_TPL_I4: return re4_decode_i4(src, img, dst, dst_stride);
    case RE4_TPL_I8: return re4_decode_i8_or_ia4(src, img, dst, dst_stride, RE4_FALSE);
    case RE4_TPL_IA4: return re4_decode_i8_or_ia4(src, img, dst, dst_stride, RE4_TRUE);
    case RE4_TPL_IA8:
    case RE4_TPL_RGB565:
    case RE4_TPL_RGB5A3: return re4_decode_ia8_rgb16(src, img, dst, dst_stride);
    case RE4_TPL_RGBA8: return re4_decode_rgba8(src, img, dst, dst_stride);
    case RE4_TPL_CI4:
    case RE4_TPL_CI8:
    case RE4_TPL_CI14X2: return re4_decode_ci(src, img, dst, dst_stride);
    case RE4_TPL_CMPR: return re4_decode_cmpr(src, img, dst, dst_stride);
    default: return RE4_ERR_UNSUPPORTED;
    }
}

re4_err re4_tplcue_read_le(re4_span src, re4_arena *arena, re4_tplcue *out)
{
    re4_u32 n;
    re4_u32 i;
    re4_tplcue_record *records;
    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if ((src.size & 7UL) != 0) return RE4_ERR_BAD_FORMAT;
    n = (re4_u32)(src.size / 8UL);
    records = (re4_tplcue_record *)re4_arena_push(arena, (re4_size)n * sizeof(re4_tplcue_record), sizeof(re4_u32));
    if (n != 0 && records == 0) return RE4_ERR_NO_SPACE;
    for (i = 0; i < n; ++i) {
        const re4_u8 *p;
        re4_u32 a;
        re4_u32 b;
        p = src.data + ((re4_size)i * 8UL);
        a = re4_read_u32_le(p + 0);
        b = re4_read_u32_le(p + 4);
        records[i].a = a;
        records[i].b = b;
        if (a < 65536U && b >= 65536U) {
            records[i].likely_texture_id = a;
            records[i].likely_pack_id = b;
        } else if (b < 65536U && a >= 65536U) {
            records[i].likely_texture_id = b;
            records[i].likely_pack_id = a;
        } else {
            records[i].likely_texture_id = a;
            records[i].likely_pack_id = b;
        }
    }
    out->record_count = n;
    out->records = records;
    return RE4_OK;
}

re4_u32 re4_tplcue_record_pack_id(const re4_tplcue_record *rec)
{
    if (rec == 0) return 0U;
    return rec->likely_pack_id;
}

re4_u32 re4_tplcue_record_texture_id(const re4_tplcue_record *rec)
{
    if (rec == 0) return 0U;
    return rec->likely_texture_id;
}

re4_err re4_tplcue_find(const re4_tplcue *cue, re4_u32 pack_id, re4_u32 texture_id, re4_u32 *out_index)
{
    re4_u32 i;
    if (cue == 0 || out_index == 0) return RE4_ERR_NULL;
    for (i = 0U; i < cue->record_count; ++i) {
        if (re4_tplcue_record_pack_id(&cue->records[i]) == pack_id &&
            re4_tplcue_record_texture_id(&cue->records[i]) == texture_id) {
            *out_index = i;
            return RE4_OK;
        }
    }
    return RE4_ERR_RANGE;
}

static re4_err re4_tplcue_emit_cstr(re4_emit_fn emit, void *user, const char *text)
{
    re4_size n;
    if (emit == 0 || text == 0) return RE4_ERR_NULL;
    n = 0UL;
    while (text[n] != 0) ++n;
    return emit(user, text, n);
}

re4_err re4_tplcue_emit_idxuhdtpl(const re4_tplcue *cue, re4_u32 pixel_format_type,
                                  re4_u32 width, re4_u32 height,
                                  re4_emit_fn emit, void *emit_user)
{
    re4_u32 i;
    char buf[256];
    re4_err er;
    if (cue == 0 || emit == 0) return RE4_ERR_NULL;
    if (width == 0U) width = 1U;
    if (height == 0U) height = 1U;
    for (i = 0U; i < cue->record_count; ++i) {
        sprintf(buf,
                "TPL_%03u\nPackID:%08X\nTextureID:%04u\nPixelFormatType:%02X\nwidth:%u\nheight:%u\n\n",
                (unsigned)i,
                (unsigned)re4_tplcue_record_pack_id(&cue->records[i]),
                (unsigned)re4_tplcue_record_texture_id(&cue->records[i]),
                (unsigned)(pixel_format_type & 0xFFU),
                (unsigned)width,
                (unsigned)height);
        er = re4_tplcue_emit_cstr(emit, emit_user, buf);
        if (er != RE4_OK) return er;
    }
    return RE4_OK;
}

re4_bool re4_tpl_format_is_indexed(re4_u32 fmt)
{
    if (fmt == RE4_TPL_CI4 || fmt == RE4_TPL_CI8 || fmt == RE4_TPL_CI14X2) return RE4_TRUE;
    return RE4_FALSE;
}

re4_err re4_tpl_format_block_info(re4_u32 fmt, re4_u32 *block_w, re4_u32 *block_h, re4_u32 *block_bytes)
{
    if (block_w == 0 || block_h == 0 || block_bytes == 0) return RE4_ERR_NULL;
    switch (fmt) {
    case RE4_TPL_I4:
    case RE4_TPL_CI4:
        *block_w = 8U;
        *block_h = 8U;
        *block_bytes = 32U;
        return RE4_OK;
    case RE4_TPL_I8:
    case RE4_TPL_IA4:
    case RE4_TPL_CI8:
        *block_w = 8U;
        *block_h = 4U;
        *block_bytes = 32U;
        return RE4_OK;
    case RE4_TPL_IA8:
    case RE4_TPL_RGB565:
    case RE4_TPL_RGB5A3:
    case RE4_TPL_CI14X2:
        *block_w = 4U;
        *block_h = 4U;
        *block_bytes = 32U;
        return RE4_OK;
    case RE4_TPL_RGBA8:
        *block_w = 4U;
        *block_h = 4U;
        *block_bytes = 64U;
        return RE4_OK;
    case RE4_TPL_CMPR:
        *block_w = 8U;
        *block_h = 8U;
        *block_bytes = 32U;
        return RE4_OK;
    default:
        return RE4_ERR_UNSUPPORTED;
    }
}

re4_err re4_tpl_palette_payload_size(const re4_tpl_image *img, re4_size *out_bytes)
{
    if (img == 0 || out_bytes == 0) return RE4_ERR_NULL;
    if (!re4_tpl_format_is_indexed(img->format)) {
        *out_bytes = 0;
        return RE4_OK;
    }
    if (img->palette_offset == 0U || img->palette_items == 0U) return RE4_ERR_BAD_FORMAT;
    *out_bytes = (re4_size)img->palette_items * 2UL;
    return RE4_OK;
}

re4_err re4_tpl_palette_span(re4_span src, const re4_tpl_image *img, re4_span *out)
{
    re4_size bytes;
    re4_err er;
    if (out == 0) return RE4_ERR_NULL;
    er = re4_tpl_palette_payload_size(img, &bytes);
    if (er != RE4_OK) return er;
    if (bytes == 0) {
        out->data = 0;
        out->size = 0;
        return RE4_OK;
    }
    return re4_span_sub(src, (re4_size)img->palette_data_offset, bytes, out);
}

re4_err re4_uhd_tpl_read(re4_span src, re4_endian endian, re4_bool wide_offsets,
                            re4_arena *arena, re4_uhd_tpl *out)
{
    re4_u32 magic;
    re4_u32 n;
    re4_u32 table_off;
    re4_u32 i;
    re4_size entry_stride;
    re4_uhd_tpl_image *images;
    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0UL, 12UL)) return RE4_ERR_RANGE;
    magic = re4_read_u32_e(src.data + 0UL, endian);
    if (!(magic == 0x78563412U || magic == 0x12345678U)) return RE4_ERR_BAD_MAGIC;
    n = re4_read_u32_e(src.data + 4UL, endian);
    table_off = re4_read_u32_e(src.data + 8UL, endian);
    if (n > 0x00010000U || table_off > src.size) return RE4_ERR_BAD_FORMAT;
    entry_stride = wide_offsets ? 16UL : 8UL;
    if (!re4_span_ok(src, (re4_size)table_off, (re4_size)n * entry_stride)) return RE4_ERR_RANGE;
    images = 0;
    if (n != 0U) {
        images = (re4_uhd_tpl_image *)re4_arena_push(arena, (re4_size)n * sizeof(re4_uhd_tpl_image), sizeof(re4_u32));
        if (images == 0) return RE4_ERR_NO_SPACE;
        memset(images, 0, (re4_size)n * sizeof(re4_uhd_tpl_image));
    }
    for (i = 0; i < n; ++i) {
        re4_size ent;
        re4_u32 img_off;
        re4_u32 pal_off;
        ent = (re4_size)table_off + ((re4_size)i * entry_stride);
        img_off = re4_read_u32_e(src.data + ent, endian);
        if (wide_offsets) pal_off = re4_read_u32_e(src.data + ent + 8UL, endian);
        else pal_off = re4_read_u32_e(src.data + ent + 4UL, endian);
        images[i].image_header_offset = img_off;
        images[i].palette_header_offset = pal_off;
        if (pal_off != 0U) {
            if (!re4_span_ok(src, (re4_size)pal_off, 8UL)) return RE4_ERR_RANGE;
            images[i].has_palette = 1U;
            images[i].colors_count = re4_read_u16_e(src.data + pal_off + 0UL, endian);
            images[i].unpacked = src.data[(re4_size)pal_off + 2UL];
            images[i].pad = src.data[(re4_size)pal_off + 3UL];
            images[i].palette_format = re4_read_u32_e(src.data + pal_off + 4UL, endian);
        }
        if (!re4_span_ok(src, (re4_size)img_off, wide_offsets ? 44UL : 40UL)) return RE4_ERR_RANGE;
        images[i].height = re4_read_u16_e(src.data + img_off + 0UL, endian);
        images[i].width = re4_read_u16_e(src.data + img_off + 2UL, endian);
        images[i].pixel_format = re4_read_u32_e(src.data + img_off + 4UL, endian);
        images[i].second_offset = re4_read_u32_e(src.data + img_off + 8UL, endian);
        {
            re4_size base;
            base = (re4_size)img_off + (wide_offsets ? 16UL : 12UL);
            images[i].wrap_s = re4_read_u32_e(src.data + base + 0UL, endian);
            images[i].wrap_t = re4_read_u32_e(src.data + base + 4UL, endian);
            images[i].min_filter = re4_read_u32_e(src.data + base + 8UL, endian);
            images[i].mag_filter = re4_read_u32_e(src.data + base + 12UL, endian);
            images[i].lod_bias_bits = re4_read_u32_e(src.data + base + 16UL, endian);
            images[i].enable_lod = src.data[base + 20UL];
            images[i].min_lod = src.data[base + 21UL];
            images[i].max_lod = src.data[base + 22UL];
            images[i].is_compressed = src.data[base + 23UL];
        }
        if (!re4_span_ok(src, (re4_size)images[i].second_offset, 8UL)) return RE4_ERR_RANGE;
        images[i].pack_id = re4_read_u32_e(src.data + images[i].second_offset + 0UL, endian);
        images[i].texture_id = re4_read_u32_e(src.data + images[i].second_offset + 4UL, endian);
    }
    out->magic = magic;
    out->image_count = n;
    out->table_offset = table_off;
    out->images = images;
    out->endian = endian;
    out->wide_offsets = wide_offsets ? 1U : 0U;
    return RE4_OK;
}

static re4_err re4_ps2_tpl_read_image_header_at(re4_span src, re4_endian endian, re4_u32 off, re4_ps2_tpl_image_header *out)
{
    const re4_u8 *p;
    if (out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, (re4_size)off, 0x30UL)) return RE4_ERR_RANGE;
    p = src.data + (re4_size)off;
    out->width = re4_read_u16_e(p + 0x00UL, endian);
    out->height = re4_read_u16_e(p + 0x02UL, endian);
    out->bitdepth = re4_read_u16_e(p + 0x04UL, endian);
    out->interlace = re4_read_u16_e(p + 0x06UL, endian);
    out->next = re4_read_u16_e(p + 0x08UL, endian);
    out->mipmap_status = re4_read_u16_e(p + 0x0AUL, endian);
    out->qwc = re4_read_u16_e(p + 0x0CUL, endian);
    out->ref = re4_read_u16_e(p + 0x0EUL, endian);
    out->mipmap_header1_offset = re4_read_u32_e(p + 0x10UL, endian);
    out->mipmap_header2_offset = re4_read_u32_e(p + 0x14UL, endian);
    out->gs_mip_lo = re4_read_u32_e(p + 0x18UL, endian);
    out->gs_mip_hi = re4_read_u32_e(p + 0x1CUL, endian);
    out->indexes_offset = re4_read_u32_e(p + 0x20UL, endian);
    out->palette_offset = re4_read_u32_e(p + 0x24UL, endian);
    out->gs_tex_lo = re4_read_u32_e(p + 0x28UL, endian);
    out->gs_tex_hi = re4_read_u32_e(p + 0x2CUL, endian);
    out->source_offset = off;
    return RE4_OK;
}

re4_err re4_ps2_tpl_read(re4_span src, re4_endian endian, re4_arena *arena, re4_ps2_tpl *out)
{
    re4_u32 n;
    re4_u32 start;
    re4_u32 i;
    re4_ps2_tpl_image *images;
    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0UL, 16UL)) return RE4_ERR_RANGE;
    n = re4_read_u32_e(src.data + 4UL, endian);
    start = re4_read_u32_e(src.data + 8UL, endian);
    if (n > 0x10000U) return RE4_ERR_BAD_FORMAT;
    if (!re4_span_ok(src, (re4_size)start, (re4_size)n * 0x30UL)) return RE4_ERR_RANGE;
    images = 0;
    if (n != 0U) {
        images = (re4_ps2_tpl_image *)re4_arena_push(arena, (re4_size)n * sizeof(re4_ps2_tpl_image), sizeof(re4_u32));
        if (images == 0) return RE4_ERR_NO_SPACE;
        memset(images, 0, (re4_size)n * sizeof(re4_ps2_tpl_image));
    }
    for (i = 0U; i < n; ++i) {
        re4_u32 off;
        re4_err er;
        off = start + (i * 0x30U);
        er = re4_ps2_tpl_read_image_header_at(src, endian, off, &images[i].main_header);
        if (er != RE4_OK) return er;
        if (images[i].main_header.mipmap_status == RE4_PS2TPL_MIPMAP_TWO) {
            if (images[i].main_header.mipmap_header1_offset != 0U) {
                er = re4_ps2_tpl_read_image_header_at(src, endian, images[i].main_header.mipmap_header1_offset, &images[i].mipmap1_header);
                if (er != RE4_OK) return er;
                images[i].has_mipmap1 = 1U;
            }
            if (images[i].main_header.mipmap_header2_offset != 0U) {
                er = re4_ps2_tpl_read_image_header_at(src, endian, images[i].main_header.mipmap_header2_offset, &images[i].mipmap2_header);
                if (er != RE4_OK) return er;
                images[i].has_mipmap2 = 1U;
            }
        }
    }
    out->magic = re4_read_u32_e(src.data + 0UL, endian);
    out->image_count = n;
    out->start_offset = start;
    out->header_unk1 = re4_read_u32_e(src.data + 12UL, endian);
    out->images = images;
    out->endian = endian;
    return RE4_OK;
}

re4_bool re4_ps2_tpl_image_has_mipmaps(const re4_ps2_tpl_image *img)
{
    if (img == 0) return RE4_FALSE;
    if (img->has_mipmap1 || img->has_mipmap2) return RE4_TRUE;
    return img->main_header.mipmap_status == RE4_PS2TPL_MIPMAP_TWO ? RE4_TRUE : RE4_FALSE;
}

re4_err re4_ps2_tpl_indexes_span(re4_span src, const re4_ps2_tpl_image_header *h, re4_span *out)
{
    re4_size bytes;
    if (h == 0 || out == 0) return RE4_ERR_NULL;
    if (h->indexes_offset == 0U || h->qwc == 0U) {
        out->data = 0;
        out->size = 0;
        return RE4_OK;
    }
    bytes = (re4_size)h->qwc * 16UL;
    return re4_span_sub(src, (re4_size)h->indexes_offset, bytes, out);
}

re4_err re4_ps2_tpl_palette_span(re4_span src, const re4_ps2_tpl_image_header *h, re4_span *out)
{
    re4_size bytes;
    if (h == 0 || out == 0) return RE4_ERR_NULL;
    bytes = re4_ps2_tpl_palette_payload_size_for_header(h);
    if (h->palette_offset == 0U || bytes == 0UL) {
        out->data = 0;
        out->size = 0;
        return RE4_OK;
    }
    return re4_span_sub(src, (re4_size)h->palette_offset, bytes, out);
}

re4_err re4_ps2_tpl_main_header_as_idxtpl(const re4_ps2_tpl_image *img, re4_idxtpl_header *out)
{
    const re4_ps2_tpl_image_header *h;
    if (img == 0 || out == 0) return RE4_ERR_NULL;
    h = &img->main_header;
    memset(out, 0, sizeof(*out));
    out->width = h->width;
    out->height = h->height;
    out->bitdepth = h->bitdepth;
    out->interlace = h->interlace;
    out->next = h->next;
    out->mipmap_status = h->mipmap_status;
    out->qwc = h->qwc;
    out->ref = h->ref;
    out->mipmap_header1_offset = h->mipmap_header1_offset;
    out->mipmap_header2_offset = h->mipmap_header2_offset;
    out->gs_mip_lo = h->gs_mip_lo;
    out->gs_mip_hi = h->gs_mip_hi;
    out->indexes_offset = h->indexes_offset;
    out->palette_offset = h->palette_offset;
    out->gs_tex = h->gs_tex_lo;
    out->gs_tex_hi = h->gs_tex_hi;
    out->seen_mask = RE4_IDXTPL_HEADER_SEEN_WIDTH |
                     RE4_IDXTPL_HEADER_SEEN_HEIGHT |
                     RE4_IDXTPL_HEADER_SEEN_BITDEPTH |
                     RE4_IDXTPL_HEADER_SEEN_INTERLACE |
                     RE4_IDXTPL_HEADER_SEEN_NEXT |
                     RE4_IDXTPL_HEADER_SEEN_MIPMAP_STATUS |
                     RE4_IDXTPL_HEADER_SEEN_QWC |
                     RE4_IDXTPL_HEADER_SEEN_REF |
                     RE4_IDXTPL_HEADER_SEEN_MIP1OFF |
                     RE4_IDXTPL_HEADER_SEEN_MIP2OFF |
                     RE4_IDXTPL_HEADER_SEEN_GSMIP |
                     RE4_IDXTPL_HEADER_SEEN_INDEXES_OFFSET |
                     RE4_IDXTPL_HEADER_SEEN_PALETTE_OFFSET |
                     RE4_IDXTPL_HEADER_SEEN_GSTEX;
    return RE4_OK;
}
