#include "g3d_kib.h"

#include <string.h>

static g3d_u32 g3d_kib_rd_u32le(const g3d_u8 *p)
{
    return ((g3d_u32)p[0]) |
           ((g3d_u32)p[1] << 8) |
           ((g3d_u32)p[2] << 16) |
           ((g3d_u32)p[3] << 24);
}

static g3d_u16 g3d_kib_rd_u16le(const g3d_u8 *p)
{
    return (g3d_u16)(((g3d_u16)p[0]) | ((g3d_u16)p[1] << 8));
}

static void g3d_kib_wr_u32le(g3d_u8 *p, g3d_u32 v)
{
    p[0] = (g3d_u8)(v & 0xffu);
    p[1] = (g3d_u8)((v >> 8) & 0xffu);
    p[2] = (g3d_u8)((v >> 16) & 0xffu);
    p[3] = (g3d_u8)((v >> 24) & 0xffu);
}

static int g3d_kib_add_u32(g3d_u32 a, g3d_u32 b, g3d_u32 *out)
{
    if (out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (b > (0xffffffffu - a))
    {
        return G3D_ERR_OVERFLOW;
    }
    *out = a + b;
    return G3D_OK;
}

static int g3d_kib_mul_u32(g3d_u32 a, g3d_u32 b, g3d_u32 *out)
{
    if (out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (a != 0u && b > (0xffffffffu / a))
    {
        return G3D_ERR_OVERFLOW;
    }
    *out = a * b;
    return G3D_OK;
}

const char *g3d_kib_method_name(g3d_u32 method)
{
    switch (method)
    {
        case G3D_KIB_METHOD_STORE: return "store";
        case G3D_KIB_METHOD_PACKBITS: return "packbits-rle";
        case G3D_KIB_METHOD_LZSS12: return "lzss12";
        case G3D_KIB_METHOD_WAVELET8: return "wavelet8";
        case G3D_KIB_METHOD_RAW_FALLBACK: return "raw-fallback";
        default: return "unknown";
    }
}

g3d_u32 g3d_kib_container_header_size(void)
{
    return 16u;
}

int g3d_kib_wrap_container(g3d_u32 method,
                           const g3d_u8 *payload, g3d_u32 payload_size,
                           g3d_u32 unpacked_size,
                           g3d_u8 *dst, g3d_u32 dst_size,
                           g3d_u32 *used_bytes)
{
    g3d_u32 total;
    int rc;
    if ((payload_size != 0u && payload == NULL) || used_bytes == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_kib_add_u32(16u, payload_size, &total);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *used_bytes = total;
    if (dst == NULL)
    {
        return G3D_OK;
    }
    if (dst_size < total)
    {
        return G3D_ERR_NO_SPACE;
    }
    g3d_kib_wr_u32le(dst + 0u, G3D_KIB_MAGIC);
    dst[4] = (g3d_u8)G3D_KIB_VERSION;
    dst[5] = (g3d_u8)(method & 0xffu);
    dst[6] = 0u;
    dst[7] = 0u;
    g3d_kib_wr_u32le(dst + 8u, unpacked_size);
    g3d_kib_wr_u32le(dst + 12u, payload_size);
    if (payload_size != 0u)
    {
        memcpy(dst + 16u, payload, (size_t)payload_size);
    }
    return G3D_OK;
}

int g3d_kib_packbits_encode(const g3d_u8 *src, g3d_u32 src_size,
                            g3d_u8 *dst, g3d_u32 dst_size,
                            g3d_u32 *used_bytes)
{
    g3d_u32 in_pos;
    g3d_u32 out_pos;
    if ((src_size != 0u && src == NULL) || used_bytes == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    in_pos = 0u;
    out_pos = 0u;
    while (in_pos < src_size)
    {
        g3d_u32 run;
        g3d_u32 lit_start;
        g3d_u32 lit_count;
        run = 1u;
        while (in_pos + run < src_size && run < 130u && src[in_pos + run] == src[in_pos])
        {
            ++run;
        }
        if (run >= 3u)
        {
            if (dst != NULL)
            {
                if (out_pos + 2u > dst_size)
                {
                    return G3D_ERR_NO_SPACE;
                }
                dst[out_pos] = (g3d_u8)(0x80u | ((run - 3u) & 0x7fu));
                dst[out_pos + 1u] = src[in_pos];
            }
            out_pos += 2u;
            in_pos += run;
            continue;
        }
        lit_start = in_pos;
        lit_count = 0u;
        while (in_pos < src_size && lit_count < 128u)
        {
            run = 1u;
            while (in_pos + run < src_size && run < 130u && src[in_pos + run] == src[in_pos])
            {
                ++run;
            }
            if (run >= 3u && lit_count != 0u)
            {
                break;
            }
            if (run >= 3u && lit_count == 0u)
            {
                break;
            }
            ++in_pos;
            ++lit_count;
        }
        if (lit_count == 0u)
        {
            lit_count = 1u;
            ++in_pos;
        }
        if (dst != NULL)
        {
            if (out_pos + 1u + lit_count > dst_size)
            {
                return G3D_ERR_NO_SPACE;
            }
            dst[out_pos++] = (g3d_u8)((lit_count - 1u) & 0x7fu);
            memcpy(dst + out_pos, src + lit_start, (size_t)lit_count);
        }
        else
        {
            out_pos += 1u;
        }
        out_pos += lit_count;
    }
    *used_bytes = out_pos;
    return G3D_OK;
}

int g3d_kib_packbits_decode(const g3d_u8 *src, g3d_u32 src_size,
                            g3d_u8 *dst, g3d_u32 dst_size,
                            g3d_u32 *src_used)
{
    g3d_u32 in_pos;
    g3d_u32 out_pos;
    if ((src_size != 0u && src == NULL) || src_used == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    in_pos = 0u;
    out_pos = 0u;
    while (out_pos < dst_size)
    {
        g3d_u8 control;
        g3d_u32 count;
        if (in_pos >= src_size)
        {
            return G3D_ERR_EOF;
        }
        control = src[in_pos++];
        if ((control & 0x80u) == 0u)
        {
            count = ((g3d_u32)control) + 1u;
            if (count > (dst_size - out_pos) || count > (src_size - in_pos))
            {
                return G3D_ERR_EOF;
            }
            if (dst != NULL)
            {
                memcpy(dst + out_pos, src + in_pos, (size_t)count);
            }
            in_pos += count;
            out_pos += count;
        }
        else
        {
            g3d_u8 value;
            count = ((g3d_u32)(control & 0x7fu)) + 3u;
            if (count > (dst_size - out_pos) || in_pos >= src_size)
            {
                return G3D_ERR_EOF;
            }
            value = src[in_pos++];
            if (dst != NULL)
            {
                memset(dst + out_pos, (int)value, (size_t)count);
            }
            out_pos += count;
        }
    }
    *src_used = in_pos;
    return G3D_OK;
}

int g3d_kib_lzss12_decode(const g3d_u8 *src, g3d_u32 src_size,
                          g3d_u8 *dst, g3d_u32 dst_size,
                          g3d_u32 *src_used)
{
    g3d_u32 in_pos;
    g3d_u32 out_pos;
    g3d_u8 flags;
    g3d_u32 mask;
    if ((src_size != 0u && src == NULL) || src_used == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    in_pos = 0u;
    out_pos = 0u;
    flags = 0u;
    mask = 0u;
    while (out_pos < dst_size)
    {
        if (mask == 0u)
        {
            if (in_pos >= src_size)
            {
                return G3D_ERR_EOF;
            }
            flags = src[in_pos++];
            mask = 1u;
        }
        if ((flags & mask) != 0u)
        {
            if (in_pos >= src_size)
            {
                return G3D_ERR_EOF;
            }
            if (dst != NULL)
            {
                dst[out_pos] = src[in_pos];
            }
            ++in_pos;
            ++out_pos;
        }
        else
        {
            g3d_u8 lo;
            g3d_u8 hi;
            g3d_u32 offset;
            g3d_u32 length;
            g3d_u32 i;
            if (in_pos + 2u > src_size)
            {
                return G3D_ERR_EOF;
            }
            lo = src[in_pos++];
            hi = src[in_pos++];
            offset = ((g3d_u32)lo | (((g3d_u32)hi & 0xf0u) << 4)) + 1u;
            length = ((g3d_u32)hi & 0x0fu) + 3u;
            if (offset > out_pos || length > (dst_size - out_pos))
            {
                return G3D_ERR_CORRUPT;
            }
            if (dst != NULL)
            {
                for (i = 0u; i < length; ++i)
                {
                    dst[out_pos] = dst[out_pos - offset];
                    ++out_pos;
                }
            }
            else
            {
                out_pos += length;
            }
        }
        mask <<= 1;
        if (mask == 0x100u)
        {
            mask = 0u;
        }
    }
    *src_used = in_pos;
    return G3D_OK;
}

static int g3d_kib_paeth_predictor(int a, int b, int c)
{
    int p;
    int pa;
    int pb;
    int pc;
    p = a + b - c;
    pa = p - a;
    if (pa < 0) pa = -pa;
    pb = p - b;
    if (pb < 0) pb = -pb;
    pc = p - c;
    if (pc < 0) pc = -pc;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

int g3d_kib_paeth_filter(g3d_u8 *data, g3d_u32 width, g3d_u32 height, g3d_u32 bytes_per_pixel)
{
    g3d_u32 row_bytes;
    g3d_u32 y;
    int rc;
    if (data == NULL || width == 0u || height == 0u || bytes_per_pixel == 0u)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_kib_mul_u32(width, bytes_per_pixel, &row_bytes);
    if (rc != G3D_OK)
    {
        return rc;
    }
    y = height;
    while (y > 0u)
    {
        g3d_u32 yy;
        g3d_u32 x;
        yy = y - 1u;
        x = row_bytes;
        while (x > 0u)
        {
            g3d_u32 xx;
            g3d_u32 pos;
            int left;
            int up;
            int up_left;
            int pred;
            xx = x - 1u;
            pos = yy * row_bytes + xx;
            left = (xx >= bytes_per_pixel) ? (int)data[pos - bytes_per_pixel] : 0;
            up = (yy != 0u) ? (int)data[pos - row_bytes] : 0;
            up_left = (yy != 0u && xx >= bytes_per_pixel) ? (int)data[pos - row_bytes - bytes_per_pixel] : 0;
            pred = g3d_kib_paeth_predictor(left, up, up_left);
            data[pos] = (g3d_u8)(((int)data[pos] - pred) & 0xff);
            --x;
        }
        --y;
    }
    return G3D_OK;
}

int g3d_kib_paeth_unfilter(g3d_u8 *data, g3d_u32 width, g3d_u32 height, g3d_u32 bytes_per_pixel)
{
    g3d_u32 row_bytes;
    g3d_u32 y;
    int rc;
    if (data == NULL || width == 0u || height == 0u || bytes_per_pixel == 0u)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_kib_mul_u32(width, bytes_per_pixel, &row_bytes);
    if (rc != G3D_OK)
    {
        return rc;
    }
    for (y = 0u; y < height; ++y)
    {
        g3d_u32 x;
        for (x = 0u; x < row_bytes; ++x)
        {
            g3d_u32 pos;
            int left;
            int up;
            int up_left;
            int pred;
            pos = y * row_bytes + x;
            left = (x >= bytes_per_pixel) ? (int)data[pos - bytes_per_pixel] : 0;
            up = (y != 0u) ? (int)data[pos - row_bytes] : 0;
            up_left = (y != 0u && x >= bytes_per_pixel) ? (int)data[pos - row_bytes - bytes_per_pixel] : 0;
            pred = g3d_kib_paeth_predictor(left, up, up_left);
            data[pos] = (g3d_u8)(((int)data[pos] + pred) & 0xff);
        }
    }
    return G3D_OK;
}

static int g3d_kib_clamp_byte(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

static int g3d_kib_wavelet_inverse_haar_int(int *a, int *tmp, g3d_u32 width, g3d_u32 height, g3d_u32 levels)
{
    g3d_u32 pixels;
    g3d_u32 level;
    int rc;
    if (a == NULL || width == 0u || height == 0u)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_kib_mul_u32(width, height, &pixels);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (levels == 0u)
    {
        return G3D_OK;
    }
    if (tmp == NULL && pixels != 0u)
    {
        return G3D_ERR_ARENA_EXHAUSTED;
    }
    for (level = levels; level > 0u; --level)
    {
        g3d_u32 rw;
        g3d_u32 rh;
        g3d_u32 hw;
        g3d_u32 hh;
        g3d_u32 y;
        rw = width >> (level - 1u);
        rh = height >> (level - 1u);
        if (rw < 2u || rh < 2u || (rw & 1u) != 0u || (rh & 1u) != 0u)
        {
            return G3D_ERR_UNSUPPORTED;
        }
        hw = rw >> 1;
        hh = rh >> 1;
        memcpy(tmp, a, (size_t)pixels * sizeof(int));
        for (y = 0u; y < rh; ++y)
        {
            g3d_u32 x;
            for (x = 0u; x < hw; ++x)
            {
                int low;
                int high;
                low = tmp[y * width + x];
                high = tmp[y * width + hw + x];
                a[y * width + (x * 2u)] = (low + high) / 2;
                a[y * width + (x * 2u) + 1u] = (low - high) / 2;
            }
        }
        memcpy(tmp, a, (size_t)pixels * sizeof(int));
        for (y = 0u; y < hh; ++y)
        {
            g3d_u32 x;
            for (x = 0u; x < rw; ++x)
            {
                int low;
                int high;
                low = tmp[y * width + x];
                high = tmp[(hh + y) * width + x];
                a[(y * 2u) * width + x] = (low + high) / 2;
                a[(y * 2u + 1u) * width + x] = (low - high) / 2;
            }
        }
    }
    return G3D_OK;
}

int g3d_kib_wavelet8_decode_arena(const g3d_u8 *payload, g3d_u32 payload_size,
                                  g3d_u8 *dst, g3d_u32 dst_size,
                                  g3d_u32 *width_out, g3d_u32 *height_out,
                                  g3d_u32 *levels_out,
                                  g3d_arena *scratch_arena)
{
    g3d_u32 width;
    g3d_u32 height;
    g3d_u32 levels;
    g3d_u32 pixels;
    g3d_u32 need;
    g3d_u32 coeff_bytes;
    g3d_u32 tmp_bytes;
    g3d_u32 i;
    g3d_u32 mark;
    int *coeffs;
    int *tmp;
    int rc;
    if (payload == NULL || dst == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (payload_size < 6u)
    {
        return G3D_ERR_EOF;
    }
    width = (g3d_u32)g3d_kib_rd_u16le(payload + 0u);
    height = (g3d_u32)g3d_kib_rd_u16le(payload + 2u);
    levels = (g3d_u32)payload[4];
    if (width == 0u || height == 0u)
    {
        return G3D_ERR_CORRUPT;
    }
    rc = g3d_kib_mul_u32(width, height, &pixels);
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_kib_mul_u32(pixels, 2u, &need);
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_kib_add_u32(6u, need, &need);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (need > payload_size)
    {
        return G3D_ERR_EOF;
    }
    if (dst_size < pixels)
    {
        return G3D_ERR_NO_SPACE;
    }
    rc = g3d_kib_mul_u32(pixels, (g3d_u32)sizeof(int), &coeff_bytes);
    if (rc != G3D_OK)
    {
        return rc;
    }
    tmp_bytes = coeff_bytes;
    if (scratch_arena == NULL && pixels != 0u)
    {
        return G3D_ERR_ARENA_EXHAUSTED;
    }
    mark = 0u;
    coeffs = NULL;
    tmp = NULL;
    if (scratch_arena != NULL)
    {
        mark = g3d_arena_mark(scratch_arena);
        coeffs = (int *)g3d_arena_alloc(scratch_arena, coeff_bytes, (g3d_u32)sizeof(int));
        tmp = (int *)g3d_arena_alloc(scratch_arena, tmp_bytes, (g3d_u32)sizeof(int));
    }
    if ((coeffs == NULL || tmp == NULL) && pixels != 0u)
    {
        if (scratch_arena != NULL)
        {
            g3d_arena_rewind(scratch_arena, mark);
        }
        return G3D_ERR_ARENA_EXHAUSTED;
    }
    for (i = 0u; i < pixels; ++i)
    {
        g3d_u16 raw;
        raw = g3d_kib_rd_u16le(payload + 6u + (i * 2u));
        coeffs[i] = (int)(g3d_s16)raw;
    }
    rc = g3d_kib_wavelet_inverse_haar_int(coeffs, tmp, width, height, levels);
    if (rc == G3D_OK)
    {
        for (i = 0u; i < pixels; ++i)
        {
            dst[i] = (g3d_u8)g3d_kib_clamp_byte(coeffs[i]);
        }
        if (width_out != NULL) *width_out = width;
        if (height_out != NULL) *height_out = height;
        if (levels_out != NULL) *levels_out = levels;
    }
    if (scratch_arena != NULL)
    {
        g3d_arena_rewind(scratch_arena, mark);
    }
    return rc;
}

int g3d_kib_wavelet8_decode(const g3d_u8 *payload, g3d_u32 payload_size,
                            g3d_u8 *dst, g3d_u32 dst_size,
                            g3d_u32 *width_out, g3d_u32 *height_out,
                            g3d_u32 *levels_out)
{
    return g3d_kib_wavelet8_decode_arena(payload, payload_size, dst, dst_size,
                                         width_out, height_out, levels_out, NULL);
}

static int g3d_kib_decode_container(const g3d_u8 *src, g3d_u32 src_size,
                                    g3d_u8 *dst, g3d_u32 dst_size,
                                    g3d_u32 *src_used,
                                    g3d_u32 *method_out,
                                    g3d_arena *scratch_arena)
{
    g3d_u32 magic;
    g3d_u32 method;
    g3d_u32 unpacked_size;
    g3d_u32 payload_size;
    const g3d_u8 *payload;
    g3d_u32 used;
    int rc;
    if (src == NULL || src_used == NULL || method_out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (src_size < 16u)
    {
        return G3D_ERR_EOF;
    }
    magic = g3d_kib_rd_u32le(src);
    if (magic != G3D_KIB_MAGIC)
    {
        return G3D_ERR_BAD_MAGIC;
    }
    if ((g3d_u32)src[4] != G3D_KIB_VERSION)
    {
        return G3D_ERR_BAD_VERSION;
    }
    method = (g3d_u32)src[5];
    unpacked_size = g3d_kib_rd_u32le(src + 8u);
    payload_size = g3d_kib_rd_u32le(src + 12u);
    rc = g3d_kib_add_u32(16u, payload_size, &used);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used > src_size)
    {
        return G3D_ERR_EOF;
    }
    if (unpacked_size != dst_size)
    {
        return G3D_ERR_CORRUPT;
    }
    payload = src + 16u;
    if (method == G3D_KIB_METHOD_STORE)
    {
        if (payload_size != dst_size)
        {
            return G3D_ERR_CORRUPT;
        }
        if (dst != NULL && dst_size != 0u)
        {
            memcpy(dst, payload, (size_t)dst_size);
        }
    }
    else if (method == G3D_KIB_METHOD_PACKBITS)
    {
        g3d_u32 inner_used;
        rc = g3d_kib_packbits_decode(payload, payload_size, dst, dst_size, &inner_used);
        if (rc != G3D_OK)
        {
            return rc;
        }
        if (inner_used != payload_size)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    else if (method == G3D_KIB_METHOD_LZSS12)
    {
        g3d_u32 inner_used_lz;
        rc = g3d_kib_lzss12_decode(payload, payload_size, dst, dst_size, &inner_used_lz);
        if (rc != G3D_OK)
        {
            return rc;
        }
        if (inner_used_lz != payload_size)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    else if (method == G3D_KIB_METHOD_WAVELET8)
    {
        g3d_u32 ww;
        g3d_u32 hh;
        g3d_u32 pixels;
        if (dst == NULL)
        {
            if (payload_size < 6u)
            {
                return G3D_ERR_EOF;
            }
            ww = (g3d_u32)g3d_kib_rd_u16le(payload + 0u);
            hh = (g3d_u32)g3d_kib_rd_u16le(payload + 2u);
            rc = g3d_kib_mul_u32(ww, hh, &pixels);
            if (rc != G3D_OK)
            {
                return rc;
            }
            if (pixels != dst_size)
            {
                return G3D_ERR_CORRUPT;
            }
        }
        else
        {
            rc = g3d_kib_wavelet8_decode_arena(payload, payload_size, dst, dst_size, NULL, NULL, NULL, scratch_arena);
            if (rc != G3D_OK)
            {
                return rc;
            }
        }
    }
    else
    {
        return G3D_ERR_UNSUPPORTED;
    }
    *src_used = used;
    *method_out = method;
    return G3D_OK;
}

int g3d_kib_decode_auto_arena(const g3d_u8 *src, g3d_u32 src_size,
                              g3d_u8 *dst, g3d_u32 dst_size,
                              g3d_u32 flags,
                              g3d_u32 *src_used,
                              g3d_u32 *method_out,
                              g3d_arena *scratch_arena)
{
    g3d_u32 used;
    int rc;
    if ((src_size != 0u && src == NULL) || src_used == NULL || method_out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (src_size >= 4u && g3d_kib_rd_u32le(src) == G3D_KIB_MAGIC)
    {
        return g3d_kib_decode_container(src, src_size, dst, dst_size, src_used, method_out, scratch_arena);
    }
    if ((flags & G3D_KIB_DECODE_REQUIRE_CONTAINER) != 0u)
    {
        return G3D_ERR_BAD_MAGIC;
    }

    rc = g3d_kib_lzss12_decode(src, src_size, dst, dst_size, &used);
    if (rc == G3D_OK)
    {
        *src_used = used;
        *method_out = G3D_KIB_METHOD_LZSS12;
        return G3D_OK;
    }
    rc = g3d_kib_packbits_decode(src, src_size, dst, dst_size, &used);
    if (rc == G3D_OK)
    {
        *src_used = used;
        *method_out = G3D_KIB_METHOD_PACKBITS;
        return G3D_OK;
    }
    if ((flags & G3D_KIB_DECODE_ALLOW_RAW_FALLBACK) != 0u)
    {
        if (src_size < dst_size)
        {
            return G3D_ERR_EOF;
        }
        if (dst != NULL && dst_size != 0u)
        {
            memcpy(dst, src, (size_t)dst_size);
        }
        *src_used = dst_size;
        *method_out = G3D_KIB_METHOD_RAW_FALLBACK;
        return G3D_OK;
    }
    return G3D_ERR_UNSUPPORTED;
}

int g3d_kib_decode_auto(const g3d_u8 *src, g3d_u32 src_size,
                        g3d_u8 *dst, g3d_u32 dst_size,
                        g3d_u32 flags,
                        g3d_u32 *src_used,
                        g3d_u32 *method_out)
{
    return g3d_kib_decode_auto_arena(src, src_size, dst, dst_size, flags,
                                     src_used, method_out, NULL);
}
