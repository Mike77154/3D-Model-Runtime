#include "qmdl_internal.h"

#define QMDL_PNG_SIG_0 0x89u
#define QMDL_PNG_SIG_1 0x50u
#define QMDL_PNG_SIG_2 0x4Eu
#define QMDL_PNG_SIG_3 0x47u
#define QMDL_PNG_SIG_4 0x0Du
#define QMDL_PNG_SIG_5 0x0Au
#define QMDL_PNG_SIG_6 0x1Au
#define QMDL_PNG_SIG_7 0x0Au

#define QMDL_PNG_TYPE_IHDR 0x49484452u
#define QMDL_PNG_TYPE_PLTE 0x504C5445u
#define QMDL_PNG_TYPE_IDAT 0x49444154u
#define QMDL_PNG_TYPE_IEND 0x49454E44u

#define QMDL_PCX_HEADER_SIZE 128u
#define QMDL_PCX_PALETTE_MARKER 0x0Cu

static qmdl_u16 qmdl_img_load_le_u16(const qmdl_u8 *src)
{
    return (qmdl_u16)(((qmdl_u16)src[0]) | ((qmdl_u16)src[1] << 8));
}

static void qmdl_img_store_le_u16(qmdl_u8 *dst, qmdl_u16 value)
{
    dst[0] = (qmdl_u8)(value & 0xFFu);
    dst[1] = (qmdl_u8)((value >> 8) & 0xFFu);
}

static qmdl_u32 qmdl_img_load_be_u32(const qmdl_u8 *src)
{
    return ((qmdl_u32)src[0] << 24) |
           ((qmdl_u32)src[1] << 16) |
           ((qmdl_u32)src[2] << 8) |
           ((qmdl_u32)src[3]);
}

static void qmdl_img_store_be_u32(qmdl_u8 *dst, qmdl_u32 value)
{
    dst[0] = (qmdl_u8)((value >> 24) & 0xFFu);
    dst[1] = (qmdl_u8)((value >> 16) & 0xFFu);
    dst[2] = (qmdl_u8)((value >> 8) & 0xFFu);
    dst[3] = (qmdl_u8)(value & 0xFFu);
}

static int qmdl_img_compute_pixel_count(qmdl_u32 width,
                                        qmdl_u32 height,
                                        qmdl_u32 *pixel_count_out)
{
    qmdl_u32 pixel_count;

    if (pixel_count_out == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (width == 0u || height == 0u)
    {
        return QMDL_ERR_RANGE;
    }
    if (qmdl_u32_mul(width, height, &pixel_count) != QMDL_OK)
    {
        return QMDL_ERR_OVERFLOW;
    }
    *pixel_count_out = pixel_count;
    return QMDL_OK;
}

static int qmdl_img_model_skin_pixel_count(const qmdl_model *model,
                                           qmdl_u32 *pixel_count_out)
{
    if (model == 0 || pixel_count_out == 0)
    {
        return QMDL_ERR_ARGS;
    }
    return qmdl_img_compute_pixel_count(model->header.skinwidth,
                                        model->header.skinheight,
                                        pixel_count_out);
}

static int qmdl_img_get_skin_image_bounds(const qmdl_model *model,
                                          qmdl_u32 skin_index,
                                          qmdl_u32 image_index,
                                          qmdl_u32 *offset_out,
                                          qmdl_u32 *pixel_count_out)
{
    qmdl_u32 pixel_count;
    qmdl_u32 rel;
    qmdl_u32 image_count;
    qmdl_u32 end;
    int rc;

    if (model == 0 || offset_out == 0 || pixel_count_out == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (model->skins == 0 || model->skin_pixels == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (skin_index >= model->header.numskins)
    {
        return QMDL_ERR_RANGE;
    }

    rc = qmdl_img_model_skin_pixel_count(model, &pixel_count);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    image_count = model->skins[skin_index].num_images;
    if (image_count == 0u || image_index >= image_count)
    {
        return QMDL_ERR_RANGE;
    }
    if (qmdl_u32_mul(pixel_count, image_index, &rel) != QMDL_OK)
    {
        return QMDL_ERR_OVERFLOW;
    }
    if (qmdl_u32_add(&rel, model->skins[skin_index].first_pixel) != QMDL_OK)
    {
        return QMDL_ERR_OVERFLOW;
    }

    end = rel;
    if (qmdl_u32_add(&end, pixel_count) != QMDL_OK)
    {
        return QMDL_ERR_OVERFLOW;
    }
    if (end > model->num_skin_pixels)
    {
        return QMDL_ERR_RANGE;
    }

    *offset_out = rel;
    *pixel_count_out = pixel_count;
    return QMDL_OK;
}

void qmdl_palette_make_grayscale(qmdl_u8 *palette_rgb, qmdl_u32 num_entries)
{
    qmdl_u32 i;

    if (palette_rgb == 0)
    {
        return;
    }
    for (i = 0u; i < num_entries; ++i)
    {
        palette_rgb[i * 3u + 0u] = (qmdl_u8)i;
        palette_rgb[i * 3u + 1u] = (qmdl_u8)i;
        palette_rgb[i * 3u + 2u] = (qmdl_u8)i;
    }
}

int qmdl_palette_quantize_rgb(const qmdl_u8 *palette_rgb,
                              qmdl_u32 num_entries,
                              qmdl_u8 r,
                              qmdl_u8 g,
                              qmdl_u8 b,
                              qmdl_u8 *out_index)
{
    qmdl_u32 i;
    qmdl_u32 best_dist;
    qmdl_u32 best_index;

    if (palette_rgb == 0 || out_index == 0 || num_entries == 0u || num_entries > 256u)
    {
        return QMDL_ERR_ARGS;
    }

    best_dist = 0xFFFFFFFFu;
    best_index = 0u;
    for (i = 0u; i < num_entries; ++i)
    {
        qmdl_i32 dr;
        qmdl_i32 dg;
        qmdl_i32 db;
        qmdl_u32 dist;

        dr = (qmdl_i32)r - (qmdl_i32)palette_rgb[i * 3u + 0u];
        dg = (qmdl_i32)g - (qmdl_i32)palette_rgb[i * 3u + 1u];
        db = (qmdl_i32)b - (qmdl_i32)palette_rgb[i * 3u + 2u];
        dist = (qmdl_u32)(dr * dr + dg * dg + db * db);
        if (dist < best_dist)
        {
            best_dist = dist;
            best_index = i;
            if (dist == 0u)
            {
                break;
            }
        }
    }

    *out_index = (qmdl_u8)best_index;
    return QMDL_OK;
}

int qmdl_indexed_image_to_palette(const qmdl_indexed_image *image,
                                  const qmdl_u8 *target_palette_rgb,
                                  qmdl_u32 target_palette_entries,
                                  qmdl_u8 *dst_pixels,
                                  qmdl_u32 dst_pixels_cap)
{
    qmdl_u32 i;

    if (image == 0 || dst_pixels == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (image->pixels == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (image->pixel_count > dst_pixels_cap)
    {
        return QMDL_ERR_CAPACITY;
    }

    if (target_palette_rgb == 0 || target_palette_entries == 0u)
    {
        memcpy(dst_pixels, image->pixels, image->pixel_count);
        return QMDL_OK;
    }

    if (image->palette_rgb == 0 || image->num_palette_entries == 0u)
    {
        memcpy(dst_pixels, image->pixels, image->pixel_count);
        return QMDL_OK;
    }

    if (image->num_palette_entries == target_palette_entries &&
        memcmp(image->palette_rgb, target_palette_rgb, image->num_palette_entries * 3u) == 0)
    {
        memcpy(dst_pixels, image->pixels, image->pixel_count);
        return QMDL_OK;
    }

    for (i = 0u; i < image->pixel_count; ++i)
    {
        qmdl_u8 src_index;
        qmdl_u8 out_index;
        int rc;

        src_index = image->pixels[i];
        if ((qmdl_u32)src_index >= image->num_palette_entries)
        {
            return QMDL_ERR_RANGE;
        }
        rc = qmdl_palette_quantize_rgb(target_palette_rgb,
                                       target_palette_entries,
                                       image->palette_rgb[src_index * 3u + 0u],
                                       image->palette_rgb[src_index * 3u + 1u],
                                       image->palette_rgb[src_index * 3u + 2u],
                                       &out_index);
        if (rc != QMDL_OK)
        {
            return rc;
        }
        dst_pixels[i] = out_index;
    }

    return QMDL_OK;
}

int qmdl_model_skin_image_count(const qmdl_model *model,
                                qmdl_u32 skin_index,
                                qmdl_u32 *count_out)
{
    if (model == 0 || count_out == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (model->skins == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (skin_index >= model->header.numskins)
    {
        return QMDL_ERR_RANGE;
    }
    *count_out = model->skins[skin_index].num_images;
    return QMDL_OK;
}

int qmdl_model_skin_image_view(const qmdl_model *model,
                               qmdl_u32 skin_index,
                               qmdl_u32 image_index,
                               const qmdl_u8 **pixels_out,
                               qmdl_u32 *pixel_count_out)
{
    qmdl_u32 offset;
    qmdl_u32 pixel_count;
    int rc;

    if (pixels_out == 0)
    {
        return QMDL_ERR_ARGS;
    }

    rc = qmdl_img_get_skin_image_bounds(model, skin_index, image_index, &offset, &pixel_count);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    *pixels_out = model->skin_pixels + offset;
    if (pixel_count_out != 0)
    {
        *pixel_count_out = pixel_count;
    }
    return QMDL_OK;
}

int qmdl_model_skin_image_replace(qmdl_model *model,
                                  qmdl_u32 skin_index,
                                  qmdl_u32 image_index,
                                  const qmdl_u8 *pixels,
                                  qmdl_u32 pixel_count)
{
    qmdl_u32 offset;
    qmdl_u32 expected_count;
    int rc;

    if (model == 0 || pixels == 0)
    {
        return QMDL_ERR_ARGS;
    }

    rc = qmdl_img_get_skin_image_bounds(model, skin_index, image_index, &offset, &expected_count);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    if (pixel_count != expected_count)
    {
        return QMDL_ERR_RANGE;
    }

    memcpy(model->skin_pixels + offset, pixels, pixel_count);
    return QMDL_OK;
}

static int qmdl_pcx_rle_emit_run(qmdl_writer *wr, qmdl_u8 value, qmdl_u32 count)
{
    qmdl_u32 remaining;
    int rc;

    remaining = count;
    while (remaining != 0u)
    {
        qmdl_u32 run;

        run = (remaining > 63u) ? 63u : remaining;
        if (run == 1u && (value & 0xC0u) != 0xC0u)
        {
            rc = qmdl_write_u8(wr, value);
        }
        else
        {
            rc = qmdl_write_u8(wr, (qmdl_u8)(0xC0u | run));
            if (rc == QMDL_OK)
            {
                rc = qmdl_write_u8(wr, value);
            }
        }
        if (rc != QMDL_OK)
        {
            return rc;
        }
        remaining -= run;
    }
    return QMDL_OK;
}

int qmdl_pcx_encode_measure(qmdl_u32 width,
                            qmdl_u32 height,
                            const qmdl_u8 *pixels,
                            const qmdl_u8 *palette_rgb,
                            size_t *encoded_size)
{
    qmdl_u32 pixel_count;
    qmdl_u32 bytes_per_line;
    size_t max_body;
    int rc;

    (void)palette_rgb;
    if (pixels == 0 || encoded_size == 0)
    {
        return QMDL_ERR_ARGS;
    }
    rc = qmdl_img_compute_pixel_count(width, height, &pixel_count);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    if (width > 65535u || height > 65535u)
    {
        return QMDL_ERR_RANGE;
    }

    bytes_per_line = width;
    if ((bytes_per_line & 1u) != 0u)
    {
        bytes_per_line += 1u;
    }
    max_body = (size_t)bytes_per_line * (size_t)height * 2u;
    *encoded_size = (size_t)QMDL_PCX_HEADER_SIZE + max_body + 1u + (size_t)QMDL_PALETTE_RGB_BYTES;
    (void)pixel_count;
    return QMDL_OK;
}

int qmdl_pcx_encode(qmdl_u32 width,
                    qmdl_u32 height,
                    const qmdl_u8 *pixels,
                    const qmdl_u8 *palette_rgb,
                    void *dst,
                    size_t dst_size,
                    size_t *encoded_size)
{
    qmdl_writer wr;
    qmdl_u8 header[QMDL_PCX_HEADER_SIZE];
    qmdl_u32 bytes_per_line;
    qmdl_u32 row;
    qmdl_u32 col;
    qmdl_u32 pixel_count;
    int rc;

    if (pixels == 0 || dst == 0)
    {
        return QMDL_ERR_ARGS;
    }
    rc = qmdl_img_compute_pixel_count(width, height, &pixel_count);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    if (width > 65535u || height > 65535u)
    {
        return QMDL_ERR_RANGE;
    }

    bytes_per_line = width;
    if ((bytes_per_line & 1u) != 0u)
    {
        bytes_per_line += 1u;
    }

    memset(header, 0, sizeof(header));
    header[0] = 0x0Au;
    header[1] = 5u;
    header[2] = 1u;
    header[3] = 8u;
    qmdl_img_store_le_u16(header + 4u, 0u);
    qmdl_img_store_le_u16(header + 6u, 0u);
    qmdl_img_store_le_u16(header + 8u, (qmdl_u16)(width - 1u));
    qmdl_img_store_le_u16(header + 10u, (qmdl_u16)(height - 1u));
    qmdl_img_store_le_u16(header + 12u, (qmdl_u16)width);
    qmdl_img_store_le_u16(header + 14u, (qmdl_u16)height);
    header[65] = 1u;
    qmdl_img_store_le_u16(header + 66u, (qmdl_u16)bytes_per_line);
    qmdl_img_store_le_u16(header + 68u, 1u);

    wr.ptr = (qmdl_u8 *)dst;
    wr.end = wr.ptr + dst_size;

    rc = qmdl_write_bytes(&wr, header, sizeof(header));
    if (rc != QMDL_OK)
    {
        return rc;
    }

    for (row = 0u; row < height; ++row)
    {
        const qmdl_u8 *row_pixels;
        qmdl_u32 run_count;
        qmdl_u8 run_value;

        row_pixels = pixels + (size_t)row * (size_t)width;
        run_count = 0u;
        run_value = 0u;
        for (col = 0u; col < bytes_per_line; ++col)
        {
            qmdl_u8 value;

            value = (col < width) ? row_pixels[col] : 0u;
            if (run_count == 0u)
            {
                run_value = value;
                run_count = 1u;
            }
            else if (value == run_value && run_count < 63u)
            {
                run_count += 1u;
            }
            else
            {
                rc = qmdl_pcx_rle_emit_run(&wr, run_value, run_count);
                if (rc != QMDL_OK)
                {
                    return rc;
                }
                run_value = value;
                run_count = 1u;
            }
        }
        if (run_count != 0u)
        {
            rc = qmdl_pcx_rle_emit_run(&wr, run_value, run_count);
            if (rc != QMDL_OK)
            {
                return rc;
            }
        }
    }

    rc = qmdl_write_u8(&wr, QMDL_PCX_PALETTE_MARKER);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    if (palette_rgb != 0)
    {
        rc = qmdl_write_bytes(&wr, palette_rgb, QMDL_PALETTE_RGB_BYTES);
        if (rc != QMDL_OK)
        {
            return rc;
        }
    }
    else
    {
        for (col = 0u; col < 256u; ++col)
        {
            rc = qmdl_write_u8(&wr, (qmdl_u8)col);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_write_u8(&wr, (qmdl_u8)col);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_write_u8(&wr, (qmdl_u8)col);
            if (rc != QMDL_OK) return rc;
        }
    }

    if (encoded_size != 0)
    {
        *encoded_size = (size_t)(wr.ptr - (qmdl_u8 *)dst);
    }
    return QMDL_OK;
}

int qmdl_pcx_decode(const void *data,
                    size_t data_size,
                    const qmdl_indexed_image_storage *storage,
                    qmdl_indexed_image *image)
{
    const qmdl_u8 *bytes;
    qmdl_u32 width;
    qmdl_u32 height;
    qmdl_u32 pixel_count;
    qmdl_u32 bytes_per_line;
    qmdl_u32 row;
    qmdl_u32 col;
    size_t pos;
    qmdl_u32 cur_row;
    qmdl_u32 cur_col;
    qmdl_u8 value;
    int has_palette;

    if (data == 0 || storage == 0 || image == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (storage->pixels == 0 || storage->palette_rgb == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (data_size < QMDL_PCX_HEADER_SIZE)
    {
        return QMDL_ERR_SIZE;
    }

    bytes = (const qmdl_u8 *)data;
    if (bytes[0] != 0x0Au)
    {
        return QMDL_ERR_FORMAT;
    }
    if (bytes[2] > 1u)
    {
        return QMDL_ERR_FORMAT;
    }
    if (bytes[3] != 8u || bytes[65] != 1u)
    {
        return QMDL_ERR_FORMAT;
    }

    if (qmdl_img_load_le_u16(bytes + 8u) < qmdl_img_load_le_u16(bytes + 4u) ||
        qmdl_img_load_le_u16(bytes + 10u) < qmdl_img_load_le_u16(bytes + 6u))
    {
        return QMDL_ERR_FORMAT;
    }

    width = (qmdl_u32)(qmdl_img_load_le_u16(bytes + 8u) - qmdl_img_load_le_u16(bytes + 4u) + 1u);
    height = (qmdl_u32)(qmdl_img_load_le_u16(bytes + 10u) - qmdl_img_load_le_u16(bytes + 6u) + 1u);
    bytes_per_line = (qmdl_u32)qmdl_img_load_le_u16(bytes + 66u);

    if (bytes_per_line < width)
    {
        return QMDL_ERR_FORMAT;
    }

    if (qmdl_img_compute_pixel_count(width, height, &pixel_count) != QMDL_OK)
    {
        return QMDL_ERR_OVERFLOW;
    }
    if (pixel_count > storage->pixels_cap)
    {
        return QMDL_ERR_CAPACITY;
    }
    if (storage->palette_rgb_cap < QMDL_PALETTE_RGB_BYTES)
    {
        return QMDL_ERR_CAPACITY;
    }

    pos = QMDL_PCX_HEADER_SIZE;
    cur_row = 0u;
    cur_col = 0u;
    while (cur_row < height)
    {
        qmdl_u8 count;
        if (pos >= data_size)
        {
            return QMDL_ERR_SIZE;
        }
        value = bytes[pos++];
        if (bytes[2] == 1u && (value & 0xC0u) == 0xC0u)
        {
            count = (qmdl_u8)(value & 0x3Fu);
            if (pos >= data_size)
            {
                return QMDL_ERR_SIZE;
            }
            value = bytes[pos++];
        }
        else
        {
            count = 1u;
        }

        while (count != 0u)
        {
            if (cur_col < width)
            {
                storage->pixels[(size_t)cur_row * (size_t)width + (size_t)cur_col] = value;
            }
            cur_col += 1u;
            if (cur_col >= bytes_per_line)
            {
                cur_col = 0u;
                cur_row += 1u;
                if (cur_row > height)
                {
                    return QMDL_ERR_FORMAT;
                }
            }
            count -= 1u;
        }
    }

    has_palette = 0;
    if (data_size >= QMDL_PCX_HEADER_SIZE + 1u + QMDL_PALETTE_RGB_BYTES &&
        bytes[data_size - (QMDL_PALETTE_RGB_BYTES + 1u)] == QMDL_PCX_PALETTE_MARKER)
    {
        memcpy(storage->palette_rgb,
               bytes + data_size - QMDL_PALETTE_RGB_BYTES,
               QMDL_PALETTE_RGB_BYTES);
        has_palette = 1;
    }
    if (!has_palette)
    {
        qmdl_palette_make_grayscale(storage->palette_rgb, 256u);
    }

    qmdl_indexed_image_clear(image);
    image->width = width;
    image->height = height;
    image->pixels = storage->pixels;
    image->pixel_count = pixel_count;
    image->palette_rgb = storage->palette_rgb;
    image->num_palette_entries = 256u;
    (void)row;
    (void)col;
    return QMDL_OK;
}

typedef struct qmdl_png_raw_stream_s {
    qmdl_u32 width;
    qmdl_u32 height;
    const qmdl_u8 *pixels;
    qmdl_u32 row;
    qmdl_u32 col;
} qmdl_png_raw_stream;

typedef struct qmdl_png_idat_stream_s {
    const qmdl_u8 *chunk_ptr;
    size_t chunk_rem;
    const qmdl_u8 *next_chunk;
    const qmdl_u8 *file_end;
} qmdl_png_idat_stream;

typedef struct qmdl_png_bit_reader_s {
    qmdl_png_idat_stream *stream;
    qmdl_u32 bits;
    int bit_count;
} qmdl_png_bit_reader;

typedef struct qmdl_png_scanline_state_s {
    qmdl_u32 width;
    qmdl_u32 height;
    qmdl_u32 row;
    qmdl_u32 col;
    qmdl_u8 filter;
    int have_filter;
    qmdl_u8 *pixels;
    qmdl_u8 *prev_row;
    qmdl_u8 *cur_row;
} qmdl_png_scanline_state;

static qmdl_u32 qmdl_png_crc32_update(qmdl_u32 crc, const qmdl_u8 *data, size_t size)
{
    size_t i;
    qmdl_u32 c;
    int bit;

    c = crc;
    for (i = 0u; i < size; ++i)
    {
        c ^= (qmdl_u32)data[i];
        for (bit = 0; bit < 8; ++bit)
        {
            if ((c & 1u) != 0u)
            {
                c = (c >> 1) ^ 0xEDB88320u;
            }
            else
            {
                c >>= 1;
            }
        }
    }
    return c;
}

static qmdl_u32 qmdl_png_adler32_update(qmdl_u32 adler, qmdl_u8 value)
{
    qmdl_u32 s1;
    qmdl_u32 s2;

    s1 = adler & 0xFFFFu;
    s2 = (adler >> 16) & 0xFFFFu;
    s1 = (s1 + value) % 65521u;
    s2 = (s2 + s1) % 65521u;
    return (s2 << 16) | s1;
}

static int qmdl_png_write_chunk_header(qmdl_writer *wr,
                                       qmdl_u32 type,
                                       qmdl_u32 length,
                                       qmdl_u32 *crc)
{
    qmdl_u8 header[8];

    if (crc == 0)
    {
        return QMDL_ERR_ARGS;
    }
    qmdl_img_store_be_u32(header + 0u, length);
    qmdl_img_store_be_u32(header + 4u, type);
    *crc = 0xFFFFFFFFu;
    *crc = qmdl_png_crc32_update(*crc, header + 4u, 4u);
    return qmdl_write_bytes(wr, header, sizeof(header));
}

static int qmdl_png_write_chunk_data(qmdl_writer *wr,
                                     qmdl_u32 *crc,
                                     const qmdl_u8 *data,
                                     size_t size)
{
    int rc;

    if (wr == 0 || crc == 0 || (size != 0u && data == 0))
    {
        return QMDL_ERR_ARGS;
    }
    if (size != 0u)
    {
        *crc = qmdl_png_crc32_update(*crc, data, size);
        rc = qmdl_write_bytes(wr, data, size);
        if (rc != QMDL_OK)
        {
            return rc;
        }
    }
    return QMDL_OK;
}

static int qmdl_png_write_chunk_footer(qmdl_writer *wr, qmdl_u32 crc)
{
    return qmdl_write_u32_le(wr, ((crc ^ 0xFFFFFFFFu) >> 24) |
                                 (((crc ^ 0xFFFFFFFFu) >> 8) & 0x0000FF00u) |
                                 (((crc ^ 0xFFFFFFFFu) << 8) & 0x00FF0000u) |
                                 ((crc ^ 0xFFFFFFFFu) << 24));
}

static int qmdl_png_raw_next_byte(qmdl_png_raw_stream *stream, qmdl_u8 *out_value)
{
    if (stream == 0 || out_value == 0)
    {
        return 0;
    }
    if (stream->row >= stream->height)
    {
        return 0;
    }
    if (stream->col == 0u)
    {
        *out_value = 0u;
        stream->col = 1u;
        return 1;
    }

    *out_value = stream->pixels[(size_t)stream->row * (size_t)stream->width + (size_t)(stream->col - 1u)];
    stream->col += 1u;
    if (stream->col > stream->width)
    {
        stream->col = 0u;
        stream->row += 1u;
    }
    return 1;
}

int qmdl_png_encode_measure(qmdl_u32 width,
                            qmdl_u32 height,
                            const qmdl_u8 *pixels,
                            const qmdl_u8 *palette_rgb,
                            size_t *encoded_size)
{
    qmdl_u32 pixel_count;
    size_t raw_size;
    size_t zlib_size;
    size_t blocks;
    int rc;

    (void)palette_rgb;
    if (pixels == 0 || encoded_size == 0)
    {
        return QMDL_ERR_ARGS;
    }
    rc = qmdl_img_compute_pixel_count(width, height, &pixel_count);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    raw_size = (size_t)height * ((size_t)width + 1u);
    blocks = (raw_size + 65534u) / 65535u;
    zlib_size = 2u + raw_size + blocks * 5u + 4u;
    *encoded_size = 8u + (12u + 13u) + (12u + QMDL_PALETTE_RGB_BYTES) + (12u + zlib_size) + 12u;
    (void)pixel_count;
    return QMDL_OK;
}

int qmdl_png_encode(qmdl_u32 width,
                    qmdl_u32 height,
                    const qmdl_u8 *pixels,
                    const qmdl_u8 *palette_rgb,
                    void *dst,
                    size_t dst_size,
                    size_t *encoded_size)
{
    qmdl_writer wr;
    qmdl_u8 ihdr[13];
    qmdl_u8 gray_palette[QMDL_PALETTE_RGB_BYTES];
    qmdl_u32 crc;
    qmdl_u32 pixel_count;
    qmdl_u32 adler;
    size_t raw_size;
    size_t remaining;
    qmdl_png_raw_stream raw_stream;
    int rc;

    if (pixels == 0 || dst == 0)
    {
        return QMDL_ERR_ARGS;
    }
    rc = qmdl_img_compute_pixel_count(width, height, &pixel_count);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    raw_size = (size_t)height * ((size_t)width + 1u);

    wr.ptr = (qmdl_u8 *)dst;
    wr.end = wr.ptr + dst_size;

    {
        static const qmdl_u8 sig[8] = {
            QMDL_PNG_SIG_0, QMDL_PNG_SIG_1, QMDL_PNG_SIG_2, QMDL_PNG_SIG_3,
            QMDL_PNG_SIG_4, QMDL_PNG_SIG_5, QMDL_PNG_SIG_6, QMDL_PNG_SIG_7
        };
        rc = qmdl_write_bytes(&wr, sig, sizeof(sig));
        if (rc != QMDL_OK)
        {
            return rc;
        }
    }

    qmdl_img_store_be_u32(ihdr + 0u, width);
    qmdl_img_store_be_u32(ihdr + 4u, height);
    ihdr[8] = 8u;
    ihdr[9] = 3u;
    ihdr[10] = 0u;
    ihdr[11] = 0u;
    ihdr[12] = 0u;
    rc = qmdl_png_write_chunk_header(&wr, QMDL_PNG_TYPE_IHDR, 13u, &crc);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_png_write_chunk_data(&wr, &crc, ihdr, sizeof(ihdr));
    if (rc != QMDL_OK) return rc;
    rc = qmdl_png_write_chunk_footer(&wr, crc);
    if (rc != QMDL_OK) return rc;

    if (palette_rgb == 0)
    {
        qmdl_palette_make_grayscale(gray_palette, 256u);
        palette_rgb = gray_palette;
    }
    rc = qmdl_png_write_chunk_header(&wr, QMDL_PNG_TYPE_PLTE, QMDL_PALETTE_RGB_BYTES, &crc);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_png_write_chunk_data(&wr, &crc, palette_rgb, QMDL_PALETTE_RGB_BYTES);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_png_write_chunk_footer(&wr, crc);
    if (rc != QMDL_OK) return rc;

    {
        size_t zlib_size;
        size_t blocks;
        blocks = (raw_size + 65534u) / 65535u;
        zlib_size = 2u + raw_size + blocks * 5u + 4u;
        rc = qmdl_png_write_chunk_header(&wr, QMDL_PNG_TYPE_IDAT, (qmdl_u32)zlib_size, &crc);
        if (rc != QMDL_OK) return rc;
    }

    {
        qmdl_u8 zhdr[2];
        zhdr[0] = 0x78u;
        zhdr[1] = 0x01u;
        rc = qmdl_png_write_chunk_data(&wr, &crc, zhdr, 2u);
        if (rc != QMDL_OK) return rc;
    }

    raw_stream.width = width;
    raw_stream.height = height;
    raw_stream.pixels = pixels;
    raw_stream.row = 0u;
    raw_stream.col = 0u;
    remaining = raw_size;
    adler = 1u;

    while (remaining != 0u)
    {
        qmdl_u8 hdr[5];
        qmdl_u32 block_len;
        qmdl_u8 final_flag;
        qmdl_u32 i;

        block_len = (remaining > 65535u) ? 65535u : (qmdl_u32)remaining;
        final_flag = (remaining == (size_t)block_len) ? 1u : 0u;
        hdr[0] = final_flag;
        hdr[1] = (qmdl_u8)(block_len & 0xFFu);
        hdr[2] = (qmdl_u8)((block_len >> 8) & 0xFFu);
        hdr[3] = (qmdl_u8)(~block_len & 0xFFu);
        hdr[4] = (qmdl_u8)(((~block_len) >> 8) & 0xFFu);
        rc = qmdl_png_write_chunk_data(&wr, &crc, hdr, sizeof(hdr));
        if (rc != QMDL_OK) return rc;

        for (i = 0u; i < block_len; ++i)
        {
            qmdl_u8 value;
            if (!qmdl_png_raw_next_byte(&raw_stream, &value))
            {
                return QMDL_ERR_FORMAT;
            }
            adler = qmdl_png_adler32_update(adler, value);
            rc = qmdl_png_write_chunk_data(&wr, &crc, &value, 1u);
            if (rc != QMDL_OK) return rc;
        }
        remaining -= block_len;
    }

    {
        qmdl_u8 atrail[4];
        qmdl_img_store_be_u32(atrail, adler);
        rc = qmdl_png_write_chunk_data(&wr, &crc, atrail, sizeof(atrail));
        if (rc != QMDL_OK) return rc;
    }
    rc = qmdl_png_write_chunk_footer(&wr, crc);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_png_write_chunk_header(&wr, QMDL_PNG_TYPE_IEND, 0u, &crc);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_png_write_chunk_footer(&wr, crc);
    if (rc != QMDL_OK) return rc;

    if (encoded_size != 0)
    {
        *encoded_size = (size_t)(wr.ptr - (qmdl_u8 *)dst);
    }
    (void)pixel_count;
    return QMDL_OK;
}

static int qmdl_png_stream_init(qmdl_png_idat_stream *stream,
                                const qmdl_u8 *chunk_data,
                                size_t chunk_len,
                                const qmdl_u8 *next_chunk,
                                const qmdl_u8 *file_end)
{
    if (stream == 0)
    {
        return QMDL_ERR_ARGS;
    }
    stream->chunk_ptr = chunk_data;
    stream->chunk_rem = chunk_len;
    stream->next_chunk = next_chunk;
    stream->file_end = file_end;
    return QMDL_OK;
}

static int qmdl_png_stream_read_byte(qmdl_png_idat_stream *stream, qmdl_u8 *out_value)
{
    if (stream == 0 || out_value == 0)
    {
        return QMDL_ERR_ARGS;
    }

    while (stream->chunk_rem == 0u)
    {
        qmdl_u32 length;
        qmdl_u32 type;
        const qmdl_u8 *p;

        p = stream->next_chunk;
        if (p == 0 || (size_t)(stream->file_end - p) < 12u)
        {
            return QMDL_ERR_SIZE;
        }
        length = qmdl_img_load_be_u32(p + 0u);
        type = qmdl_img_load_be_u32(p + 4u);
        if ((size_t)(stream->file_end - (p + 8u)) < (size_t)length + 4u)
        {
            return QMDL_ERR_SIZE;
        }
        if (type != QMDL_PNG_TYPE_IDAT)
        {
            return QMDL_ERR_SIZE;
        }
        stream->chunk_ptr = p + 8u;
        stream->chunk_rem = (size_t)length;
        stream->next_chunk = p + 8u + (size_t)length + 4u;
    }

    *out_value = *stream->chunk_ptr;
    stream->chunk_ptr += 1;
    stream->chunk_rem -= 1u;
    return QMDL_OK;
}

static int qmdl_png_bits_read(qmdl_png_bit_reader *br, int count, qmdl_u32 *out_value)
{
    qmdl_u32 value;
    int shift;
    int rc;

    if (br == 0 || out_value == 0 || count < 0 || count > 16)
    {
        return QMDL_ERR_ARGS;
    }

    value = 0u;
    shift = 0;
    while (count > 0)
    {
        if (br->bit_count == 0)
        {
            qmdl_u8 byte;
            rc = qmdl_png_stream_read_byte(br->stream, &byte);
            if (rc != QMDL_OK)
            {
                return rc;
            }
            br->bits = byte;
            br->bit_count = 8;
        }

        value |= ((br->bits & 1u) << shift);
        br->bits >>= 1;
        br->bit_count -= 1;
        shift += 1;
        count -= 1;
    }

    *out_value = value;
    return QMDL_OK;
}

static void qmdl_png_bits_align(qmdl_png_bit_reader *br)
{
    if (br != 0)
    {
        br->bits = 0u;
        br->bit_count = 0;
    }
}

static qmdl_u8 qmdl_png_paeth(qmdl_u8 a, qmdl_u8 b, qmdl_u8 c)
{
    qmdl_i32 p;
    qmdl_i32 pa;
    qmdl_i32 pb;
    qmdl_i32 pc;

    p = (qmdl_i32)a + (qmdl_i32)b - (qmdl_i32)c;
    pa = p - (qmdl_i32)a;
    if (pa < 0) pa = -pa;
    pb = p - (qmdl_i32)b;
    if (pb < 0) pb = -pb;
    pc = p - (qmdl_i32)c;
    if (pc < 0) pc = -pc;

    if (pa <= pb && pa <= pc)
    {
        return a;
    }
    if (pb <= pc)
    {
        return b;
    }
    return c;
}

static int qmdl_png_scanline_consume(qmdl_png_scanline_state *state, qmdl_u8 byte)
{
    qmdl_u8 left;
    qmdl_u8 up;
    qmdl_u8 upleft;
    qmdl_u8 recon;

    if (state == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (state->row >= state->height)
    {
        return QMDL_ERR_FORMAT;
    }
    if (!state->have_filter)
    {
        if (byte > 4u)
        {
            return QMDL_ERR_FORMAT;
        }
        state->filter = byte;
        state->have_filter = 1;
        state->col = 0u;
        return QMDL_OK;
    }

    left = (state->col != 0u) ? state->cur_row[state->col - 1u] : 0u;
    up = state->prev_row[state->col];
    upleft = (state->col != 0u) ? state->prev_row[state->col - 1u] : 0u;

    switch (state->filter)
    {
        case 0u: recon = byte; break;
        case 1u: recon = (qmdl_u8)(byte + left); break;
        case 2u: recon = (qmdl_u8)(byte + up); break;
        case 3u: recon = (qmdl_u8)(byte + (qmdl_u8)(((qmdl_u32)left + (qmdl_u32)up) / 2u)); break;
        case 4u: recon = (qmdl_u8)(byte + qmdl_png_paeth(left, up, upleft)); break;
        default: return QMDL_ERR_FORMAT;
    }

    state->cur_row[state->col] = recon;
    state->col += 1u;
    if (state->col == state->width)
    {
        memcpy(state->pixels + (size_t)state->row * (size_t)state->width,
               state->cur_row,
               state->width);
        memcpy(state->prev_row, state->cur_row, state->width);
        state->row += 1u;
        state->have_filter = 0;
        state->col = 0u;
    }
    return QMDL_OK;
}

int qmdl_png_decode(const void *data,
                    size_t data_size,
                    const qmdl_indexed_image_storage *storage,
                    qmdl_indexed_image *image)
{
    const qmdl_u8 *bytes;
    const qmdl_u8 *p;
    const qmdl_u8 *end;
    qmdl_u32 width;
    qmdl_u32 height;
    qmdl_u8 bit_depth;
    qmdl_u8 color_type;
    qmdl_u8 compression_method;
    qmdl_u8 filter_method;
    qmdl_u8 interlace_method;
    qmdl_u32 palette_entries;
    const qmdl_u8 *first_idat_data;
    size_t first_idat_len;
    const qmdl_u8 *after_first_idat;
    qmdl_u32 pixel_count;
    qmdl_png_idat_stream stream;
    qmdl_png_bit_reader br;
    qmdl_png_scanline_state scan;
    qmdl_u32 adler;
    qmdl_u32 trailer_adler;
    int rc;

    if (data == 0 || storage == 0 || image == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (storage->pixels == 0 || storage->palette_rgb == 0 || storage->scratch == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (data_size < 8u)
    {
        return QMDL_ERR_SIZE;
    }

    bytes = (const qmdl_u8 *)data;
    if (bytes[0] != QMDL_PNG_SIG_0 || bytes[1] != QMDL_PNG_SIG_1 ||
        bytes[2] != QMDL_PNG_SIG_2 || bytes[3] != QMDL_PNG_SIG_3 ||
        bytes[4] != QMDL_PNG_SIG_4 || bytes[5] != QMDL_PNG_SIG_5 ||
        bytes[6] != QMDL_PNG_SIG_6 || bytes[7] != QMDL_PNG_SIG_7)
    {
        return QMDL_ERR_FORMAT;
    }

    width = 0u;
    height = 0u;
    bit_depth = 0u;
    color_type = 0u;
    compression_method = 0u;
    filter_method = 0u;
    interlace_method = 0u;
    palette_entries = 0u;
    first_idat_data = 0;
    first_idat_len = 0u;
    after_first_idat = 0;
    p = bytes + 8u;
    end = bytes + data_size;

    while ((size_t)(end - p) >= 12u)
    {
        qmdl_u32 length;
        qmdl_u32 type;
        const qmdl_u8 *chunk_data;
        const qmdl_u8 *chunk_end;

        length = qmdl_img_load_be_u32(p + 0u);
        type = qmdl_img_load_be_u32(p + 4u);
        chunk_data = p + 8u;
        if ((size_t)(end - chunk_data) < (size_t)length + 4u)
        {
            return QMDL_ERR_SIZE;
        }
        chunk_end = chunk_data + (size_t)length;

        if (type == QMDL_PNG_TYPE_IHDR)
        {
            if (length != 13u)
            {
                return QMDL_ERR_FORMAT;
            }
            width = qmdl_img_load_be_u32(chunk_data + 0u);
            height = qmdl_img_load_be_u32(chunk_data + 4u);
            bit_depth = chunk_data[8];
            color_type = chunk_data[9];
            compression_method = chunk_data[10];
            filter_method = chunk_data[11];
            interlace_method = chunk_data[12];
        }
        else if (type == QMDL_PNG_TYPE_PLTE)
        {
            if ((length % 3u) != 0u || length == 0u || length > QMDL_PALETTE_RGB_BYTES)
            {
                return QMDL_ERR_FORMAT;
            }
            if (storage->palette_rgb_cap < length)
            {
                return QMDL_ERR_CAPACITY;
            }
            memcpy(storage->palette_rgb, chunk_data, length);
            palette_entries = length / 3u;
        }
        else if (type == QMDL_PNG_TYPE_IDAT)
        {
            first_idat_data = chunk_data;
            first_idat_len = (size_t)length;
            after_first_idat = chunk_end + 4u;
            break;
        }
        else if (type == QMDL_PNG_TYPE_IEND)
        {
            break;
        }

        p = chunk_end + 4u;
    }

    if (width == 0u || height == 0u || first_idat_data == 0)
    {
        return QMDL_ERR_FORMAT;
    }
    if (bit_depth != 8u || color_type != 3u || compression_method != 0u ||
        filter_method != 0u || interlace_method != 0u)
    {
        return QMDL_ERR_FORMAT;
    }
    if (palette_entries == 0u)
    {
        return QMDL_ERR_FORMAT;
    }
    rc = qmdl_img_compute_pixel_count(width, height, &pixel_count);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    if (pixel_count > storage->pixels_cap)
    {
        return QMDL_ERR_CAPACITY;
    }
    if (storage->scratch_cap < width * 2u)
    {
        return QMDL_ERR_CAPACITY;
    }

    qmdl_png_stream_init(&stream, first_idat_data, first_idat_len, after_first_idat, end);
    br.stream = &stream;
    br.bits = 0u;
    br.bit_count = 0;

    {
        qmdl_u8 cmf;
        qmdl_u8 flg;
        rc = qmdl_png_stream_read_byte(&stream, &cmf);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_png_stream_read_byte(&stream, &flg);
        if (rc != QMDL_OK) return rc;
        if ((cmf & 0x0Fu) != 8u || (cmf >> 4) > 7u)
        {
            return QMDL_ERR_FORMAT;
        }
        if ((((qmdl_u32)cmf << 8) | (qmdl_u32)flg) % 31u != 0u)
        {
            return QMDL_ERR_FORMAT;
        }
        if ((flg & 0x20u) != 0u)
        {
            return QMDL_ERR_FORMAT;
        }
    }

    memset(storage->scratch, 0, width * 2u);
    scan.width = width;
    scan.height = height;
    scan.row = 0u;
    scan.col = 0u;
    scan.filter = 0u;
    scan.have_filter = 0;
    scan.pixels = storage->pixels;
    scan.prev_row = storage->scratch;
    scan.cur_row = storage->scratch + width;
    adler = 1u;

    for (;;)
    {
        qmdl_u32 final_flag;
        qmdl_u32 btype;
        qmdl_u32 len;
        qmdl_u32 nlen;
        qmdl_u32 i;

        rc = qmdl_png_bits_read(&br, 1, &final_flag);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_png_bits_read(&br, 2, &btype);
        if (rc != QMDL_OK) return rc;
        if (btype != 0u)
        {
            return QMDL_ERR_FORMAT;
        }
        qmdl_png_bits_align(&br);

        {
            qmdl_u8 l0;
            qmdl_u8 l1;
            qmdl_u8 n0;
            qmdl_u8 n1;
            rc = qmdl_png_stream_read_byte(&stream, &l0);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_png_stream_read_byte(&stream, &l1);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_png_stream_read_byte(&stream, &n0);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_png_stream_read_byte(&stream, &n1);
            if (rc != QMDL_OK) return rc;
            len = (qmdl_u32)l0 | ((qmdl_u32)l1 << 8);
            nlen = (qmdl_u32)n0 | ((qmdl_u32)n1 << 8);
            if (((len ^ 0xFFFFu) & 0xFFFFu) != nlen)
            {
                return QMDL_ERR_FORMAT;
            }
        }

        for (i = 0u; i < len; ++i)
        {
            qmdl_u8 value;
            rc = qmdl_png_stream_read_byte(&stream, &value);
            if (rc != QMDL_OK) return rc;
            adler = qmdl_png_adler32_update(adler, value);
            rc = qmdl_png_scanline_consume(&scan, value);
            if (rc != QMDL_OK) return rc;
        }

        if (final_flag != 0u)
        {
            break;
        }
    }

    {
        qmdl_u8 a0;
        qmdl_u8 a1;
        qmdl_u8 a2;
        qmdl_u8 a3;
        rc = qmdl_png_stream_read_byte(&stream, &a0);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_png_stream_read_byte(&stream, &a1);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_png_stream_read_byte(&stream, &a2);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_png_stream_read_byte(&stream, &a3);
        if (rc != QMDL_OK) return rc;
        trailer_adler = ((qmdl_u32)a0 << 24) | ((qmdl_u32)a1 << 16) |
                        ((qmdl_u32)a2 << 8) | (qmdl_u32)a3;
    }
    if (adler != trailer_adler)
    {
        return QMDL_ERR_FORMAT;
    }
    if (scan.row != height || scan.have_filter)
    {
        return QMDL_ERR_FORMAT;
    }

    qmdl_indexed_image_clear(image);
    image->width = width;
    image->height = height;
    image->pixels = storage->pixels;
    image->pixel_count = pixel_count;
    image->palette_rgb = storage->palette_rgb;
    image->num_palette_entries = palette_entries;
    return QMDL_OK;
}

int qmdl_decode_pak_mdl(const void *pak_data,
                        size_t pak_size,
                        const char *entry_name,
                        const qmdl_storage *storage,
                        qmdl_model *model,
                        qmdl_requirements *used)
{
    qmdl_pak_archive archive;
    qmdl_pak_entry entry;
    const void *view;
    size_t view_size;
    int rc;

    if (pak_data == 0 || entry_name == 0 || storage == 0 || model == 0)
    {
        return QMDL_ERR_ARGS;
    }

    qmdl_pak_archive_clear(&archive);
    qmdl_pak_entry_clear(&entry);
    rc = qmdl_pak_open(pak_data, pak_size, &archive);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    rc = qmdl_pak_find_entry(&archive, entry_name, 0, &entry);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    rc = qmdl_pak_entry_view(&archive, &entry, &view, &view_size);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    return qmdl_decode(view, view_size, storage, model, used);
}

int qmdl_pak_find_quake_palette(const qmdl_pak_archive *archive,
                                const qmdl_u8 **palette_out,
                                size_t *palette_size_out)
{
    qmdl_pak_entry entry;
    const void *view;
    size_t view_size;
    int rc;

    if (archive == 0 || palette_out == 0)
    {
        return QMDL_ERR_ARGS;
    }
    qmdl_pak_entry_clear(&entry);
    rc = qmdl_pak_find_entry(archive, "gfx/palette.lmp", 0, &entry);
    if (rc != QMDL_OK)
    {
        rc = qmdl_pak_find_entry(archive, "PALETTE.LMP", 0, &entry);
    }
    if (rc != QMDL_OK)
    {
        rc = qmdl_pak_find_entry(archive, "palette.lmp", 0, &entry);
    }
    if (rc != QMDL_OK)
    {
        return rc;
    }
    rc = qmdl_pak_entry_view(archive, &entry, &view, &view_size);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    if (view_size < QMDL_PALETTE_RGB_BYTES)
    {
        return QMDL_ERR_SIZE;
    }
    *palette_out = (const qmdl_u8 *)view;
    if (palette_size_out != 0)
    {
        *palette_size_out = view_size;
    }
    return QMDL_OK;
}

int qmdl_wad2_find_palette(const qmdl_wad2_archive *archive,
                           const qmdl_u8 **palette_out,
                           size_t *palette_size_out)
{
    qmdl_wad2_entry entry;
    const void *view;
    size_t view_size;
    int rc;

    if (archive == 0 || palette_out == 0)
    {
        return QMDL_ERR_ARGS;
    }
    qmdl_wad2_entry_clear(&entry);
    rc = qmdl_wad2_find_entry(archive, "PALETTE", 0, &entry);
    if (rc != QMDL_OK)
    {
        rc = qmdl_wad2_find_entry(archive, "palette", 0, &entry);
    }
    if (rc != QMDL_OK)
    {
        return rc;
    }
    rc = qmdl_wad2_entry_view(archive, &entry, &view, &view_size);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    if (view_size < QMDL_PALETTE_RGB_BYTES)
    {
        return QMDL_ERR_SIZE;
    }
    *palette_out = (const qmdl_u8 *)view;
    if (palette_size_out != 0)
    {
        *palette_size_out = view_size;
    }
    return QMDL_OK;
}
