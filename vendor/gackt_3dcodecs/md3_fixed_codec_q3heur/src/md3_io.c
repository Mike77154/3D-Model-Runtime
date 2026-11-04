#include "md3_internal.h"

const char *md3_error_string(int code)
{
    switch (code)
    {
        case MD3_OK: return "MD3_OK";
        case MD3_EINVAL: return "MD3_EINVAL";
        case MD3_EIO: return "MD3_EIO";
        case MD3_EFORMAT: return "MD3_EFORMAT";
        case MD3_ERANGE: return "MD3_ERANGE";
        case MD3_ENOSPACE: return "MD3_ENOSPACE";
        case MD3_ESTATE: return "MD3_ESTATE";
        default: return "MD3_EUNKNOWN";
    }
}

void md3_zero_memory(void *ptr, unsigned int size_bytes)
{
    memset(ptr, 0, (size_t) size_bytes);
}

void md3_copy_cstr_to_field(char *dst, unsigned int dst_size, const char *src)
{
    unsigned int i;

    if (dst == 0 || dst_size == 0)
    {
        return;
    }

    for (i = 0; i < dst_size; ++i)
    {
        dst[i] = '\0';
    }

    if (src == 0)
    {
        return;
    }

    for (i = 0; i + 1U < dst_size && src[i] != '\0'; ++i)
    {
        dst[i] = src[i];
    }
}

void md3_read_field_to_cstr(char *dst, unsigned int dst_size,
    const char *src, unsigned int src_size)
{
    unsigned int i;
    unsigned int limit;

    if (dst == 0 || dst_size == 0)
    {
        return;
    }

    limit = (dst_size - 1U < src_size) ? (dst_size - 1U) : src_size;

    for (i = 0; i < limit; ++i)
    {
        if (src[i] == '\0')
        {
            break;
        }
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

int md3_io_seek(FILE *fp, long offset)
{
    if (fp == 0)
    {
        return MD3_EINVAL;
    }

    if (fseek(fp, offset, SEEK_SET) != 0)
    {
        return MD3_EIO;
    }

    return MD3_OK;
}

int md3_io_read_exact(FILE *fp, void *dst, unsigned int size_bytes)
{
    if (fp == 0 || dst == 0)
    {
        return MD3_EINVAL;
    }

    if (size_bytes == 0U)
    {
        return MD3_OK;
    }

    if (fread(dst, 1U, (size_t) size_bytes, fp) != (size_t) size_bytes)
    {
        return MD3_EIO;
    }

    return MD3_OK;
}

int md3_io_write_exact(FILE *fp, const void *src, unsigned int size_bytes)
{
    if (fp == 0 || src == 0)
    {
        return MD3_EINVAL;
    }

    if (size_bytes == 0U)
    {
        return MD3_OK;
    }

    if (fwrite(src, 1U, (size_t) size_bytes, fp) != (size_t) size_bytes)
    {
        return MD3_EIO;
    }

    return MD3_OK;
}

int md3_io_zero_fill(FILE *fp, long total_size)
{
    static const md3_u8 zero_block[256] = { 0 };
    long remaining;
    unsigned int chunk;

    if (fp == 0 || total_size < 0L)
    {
        return MD3_EINVAL;
    }

    if (md3_io_seek(fp, 0L) != MD3_OK)
    {
        return MD3_EIO;
    }

    remaining = total_size;
    while (remaining > 0L)
    {
        chunk = (remaining > (long) sizeof(zero_block)) ?
            (unsigned int) sizeof(zero_block) : (unsigned int) remaining;
        if (md3_io_write_exact(fp, zero_block, chunk) != MD3_OK)
        {
            return MD3_EIO;
        }
        remaining -= (long) chunk;
    }

    return MD3_OK;
}

int md3_io_read_u16_le(FILE *fp, md3_u16 *out_value)
{
    md3_u8 bytes[2];

    if (out_value == 0)
    {
        return MD3_EINVAL;
    }

    if (md3_io_read_exact(fp, bytes, 2U) != MD3_OK)
    {
        return MD3_EIO;
    }

    *out_value = (md3_u16) ((md3_u16) bytes[0] |
        ((md3_u16) bytes[1] << 8));

    return MD3_OK;
}

int md3_io_read_i16_le(FILE *fp, md3_i16 *out_value)
{
    md3_u16 temp;
    int rc;

    if (out_value == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_io_read_u16_le(fp, &temp);
    if (rc != MD3_OK)
    {
        return rc;
    }

    *out_value = (md3_i16) temp;
    return MD3_OK;
}

int md3_io_read_u32_le(FILE *fp, md3_u32 *out_value)
{
    md3_u8 bytes[4];

    if (out_value == 0)
    {
        return MD3_EINVAL;
    }

    if (md3_io_read_exact(fp, bytes, 4U) != MD3_OK)
    {
        return MD3_EIO;
    }

    *out_value = ((md3_u32) bytes[0]) |
        ((md3_u32) bytes[1] << 8) |
        ((md3_u32) bytes[2] << 16) |
        ((md3_u32) bytes[3] << 24);

    return MD3_OK;
}

int md3_io_read_i32_le(FILE *fp, md3_i32 *out_value)
{
    md3_u32 temp;
    int rc;

    if (out_value == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_io_read_u32_le(fp, &temp);
    if (rc != MD3_OK)
    {
        return rc;
    }

    *out_value = (md3_i32) temp;
    return MD3_OK;
}

int md3_io_write_u16_le(FILE *fp, md3_u16 value)
{
    md3_u8 bytes[2];

    bytes[0] = (md3_u8) (value & 0xFFU);
    bytes[1] = (md3_u8) ((value >> 8) & 0xFFU);

    return md3_io_write_exact(fp, bytes, 2U);
}

int md3_io_write_i16_le(FILE *fp, md3_i16 value)
{
    return md3_io_write_u16_le(fp, (md3_u16) value);
}

int md3_io_write_u32_le(FILE *fp, md3_u32 value)
{
    md3_u8 bytes[4];

    bytes[0] = (md3_u8) (value & 0xFFUL);
    bytes[1] = (md3_u8) ((value >> 8) & 0xFFUL);
    bytes[2] = (md3_u8) ((value >> 16) & 0xFFUL);
    bytes[3] = (md3_u8) ((value >> 24) & 0xFFUL);

    return md3_io_write_exact(fp, bytes, 4U);
}

int md3_io_write_i32_le(FILE *fp, md3_i32 value)
{
    return md3_io_write_u32_le(fp, (md3_u32) value);
}

int md3_read_fixed_f32_raw(FILE *fp, md3_fixed_t *out_value,
    md3_u32 *out_bits)
{
    md3_u32 bits;
    int rc;

    if (out_value == 0)
    {
        return MD3_EINVAL;
    }

    rc = md3_io_read_u32_le(fp, &bits);
    if (rc != MD3_OK)
    {
        return rc;
    }

    if (out_bits != 0)
    {
        *out_bits = bits;
    }

    return md3_fixed_from_ieee754_u32(bits, out_value);
}

int md3_read_fixed_f32(FILE *fp, md3_fixed_t *out_value)
{
    return md3_read_fixed_f32_raw(fp, out_value, 0);
}

int md3_write_fixed_f32_raw(FILE *fp, md3_fixed_t value,
    int use_raw_bits, md3_u32 raw_bits)
{
    md3_u32 bits;

    bits = (use_raw_bits != 0) ? raw_bits : md3_fixed_to_ieee754_u32(value);
    return md3_io_write_u32_le(fp, bits);
}

int md3_write_fixed_f32(FILE *fp, md3_fixed_t value)
{
    return md3_write_fixed_f32_raw(fp, value, 0, 0U);
}
