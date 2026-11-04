#include "re4c3d/re4_base.h"
#include <string.h>

const char *re4_err_name(re4_err err)
{
    switch (err) {
    case RE4_OK: return "RE4_OK";
    case RE4_ERR_NULL: return "RE4_ERR_NULL";
    case RE4_ERR_RANGE: return "RE4_ERR_RANGE";
    case RE4_ERR_BAD_MAGIC: return "RE4_ERR_BAD_MAGIC";
    case RE4_ERR_BAD_FORMAT: return "RE4_ERR_BAD_FORMAT";
    case RE4_ERR_NO_SPACE: return "RE4_ERR_NO_SPACE";
    case RE4_ERR_UNSUPPORTED: return "RE4_ERR_UNSUPPORTED";
    case RE4_ERR_NEED_EXTERNAL_CODEC: return "RE4_ERR_NEED_EXTERNAL_CODEC";
    case RE4_ERR_TODO: return "RE4_ERR_TODO";
    default: return "RE4_ERR_UNKNOWN";
    }
}

re4_u16 re4_read_u16_le(const re4_u8 *p)
{
    return (re4_u16)((re4_u16)p[0] | ((re4_u16)p[1] << 8));
}

re4_u16 re4_read_u16_be(const re4_u8 *p)
{
    return (re4_u16)(((re4_u16)p[0] << 8) | (re4_u16)p[1]);
}

re4_u32 re4_read_u32_le(const re4_u8 *p)
{
    return ((re4_u32)p[0]) |
           ((re4_u32)p[1] << 8) |
           ((re4_u32)p[2] << 16) |
           ((re4_u32)p[3] << 24);
}

re4_u32 re4_read_u32_be(const re4_u8 *p)
{
    return ((re4_u32)p[0] << 24) |
           ((re4_u32)p[1] << 16) |
           ((re4_u32)p[2] << 8) |
           ((re4_u32)p[3]);
}

re4_u16 re4_read_u16_e(const re4_u8 *p, re4_endian e)
{
    if (e == RE4_ENDIAN_BIG) return re4_read_u16_be(p);
    return re4_read_u16_le(p);
}

re4_u32 re4_read_u32_e(const re4_u8 *p, re4_endian e)
{
    if (e == RE4_ENDIAN_BIG) return re4_read_u32_be(p);
    return re4_read_u32_le(p);
}

re4_i16 re4_read_i16_e(const re4_u8 *p, re4_endian e)
{
    return (re4_i16)re4_read_u16_e(p, e);
}

re4_i32 re4_read_i32_e(const re4_u8 *p, re4_endian e)
{
    return (re4_i32)re4_read_u32_e(p, e);
}

re4_bool re4_span_ok(re4_span src, re4_size off, re4_size len)
{
    if (off > src.size) return RE4_FALSE;
    if (len > src.size - off) return RE4_FALSE;
    return RE4_TRUE;
}

re4_err re4_span_sub(re4_span src, re4_size off, re4_size len, re4_span *out)
{
    if (out == 0) return RE4_ERR_NULL;
    if (!re4_span_ok(src, off, len)) return RE4_ERR_RANGE;
    out->data = src.data + off;
    out->size = len;
    return RE4_OK;
}

re4_strref re4_strref_fixed(const re4_u8 *p, re4_size cap)
{
    re4_strref s;
    re4_size i;
    s.ptr = (const char *)p;
    s.len = 0;
    for (i = 0; i < cap; ++i) {
        if (p[i] == 0) break;
        s.len = i + 1;
    }
    return s;
}

re4_bool re4_strref_eq_cstr(re4_strref s, const char *z)
{
    re4_size i;
    if (z == 0) return RE4_FALSE;
    for (i = 0; i < s.len; ++i) {
        if (z[i] == 0) return RE4_FALSE;
        if ((char)s.ptr[i] != z[i]) return RE4_FALSE;
    }
    return z[s.len] == 0 ? RE4_TRUE : RE4_FALSE;
}

re4_u8 re4_ascii_tolower_u8(re4_u8 c)
{
    if (c >= (re4_u8)'A' && c <= (re4_u8)'Z') return (re4_u8)(c + ((re4_u8)'a' - (re4_u8)'A'));
    return c;
}

re4_bool re4_strref_ends_with_cstr_nocase(re4_strref s, const char *suffix)
{
    re4_size n;
    re4_size i;
    if (suffix == 0) return RE4_FALSE;
    n = 0;
    while (suffix[n] != 0) ++n;
    if (n > s.len) return RE4_FALSE;
    for (i = 0; i < n; ++i) {
        re4_u8 a;
        re4_u8 b;
        a = re4_ascii_tolower_u8((re4_u8)s.ptr[s.len - n + i]);
        b = re4_ascii_tolower_u8((re4_u8)suffix[i]);
        if (a != b) return RE4_FALSE;
    }
    return RE4_TRUE;
}

re4_i32 re4_clamp_i64_like_q16(re4_u32 mag, re4_bool neg)
{
    if (neg) {
        if (mag >= (re4_u32)0x80000000U) return RE4_I32_MIN;
        return (re4_i32)(0 - (re4_i32)mag);
    }
    if (mag > (re4_u32)0x7FFFFFFFU) return RE4_I32_MAX;
    return (re4_i32)mag;
}

re4_q16 re4_f32bits_to_q16(re4_u32 bits)
{
    re4_u32 sign;
    re4_u32 exp;
    re4_u32 mant;
    re4_u32 sig;
    re4_i32 e2;
    re4_i32 shift;
    re4_u32 mag;

    sign = (bits >> 31) & 1U;
    exp = (bits >> 23) & 0xFFU;
    mant = bits & 0x7FFFFFU;

    if (exp == 0U && mant == 0U) return 0;
    if (exp == 0xFFU) return sign ? RE4_I32_MIN : RE4_I32_MAX;

    if (exp == 0U) {
        sig = mant;
        e2 = -126;
    } else {
        sig = 0x800000U | mant;
        e2 = (re4_i32)exp - 127;
    }

    shift = e2 - 7;
    if (shift >= 8) {
        mag = 0x80000000U;
    } else if (shift >= 0) {
        mag = sig << (re4_u32)shift;
    } else {
        re4_i32 rshift;
        rshift = -shift;
        if (rshift >= 32) mag = 0;
        else mag = sig >> (re4_u32)rshift;
    }

    return re4_clamp_i64_like_q16(mag, sign ? RE4_TRUE : RE4_FALSE);
}
