
#include "qmdl_internal.h"
#include <stdio.h>

static qmdl_u32 qmdl_load_le_u32(const qmdl_u8 *src)
{
    return ((qmdl_u32)src[0]) |
           ((qmdl_u32)src[1] << 8) |
           ((qmdl_u32)src[2] << 16) |
           ((qmdl_u32)src[3] << 24);
}

static void qmdl_store_le_u32(qmdl_u8 *dst, qmdl_u32 value)
{
    dst[0] = (qmdl_u8)(value & 0xFFu);
    dst[1] = (qmdl_u8)((value >> 8) & 0xFFu);
    dst[2] = (qmdl_u8)((value >> 16) & 0xFFu);
    dst[3] = (qmdl_u8)((value >> 24) & 0xFFu);
}

int qmdl_read_u8(qmdl_cursor *cur, qmdl_u8 *out_value)
{
    if (cur == 0 || out_value == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (cur->ptr >= cur->end)
    {
        return QMDL_ERR_SIZE;
    }
    *out_value = *cur->ptr;
    cur->ptr += 1;
    return QMDL_OK;
}

int qmdl_read_u32_le(qmdl_cursor *cur, qmdl_u32 *out_value)
{
    if (cur == 0 || out_value == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if ((size_t)(cur->end - cur->ptr) < 4u)
    {
        return QMDL_ERR_SIZE;
    }
    *out_value = qmdl_load_le_u32(cur->ptr);
    cur->ptr += 4;
    return QMDL_OK;
}

int qmdl_read_i32_le(qmdl_cursor *cur, qmdl_i32 *out_value)
{
    qmdl_u32 value;
    int rc;

    if (cur == 0 || out_value == 0)
    {
        return QMDL_ERR_ARGS;
    }
    rc = qmdl_read_u32_le(cur, &value);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    if (value == 0x80000000u)
    {
        *out_value = QMDL_I32_MIN;
    }
    else
    {
        *out_value = (qmdl_i32)value;
    }
    return QMDL_OK;
}

int qmdl_read_fx16_f32_le(qmdl_cursor *cur, qmdl_fx16 *out_value)
{
    qmdl_u32 bits;
    int rc;

    if (cur == 0 || out_value == 0)
    {
        return QMDL_ERR_ARGS;
    }
    rc = qmdl_read_u32_le(cur, &bits);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    *out_value = qmdl_f32_bits_to_fx16(bits);
    return QMDL_OK;
}

int qmdl_read_bytes(qmdl_cursor *cur, void *dst, size_t count)
{
    if (cur == 0 || dst == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if ((size_t)(cur->end - cur->ptr) < count)
    {
        return QMDL_ERR_SIZE;
    }
    memcpy(dst, cur->ptr, count);
    cur->ptr += count;
    return QMDL_OK;
}

int qmdl_write_u8(qmdl_writer *wr, qmdl_u8 value)
{
    if (wr == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (wr->ptr >= wr->end)
    {
        return QMDL_ERR_SIZE;
    }
    *wr->ptr = value;
    wr->ptr += 1;
    return QMDL_OK;
}

int qmdl_write_u32_le(qmdl_writer *wr, qmdl_u32 value)
{
    if (wr == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if ((size_t)(wr->end - wr->ptr) < 4u)
    {
        return QMDL_ERR_SIZE;
    }
    qmdl_store_le_u32(wr->ptr, value);
    wr->ptr += 4;
    return QMDL_OK;
}

int qmdl_write_i32_le(qmdl_writer *wr, qmdl_i32 value)
{
    qmdl_u32 raw;

    if (value == QMDL_I32_MIN)
    {
        raw = 0x80000000u;
    }
    else
    {
        raw = (qmdl_u32)value;
    }
    return qmdl_write_u32_le(wr, raw);
}

int qmdl_write_fx16_f32_le(qmdl_writer *wr, qmdl_fx16 value)
{
    return qmdl_write_u32_le(wr, qmdl_fx16_to_f32_bits(value));
}

int qmdl_write_bytes(qmdl_writer *wr, const void *src, size_t count)
{
    if (wr == 0 || src == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if ((size_t)(wr->end - wr->ptr) < count)
    {
        return QMDL_ERR_SIZE;
    }
    memcpy(wr->ptr, src, count);
    wr->ptr += count;
    return QMDL_OK;
}

int qmdl_size_add(size_t *value, size_t delta)
{
    if (value == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (((size_t)-1) - *value < delta)
    {
        return QMDL_ERR_OVERFLOW;
    }
    *value += delta;
    return QMDL_OK;
}

int qmdl_size_mul(size_t a, size_t b, size_t *out_value)
{
    if (out_value == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (a != 0u && b > ((size_t)-1) / a)
    {
        return QMDL_ERR_OVERFLOW;
    }
    *out_value = a * b;
    return QMDL_OK;
}

int qmdl_size_add_mul(size_t *acc, size_t a, size_t b)
{
    size_t product;
    int rc;

    if (acc == 0)
    {
        return QMDL_ERR_ARGS;
    }
    rc = qmdl_size_mul(a, b, &product);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    return qmdl_size_add(acc, product);
}

int qmdl_u32_add(qmdl_u32 *value, qmdl_u32 delta)
{
    if (value == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (0xFFFFFFFFu - *value < delta)
    {
        return QMDL_ERR_OVERFLOW;
    }
    *value += delta;
    return QMDL_OK;
}

int qmdl_u32_mul(qmdl_u32 a, qmdl_u32 b, qmdl_u32 *out_value)
{
    if (out_value == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (a != 0u && b > 0xFFFFFFFFu / a)
    {
        return QMDL_ERR_OVERFLOW;
    }
    *out_value = a * b;
    return QMDL_OK;
}

void qmdl_copy_name16(char dst[16], const char src[16])
{
    if (dst == 0)
    {
        return;
    }
    if (src == 0)
    {
        memset(dst, 0, 16u);
        return;
    }
    memcpy(dst, src, 16u);
}
