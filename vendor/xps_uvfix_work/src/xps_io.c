#include "xps_internal.h"

int xps_internal_read_u8(FILE *fp, xps_u8 *out)
{
    int c;

    c = fgetc(fp);
    if (c == EOF) {
        return ferror(fp) ? XPS_ERR_IO : XPS_ERR_PARSE;
    }
    *out = (xps_u8)c;
    return XPS_OK;
}

int xps_internal_read_i16le(FILE *fp, xps_i16 *out)
{
    xps_u8 b0;
    xps_u8 b1;
    int rc;

    rc = xps_internal_read_u8(fp, &b0);
    if (rc != XPS_OK) {
        return rc;
    }
    rc = xps_internal_read_u8(fp, &b1);
    if (rc != XPS_OK) {
        return rc;
    }
    *out = (xps_i16)((xps_u16)b0 | ((xps_u16)b1 << 8));
    return XPS_OK;
}

int xps_internal_read_u16le(FILE *fp, xps_u16 *out)
{
    xps_u8 b0;
    xps_u8 b1;
    int rc;

    rc = xps_internal_read_u8(fp, &b0);
    if (rc != XPS_OK) {
        return rc;
    }
    rc = xps_internal_read_u8(fp, &b1);
    if (rc != XPS_OK) {
        return rc;
    }
    *out = (xps_u16)((xps_u16)b0 | ((xps_u16)b1 << 8));
    return XPS_OK;
}

int xps_internal_read_u32le(FILE *fp, xps_u32 *out)
{
    xps_u8 b0;
    xps_u8 b1;
    xps_u8 b2;
    xps_u8 b3;
    int rc;

    rc = xps_internal_read_u8(fp, &b0);
    if (rc != XPS_OK) {
        return rc;
    }
    rc = xps_internal_read_u8(fp, &b1);
    if (rc != XPS_OK) {
        return rc;
    }
    rc = xps_internal_read_u8(fp, &b2);
    if (rc != XPS_OK) {
        return rc;
    }
    rc = xps_internal_read_u8(fp, &b3);
    if (rc != XPS_OK) {
        return rc;
    }

    *out = (xps_u32)b0 |
           ((xps_u32)b1 << 8) |
           ((xps_u32)b2 << 16) |
           ((xps_u32)b3 << 24);
    return XPS_OK;
}

int xps_internal_read_f32fx(FILE *fp, xps_fx *out)
{
    xps_u32 bits;
    int rc;

    rc = xps_internal_read_u32le(fp, &bits);
    if (rc != XPS_OK) {
        return rc;
    }
    return xps_internal_f32_bits_to_fx(bits, out);
}

int xps_internal_skip(FILE *fp, xps_u32 count)
{
    unsigned char buf[64];
    xps_u32 remaining;

    remaining = count;
    while (remaining > 0U) {
        size_t chunk;
        size_t got;

        chunk = sizeof(buf);
        if ((xps_u32)chunk > remaining) {
            chunk = (size_t)remaining;
        }
        got = fread(buf, 1U, chunk, fp);
        if (got != chunk) {
            return ferror(fp) ? XPS_ERR_IO : XPS_ERR_PARSE;
        }
        remaining -= (xps_u32)got;
    }
    return XPS_OK;
}

int xps_internal_read_lpstring(FILE *fp, char *dst, size_t cap)
{
    xps_u8 first;
    xps_u8 second;
    xps_u32 len;
    size_t i;
    int rc;

    rc = xps_internal_read_u8(fp, &first);
    if (rc != XPS_OK) {
        return rc;
    }

    len = (xps_u32)first;
    if (first >= XPS_LIMIT_1BYTE) {
        rc = xps_internal_read_u8(fp, &second);
        if (rc != XPS_OK) {
            return rc;
        }
        len = (xps_u32)(first % XPS_LIMIT_1BYTE) + ((xps_u32)second * XPS_LIMIT_1BYTE);
    }

    if (cap == 0U) {
        return xps_internal_skip(fp, len);
    }

    i = 0U;
    while (len > 0U) {
        xps_u8 ch;

        rc = xps_internal_read_u8(fp, &ch);
        if (rc != XPS_OK) {
            return rc;
        }
        if (i + 1U < cap) {
            dst[i] = (char)ch;
            ++i;
        }
        --len;
    }
    dst[i] = '\0';
    return XPS_OK;
}

int xps_internal_read_scene_string(FILE *fp, char *dst, size_t cap)
{
    xps_u8 len8;
    xps_u8 marker;
    xps_u32 len;
    size_t i;
    int rc;
    int saw_null;

    rc = xps_internal_read_u8(fp, &len8);
    if (rc != XPS_OK) {
        return rc;
    }
    len = (xps_u32)len8;

    rc = xps_internal_read_u8(fp, &marker);
    if (rc != XPS_OK) {
        return rc;
    }
    if (marker != 1U) {
        if (fseek(fp, -1L, SEEK_CUR) != 0) {
            return XPS_ERR_IO;
        }
    }

    if (cap == 0U) {
        return xps_internal_skip(fp, len);
    }

    i = 0U;
    saw_null = 0;
    while (len > 0U) {
        xps_u8 ch;

        rc = xps_internal_read_u8(fp, &ch);
        if (rc != XPS_OK) {
            return rc;
        }
        if (ch == 0U) {
            if (fseek(fp, -1L, SEEK_CUR) != 0) {
                return XPS_ERR_IO;
            }
            saw_null = 1;
            break;
        }
        if (i + 1U < cap) {
            dst[i] = (char)ch;
            ++i;
        }
        --len;
    }
    if (!saw_null && len > 0U) {
        rc = xps_internal_skip(fp, len);
        if (rc != XPS_OK) {
            return rc;
        }
    }
    dst[i] = '\0';
    return XPS_OK;
}
