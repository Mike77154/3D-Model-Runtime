#include "md2/md2_error.h"
#include "md2/md2_io.h"

int md2_io_get_file_size(FILE *fp, md2_i32 *out_size)
{
    long cur;
    long end;

    if (fp == 0 || out_size == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    cur = ftell(fp);
    if (cur < 0L)
    {
        return MD2_ERR_IO;
    }
    if (fseek(fp, 0L, SEEK_END) != 0)
    {
        return MD2_ERR_IO;
    }
    end = ftell(fp);
    if (end < 0L)
    {
        return MD2_ERR_IO;
    }
    if (fseek(fp, cur, SEEK_SET) != 0)
    {
        return MD2_ERR_IO;
    }
    if (end > 0x7FFFFFFFL)
    {
        return MD2_ERR_OVERFLOW;
    }

    *out_size = (md2_i32)end;
    return MD2_OK;
}

int md2_io_read_u8(FILE *fp, md2_u8 *out_value)
{
    int c;

    if (fp == 0 || out_value == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    c = fgetc(fp);
    if (c == EOF)
    {
        return MD2_ERR_IO;
    }

    *out_value = (md2_u8)c;
    return MD2_OK;
}

int md2_io_read_i16_le(FILE *fp, md2_i16 *out_value)
{
    md2_u8 b0;
    md2_u8 b1;
    int rc;

    if (out_value == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    rc = md2_io_read_u8(fp, &b0);
    if (rc != MD2_OK) return rc;
    rc = md2_io_read_u8(fp, &b1);
    if (rc != MD2_OK) return rc;

    *out_value = (md2_i16)((md2_u16)b0 | ((md2_u16)b1 << 8));
    return MD2_OK;
}

int md2_io_read_u16_le(FILE *fp, md2_u16 *out_value)
{
    md2_u8 b0;
    md2_u8 b1;
    int rc;

    if (out_value == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    rc = md2_io_read_u8(fp, &b0);
    if (rc != MD2_OK) return rc;
    rc = md2_io_read_u8(fp, &b1);
    if (rc != MD2_OK) return rc;

    *out_value = (md2_u16)((md2_u16)b0 | ((md2_u16)b1 << 8));
    return MD2_OK;
}

int md2_io_read_i32_le(FILE *fp, md2_i32 *out_value)
{
    md2_u8 b0;
    md2_u8 b1;
    md2_u8 b2;
    md2_u8 b3;
    int rc;

    if (out_value == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    rc = md2_io_read_u8(fp, &b0);
    if (rc != MD2_OK) return rc;
    rc = md2_io_read_u8(fp, &b1);
    if (rc != MD2_OK) return rc;
    rc = md2_io_read_u8(fp, &b2);
    if (rc != MD2_OK) return rc;
    rc = md2_io_read_u8(fp, &b3);
    if (rc != MD2_OK) return rc;

    *out_value = (md2_i32)((md2_u32)b0 |
                           ((md2_u32)b1 << 8) |
                           ((md2_u32)b2 << 16) |
                           ((md2_u32)b3 << 24));
    return MD2_OK;
}

int md2_io_read_u32_le(FILE *fp, md2_u32 *out_value)
{
    md2_i32 tmp;
    int rc;

    if (out_value == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    rc = md2_io_read_i32_le(fp, &tmp);
    if (rc != MD2_OK)
    {
        return rc;
    }

    *out_value = (md2_u32)tmp;
    return MD2_OK;
}

int md2_io_write_u8(FILE *fp, md2_u8 value)
{
    if (fp == 0)
    {
        return MD2_ERR_BAD_ARG;
    }

    if (fputc((int)value, fp) == EOF)
    {
        return MD2_ERR_IO;
    }

    return MD2_OK;
}

int md2_io_write_i16_le(FILE *fp, md2_i16 value)
{
    int rc;

    rc = md2_io_write_u8(fp, (md2_u8)(value & 0xFF));
    if (rc != MD2_OK) return rc;
    rc = md2_io_write_u8(fp, (md2_u8)(((md2_u16)value >> 8) & 0xFF));
    if (rc != MD2_OK) return rc;

    return MD2_OK;
}

int md2_io_write_u16_le(FILE *fp, md2_u16 value)
{
    int rc;

    rc = md2_io_write_u8(fp, (md2_u8)(value & 0xFFU));
    if (rc != MD2_OK) return rc;
    rc = md2_io_write_u8(fp, (md2_u8)((value >> 8) & 0xFFU));
    if (rc != MD2_OK) return rc;

    return MD2_OK;
}

int md2_io_write_i32_le(FILE *fp, md2_i32 value)
{
    md2_u32 uvalue;
    int rc;

    uvalue = (md2_u32)value;

    rc = md2_io_write_u8(fp, (md2_u8)(uvalue & 0xFFU));
    if (rc != MD2_OK) return rc;
    rc = md2_io_write_u8(fp, (md2_u8)((uvalue >> 8) & 0xFFU));
    if (rc != MD2_OK) return rc;
    rc = md2_io_write_u8(fp, (md2_u8)((uvalue >> 16) & 0xFFU));
    if (rc != MD2_OK) return rc;
    rc = md2_io_write_u8(fp, (md2_u8)((uvalue >> 24) & 0xFFU));
    if (rc != MD2_OK) return rc;

    return MD2_OK;
}

int md2_io_write_u32_le(FILE *fp, md2_u32 value)
{
    return md2_io_write_i32_le(fp, (md2_i32)value);
}
