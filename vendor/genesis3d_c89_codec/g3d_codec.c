
#include "g3d_codec.h"
#include "g3d_kib.h"

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>

typedef struct g3d_reader
{
    const g3d_u8 *buf;
    g3d_u32 size;
    g3d_u32 pos;
} g3d_reader;

typedef struct g3d_writer
{
    g3d_u8 *buf;
    g3d_u32 size;
    g3d_u32 pos;
    int measure_only;
    int failed;
} g3d_writer;

#define G3D_FOURCC(a,b,c,d) \
    ((g3d_u32)(g3d_u8)(a) | ((g3d_u32)(g3d_u8)(b) << 8) | ((g3d_u32)(g3d_u8)(c) << 16) | ((g3d_u32)(g3d_u8)(d) << 24))

#define G3D_VFS_SIG_VF00 0x30304656u
#define G3D_VFS_SIG_CF00 0x30304643u
#define G3D_DIRTREE_SIG  G3D_FOURCC('D','T','0','1')

#define G3D_BODY_SIG     0x5E444F42u
#define G3D_BODY_VER     0x00F1u
#define G3D_BODY_GEOM    "Geometry"
#define G3D_BODY_BITMAPS "Bitmaps"

#define G3D_ACTOR_SIG    0x52544341u
#define G3D_ACTOR_VER    0x00F1u
#define G3D_ACTOR_HEADER "Header"
#define G3D_ACTOR_BODY   "Body"
#define G3D_ACTOR_MOTDIR "Motions"

#define G3D_MOT_ASCII    0x4E544F4Du
#define G3D_MOT_BIN      0x424E544Du
#define G3D_MOT_VER      0x00F0u

#define G3D_PATH_ASCII   0x48544150u
#define G3D_PATH_BIN_VER 0x1001u
#define G3D_PATH_ASCII_VER0 0x00F0u
#define G3D_PATH_ASCII_VER1 0x00F1u
#define G3D_PATH_ASCII_VER2 0x00F2u
#define G3D_PATH_CHANNEL_ASCII G3D_FOURCC('C','H','N','L')
#define G3D_QKFL_ASCII G3D_FOURCC('Q','K','F','L')
#define G3D_QKFS_ASCII G3D_FOURCC('Q','K','F','S')
#define G3D_QKFQ_ASCII G3D_FOURCC('Q','K','F','Q')
#define G3D_VKFL_ASCII G3D_FOURCC('V','K','F','L')
#define G3D_VKFH_ASCII G3D_FOURCC('V','K','F','H')

#define G3D_OLD_PATH_INTERP_LINEAR 0
#define G3D_OLD_PATH_INTERP_HERMITE 1
#define G3D_OLD_PATH_INTERP_SLERP 2
#define G3D_OLD_PATH_INTERP_SQUAD 3
#define G3D_OLD_PATH_INTERP_HERMITE_ZERO_DERIV 7

#define G3D_SB_ASCII     0x4B4C4253u
#define G3D_SB_BIN       0x424B4253u
#define G3D_SB_VER       0x00F0u

#define G3D_TKEV_ASCII   0x56454B54u
#define G3D_TKEV_BIN     0x42454B54u
#define G3D_TKEV_VER     0x00F0u

#define G3D_QK_HINGE_COMPRESSION      0x1u
#define G3D_QK_LINEARTIME_COMPRESSION 0x2u
#define G3D_VK_LINEARTIME_COMPRESSION 0x2u

#define G3D_DIRTREE_LIST_TERMINATED   0xffffffffu
#define G3D_DIRTREE_LIST_NOTTERMINATED 0u

#define G3D_PATH_MAX_INT_TYPE_COUNT 127u
#define G3D_PATH_TRANS_SHIFT        9u
#define G3D_PATH_ROT_SHIFT          2u

#define G3D_ALIGN_UP(v,a) (((v) + ((a) - 1u)) & ~((a) - 1u))

#define G3D_Q15_ONE  32768
#define G3D_Q15_HALF 16384

typedef char g3d_static_assert_u16[(sizeof(g3d_u16) == 2) ? 1 : -1];
typedef char g3d_static_assert_s16[(sizeof(g3d_s16) == 2) ? 1 : -1];
typedef char g3d_static_assert_u32[(sizeof(g3d_u32) == 4) ? 1 : -1];
typedef char g3d_static_assert_s32[(sizeof(g3d_s32) == 4) ? 1 : -1];

static int g3d_wr_bytes(g3d_writer *w, const void *src, g3d_u32 n);
static int g3d_strn_has_zero(const g3d_u8 *p, g3d_u32 n);
static g3d_fix16 g3d_fix16_add_indexed_delta(g3d_fix16 start, g3d_fix16 delta, g3d_u32 index);

static void g3d_zero(void *p, g3d_u32 n)
{
    if (p != NULL && n != 0u)
    {
        memset(p, 0, (size_t)n);
    }
}

static g3d_u32 g3d_strlen32(const char *s)
{
    g3d_u32 n;
    n = 0u;
    if (s == NULL)
    {
        return 0u;
    }
    while (s[n] != '\0')
    {
        ++n;
    }
    return n;
}

g3d_u8 g3d_vfs_context_mask_from_string(const char *context)
{
    g3d_u8 e[8];
    g3d_u32 i;
    g3d_u8 mask;

    for (i = 0u; i < 8u; ++i)
    {
        e[i] = 0u;
    }
    if (context != NULL)
    {
        for (i = 0u; i < 8u && context[i] != '\0'; ++i)
        {
            e[i] = (g3d_u8)context[i];
        }
    }

    mask = 0u;
    mask = (g3d_u8)(mask | (e[0] & 0x01u));
    mask = (g3d_u8)(mask | (e[7] & 0x02u));
    mask = (g3d_u8)(mask | (e[1] & 0x04u));
    mask = (g3d_u8)(mask | (e[6] & 0x08u));
    mask = (g3d_u8)(mask | (e[2] & 0x10u));
    mask = (g3d_u8)(mask | (e[5] & 0x20u));
    mask = (g3d_u8)(mask | (e[3] & 0x40u));
    mask = (g3d_u8)(mask | (e[4] & 0x80u));
    return mask;
}

static int g3d_str_eq(const char *a, const char *b)
{
    if (a == NULL || b == NULL)
    {
        return G3D_FALSE;
    }
    return (strcmp(a, b) == 0) ? G3D_TRUE : G3D_FALSE;
}

static int g3d_ascii_lower_int(int c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c + ('a' - 'A');
    }
    return c;
}

static int g3d_str_ieq(const char *a, const char *b)
{
    unsigned char ca;
    unsigned char cb;
    if (a == NULL || b == NULL)
    {
        return G3D_FALSE;
    }
    while (*a != '\0' && *b != '\0')
    {
        ca = (unsigned char)*a;
        cb = (unsigned char)*b;
        if (g3d_ascii_lower_int((int)ca) != g3d_ascii_lower_int((int)cb))
        {
            return G3D_FALSE;
        }
        ++a;
        ++b;
    }
    return (*a == '\0' && *b == '\0') ? G3D_TRUE : G3D_FALSE;
}

static int g3d_str_iprefix(const char *text, const char *prefix)
{
    unsigned char ca;
    unsigned char cb;
    if (text == NULL || prefix == NULL)
    {
        return G3D_FALSE;
    }
    while (*prefix != '\0')
    {
        if (*text == '\0')
        {
            return G3D_FALSE;
        }
        ca = (unsigned char)*text;
        cb = (unsigned char)*prefix;
        if (g3d_ascii_lower_int((int)ca) != g3d_ascii_lower_int((int)cb))
        {
            return G3D_FALSE;
        }
        ++text;
        ++prefix;
    }
    return G3D_TRUE;
}

static const char *g3d_line_after_tag(const char *line, const char *tag)
{
    g3d_u32 len;
    const char *p;
    if (line == NULL || tag == NULL)
    {
        return NULL;
    }
    if (g3d_str_iprefix(line, tag) == G3D_FALSE)
    {
        return NULL;
    }
    len = g3d_strlen32(tag);
    p = line + len;
    while (*p == ' ' || *p == '\t')
    {
        ++p;
    }
    return p;
}


static const char *g3d_skip_ws_text(const char *p)
{
    while (p != NULL && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
    {
        ++p;
    }
    return p;
}

static int g3d_is_dec_digit(int c)
{
    return (c >= '0' && c <= '9') ? G3D_TRUE : G3D_FALSE;
}

static int g3d_hex_digit_value(int c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

static int g3d_parse_hex_u32_token(const char **cursor, g3d_u32 *out)
{
    const char *p;
    g3d_u32 value;
    int hv;
    int have_digit;
    if (cursor == NULL || *cursor == NULL || out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    p = g3d_skip_ws_text(*cursor);
    value = 0u;
    have_digit = G3D_FALSE;
    hv = g3d_hex_digit_value((int)(unsigned char)*p);
    while (hv >= 0)
    {
        if (value > 0x0fffffffu)
        {
            return G3D_ERR_OVERFLOW;
        }
        value = (value << 4) | (g3d_u32)hv;
        have_digit = G3D_TRUE;
        ++p;
        hv = g3d_hex_digit_value((int)(unsigned char)*p);
    }
    if (have_digit == G3D_FALSE)
    {
        return G3D_ERR_CORRUPT;
    }
    *out = value;
    *cursor = p;
    return G3D_OK;
}

static int g3d_parse_version_text(const char *line, g3d_u32 *version)
{
    const char *p;
    g3d_u32 major_v;
    g3d_u32 minor_v;
    int rc;
    if (line == NULL || version == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    p = line;
    rc = g3d_parse_hex_u32_token(&p, &major_v);
    if (rc != G3D_OK)
    {
        return G3D_ERR_CORRUPT;
    }
    if (*p != '.')
    {
        return G3D_ERR_CORRUPT;
    }
    ++p;
    rc = g3d_parse_hex_u32_token(&p, &minor_v);
    if (rc != G3D_OK)
    {
        return G3D_ERR_CORRUPT;
    }
    if (major_v > 0xffu || minor_v > 0xffu)
    {
        return G3D_ERR_BAD_VERSION;
    }
    *version = (major_v << 8) | minor_v;
    return G3D_OK;
}

static void g3d_store_u32ptr(g3d_u8 *p, g3d_u32 v)
{
    p[0] = (g3d_u8)(v & 0xffu);
    p[1] = (g3d_u8)((v >> 8) & 0xffu);
    p[2] = (g3d_u8)((v >> 16) & 0xffu);
    p[3] = (g3d_u8)((v >> 24) & 0xffu);
}

static void g3d_store_s32ptr(g3d_u8 *p, g3d_s32 v)
{
    g3d_store_u32ptr(p, (g3d_u32)v);
}

static g3d_u32 g3d_abs_fix16_mag(g3d_fix16 value)
{
    if (value < 0)
    {
        if (value == (g3d_fix16)INT_MIN)
        {
            return 0x80000000u;
        }
        return (g3d_u32)(-(g3d_s32)value);
    }
    return (g3d_u32)value;
}

static g3d_fix16 g3d_make_fix16_from_mag(g3d_u32 mag, int neg)
{
    if (neg)
    {
        if (mag >= 0x80000000u)
        {
            return (g3d_fix16)INT_MIN;
        }
        return (g3d_fix16)(-(g3d_s32)mag);
    }
    if (mag > 0x7fffffffu)
    {
        return (g3d_fix16)INT_MAX;
    }
    return (g3d_fix16)mag;
}

static g3d_fix16 g3d_fix16_mul10_sat(g3d_fix16 value)
{
    if (value > (g3d_fix16)(INT_MAX / 10))
    {
        return (g3d_fix16)INT_MAX;
    }
    if (value < (g3d_fix16)(INT_MIN / 10))
    {
        return (g3d_fix16)INT_MIN;
    }
    return (g3d_fix16)((g3d_s32)value * 10);
}

static g3d_fix16 g3d_fix16_div10_trunc(g3d_fix16 value)
{
    g3d_u32 mag;
    if (value < 0)
    {
        mag = g3d_abs_fix16_mag(value);
        mag /= 10u;
        return g3d_make_fix16_from_mag(mag, G3D_TRUE);
    }
    return (g3d_fix16)((g3d_u32)value / 10u);
}

static int g3d_parse_s32_token(const char **cursor, g3d_s32 *out)
{
    const char *p;
    int neg;
    g3d_u32 mag;
    g3d_u32 limit;
    int have_digit;
    if (cursor == NULL || *cursor == NULL || out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    p = g3d_skip_ws_text(*cursor);
    neg = G3D_FALSE;
    if (*p == '+' || *p == '-')
    {
        neg = (*p == '-') ? G3D_TRUE : G3D_FALSE;
        ++p;
    }
    mag = 0u;
    have_digit = G3D_FALSE;
    limit = neg ? 0x80000000u : 0x7fffffffu;
    while (g3d_is_dec_digit((int)(unsigned char)*p) == G3D_TRUE)
    {
        g3d_u32 d;
        d = (g3d_u32)(*p - '0');
        have_digit = G3D_TRUE;
        if (mag > ((limit - d) / 10u))
        {
            return G3D_ERR_OVERFLOW;
        }
        mag = (mag * 10u) + d;
        ++p;
    }
    if (have_digit == G3D_FALSE)
    {
        return G3D_ERR_CORRUPT;
    }
    if (neg)
    {
        if (mag == 0x80000000u)
        {
            *out = (g3d_s32)INT_MIN;
        }
        else
        {
            *out = -(g3d_s32)mag;
        }
    }
    else
    {
        *out = (g3d_s32)mag;
    }
    *cursor = p;
    return G3D_OK;
}

static int g3d_parse_u32_token(const char **cursor, g3d_u32 *out)
{
    g3d_s32 tmp;
    int rc;
    rc = g3d_parse_s32_token(cursor, &tmp);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (tmp < 0)
    {
        return G3D_ERR_CORRUPT;
    }
    *out = (g3d_u32)tmp;
    return G3D_OK;
}

static int g3d_parse_s32_1(const char *line, int *a)
{
    const char *p;
    g3d_s32 v;
    int rc;
    if (line == NULL || a == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    p = line;
    rc = g3d_parse_s32_token(&p, &v);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *a = (int)v;
    return G3D_OK;
}

static int g3d_parse_s32_2(const char *line, int *a, int *b)
{
    const char *p;
    g3d_s32 va;
    g3d_s32 vb;
    int rc;
    if (line == NULL || a == NULL || b == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    p = line;
    rc = g3d_parse_s32_token(&p, &va);
    if (rc != G3D_OK) return rc;
    rc = g3d_parse_s32_token(&p, &vb);
    if (rc != G3D_OK) return rc;
    *a = (int)va;
    *b = (int)vb;
    return G3D_OK;
}

static int g3d_parse_s32_4(const char *line, int *a, int *b, int *c, int *d)
{
    const char *p;
    g3d_s32 va;
    g3d_s32 vb;
    g3d_s32 vc;
    g3d_s32 vd;
    int rc;
    if (line == NULL || a == NULL || b == NULL || c == NULL || d == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    p = line;
    rc = g3d_parse_s32_token(&p, &va);
    if (rc != G3D_OK) return rc;
    rc = g3d_parse_s32_token(&p, &vb);
    if (rc != G3D_OK) return rc;
    rc = g3d_parse_s32_token(&p, &vc);
    if (rc != G3D_OK) return rc;
    rc = g3d_parse_s32_token(&p, &vd);
    if (rc != G3D_OK) return rc;
    *a = (int)va;
    *b = (int)vb;
    *c = (int)vc;
    *d = (int)vd;
    return G3D_OK;
}

static int g3d_parse_u32_1(const char *line, g3d_u32 *a)
{
    const char *p;
    if (line == NULL || a == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    p = line;
    return g3d_parse_u32_token(&p, a);
}

static int g3d_parse_fix16_token(const char **cursor, g3d_fix16 *out)
{
    const char *p;
    int neg;
    int have_digit;
    g3d_fix16 value;
    g3d_u32 frac_weight;
    g3d_s32 exp_value;
    int exp_neg;
    if (cursor == NULL || *cursor == NULL || out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    p = g3d_skip_ws_text(*cursor);
    neg = G3D_FALSE;
    if (*p == '+' || *p == '-')
    {
        neg = (*p == '-') ? G3D_TRUE : G3D_FALSE;
        ++p;
    }
    value = 0;
    have_digit = G3D_FALSE;
    while (g3d_is_dec_digit((int)(unsigned char)*p) == G3D_TRUE)
    {
        g3d_s32 digit_add;
        value = g3d_fix16_mul10_sat(value);
        digit_add = ((g3d_s32)(*p - '0')) << 16;
        if (value > (g3d_fix16)(INT_MAX - digit_add))
        {
            value = (g3d_fix16)INT_MAX;
        }
        else
        {
            value = (g3d_fix16)((g3d_s32)value + digit_add);
        }
        have_digit = G3D_TRUE;
        ++p;
    }
    frac_weight = 10u;
    if (*p == '.')
    {
        ++p;
        while (g3d_is_dec_digit((int)(unsigned char)*p) == G3D_TRUE)
        {
            if (frac_weight <= 1000000000u)
            {
                g3d_u32 d;
                g3d_u32 term;
                d = (g3d_u32)(*p - '0');
                term = (d * 65536u + (frac_weight / 2u)) / frac_weight;
                if ((g3d_u32)value > 0x7fffffffu - term)
                {
                    value = (g3d_fix16)INT_MAX;
                }
                else
                {
                    value = (g3d_fix16)((g3d_u32)value + term);
                }
                if (frac_weight <= 100000000u)
                {
                    frac_weight *= 10u;
                }
                else
                {
                    frac_weight = 1000000001u;
                }
            }
            have_digit = G3D_TRUE;
            ++p;
        }
    }
    if (have_digit == G3D_FALSE)
    {
        return G3D_ERR_CORRUPT;
    }
    exp_value = 0;
    exp_neg = G3D_FALSE;
    if (*p == 'e' || *p == 'E')
    {
        const char *ep;
        int exp_have;
        ep = p + 1;
        if (*ep == '+' || *ep == '-')
        {
            exp_neg = (*ep == '-') ? G3D_TRUE : G3D_FALSE;
            ++ep;
        }
        exp_have = G3D_FALSE;
        while (g3d_is_dec_digit((int)(unsigned char)*ep) == G3D_TRUE)
        {
            exp_have = G3D_TRUE;
            if (exp_value < 1000)
            {
                exp_value = (g3d_s32)((exp_value * 10) + (*ep - '0'));
            }
            ++ep;
        }
        if (exp_have == G3D_FALSE)
        {
            return G3D_ERR_CORRUPT;
        }
        p = ep;
    }
    while (exp_value > 0)
    {
        if (exp_neg)
        {
            value = g3d_fix16_div10_trunc(value);
        }
        else
        {
            value = g3d_fix16_mul10_sat(value);
        }
        --exp_value;
    }
    if (neg)
    {
        value = g3d_make_fix16_from_mag(g3d_abs_fix16_mag(value), G3D_TRUE);
    }
    *out = value;
    *cursor = p;
    return G3D_OK;
}

static int g3d_parse_fbits_token(const char **cursor, g3d_u32 *bits_out)
{
    g3d_fix16 fx;
    int rc;
    if (bits_out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_parse_fix16_token(cursor, &fx);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *bits_out = g3d_fix16_to_f32bits(fx);
    return G3D_OK;
}

static int g3d_parse_fbits_list(const char *line, g3d_u32 *bits, g3d_u32 count)
{
    const char *p;
    g3d_u32 i;
    int rc;
    if (line == NULL || (count != 0u && bits == NULL))
    {
        return G3D_ERR_BAD_ARG;
    }
    p = line;
    for (i = 0u; i < count; ++i)
    {
        rc = g3d_parse_fbits_token(&p, &bits[i]);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    return G3D_OK;
}

static void g3d_fix16_to_dec6_text(g3d_fix16 value, char *dst, g3d_u32 dst_size)
{
    char rev[16];
    g3d_u32 pos;
    g3d_u32 n;
    g3d_u32 mag;
    g3d_u32 int_part;
    g3d_u32 frac;
    int i;
    if (dst == NULL || dst_size == 0u)
    {
        return;
    }
    pos = 0u;
    if (value < 0 && pos + 1u < dst_size)
    {
        dst[pos++] = '-';
    }
    mag = g3d_abs_fix16_mag(value);
    int_part = mag >> 16;
    frac = mag & 0xffffu;
    n = 0u;
    if (int_part == 0u)
    {
        rev[n++] = '0';
    }
    else
    {
        while (int_part != 0u && n < (g3d_u32)sizeof(rev))
        {
            rev[n++] = (char)('0' + (char)(int_part % 10u));
            int_part /= 10u;
        }
    }
    while (n != 0u && pos + 1u < dst_size)
    {
        dst[pos++] = rev[--n];
    }
    if (pos + 1u < dst_size)
    {
        dst[pos++] = '.';
    }
    for (i = 0; i < 6; ++i)
    {
        g3d_u32 digit;
        frac *= 10u;
        digit = frac >> 16;
        frac &= 0xffffu;
        if (pos + 1u < dst_size)
        {
            dst[pos++] = (char)('0' + (char)digit);
        }
    }
    dst[pos < dst_size ? pos : (dst_size - 1u)] = '\0';
}



static int g3d_wr_textf(g3d_writer *w, const char *fmt, ...)
{
    char tmp[1024];
    int n;
    va_list ap;
    if (w == NULL || fmt == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    va_start(ap, fmt);
    n = vsprintf(tmp, fmt, ap);
    va_end(ap);
    if (n < 0)
    {
        return G3D_ERR_CORRUPT;
    }
    return g3d_wr_bytes(w, tmp, (g3d_u32)n);
}

static int g3d_wr_cstr(g3d_writer *w, const char *text)
{
    if (w == NULL || text == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    return g3d_wr_bytes(w, text, g3d_strlen32(text));
}

static int g3d_wr_bits_dec6(g3d_writer *w, g3d_u32 bits)
{
    char tmp[48];
    g3d_fix16 fx;
    if (w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    fx = g3d_f32bits_to_fix16(bits);
    g3d_fix16_to_dec6_text(fx, tmp, (g3d_u32)sizeof(tmp));
    return g3d_wr_cstr(w, tmp);
}

static int g3d_wr_bits_dec6_chr(g3d_writer *w, g3d_u32 bits, char suffix)
{
    int rc;
    rc = g3d_wr_bits_dec6(w, bits);
    if (rc != G3D_OK)
    {
        return rc;
    }
    return g3d_wr_bytes(w, &suffix, 1u);
}


static int g3d_wr_cstr_line(g3d_writer *w, const char *text)
{
    int rc;
    rc = g3d_wr_cstr(w, text);
    if (rc != G3D_OK)
    {
        return rc;
    }
    return g3d_wr_bytes(w, "\n", 1u);
}

static int g3d_mul_u32_checked(g3d_u32 a, g3d_u32 b, g3d_u32 *out)
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

static int g3d_add_u32_checked(g3d_u32 a, g3d_u32 b, g3d_u32 *out)
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


static void g3d_reader_init(g3d_reader *r, const void *buf, g3d_u32 size)
{
    r->buf = (const g3d_u8 *)buf;
    r->size = size;
    r->pos = 0u;
}

static int g3d_rd_line(g3d_reader *r, char *dst, g3d_u32 cap)
{
    g3d_u32 out;
    int saw_any;
    int too_long;
    unsigned char c;
    if (r == NULL || dst == NULL || cap == 0u)
    {
        return G3D_ERR_BAD_ARG;
    }
    out = 0u;
    saw_any = G3D_FALSE;
    too_long = G3D_FALSE;
    while (r->pos < r->size)
    {
        c = r->buf[r->pos++];
        saw_any = G3D_TRUE;
        if (c == (unsigned char)'\n')
        {
            break;
        }
        if (out + 1u < cap)
        {
            dst[out++] = (char)c;
        }
        else
        {
            too_long = G3D_TRUE;
        }
    }
    if (saw_any == G3D_FALSE)
    {
        return G3D_ERR_EOF;
    }
    dst[out] = '\0';
    if (too_long != G3D_FALSE)
    {
        return G3D_ERR_NO_SPACE;
    }
    while (out > 0u && (dst[out - 1u] == '\r' || dst[out - 1u] == '\n'))
    {
        --out;
        dst[out] = '\0';
    }
    return G3D_OK;
}


static void g3d_writer_init(g3d_writer *w, void *buf, g3d_u32 size)
{
    w->buf = (g3d_u8 *)buf;
    w->size = size;
    w->pos = 0u;
    w->measure_only = (buf == NULL) ? G3D_TRUE : G3D_FALSE;
    w->failed = G3D_FALSE;
}

static int g3d_rd_bytes(g3d_reader *r, void *dst, g3d_u32 n)
{
    if (r == NULL || (n != 0u && dst == NULL))
    {
        return G3D_ERR_BAD_ARG;
    }
    if (r->pos > r->size || n > (r->size - r->pos))
    {
        return G3D_ERR_EOF;
    }
    if (n != 0u)
    {
        memcpy(dst, r->buf + r->pos, (size_t)n);
    }
    r->pos += n;
    return G3D_OK;
}

static int g3d_rd_u8(g3d_reader *r, g3d_u8 *out)
{
    return g3d_rd_bytes(r, out, 1u);
}

static int g3d_rd_u16le(g3d_reader *r, g3d_u16 *out)
{
    g3d_u8 b[2];
    int rc;
    rc = g3d_rd_bytes(r, b, 2u);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *out = (g3d_u16)((g3d_u16)b[0] | ((g3d_u16)b[1] << 8));
    return G3D_OK;
}

static int g3d_rd_s16le(g3d_reader *r, g3d_s16 *out)
{
    g3d_u16 u;
    int rc;
    rc = g3d_rd_u16le(r, &u);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *out = (g3d_s16)u;
    return G3D_OK;
}

static int g3d_rd_u32le(g3d_reader *r, g3d_u32 *out)
{
    g3d_u8 b[4];
    int rc;
    rc = g3d_rd_bytes(r, b, 4u);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *out = ((g3d_u32)b[0]) |
           ((g3d_u32)b[1] << 8) |
           ((g3d_u32)b[2] << 16) |
           ((g3d_u32)b[3] << 24);
    return G3D_OK;
}

static int g3d_rd_s32le(g3d_reader *r, g3d_s32 *out)
{
    g3d_u32 u;
    int rc;
    rc = g3d_rd_u32le(r, &u);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *out = (g3d_s32)u;
    return G3D_OK;
}

static g3d_s32 g3d_rd_s32ptr(const g3d_u8 *p)
{
    return (g3d_s32)(((g3d_u32)p[0]) |
                     ((g3d_u32)p[1] << 8) |
                     ((g3d_u32)p[2] << 16) |
                     ((g3d_u32)p[3] << 24));
}

static g3d_u32 g3d_rd_u32ptr(const g3d_u8 *p)
{
    return ((g3d_u32)p[0]) |
           ((g3d_u32)p[1] << 8) |
           ((g3d_u32)p[2] << 16) |
           ((g3d_u32)p[3] << 24);
}


#define G3D_GEBM_VERSION_MAJOR 4u
#define G3D_GEBM_VERSION_MINOR 0u
#define G3D_GEBM_INFO_WH_LOG2 0x01u
#define G3D_GEBM_INFO_HAS_CK 0x02u
#define G3D_GEBM_INFO_HAS_ALPHA 0x04u
#define G3D_GEBM_INFO_HAS_PAL 0x08u
#define G3D_GEBM_INFO_WH_BYTE 0x20u
#define G3D_GEBM_MIP_MASK 0x0fu
#define G3D_GEBM_MIP_FLAG_COMPRESSED 0x10u
#define G3D_GEBM_MIP_FLAG_PAETH 0x20u
#define G3D_GEBM_PAL_SIZE_256 0x20u
#define G3D_GEBM_PAL_COMPRESSED 0x40u
#define G3D_GEBM_ALPHA_FLAG_COMPRESSED 0x01u

static int g3d_rd_skip(g3d_reader *r, g3d_u32 n)
{
    if (r == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (r->pos > r->size || n > (r->size - r->pos))
    {
        return G3D_ERR_EOF;
    }
    r->pos += n;
    return G3D_OK;
}

const char *g3d_pixel_format_name(g3d_u32 format)
{
    static const char *names[G3D_PIXELFORMAT_COUNT] = {
        "none",
        "8bit palettized",
        "8bit gray",
        "16bit 555 rgb",
        "16bit 555 bgr",
        "16bit 565 rgb",
        "16bit 565 bgr",
        "16bit 4444 argb",
        "16bit 1555 argb",
        "24bit rgb",
        "24bit bgr",
        "24bit yuv",
        "32bit rgbx",
        "32bit xrgb",
        "32bit bgrx",
        "32bit xbgr",
        "32bit rgba",
        "32bit argb",
        "32bit bgra",
        "32bit abgr",
        "wavelet",
        "16bit 88 uv",
        "16bit 556 uvl",
        "24bit 888 uvl"
    };
    if (format >= G3D_PIXELFORMAT_COUNT)
    {
        return "unknown";
    }
    return names[format];
}

g3d_u32 g3d_pixel_format_bytes_per_pixel(g3d_u32 format)
{
    static const g3d_u8 bpp[G3D_PIXELFORMAT_COUNT] = {
        0u, 1u, 1u,
        2u, 2u, 2u, 2u, 2u, 2u,
        3u, 3u, 3u,
        4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
        0u,
        2u, 2u, 3u
    };
    if (format >= G3D_PIXELFORMAT_COUNT)
    {
        return 0u;
    }
    return (g3d_u32)bpp[format];
}

static int g3d_bitmap_read_dimensions(g3d_reader *r, g3d_u8 info_flags, g3d_u32 *width_out, g3d_u32 *height_out)
{
    int rc;
    if (r == NULL || width_out == NULL || height_out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if ((info_flags & G3D_GEBM_INFO_WH_LOG2) != 0u)
    {
        g3d_u8 packed;
        g3d_u32 logw;
        g3d_u32 logh;
        rc = g3d_rd_u8(r, &packed);
        if (rc != G3D_OK)
        {
            return rc;
        }
        logw = ((g3d_u32)packed >> 4) & 0x0fu;
        logh = (g3d_u32)packed & 0x0fu;
        *width_out = (g3d_u32)1u << logw;
        *height_out = (g3d_u32)1u << logh;
    }
    else if ((info_flags & G3D_GEBM_INFO_WH_BYTE) != 0u)
    {
        g3d_u8 w8;
        g3d_u8 h8;
        rc = g3d_rd_u8(r, &w8);
        if (rc != G3D_OK)
        {
            return rc;
        }
        rc = g3d_rd_u8(r, &h8);
        if (rc != G3D_OK)
        {
            return rc;
        }
        *width_out = (g3d_u32)w8;
        *height_out = (g3d_u32)h8;
    }
    else
    {
        g3d_u16 w16;
        g3d_u16 h16;
        rc = g3d_rd_u16le(r, &w16);
        if (rc != G3D_OK)
        {
            return rc;
        }
        rc = g3d_rd_u16le(r, &h16);
        if (rc != G3D_OK)
        {
            return rc;
        }
        *width_out = (g3d_u32)w16;
        *height_out = (g3d_u32)h16;
    }
    if (*width_out == 0u || *height_out == 0u)
    {
        return G3D_ERR_CORRUPT;
    }
    return G3D_OK;
}

static g3d_u32 g3d_bitmap_mip_dim(g3d_u32 base, g3d_u32 mip)
{
    g3d_u32 v;
    if (mip >= 31u)
    {
        return 1u;
    }
    v = base >> mip;
    return (v == 0u) ? 1u : v;
}

static int g3d_bitmap_skip_color_key(g3d_reader *r, g3d_u32 format, g3d_bitmap_info *info)
{
    g3d_u32 bpp;
    g3d_u32 i;
    g3d_u8 byte;
    bpp = g3d_pixel_format_bytes_per_pixel(format);
    if (bpp == 0u || bpp > 4u)
    {
        return G3D_ERR_UNSUPPORTED;
    }
    info->color_key = 0u;
    for (i = 0u; i < bpp; ++i)
    {
        int rc;
        rc = g3d_rd_u8(r, &byte);
        if (rc != G3D_OK)
        {
            return rc;
        }
        info->color_key |= ((g3d_u32)byte << (i * 8u));
    }
    info->flags |= G3D_BITMAP_INFO_HAS_COLOR_KEY;
    return G3D_OK;
}

static int g3d_bitmap_parse_palette(g3d_reader *r, g3d_bitmap_info *info)
{
    g3d_u8 flags;
    g3d_u8 size_byte;
    g3d_u32 palette_format;
    g3d_u32 palette_size;
    g3d_u32 bpp;
    g3d_u32 data_size;
    int rc;

    rc = g3d_rd_u8(r, &flags);
    if (rc != G3D_OK)
    {
        return rc;
    }
    palette_format = (g3d_u32)(flags & 0x1fu);
    if ((flags & G3D_GEBM_PAL_SIZE_256) != 0u)
    {
        palette_size = 256u;
    }
    else
    {
        rc = g3d_rd_u8(r, &size_byte);
        if (rc != G3D_OK)
        {
            return rc;
        }
        palette_size = (g3d_u32)size_byte;
    }
    info->flags |= G3D_BITMAP_INFO_HAS_PALETTE;
    info->palette_format = (g3d_u8)palette_format;
    info->palette_size = palette_size;
    bpp = g3d_pixel_format_bytes_per_pixel(palette_format);
    if (palette_format >= G3D_PIXELFORMAT_COUNT || bpp == 0u)
    {
        return G3D_ERR_UNSUPPORTED;
    }
    rc = g3d_mul_u32_checked(palette_size, bpp, &data_size);
    if (rc != G3D_OK)
    {
        return rc;
    }
    info->palette_data_offset = r->pos;
    info->palette_data_size = data_size;
    info->palette_stored_size = data_size;
    info->palette_codec_method = G3D_KIB_METHOD_STORE;
    if ((flags & G3D_GEBM_PAL_COMPRESSED) != 0u)
    {
        g3d_u32 used;
        g3d_u32 method;
        rc = g3d_kib_decode_auto(r->buf + r->pos, r->size - r->pos,
                                 NULL, data_size, 0u, &used, &method);
        if (rc != G3D_OK)
        {
            info->flags |= G3D_BITMAP_INFO_UNSUPPORTED_COMPRESSED;
            return rc;
        }
        info->flags |= G3D_BITMAP_INFO_HAS_COMPRESSED_PALETTE;
        info->palette_stored_size = used;
        info->palette_codec_method = method;
        return g3d_rd_skip(r, used);
    }
    return g3d_rd_skip(r, data_size);
}

static int g3d_bitmap_parse_alpha(g3d_reader *r, g3d_bitmap_info *info)
{
    g3d_bitmap_info alpha_info;
    int rc;
    if (r == NULL || info == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (r->pos >= r->size)
    {
        return G3D_ERR_EOF;
    }
    rc = g3d_bitmap_probe(r->buf + r->pos, r->size - r->pos, &alpha_info);
    if (rc != G3D_OK)
    {
        return rc;
    }
    info->flags |= G3D_BITMAP_INFO_HAS_ALPHA;
    info->alpha_data_offset = r->pos;
    info->alpha_data_size = alpha_info.used_bytes;
    return g3d_rd_skip(r, alpha_info.used_bytes);
}

int g3d_bitmap_probe(const void *data, g3d_u32 size, g3d_bitmap_info *info)
{
    g3d_reader r;
    g3d_u32 tag;
    g3d_u8 version;
    g3d_u8 info_flags;
    g3d_u8 format;
    g3d_u8 max_seek;
    g3d_u8 maximum_mip;
    g3d_u8 seek_mip_count;
    g3d_u32 width;
    g3d_u32 height;
    g3d_u32 bpp;
    g3d_u32 mip_count;
    g3d_u32 i;
    int rc;

    if (data == NULL || info == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    g3d_zero(info, (g3d_u32)sizeof(*info));
    g3d_reader_init(&r, data, size);
    rc = g3d_rd_u32le(&r, &tag);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (tag != G3D_BITMAP_TAG_GEBM)
    {
        return G3D_ERR_BAD_MAGIC;
    }
    rc = g3d_rd_u8(&r, &version);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (((g3d_u32)(version >> 4)) != G3D_GEBM_VERSION_MAJOR)
    {
        return G3D_ERR_BAD_VERSION;
    }
    rc = g3d_rd_u8(&r, &info_flags);
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_rd_u8(&r, &format);
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_rd_u8(&r, &max_seek);
    if (rc != G3D_OK)
    {
        return rc;
    }
    maximum_mip = (g3d_u8)((max_seek >> 4) & 0x0fu);
    seek_mip_count = (g3d_u8)(max_seek & 0x0fu);
    rc = g3d_bitmap_read_dimensions(&r, info_flags, &width, &height);
    if (rc != G3D_OK)
    {
        return rc;
    }
    info->tag = tag;
    info->version = version;
    info->version_major = (g3d_u8)(version >> 4);
    info->version_minor = (g3d_u8)(version & 0x0fu);
    info->format = format;
    info->width = width;
    info->height = height;
    info->maximum_mip = maximum_mip;
    info->seek_mip_count = seek_mip_count;

    bpp = g3d_pixel_format_bytes_per_pixel((g3d_u32)format);
    if (format == G3D_PIXELFORMAT_WAVELET)
    {
        /* Genesis3D 1.0 leaves wavelet bitmaps disabled.  This codec accepts
           KIB experimental 8-bit wavelet payloads for extraction. */
        info->flags |= G3D_BITMAP_INFO_WAVELET_FORMAT;
        bpp = 1u;
    }
    else if ((g3d_u32)format >= G3D_PIXELFORMAT_COUNT || bpp == 0u)
    {
        return G3D_ERR_UNSUPPORTED;
    }
    if ((info_flags & G3D_GEBM_INFO_HAS_CK) != 0u)
    {
        rc = g3d_bitmap_skip_color_key(&r, (g3d_u32)format, info);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    if ((info_flags & G3D_GEBM_INFO_HAS_PAL) != 0u)
    {
        rc = g3d_bitmap_parse_palette(&r, info);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }

    mip_count = 0u;
    while (r.pos < r.size)
    {
        g3d_u8 mip_flags;
        g3d_u32 mip_index;
        g3d_u32 mw;
        g3d_u32 mh;
        g3d_u32 data_size;
        g3d_u32 pixels;
        rc = g3d_rd_u8(&r, &mip_flags);
        if (rc != G3D_OK)
        {
            return rc;
        }
        mip_index = (g3d_u32)(mip_flags & G3D_GEBM_MIP_MASK);
        if (mip_index > (g3d_u32)maximum_mip)
        {
            break;
        }
        if (mip_count >= G3D_BITMAP_MAX_MIPS)
        {
            return G3D_ERR_UNSUPPORTED;
        }
        mw = g3d_bitmap_mip_dim(width, mip_index);
        mh = g3d_bitmap_mip_dim(height, mip_index);
        rc = g3d_mul_u32_checked(mw, mh, &pixels);
        if (rc != G3D_OK)
        {
            return rc;
        }
        rc = g3d_mul_u32_checked(pixels, bpp, &data_size);
        if (rc != G3D_OK)
        {
            return rc;
        }
        info->mips[mip_count].mip_index = (g3d_u8)mip_index;
        info->mips[mip_count].flags = mip_flags;
        info->mips[mip_count].width = mw;
        info->mips[mip_count].height = mh;
        info->mips[mip_count].data_offset = r.pos;
        info->mips[mip_count].data_size = data_size;
        info->mips[mip_count].stored_size = data_size;
        info->mips[mip_count].codec_method = G3D_KIB_METHOD_STORE;
        if ((mip_flags & G3D_GEBM_MIP_FLAG_COMPRESSED) != 0u)
        {
            g3d_u32 used;
            g3d_u32 method;
            rc = g3d_kib_decode_auto(r.buf + r.pos, r.size - r.pos,
                                     NULL, data_size, 0u, &used, &method);
            if (rc != G3D_OK)
            {
                info->flags |= G3D_BITMAP_INFO_UNSUPPORTED_COMPRESSED;
                return rc;
            }
            info->flags |= G3D_BITMAP_INFO_HAS_COMPRESSED_MIPS;
            info->mips[mip_count].stored_size = used;
            info->mips[mip_count].codec_method = method;
        }
        if ((mip_flags & G3D_GEBM_MIP_FLAG_PAETH) != 0u)
        {
            info->flags |= G3D_BITMAP_INFO_HAS_PAETH_MIPS;
        }
        ++mip_count;
        rc = g3d_rd_skip(&r, info->mips[mip_count - 1u].stored_size);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    info->mip_count = mip_count;
    if ((info_flags & G3D_GEBM_INFO_HAS_ALPHA) != 0u)
    {
        rc = g3d_bitmap_parse_alpha(&r, info);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    if (mip_count == 0u)
    {
        return G3D_ERR_CORRUPT;
    }
    for (i = 1u; i < mip_count; ++i)
    {
        if (info->mips[i].mip_index <= info->mips[i - 1u].mip_index)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    info->used_bytes = r.pos;
    if (r.pos != r.size)
    {
        return G3D_ERR_CORRUPT;
    }
    return G3D_OK;
}


static g3d_u16 g3d_rd_u16ptr_le(const g3d_u8 *p)
{
    return (g3d_u16)(((g3d_u16)p[0]) | ((g3d_u16)p[1] << 8));
}

static int g3d_bitmap_find_mip_index(const g3d_bitmap_info *info, g3d_u32 wanted_mip, g3d_u32 *out_index)
{
    g3d_u32 i;
    if (info == NULL || out_index == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    for (i = 0u; i < info->mip_count; ++i)
    {
        if ((g3d_u32)info->mips[i].mip_index == wanted_mip)
        {
            *out_index = i;
            return G3D_OK;
        }
    }
    return G3D_ERR_NOT_FOUND;
}

static int g3d_bitmap_unpack_mip_from_info(const g3d_u8 *bytes, const g3d_bitmap_info *info,
                                           g3d_u32 mip_array_index,
                                           g3d_u8 *dst, g3d_u32 dst_size,
                                           g3d_u32 *width_out, g3d_u32 *height_out,
                                           g3d_u32 *raw_bytes_out,
                                           g3d_u32 *codec_method_out,
                                           g3d_arena *scratch_arena)
{
    const g3d_bitmap_mip_info *mip;
    const g3d_u8 *src;
    g3d_u32 used;
    g3d_u32 method;
    int rc;
    if (bytes == NULL || info == NULL || dst == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (mip_array_index >= info->mip_count)
    {
        return G3D_ERR_BAD_ARG;
    }
    mip = &info->mips[mip_array_index];
    if (dst_size < mip->data_size)
    {
        return G3D_ERR_NO_SPACE;
    }
    src = bytes + mip->data_offset;
    method = mip->codec_method;
    if ((mip->flags & G3D_GEBM_MIP_FLAG_COMPRESSED) != 0u)
    {
        rc = g3d_kib_decode_auto_arena(src, mip->stored_size, dst, mip->data_size, 0u, &used, &method, scratch_arena);
        if (rc != G3D_OK)
        {
            return rc;
        }
        if (used != mip->stored_size)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    else
    {
        memcpy(dst, src, (size_t)mip->data_size);
        method = G3D_KIB_METHOD_STORE;
    }
    if ((mip->flags & G3D_GEBM_MIP_FLAG_PAETH) != 0u)
    {
        g3d_u32 bpp;
        bpp = g3d_pixel_format_bytes_per_pixel((g3d_u32)info->format);
        if (info->format == G3D_PIXELFORMAT_WAVELET)
        {
            bpp = 1u;
        }
        if (bpp == 0u)
        {
            return G3D_ERR_UNSUPPORTED;
        }
        rc = g3d_kib_paeth_unfilter(dst, mip->width, mip->height, bpp);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    if (width_out != NULL)
    {
        *width_out = mip->width;
    }
    if (height_out != NULL)
    {
        *height_out = mip->height;
    }
    if (raw_bytes_out != NULL)
    {
        *raw_bytes_out = mip->data_size;
    }
    if (codec_method_out != NULL)
    {
        *codec_method_out = method;
    }
    return G3D_OK;
}

int g3d_bitmap_unpack_mip_arena(const void *data, g3d_u32 size, g3d_u32 wanted_mip,
                                g3d_u8 *dst, g3d_u32 dst_size,
                                g3d_u32 *width_out, g3d_u32 *height_out,
                                g3d_u32 *raw_bytes_out,
                                g3d_u32 *codec_method_out,
                                g3d_arena *scratch_arena)
{
    const g3d_u8 *bytes;
    g3d_bitmap_info info;
    g3d_u32 mip_i;
    int rc;
    if (data == NULL || dst == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    bytes = (const g3d_u8 *)data;
    rc = g3d_bitmap_probe(data, size, &info);
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_bitmap_find_mip_index(&info, wanted_mip, &mip_i);
    if (rc != G3D_OK)
    {
        return rc;
    }
    return g3d_bitmap_unpack_mip_from_info(bytes, &info, mip_i, dst, dst_size,
                                           width_out, height_out, raw_bytes_out,
                                           codec_method_out, scratch_arena);
}

int g3d_bitmap_unpack_mip(const void *data, g3d_u32 size, g3d_u32 wanted_mip,
                          g3d_u8 *dst, g3d_u32 dst_size,
                          g3d_u32 *width_out, g3d_u32 *height_out,
                          g3d_u32 *raw_bytes_out,
                          g3d_u32 *codec_method_out)
{
    return g3d_bitmap_unpack_mip_arena(data, size, wanted_mip, dst, dst_size,
                                       width_out, height_out, raw_bytes_out,
                                       codec_method_out, NULL);
}

static int g3d_bitmap_mip_needs_unpack_buffer(const g3d_bitmap_mip_info *mip)
{
    if (mip == NULL)
    {
        return G3D_TRUE;
    }
    if ((mip->flags & G3D_GEBM_MIP_FLAG_COMPRESSED) != 0u)
    {
        return G3D_TRUE;
    }
    if ((mip->flags & G3D_GEBM_MIP_FLAG_PAETH) != 0u)
    {
        return G3D_TRUE;
    }
    return G3D_FALSE;
}

static int g3d_bitmap_unpack_palette_arena(const g3d_u8 *base, const g3d_bitmap_info *info,
                                           const g3d_u8 **palette_out,
                                           g3d_arena *scratch_arena)
{
    g3d_u8 *decoded;
    g3d_u32 used;
    g3d_u32 method;
    int rc;
    if (base == NULL || info == NULL || palette_out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    *palette_out = NULL;
    if ((info->flags & G3D_BITMAP_INFO_HAS_PALETTE) == 0u)
    {
        return G3D_OK;
    }
    if ((info->flags & G3D_BITMAP_INFO_HAS_COMPRESSED_PALETTE) == 0u)
    {
        *palette_out = base + info->palette_data_offset;
        return G3D_OK;
    }
    if (scratch_arena == NULL && info->palette_data_size != 0u)
    {
        return G3D_ERR_ARENA_EXHAUSTED;
    }
    decoded = NULL;
    if (scratch_arena != NULL)
    {
        decoded = (g3d_u8 *)g3d_arena_alloc(scratch_arena, info->palette_data_size, 1u);
    }
    if (decoded == NULL && info->palette_data_size != 0u)
    {
        return G3D_ERR_ARENA_EXHAUSTED;
    }
    rc = g3d_kib_decode_auto_arena(base + info->palette_data_offset, info->palette_stored_size,
                                   decoded, info->palette_data_size, 0u,
                                   &used, &method, scratch_arena);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used != info->palette_stored_size)
    {
        return G3D_ERR_CORRUPT;
    }
    *palette_out = decoded;
    return G3D_OK;
}

static g3d_u8 g3d_expand_5_to_8(g3d_u32 v)
{
    return (g3d_u8)(((v & 31u) << 3) | ((v & 31u) >> 2));
}

static g3d_u8 g3d_expand_6_to_8(g3d_u32 v)
{
    return (g3d_u8)(((v & 63u) << 2) | ((v & 63u) >> 4));
}

static g3d_u8 g3d_expand_4_to_8(g3d_u32 v)
{
    return (g3d_u8)(((v & 15u) << 4) | (v & 15u));
}

static int g3d_bitmap_raw_pixel_value(const g3d_u8 *src, g3d_u32 format, g3d_u32 *pixel_out)
{
    g3d_u32 bpp;
    if (src == NULL || pixel_out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    bpp = g3d_pixel_format_bytes_per_pixel(format);
    if (bpp == 1u)
    {
        *pixel_out = (g3d_u32)src[0];
        return G3D_OK;
    }
    if (bpp == 2u)
    {
        *pixel_out = (g3d_u32)g3d_rd_u16ptr_le(src);
        return G3D_OK;
    }
    if (bpp == 3u)
    {
        *pixel_out = ((g3d_u32)src[0] << 16) | ((g3d_u32)src[1] << 8) | (g3d_u32)src[2];
        return G3D_OK;
    }
    if (bpp == 4u)
    {
        *pixel_out = g3d_rd_u32ptr(src);
        return G3D_OK;
    }
    return G3D_ERR_UNSUPPORTED;
}

static void g3d_bitmap_rgba_write(g3d_u8 *dst, g3d_u32 r, g3d_u32 g, g3d_u32 b, g3d_u32 a)
{
    dst[0] = (g3d_u8)(r & 0xffu);
    dst[1] = (g3d_u8)(g & 0xffu);
    dst[2] = (g3d_u8)(b & 0xffu);
    dst[3] = (g3d_u8)(a & 0xffu);
}

static int g3d_bitmap_decode_direct_pixel(const g3d_u8 *src, g3d_u32 format, g3d_u8 *dst_rgba)
{
    g3d_u16 p16;
    g3d_u32 p32;
    if (src == NULL || dst_rgba == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    switch (format)
    {
        case G3D_PIXELFORMAT_8BIT_GRAY:
            g3d_bitmap_rgba_write(dst_rgba, src[0], src[0], src[0], 255u);
            return G3D_OK;
        case G3D_PIXELFORMAT_16BIT_555_RGB:
            p16 = g3d_rd_u16ptr_le(src);
            g3d_bitmap_rgba_write(dst_rgba,
                                  g3d_expand_5_to_8((p16 >> 10) & 31u),
                                  g3d_expand_5_to_8((p16 >> 5) & 31u),
                                  g3d_expand_5_to_8(p16 & 31u),
                                  255u);
            return G3D_OK;
        case G3D_PIXELFORMAT_16BIT_555_BGR:
            p16 = g3d_rd_u16ptr_le(src);
            g3d_bitmap_rgba_write(dst_rgba,
                                  g3d_expand_5_to_8(p16 & 31u),
                                  g3d_expand_5_to_8((p16 >> 5) & 31u),
                                  g3d_expand_5_to_8((p16 >> 10) & 31u),
                                  255u);
            return G3D_OK;
        case G3D_PIXELFORMAT_16BIT_565_RGB:
            p16 = g3d_rd_u16ptr_le(src);
            g3d_bitmap_rgba_write(dst_rgba,
                                  g3d_expand_5_to_8((p16 >> 11) & 31u),
                                  g3d_expand_6_to_8((p16 >> 5) & 63u),
                                  g3d_expand_5_to_8(p16 & 31u),
                                  255u);
            return G3D_OK;
        case G3D_PIXELFORMAT_16BIT_565_BGR:
            p16 = g3d_rd_u16ptr_le(src);
            g3d_bitmap_rgba_write(dst_rgba,
                                  g3d_expand_5_to_8(p16 & 31u),
                                  g3d_expand_6_to_8((p16 >> 5) & 63u),
                                  g3d_expand_5_to_8((p16 >> 11) & 31u),
                                  255u);
            return G3D_OK;
        case G3D_PIXELFORMAT_16BIT_4444_ARGB:
            p16 = g3d_rd_u16ptr_le(src);
            g3d_bitmap_rgba_write(dst_rgba,
                                  g3d_expand_4_to_8((p16 >> 8) & 15u),
                                  g3d_expand_4_to_8((p16 >> 4) & 15u),
                                  g3d_expand_4_to_8(p16 & 15u),
                                  g3d_expand_4_to_8((p16 >> 12) & 15u));
            return G3D_OK;
        case G3D_PIXELFORMAT_16BIT_1555_ARGB:
            p16 = g3d_rd_u16ptr_le(src);
            g3d_bitmap_rgba_write(dst_rgba,
                                  g3d_expand_5_to_8((p16 >> 10) & 31u),
                                  g3d_expand_5_to_8((p16 >> 5) & 31u),
                                  g3d_expand_5_to_8(p16 & 31u),
                                  ((p16 & 0x8000u) != 0u) ? 255u : 0u);
            return G3D_OK;
        case G3D_PIXELFORMAT_24BIT_RGB:
        case G3D_PIXELFORMAT_24BIT_YUV:
            g3d_bitmap_rgba_write(dst_rgba, src[0], src[1], src[2], 255u);
            return G3D_OK;
        case G3D_PIXELFORMAT_24BIT_BGR:
            g3d_bitmap_rgba_write(dst_rgba, src[2], src[1], src[0], 255u);
            return G3D_OK;
        case G3D_PIXELFORMAT_32BIT_RGBX:
            g3d_bitmap_rgba_write(dst_rgba, src[3], src[2], src[1], 255u);
            return G3D_OK;
        case G3D_PIXELFORMAT_32BIT_XRGB:
            g3d_bitmap_rgba_write(dst_rgba, src[2], src[1], src[0], 255u);
            return G3D_OK;
        case G3D_PIXELFORMAT_32BIT_BGRX:
            g3d_bitmap_rgba_write(dst_rgba, src[1], src[2], src[3], 255u);
            return G3D_OK;
        case G3D_PIXELFORMAT_32BIT_XBGR:
            g3d_bitmap_rgba_write(dst_rgba, src[0], src[1], src[2], 255u);
            return G3D_OK;
        case G3D_PIXELFORMAT_32BIT_RGBA:
            g3d_bitmap_rgba_write(dst_rgba, src[3], src[2], src[1], src[0]);
            return G3D_OK;
        case G3D_PIXELFORMAT_32BIT_ARGB:
            g3d_bitmap_rgba_write(dst_rgba, src[2], src[1], src[0], src[3]);
            return G3D_OK;
        case G3D_PIXELFORMAT_32BIT_BGRA:
            g3d_bitmap_rgba_write(dst_rgba, src[1], src[2], src[3], src[0]);
            return G3D_OK;
        case G3D_PIXELFORMAT_32BIT_ABGR:
            g3d_bitmap_rgba_write(dst_rgba, src[0], src[1], src[2], src[3]);
            return G3D_OK;
        case G3D_PIXELFORMAT_16BIT_88_UV:
            g3d_bitmap_rgba_write(dst_rgba, src[0], src[1], 128u, 255u);
            return G3D_OK;
        case G3D_PIXELFORMAT_16BIT_556_UVL:
            p16 = g3d_rd_u16ptr_le(src);
            g3d_bitmap_rgba_write(dst_rgba,
                                  g3d_expand_5_to_8(p16 & 31u),
                                  g3d_expand_5_to_8((p16 >> 5) & 31u),
                                  g3d_expand_6_to_8((p16 >> 10) & 63u),
                                  255u);
            return G3D_OK;
        case G3D_PIXELFORMAT_24BIT_888_UVL:
            p32 = ((g3d_u32)src[0] << 16) | ((g3d_u32)src[1] << 8) | (g3d_u32)src[2];
            (void)p32;
            g3d_bitmap_rgba_write(dst_rgba, src[2], src[1], src[0], 255u);
            return G3D_OK;
        default:
            return G3D_ERR_UNSUPPORTED;
    }
}

static int g3d_bitmap_decode_pixel_rgba(const g3d_bitmap_info *info,
                                        const g3d_u8 *src, g3d_u32 decode_flags,
                                        const g3d_u8 *palette_data,
                                        g3d_u8 *dst_rgba)
{
    g3d_u32 bpp;
    g3d_u32 raw_pixel;
    int rc;
    if (info == NULL || src == NULL || dst_rgba == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (info->format == G3D_PIXELFORMAT_WAVELET)
    {
        g3d_bitmap_rgba_write(dst_rgba, src[0], src[0], src[0], 255u);
        return G3D_OK;
    }
    bpp = g3d_pixel_format_bytes_per_pixel((g3d_u32)info->format);
    if (bpp == 0u)
    {
        return G3D_ERR_UNSUPPORTED;
    }
    rc = g3d_bitmap_raw_pixel_value(src, (g3d_u32)info->format, &raw_pixel);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (info->format == G3D_PIXELFORMAT_8BIT)
    {
        g3d_u32 index;
        g3d_u32 pal_bpp;
        const g3d_u8 *pal_src;
        index = raw_pixel & 0xffu;
        if ((info->flags & G3D_BITMAP_INFO_HAS_PALETTE) == 0u || palette_data == NULL || index >= info->palette_size)
        {
            return G3D_ERR_CORRUPT;
        }
        pal_bpp = g3d_pixel_format_bytes_per_pixel((g3d_u32)info->palette_format);
        if (pal_bpp == 0u || pal_bpp > info->palette_data_size ||
            index > ((info->palette_data_size / pal_bpp) - 1u))
        {
            return G3D_ERR_CORRUPT;
        }
        pal_src = palette_data + (index * pal_bpp);
        rc = g3d_bitmap_decode_direct_pixel(pal_src, (g3d_u32)info->palette_format, dst_rgba);
    }
    else
    {
        rc = g3d_bitmap_decode_direct_pixel(src, (g3d_u32)info->format, dst_rgba);
    }
    if (rc != G3D_OK)
    {
        return rc;
    }
    if ((decode_flags & G3D_BITMAP_DECODE_APPLY_COLOR_KEY) != 0u &&
        (info->flags & G3D_BITMAP_INFO_HAS_COLOR_KEY) != 0u &&
        raw_pixel == info->color_key)
    {
        dst_rgba[3] = 0u;
    }
    return G3D_OK;
}

int g3d_bitmap_decode_mip_rgba32_arena(const void *data, g3d_u32 size, g3d_u32 wanted_mip,
                                        g3d_u32 decode_flags,
                                        g3d_u8 *dst_rgba, g3d_u32 dst_size,
                                        g3d_u32 *width_out, g3d_u32 *height_out,
                                        g3d_u32 *rgba_bytes_out,
                                        g3d_arena *scratch_arena)
{
    const g3d_u8 *bytes;
    g3d_bitmap_info info;
    g3d_bitmap_info alpha_info;
    const g3d_u8 *alpha_base;
    const g3d_u8 *mip_data;
    const g3d_u8 *alpha_mip_data;
    const g3d_u8 *palette_data;
    const g3d_u8 *alpha_palette_data;
    g3d_u32 mip_i;
    g3d_u32 alpha_mip_i;
    g3d_u32 bpp;
    g3d_u32 alpha_bpp;
    g3d_u32 pixels;
    g3d_u32 out_bytes;
    g3d_u32 i;
    g3d_u32 mark;
    int have_mark;
    int have_alpha;
    int rc;

    if (data == NULL || dst_rgba == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    bytes = (const g3d_u8 *)data;
    alpha_base = NULL;
    mip_data = NULL;
    alpha_mip_data = NULL;
    palette_data = NULL;
    alpha_palette_data = NULL;
    mip_i = 0u;
    alpha_mip_i = 0u;
    bpp = 0u;
    alpha_bpp = 0u;
    pixels = 0u;
    out_bytes = 0u;
    mark = 0u;
    have_mark = G3D_FALSE;
    have_alpha = G3D_FALSE;
    rc = G3D_OK;

    if (scratch_arena != NULL)
    {
        mark = g3d_arena_mark(scratch_arena);
        have_mark = G3D_TRUE;
    }

    rc = g3d_bitmap_probe(data, size, &info);
    if (rc != G3D_OK)
    {
        goto done;
    }
    rc = g3d_bitmap_find_mip_index(&info, wanted_mip, &mip_i);
    if (rc != G3D_OK)
    {
        goto done;
    }
    rc = g3d_mul_u32_checked(info.mips[mip_i].width, info.mips[mip_i].height, &pixels);
    if (rc != G3D_OK)
    {
        goto done;
    }
    rc = g3d_mul_u32_checked(pixels, 4u, &out_bytes);
    if (rc != G3D_OK)
    {
        goto done;
    }
    if (dst_size < out_bytes)
    {
        rc = G3D_ERR_NO_SPACE;
        goto done;
    }
    bpp = g3d_pixel_format_bytes_per_pixel((g3d_u32)info.format);
    if (info.format == G3D_PIXELFORMAT_WAVELET)
    {
        bpp = 1u;
    }
    if (bpp == 0u)
    {
        rc = G3D_ERR_UNSUPPORTED;
        goto done;
    }

    rc = g3d_bitmap_unpack_palette_arena(bytes, &info, &palette_data, scratch_arena);
    if (rc != G3D_OK)
    {
        goto done;
    }
    if (g3d_bitmap_mip_needs_unpack_buffer(&info.mips[mip_i]) != G3D_FALSE)
    {
        g3d_u8 *mip_tmp;
        if (scratch_arena == NULL && info.mips[mip_i].data_size != 0u)
        {
            rc = G3D_ERR_ARENA_EXHAUSTED;
            goto done;
        }
        mip_tmp = NULL;
        if (scratch_arena != NULL)
        {
            mip_tmp = (g3d_u8 *)g3d_arena_alloc(scratch_arena, info.mips[mip_i].data_size, 1u);
        }
        if (mip_tmp == NULL && info.mips[mip_i].data_size != 0u)
        {
            rc = G3D_ERR_ARENA_EXHAUSTED;
            goto done;
        }
        rc = g3d_bitmap_unpack_mip_from_info(bytes, &info, mip_i, mip_tmp,
                                             info.mips[mip_i].data_size,
                                             NULL, NULL, NULL, NULL,
                                             scratch_arena);
        if (rc != G3D_OK)
        {
            goto done;
        }
        mip_data = mip_tmp;
    }
    else
    {
        mip_data = bytes + info.mips[mip_i].data_offset;
    }

    if ((decode_flags & G3D_BITMAP_DECODE_APPLY_ALPHA) != 0u &&
        (info.flags & G3D_BITMAP_INFO_HAS_ALPHA) != 0u &&
        info.alpha_data_size != 0u)
    {
        alpha_base = bytes + info.alpha_data_offset;
        rc = g3d_bitmap_probe(alpha_base, info.alpha_data_size, &alpha_info);
        if (rc != G3D_OK)
        {
            goto done;
        }
        rc = g3d_bitmap_find_mip_index(&alpha_info, wanted_mip, &alpha_mip_i);
        if (rc != G3D_OK)
        {
            goto done;
        }
        if (alpha_info.mips[alpha_mip_i].width != info.mips[mip_i].width ||
            alpha_info.mips[alpha_mip_i].height != info.mips[mip_i].height)
        {
            rc = G3D_ERR_UNSUPPORTED;
            goto done;
        }
        alpha_bpp = g3d_pixel_format_bytes_per_pixel((g3d_u32)alpha_info.format);
        if (alpha_info.format == G3D_PIXELFORMAT_WAVELET)
        {
            alpha_bpp = 1u;
        }
        if (alpha_bpp == 0u)
        {
            rc = G3D_ERR_UNSUPPORTED;
            goto done;
        }
        rc = g3d_bitmap_unpack_palette_arena(alpha_base, &alpha_info, &alpha_palette_data, scratch_arena);
        if (rc != G3D_OK)
        {
            goto done;
        }
        if (g3d_bitmap_mip_needs_unpack_buffer(&alpha_info.mips[alpha_mip_i]) != G3D_FALSE)
        {
            g3d_u8 *alpha_tmp;
            if (scratch_arena == NULL && alpha_info.mips[alpha_mip_i].data_size != 0u)
            {
                rc = G3D_ERR_ARENA_EXHAUSTED;
                goto done;
            }
            alpha_tmp = NULL;
            if (scratch_arena != NULL)
            {
                alpha_tmp = (g3d_u8 *)g3d_arena_alloc(scratch_arena,
                                                      alpha_info.mips[alpha_mip_i].data_size, 1u);
            }
            if (alpha_tmp == NULL && alpha_info.mips[alpha_mip_i].data_size != 0u)
            {
                rc = G3D_ERR_ARENA_EXHAUSTED;
                goto done;
            }
            rc = g3d_bitmap_unpack_mip_from_info(alpha_base, &alpha_info, alpha_mip_i,
                                                 alpha_tmp, alpha_info.mips[alpha_mip_i].data_size,
                                                 NULL, NULL, NULL, NULL,
                                                 scratch_arena);
            if (rc != G3D_OK)
            {
                goto done;
            }
            alpha_mip_data = alpha_tmp;
        }
        else
        {
            alpha_mip_data = alpha_base + alpha_info.mips[alpha_mip_i].data_offset;
        }
        have_alpha = G3D_TRUE;
    }

    for (i = 0u; i < pixels; ++i)
    {
        g3d_u8 *dst;
        dst = dst_rgba + (i * 4u);
        rc = g3d_bitmap_decode_pixel_rgba(&info, mip_data + (i * bpp), decode_flags, palette_data, dst);
        if (rc != G3D_OK)
        {
            goto done;
        }
        if (have_alpha)
        {
            g3d_u8 alpha_rgba[4];
            g3d_u32 alpha_sum;
            rc = g3d_bitmap_decode_pixel_rgba(&alpha_info, alpha_mip_data + (i * alpha_bpp), 0u,
                                              alpha_palette_data, alpha_rgba);
            if (rc != G3D_OK)
            {
                goto done;
            }
            alpha_sum = (g3d_u32)alpha_rgba[0] + (g3d_u32)alpha_rgba[1] + (g3d_u32)alpha_rgba[2];
            dst[3] = (g3d_u8)(alpha_sum / 3u);
        }
    }
    if (width_out != NULL)
    {
        *width_out = info.mips[mip_i].width;
    }
    if (height_out != NULL)
    {
        *height_out = info.mips[mip_i].height;
    }
    if (rgba_bytes_out != NULL)
    {
        *rgba_bytes_out = out_bytes;
    }

done:
    if (have_mark != G3D_FALSE)
    {
        g3d_arena_rewind(scratch_arena, mark);
    }
    return rc;
}

int g3d_bitmap_decode_mip_rgba32(const void *data, g3d_u32 size, g3d_u32 wanted_mip,
                                  g3d_u32 decode_flags,
                                  g3d_u8 *dst_rgba, g3d_u32 dst_size,
                                  g3d_u32 *width_out, g3d_u32 *height_out,
                                  g3d_u32 *rgba_bytes_out)
{
    return g3d_bitmap_decode_mip_rgba32_arena(data, size, wanted_mip,
                                              decode_flags, dst_rgba, dst_size,
                                              width_out, height_out, rgba_bytes_out,
                                              NULL);
}

int g3d_body_decode_material_rgba32_arena(const g3d_body *body, g3d_s32 material_index, g3d_u32 wanted_mip,
                                          g3d_u32 decode_flags,
                                          g3d_u8 *dst_rgba, g3d_u32 dst_size,
                                          g3d_u32 *width_out, g3d_u32 *height_out,
                                          g3d_u32 *rgba_bytes_out,
                                          g3d_arena *scratch_arena)
{
    const g3d_body_material *mat;
    if (body == NULL || dst_rgba == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (material_index < 0 || material_index >= body->material_count)
    {
        return G3D_ERR_BAD_ARG;
    }
    mat = &body->materials[(g3d_u32)material_index];
    if (mat->has_bitmap == 0u || mat->bitmap_data == NULL || mat->bitmap_size == 0u)
    {
        return G3D_ERR_NOT_FOUND;
    }
    return g3d_bitmap_decode_mip_rgba32_arena(mat->bitmap_data, mat->bitmap_size, wanted_mip,
                                              decode_flags, dst_rgba, dst_size,
                                              width_out, height_out, rgba_bytes_out,
                                              scratch_arena);
}

int g3d_body_decode_material_rgba32(const g3d_body *body, g3d_s32 material_index, g3d_u32 wanted_mip,
                                    g3d_u32 decode_flags,
                                    g3d_u8 *dst_rgba, g3d_u32 dst_size,
                                    g3d_u32 *width_out, g3d_u32 *height_out,
                                    g3d_u32 *rgba_bytes_out)
{
    return g3d_body_decode_material_rgba32_arena(body, material_index, wanted_mip,
                                                decode_flags, dst_rgba, dst_size,
                                                width_out, height_out, rgba_bytes_out,
                                                NULL);
}

static int g3d_wr_bytes(g3d_writer *w, const void *src, g3d_u32 n)
{
    if (w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (!w->measure_only)
    {
        if (w->pos > w->size || n > (w->size - w->pos))
        {
            w->failed = G3D_TRUE;
            return G3D_ERR_NO_SPACE;
        }
        if (n != 0u && src != NULL)
        {
            memcpy(w->buf + w->pos, src, (size_t)n);
        }
        else if (n != 0u && src == NULL)
        {
            memset(w->buf + w->pos, 0, (size_t)n);
        }
    }
    w->pos += n;
    return G3D_OK;
}

static int g3d_xor_writer_range(g3d_writer *w, g3d_u32 start_pos, g3d_u32 n, g3d_u8 xor_mask)
{
    g3d_u32 i;
    if (w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (n == 0u || xor_mask == 0u)
    {
        return G3D_OK;
    }
    if (w->measure_only != 0)
    {
        return G3D_OK;
    }
    if (start_pos > w->pos || n > (w->pos - start_pos))
    {
        return G3D_ERR_CORRUPT;
    }
    for (i = 0u; i < n; ++i)
    {
        w->buf[start_pos + i] = (g3d_u8)(w->buf[start_pos + i] ^ xor_mask);
    }
    return G3D_OK;
}

static int g3d_wr_u8(g3d_writer *w, g3d_u8 v)
{
    return g3d_wr_bytes(w, &v, 1u);
}

static int g3d_wr_u16le(g3d_writer *w, g3d_u16 v)
{
    g3d_u8 b[2];
    b[0] = (g3d_u8)(v & 0xffu);
    b[1] = (g3d_u8)((v >> 8) & 0xffu);
    return g3d_wr_bytes(w, b, 2u);
}

static int g3d_wr_s16le(g3d_writer *w, g3d_s16 v)
{
    return g3d_wr_u16le(w, (g3d_u16)v);
}

static int g3d_wr_u32le(g3d_writer *w, g3d_u32 v)
{
    g3d_u8 b[4];
    b[0] = (g3d_u8)(v & 0xffu);
    b[1] = (g3d_u8)((v >> 8) & 0xffu);
    b[2] = (g3d_u8)((v >> 16) & 0xffu);
    b[3] = (g3d_u8)((v >> 24) & 0xffu);
    return g3d_wr_bytes(w, b, 4u);
}

static int g3d_wr_s32le(g3d_writer *w, g3d_s32 v)
{
    return g3d_wr_u32le(w, (g3d_u32)v);
}

static int g3d_patch_u32le(g3d_writer *w, g3d_u32 offset, g3d_u32 v)
{
    if (w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (w->measure_only)
    {
        return G3D_OK;
    }
    if (offset > w->size || 4u > (w->size - offset))
    {
        return G3D_ERR_NO_SPACE;
    }
    w->buf[offset + 0u] = (g3d_u8)(v & 0xffu);
    w->buf[offset + 1u] = (g3d_u8)((v >> 8) & 0xffu);
    w->buf[offset + 2u] = (g3d_u8)((v >> 16) & 0xffu);
    w->buf[offset + 3u] = (g3d_u8)((v >> 24) & 0xffu);
    return G3D_OK;
}

void g3d_arena_init(g3d_arena *arena, void *memory, g3d_u32 size)
{
    arena->base = (g3d_u8 *)memory;
    arena->size = size;
    arena->used = 0u;
}

void g3d_arena_reset(g3d_arena *arena)
{
    if (arena != NULL)
    {
        arena->used = 0u;
    }
}

g3d_u32 g3d_arena_mark(const g3d_arena *arena)
{
    if (arena == NULL)
    {
        return 0u;
    }
    return arena->used;
}

void g3d_arena_rewind(g3d_arena *arena, g3d_u32 mark)
{
    if (arena != NULL && mark <= arena->used)
    {
        arena->used = mark;
    }
}

void *g3d_arena_alloc(g3d_arena *arena, g3d_u32 size, g3d_u32 alignment)
{
    g3d_u32 pos;
    g3d_u8 *ptr;
    if (arena == NULL || alignment == 0u)
    {
        return NULL;
    }
    pos = G3D_ALIGN_UP(arena->used, alignment);
    if (pos > arena->size || size > (arena->size - pos))
    {
        return NULL;
    }
    ptr = arena->base + pos;
    arena->used = pos + size;
    return ptr;
}

char *g3d_arena_strdup_len(g3d_arena *arena, const char *src, g3d_u32 len)
{
    char *dst;
    if (arena == NULL || src == NULL)
    {
        return NULL;
    }
    dst = (char *)g3d_arena_alloc(arena, len + 1u, 1u);
    if (dst == NULL)
    {
        return NULL;
    }
    if (len != 0u)
    {
        memcpy(dst, src, (size_t)len);
    }
    dst[len] = '\0';
    return dst;
}

char *g3d_arena_strdup(g3d_arena *arena, const char *src)
{
    if (src == NULL)
    {
        return NULL;
    }
    return g3d_arena_strdup_len(arena, src, g3d_strlen32(src));
}

const char *g3d_errstr(int code)
{
    switch (code)
    {
        case G3D_OK: return "ok";
        case G3D_ERR_BAD_ARG: return "bad argument";
        case G3D_ERR_NO_SPACE: return "buffer too small";
        case G3D_ERR_ARENA_EXHAUSTED: return "arena exhausted";
        case G3D_ERR_EOF: return "unexpected end of buffer";
        case G3D_ERR_BAD_MAGIC: return "bad magic";
        case G3D_ERR_BAD_VERSION: return "bad version";
        case G3D_ERR_CORRUPT: return "corrupt data";
        case G3D_ERR_UNSUPPORTED: return "unsupported feature";
        case G3D_ERR_NOT_FOUND: return "not found";
        case G3D_ERR_OVERFLOW: return "integer overflow";
        default: return "unknown error";
    }
}

static g3d_u32 g3d_highest_bit(g3d_u32 v)
{
    g3d_u32 i;
    i = 31u;
    while (i > 0u)
    {
        if ((v & (1u << i)) != 0u)
        {
            return i;
        }
        --i;
    }
    return 0u;
}

g3d_fix16 g3d_f32bits_to_fix16(g3d_u32 bits)
{
    g3d_u32 sign;
    g3d_u32 exp;
    g3d_u32 mant;
    g3d_s32 shift;
    g3d_u32 mag;
    g3d_s32 out;
    sign = bits >> 31;
    exp = (bits >> 23) & 0xffu;
    mant = bits & 0x7fffffu;

    if (exp == 0xffu)
    {
        return sign ? (g3d_fix16)INT_MIN : (g3d_fix16)INT_MAX;
    }

    if (exp == 0u)
    {
        if (mant == 0u)
        {
            return 0;
        }
        shift = -126 - 23 + 16;
    }
    else
    {
        mant |= 0x800000u;
        shift = (g3d_s32)exp - 127 - 23 + 16;
    }

    if (shift >= 0)
    {
        if (shift > 7)
        {
            return sign ? (g3d_fix16)INT_MIN : (g3d_fix16)INT_MAX;
        }
        mag = mant << (g3d_u32)shift;
    }
    else
    {
        if (shift < -31)
        {
            mag = 0u;
        }
        else
        {
            mag = mant >> (g3d_u32)(-shift);
        }
    }

    if (sign)
    {
        if (mag > 0x80000000u)
        {
            return (g3d_fix16)INT_MIN;
        }
        if (mag == 0x80000000u)
        {
            return (g3d_fix16)INT_MIN;
        }
        out = -(g3d_s32)mag;
    }
    else
    {
        if (mag > 0x7fffffffu)
        {
            return (g3d_fix16)INT_MAX;
        }
        out = (g3d_s32)mag;
    }
    return (g3d_fix16)out;
}

g3d_u32 g3d_fix16_to_f32bits(g3d_fix16 value)
{
    g3d_u32 sign;
    g3d_u32 mag;
    g3d_u32 hb;
    g3d_s32 exp;
    g3d_u32 mant24;

    if (value == 0)
    {
        return 0u;
    }

    sign = (value < 0) ? 1u : 0u;
    if (value < 0)
    {
        if (value == (g3d_fix16)INT_MIN)
        {
            mag = 0x80000000u;
        }
        else
        {
            mag = (g3d_u32)(-value);
        }
    }
    else
    {
        mag = (g3d_u32)value;
    }

    hb = g3d_highest_bit(mag);
    exp = (g3d_s32)hb - 16 + 127;

    if (exp <= 0)
    {
        return sign << 31;
    }
    if (exp >= 255)
    {
        return (sign << 31) | 0x7f7fffffu;
    }

    if (hb <= 23u)
    {
        mant24 = mag << (23u - hb);
    }
    else
    {
        mant24 = mag >> (hb - 23u);
    }

    return (sign << 31) | (((g3d_u32)exp & 0xffu) << 23) | (mant24 & 0x7fffffu);
}

const char *g3d_strblock_get(const g3d_strblock *sb, g3d_s32 index)
{
    g3d_s32 off;
    if (sb == NULL || sb->data == NULL)
    {
        return NULL;
    }
    if (index < 0 || index >= sb->count)
    {
        return NULL;
    }
    off = g3d_rd_s32ptr(sb->data + ((g3d_u32)index * 4u));
    if (off < 0 || off >= sb->size)
    {
        return NULL;
    }
    return (const char *)(sb->data + (g3d_u32)off);
}

g3d_s32 g3d_strblock_checksum(const g3d_strblock *sb)
{
    g3d_u32 checksum;
    g3d_s32 i;
    checksum = 0u;
    if (sb == NULL)
    {
        return 0;
    }
    for (i = 0; i < sb->count; ++i)
    {
        const char *str;
        str = g3d_strblock_get(sb, i);
        if (str == NULL)
        {
            return 0;
        }
        while (*str != '\0')
        {
            checksum += (g3d_u32)(unsigned char)*str;
            ++str;
        }
        checksum *= 3u;
    }
    return (g3d_s32)checksum;
}

int g3d_tkevents_get(const g3d_tkevents *ev, g3d_s32 index, g3d_u32 *time_bits, g3d_u32 *offset)
{
    const g3d_u8 *p;
    if (ev == NULL || time_bits == NULL || offset == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (index < 0 || index >= ev->time_keys.num_elements)
    {
        return G3D_ERR_NOT_FOUND;
    }
    if (ev->time_keys.element_size < 8)
    {
        return G3D_ERR_CORRUPT;
    }
    p = ev->time_keys.elements + ((g3d_u32)index * (g3d_u32)ev->time_keys.element_size);
    *time_bits = g3d_rd_u32ptr(p);
    *offset = g3d_rd_u32ptr(p + 4u);
    return G3D_OK;
}

int g3d_tkevents_get_text(const g3d_tkevents *ev, g3d_s32 index, g3d_u32 *time_bits, const char **text)
{
    g3d_u32 offset;
    int rc;
    if (ev == NULL || time_bits == NULL || text == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_tkevents_get(ev, index, time_bits, &offset);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (offset >= ev->data_size || ev->event_data == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    if (g3d_strn_has_zero(ev->event_data + offset, ev->data_size - offset) == G3D_FALSE)
    {
        return G3D_ERR_CORRUPT;
    }
    *text = (const char *)(ev->event_data + offset);
    return G3D_OK;
}

g3d_s32 g3d_tkevents_count_between_fix16(const g3d_tkevents *ev, g3d_fix16 start_time, g3d_fix16 end_time)
{
    g3d_s32 count;
    g3d_s32 i;
    if (ev == NULL || start_time >= end_time)
    {
        return 0;
    }
    count = 0;
    for (i = 0; i < ev->time_keys.num_elements; ++i)
    {
        g3d_u32 bits;
        g3d_u32 offset;
        g3d_fix16 t;
        if (g3d_tkevents_get(ev, i, &bits, &offset) != G3D_OK)
        {
            return count;
        }
        (void)offset;
        t = g3d_f32bits_to_fix16(bits);
        if (t >= start_time && t < end_time)
        {
            ++count;
        }
    }
    return count;
}

int g3d_tkevents_find_next_between_fix16(const g3d_tkevents *ev, g3d_fix16 start_time, g3d_fix16 end_time,
                                         g3d_s32 after_index, g3d_s32 *out_index,
                                         g3d_u32 *time_bits, const char **text)
{
    g3d_s32 i;
    if (ev == NULL || out_index == NULL || time_bits == NULL || text == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (start_time >= end_time)
    {
        return G3D_ERR_NOT_FOUND;
    }
    i = after_index + 1;
    if (i < 0)
    {
        i = 0;
    }
    for (; i < ev->time_keys.num_elements; ++i)
    {
        g3d_u32 bits;
        g3d_u32 offset;
        g3d_fix16 t;
        int rc;
        rc = g3d_tkevents_get(ev, i, &bits, &offset);
        if (rc != G3D_OK)
        {
            return rc;
        }
        t = g3d_f32bits_to_fix16(bits);
        if (t >= start_time && t < end_time)
        {
            if (offset >= ev->data_size || ev->event_data == NULL)
            {
                return G3D_ERR_CORRUPT;
            }
            if (g3d_strn_has_zero(ev->event_data + offset, ev->data_size - offset) == G3D_FALSE)
            {
                return G3D_ERR_CORRUPT;
            }
            *out_index = i;
            *time_bits = bits;
            *text = (const char *)(ev->event_data + offset);
            return G3D_OK;
        }
    }
    return G3D_ERR_NOT_FOUND;
}

static int g3d_strn_has_zero(const g3d_u8 *p, g3d_u32 n)
{
    g3d_u32 i;
    if (p == NULL)
    {
        return G3D_FALSE;
    }
    for (i = 0u; i < n; ++i)
    {
        if (p[i] == 0u)
        {
            return G3D_TRUE;
        }
    }
    return G3D_FALSE;
}

static int g3d_strblock_layout_valid(const g3d_u8 *data, g3d_u32 size, g3d_s32 count)
{
    g3d_s32 i;
    g3d_s32 prev;
    if (count < 0)
    {
        return G3D_FALSE;
    }
    if ((g3d_u32)count > (size / 4u))
    {
        return G3D_FALSE;
    }
    if (count == 0)
    {
        return (size == 0u) ? G3D_TRUE : G3D_FALSE;
    }
    if (data == NULL || size < ((g3d_u32)count * 4u))
    {
        return G3D_FALSE;
    }
    prev = -1;
    for (i = 0; i < count; ++i)
    {
        g3d_s32 off;
        off = g3d_rd_s32ptr(data + ((g3d_u32)i * 4u));
        if (off < (g3d_s32)((g3d_u32)count * 4u) || (g3d_u32)off >= size)
        {
            return G3D_FALSE;
        }
        if (off <= prev)
        {
            return G3D_FALSE;
        }
        if (g3d_strn_has_zero(data + (g3d_u32)off, size - (g3d_u32)off) == G3D_FALSE)
        {
            return G3D_FALSE;
        }
        prev = off;
    }
    return G3D_TRUE;
}


static int g3d_parse_strblock_ascii_body(g3d_strblock *sb, g3d_reader *r, g3d_arena *arena)
{
    char line[2048];
    const char *args;
    g3d_u32 version;
    g3d_s32 count;
    g3d_u32 strings_start;
    g3d_u32 total_size;
    g3d_u32 string_bytes;
    g3d_s32 i;
    int rc;

    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_parse_version_text(line, &version);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (version < G3D_SB_VER)
    {
        return G3D_ERR_BAD_VERSION;
    }

    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    args = g3d_line_after_tag(line, "Strings");
    if (args == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    if (g3d_parse_s32_1(args, &count) != G3D_OK)
    {
        return G3D_ERR_CORRUPT;
    }
    if (count < 0)
    {
        return G3D_ERR_CORRUPT;
    }

    strings_start = r->pos;
    rc = g3d_mul_u32_checked((g3d_u32)count, 4u, &total_size);
    if (rc != G3D_OK)
    {
        return rc;
    }
    for (i = 0; i < count; ++i)
    {
        rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
        if (rc != G3D_OK)
        {
            return rc;
        }
        string_bytes = g3d_strlen32(line) + 1u;
        rc = g3d_add_u32_checked(total_size, string_bytes, &total_size);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }

    if (total_size > 0x7fffffffu)
    {
        return G3D_ERR_OVERFLOW;
    }
    sb->count = count;
    sb->size = (g3d_s32)total_size;
    sb->data = (g3d_u8 *)g3d_arena_alloc(arena, total_size, 4u);
    if (sb->data == NULL)
    {
        return G3D_ERR_ARENA_EXHAUSTED;
    }
    g3d_zero(sb->data, total_size);

    r->pos = strings_start;
    rc = g3d_mul_u32_checked((g3d_u32)count, 4u, &string_bytes);
    if (rc != G3D_OK)
    {
        return rc;
    }
    for (i = 0; i < count; ++i)
    {
        rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
        if (rc != G3D_OK)
        {
            return rc;
        }
        g3d_store_s32ptr(sb->data + ((g3d_u32)i * 4u), (g3d_s32)string_bytes);
        memcpy(sb->data + string_bytes, line, (size_t)(g3d_strlen32(line) + 1u));
        string_bytes += g3d_strlen32(line) + 1u;
    }
    return G3D_OK;
}

static int g3d_parse_strblock_inner(g3d_strblock *sb, g3d_reader *r, g3d_arena *arena)
{
    g3d_u32 sig;
    g3d_s32 size;
    g3d_s32 count;
    g3d_s32 first_word;
    g3d_s32 second_word;
    g3d_u32 mark;
    int rc;

    rc = g3d_rd_u32le(r, &sig);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (sig == G3D_SB_ASCII)
    {
        return g3d_parse_strblock_ascii_body(sb, r, arena);
    }
    if (sig != G3D_SB_BIN)
    {
        return G3D_ERR_BAD_MAGIC;
    }

    rc = g3d_rd_s32le(r, &first_word);
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_rd_s32le(r, &second_word);
    if (rc != G3D_OK)
    {
        return rc;
    }

    /* Genesis3D original writes geStrBlock_BinaryFileHeader as:
       int Count; uint32 Size.  Older builds of this standalone codec wrote
       Size/Count by mistake, so accept that legacy order only as a fallback. */
    count = first_word;
    size = second_word;
    if (count < 0 || size < 0 || (g3d_u32)size > (r->size - r->pos) ||
        g3d_strblock_layout_valid(r->buf + r->pos, (g3d_u32)size, count) == G3D_FALSE)
    {
        g3d_s32 legacy_size;
        g3d_s32 legacy_count;
        legacy_size = first_word;
        legacy_count = second_word;
        if (legacy_count < 0 || legacy_size < 0 ||
            (g3d_u32)legacy_size > (r->size - r->pos) ||
            g3d_strblock_layout_valid(r->buf + r->pos, (g3d_u32)legacy_size, legacy_count) == G3D_FALSE)
        {
            if (legacy_size >= 0 && (g3d_u32)legacy_size > (r->size - r->pos))
            {
                return G3D_ERR_EOF;
            }
            return G3D_ERR_CORRUPT;
        }
        size = legacy_size;
        count = legacy_count;
    }

    mark = g3d_arena_mark(arena);
    sb->count = count;
    sb->size = size;
    sb->data = (g3d_u8 *)g3d_arena_alloc(arena, (g3d_u32)size, 4u);
    if (sb->data == NULL)
    {
        return G3D_ERR_ARENA_EXHAUSTED;
    }
    rc = g3d_rd_bytes(r, sb->data, (g3d_u32)size);
    if (rc != G3D_OK)
    {
        g3d_arena_rewind(arena, mark);
        return rc;
    }
    return G3D_OK;
}

int g3d_strblock_parse(g3d_strblock *sb, const void *data, g3d_u32 size, g3d_arena *arena, g3d_u32 *used_bytes)
{
    g3d_reader r;
    int rc;
    if (sb == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    g3d_zero(sb, (g3d_u32)sizeof(*sb));
    g3d_reader_init(&r, data, size);
    rc = g3d_parse_strblock_inner(sb, &r, arena);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = r.pos;
    }
    return G3D_OK;
}

static int g3d_write_strblock_inner(const g3d_strblock *sb, g3d_writer *w)
{
    if (sb == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if ((sb->count < 0) || (sb->size < 0))
    {
        return G3D_ERR_BAD_ARG;
    }
    if (g3d_wr_u32le(w, G3D_SB_BIN) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_s32le(w, sb->count) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_s32le(w, sb->size) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_bytes(w, sb->data, (g3d_u32)sb->size) != G3D_OK) return G3D_ERR_NO_SPACE;
    return G3D_OK;
}

int g3d_strblock_write_binary(const g3d_strblock *sb, void *dst, g3d_u32 cap, g3d_u32 *used_bytes)
{
    g3d_writer w;
    int rc;
    g3d_writer_init(&w, dst, cap);
    rc = g3d_write_strblock_inner(sb, &w);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = w.pos;
    }
    return G3D_OK;
}

int g3d_strblock_measure_binary(const g3d_strblock *sb, g3d_u32 *size_out)
{
    g3d_u32 used;
    int rc;
    rc = g3d_strblock_write_binary(sb, NULL, 0u, &used);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *size_out = used;
    return G3D_OK;
}

static int g3d_parse_tkarray_inner(g3d_tkarray *arr, g3d_reader *r, g3d_arena *arena)
{
    g3d_s32 num_elements;
    g3d_s32 element_size;
    g3d_u32 bytes;
    int rc;
    rc = g3d_rd_s32le(r, &num_elements);
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_rd_s32le(r, &element_size);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (num_elements < 0 || element_size < 0)
    {
        return G3D_ERR_CORRUPT;
    }
    if (num_elements == 0 || element_size == 0)
    {
        arr->num_elements = num_elements;
        arr->element_size = element_size;
        arr->elements = NULL;
        return G3D_OK;
    }
    if ((g3d_u32)num_elements > 0xffffffffu / (g3d_u32)element_size)
    {
        return G3D_ERR_OVERFLOW;
    }
    bytes = (g3d_u32)num_elements * (g3d_u32)element_size;
    if (bytes > (r->size - r->pos))
    {
        return G3D_ERR_EOF;
    }
    arr->num_elements = num_elements;
    arr->element_size = element_size;
    arr->elements = (g3d_u8 *)g3d_arena_alloc(arena, bytes, 4u);
    if (arr->elements == NULL)
    {
        return G3D_ERR_ARENA_EXHAUSTED;
    }
    rc = g3d_rd_bytes(r, arr->elements, bytes);
    if (rc != G3D_OK)
    {
        return rc;
    }
    return G3D_OK;
}

int g3d_tkarray_parse_binary(g3d_tkarray *arr, const void *data, g3d_u32 size, g3d_arena *arena, g3d_u32 *used_bytes)
{
    g3d_reader r;
    int rc;
    if (arr == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    g3d_zero(arr, (g3d_u32)sizeof(*arr));
    g3d_reader_init(&r, data, size);
    rc = g3d_parse_tkarray_inner(arr, &r, arena);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = r.pos;
    }
    return G3D_OK;
}

static int g3d_write_tkarray_inner(const g3d_tkarray *arr, g3d_writer *w)
{
    g3d_u32 bytes;
    if (arr == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (arr->num_elements < 0 || arr->element_size < 0)
    {
        return G3D_ERR_BAD_ARG;
    }
    if ((g3d_u32)arr->num_elements > 0xffffffffu / (g3d_u32)(arr->element_size == 0 ? 1 : arr->element_size))
    {
        return G3D_ERR_OVERFLOW;
    }
    bytes = (g3d_u32)arr->num_elements * (g3d_u32)arr->element_size;
    if (g3d_wr_s32le(w, arr->num_elements) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_s32le(w, arr->element_size) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_bytes(w, arr->elements, bytes) != G3D_OK) return G3D_ERR_NO_SPACE;
    return G3D_OK;
}

int g3d_tkarray_write_binary(const g3d_tkarray *arr, void *dst, g3d_u32 cap, g3d_u32 *used_bytes)
{
    g3d_writer w;
    int rc;
    g3d_writer_init(&w, dst, cap);
    rc = g3d_write_tkarray_inner(arr, &w);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = w.pos;
    }
    return G3D_OK;
}

int g3d_tkarray_measure_binary(const g3d_tkarray *arr, g3d_u32 *size_out)
{
    g3d_u32 used;
    int rc;
    rc = g3d_tkarray_write_binary(arr, NULL, 0u, &used);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *size_out = used;
    return G3D_OK;
}


static int g3d_parse_tkevents_ascii_body(g3d_tkevents *ev, g3d_reader *r, g3d_arena *arena)
{
    char line[512];
    char *event_line;
    const char *args;
    g3d_u32 version;
    g3d_u32 data_size;
    g3d_u32 line_cap;
    g3d_u32 mark;
    g3d_s32 count;
    g3d_s32 i;
    g3d_u32 element_bytes;
    int rc;

    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_parse_version_text(line, &version);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (version != G3D_TKEV_VER)
    {
        return G3D_ERR_BAD_VERSION;
    }

    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    args = g3d_line_after_tag(line, "DataSize");
    if (args == NULL || g3d_parse_u32_1(args, &data_size) != G3D_OK)
    {
        return G3D_ERR_CORRUPT;
    }

    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    args = g3d_line_after_tag(line, "TimeKeys");
    if (args == NULL || g3d_parse_s32_token(&args, &count) != G3D_OK)
    {
        return G3D_ERR_CORRUPT;
    }
    if (count < 0)
    {
        return G3D_ERR_CORRUPT;
    }

    ev->data_size = data_size;
    ev->event_data = NULL;
    if (data_size != 0u)
    {
        ev->event_data = (g3d_u8 *)g3d_arena_alloc(arena, data_size, 1u);
        if (ev->event_data == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        g3d_zero(ev->event_data, data_size);
    }

    ev->time_keys.num_elements = count;
    ev->time_keys.element_size = 8;
    ev->time_keys.elements = NULL;
    rc = g3d_mul_u32_checked((g3d_u32)count, 8u, &element_bytes);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (element_bytes != 0u)
    {
        ev->time_keys.elements = (g3d_u8 *)g3d_arena_alloc(arena, element_bytes, 4u);
        if (ev->time_keys.elements == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        g3d_zero(ev->time_keys.elements, element_bytes);
    }

    rc = g3d_add_u32_checked(data_size, 2u, &line_cap);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (line_cap < 64u)
    {
        line_cap = 64u;
    }
    mark = g3d_arena_mark(arena);
    event_line = (char *)g3d_arena_alloc(arena, line_cap, 1u);
    if (event_line == NULL)
    {
        return G3D_ERR_ARENA_EXHAUSTED;
    }

    for (i = 0; i < count; ++i)
    {
        const char *p;
        g3d_u32 time_bits;
        g3d_s32 offset_i;
        g3d_u32 len;
        rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
        if (rc != G3D_OK)
        {
            g3d_arena_rewind(arena, mark);
            return rc;
        }
        p = line;
        rc = g3d_parse_fbits_token(&p, &time_bits);
        if (rc != G3D_OK)
        {
            g3d_arena_rewind(arena, mark);
            return G3D_ERR_CORRUPT;
        }
        rc = g3d_parse_s32_token(&p, &offset_i);
        if (rc != G3D_OK)
        {
            g3d_arena_rewind(arena, mark);
            return G3D_ERR_CORRUPT;
        }
        if (offset_i < 0 || (g3d_u32)offset_i >= data_size)
        {
            g3d_arena_rewind(arena, mark);
            return G3D_ERR_CORRUPT;
        }
        rc = g3d_rd_line(r, event_line, line_cap);
        if (rc != G3D_OK)
        {
            g3d_arena_rewind(arena, mark);
            return rc;
        }
        len = g3d_strlen32(event_line) + 1u;
        if (len > (data_size - (g3d_u32)offset_i))
        {
            g3d_arena_rewind(arena, mark);
            return G3D_ERR_CORRUPT;
        }
        memcpy(ev->event_data + (g3d_u32)offset_i, event_line, (size_t)len);
        g3d_store_u32ptr(ev->time_keys.elements + ((g3d_u32)i * 8u), time_bits);
        g3d_store_u32ptr(ev->time_keys.elements + ((g3d_u32)i * 8u) + 4u, (g3d_u32)offset_i);
    }

    g3d_arena_rewind(arena, mark);
    return G3D_OK;
}


static int g3d_parse_tkevents_inner(g3d_tkevents *ev, g3d_reader *r, g3d_arena *arena)
{
    g3d_u32 sig;
    g3d_u32 ver;
    g3d_u32 data_size;
    int rc;

    rc = g3d_rd_u32le(r, &sig);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (sig == G3D_TKEV_ASCII)
    {
        return g3d_parse_tkevents_ascii_body(ev, r, arena);
    }
    if (sig != G3D_TKEV_BIN)
    {
        return G3D_ERR_BAD_MAGIC;
    }
    rc = g3d_rd_u32le(r, &ver);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (ver != G3D_TKEV_VER)
    {
        return G3D_ERR_BAD_VERSION;
    }
    rc = g3d_rd_u32le(r, &data_size);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (data_size > (r->size - r->pos))
    {
        return G3D_ERR_EOF;
    }
    ev->data_size = data_size;
    ev->event_data = NULL;
    if (data_size != 0u)
    {
        ev->event_data = (g3d_u8 *)g3d_arena_alloc(arena, data_size, 1u);
        if (ev->event_data == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        rc = g3d_rd_bytes(r, ev->event_data, data_size);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    rc = g3d_parse_tkarray_inner(&ev->time_keys, r, arena);
    if (rc != G3D_OK)
    {
        return rc;
    }
    return G3D_OK;
}

int g3d_tkevents_parse(g3d_tkevents *ev, const void *data, g3d_u32 size, g3d_arena *arena, g3d_u32 *used_bytes)
{
    g3d_reader r;
    int rc;
    if (ev == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    g3d_zero(ev, (g3d_u32)sizeof(*ev));
    g3d_reader_init(&r, data, size);
    rc = g3d_parse_tkevents_inner(ev, &r, arena);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = r.pos;
    }
    return G3D_OK;
}

static int g3d_write_tkevents_inner(const g3d_tkevents *ev, g3d_writer *w)
{
    int rc;
    if (ev == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (g3d_wr_u32le(w, G3D_TKEV_BIN) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, G3D_TKEV_VER) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, ev->data_size) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_bytes(w, ev->event_data, ev->data_size) != G3D_OK) return G3D_ERR_NO_SPACE;
    rc = g3d_write_tkarray_inner(&ev->time_keys, w);
    if (rc != G3D_OK)
    {
        return rc;
    }
    return G3D_OK;
}

int g3d_tkevents_write_binary(const g3d_tkevents *ev, void *dst, g3d_u32 cap, g3d_u32 *used_bytes)
{
    g3d_writer w;
    int rc;
    g3d_writer_init(&w, dst, cap);
    rc = g3d_write_tkevents_inner(ev, &w);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = w.pos;
    }
    return G3D_OK;
}

int g3d_tkevents_measure_binary(const g3d_tkevents *ev, g3d_u32 *size_out)
{
    g3d_u32 used;
    int rc;
    rc = g3d_tkevents_write_binary(ev, NULL, 0u, &used);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *size_out = used;
    return G3D_OK;
}

static int g3d_parse_qkframe_inner(g3d_qkframe *qk, g3d_reader *r, g3d_arena *arena)
{
    g3d_u32 block_size;
    g3d_u32 flags;
    g3d_reader sub;
    g3d_u32 i;
    int rc;
    rc = g3d_rd_u32le(r, &block_size);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (block_size > (r->size - r->pos))
    {
        return G3D_ERR_EOF;
    }
    g3d_reader_init(&sub, r->buf + r->pos, block_size);
    rc = g3d_rd_u32le(&sub, &flags);
    if (rc != G3D_OK)
    {
        return rc;
    }
    qk->interpolation = (g3d_u8)((flags >> 16) & 0xffu);
    qk->compression = (g3d_u8)((flags >> 8) & 0xffu);
    qk->looping = (g3d_u8)(flags & 0x1u);
    rc = g3d_rd_u32le(&sub, &qk->count);
    if (rc != G3D_OK)
    {
        return rc;
    }
    qk->time_value_count = (qk->compression & G3D_QK_LINEARTIME_COMPRESSION) ? 2u : qk->count;
    qk->time_bits = NULL;
    qk->angle_bits = NULL;
    qk->quats = NULL;

    if (qk->time_value_count != 0u)
    {
        qk->time_bits = (g3d_u32 *)g3d_arena_alloc(arena, qk->time_value_count * 4u, 4u);
        if (qk->time_bits == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        for (i = 0u; i < qk->time_value_count; ++i)
        {
            rc = g3d_rd_u32le(&sub, &qk->time_bits[i]);
            if (rc != G3D_OK)
            {
                return rc;
            }
        }
    }

    if ((qk->compression & G3D_QK_HINGE_COMPRESSION) != 0u)
    {
        rc = g3d_rd_u32le(&sub, &qk->hinge_axis.x_bits);
        if (rc != G3D_OK) return rc;
        rc = g3d_rd_u32le(&sub, &qk->hinge_axis.y_bits);
        if (rc != G3D_OK) return rc;
        rc = g3d_rd_u32le(&sub, &qk->hinge_axis.z_bits);
        if (rc != G3D_OK) return rc;

        if (qk->count != 0u)
        {
            qk->angle_bits = (g3d_u32 *)g3d_arena_alloc(arena, qk->count * 4u, 4u);
            if (qk->angle_bits == NULL)
            {
                return G3D_ERR_ARENA_EXHAUSTED;
            }
            for (i = 0u; i < qk->count; ++i)
            {
                rc = g3d_rd_u32le(&sub, &qk->angle_bits[i]);
                if (rc != G3D_OK)
                {
                    return rc;
                }
            }
        }
    }
    else
    {
        if (qk->count != 0u)
        {
            qk->quats = (g3d_fquat *)g3d_arena_alloc(arena, qk->count * (g3d_u32)sizeof(g3d_fquat), 4u);
            if (qk->quats == NULL)
            {
                return G3D_ERR_ARENA_EXHAUSTED;
            }
            for (i = 0u; i < qk->count; ++i)
            {
                /* Genesis3D geQuaternion is serialized as W, X, Y, Z.
                   Keep public g3d_fquat as X, Y, Z, W. */
                rc = g3d_rd_u32le(&sub, &qk->quats[i].w_bits);
                if (rc != G3D_OK) return rc;
                rc = g3d_rd_u32le(&sub, &qk->quats[i].x_bits);
                if (rc != G3D_OK) return rc;
                rc = g3d_rd_u32le(&sub, &qk->quats[i].y_bits);
                if (rc != G3D_OK) return rc;
                rc = g3d_rd_u32le(&sub, &qk->quats[i].z_bits);
                if (rc != G3D_OK) return rc;
            }
        }
    }

    if (sub.pos != block_size)
    {
        return G3D_ERR_CORRUPT;
    }
    r->pos += block_size;
    return G3D_OK;
}

int g3d_qkframe_parse_binary(g3d_qkframe *qk, const void *data, g3d_u32 size, g3d_arena *arena, g3d_u32 *used_bytes)
{
    g3d_reader r;
    int rc;
    if (qk == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    g3d_zero(qk, (g3d_u32)sizeof(*qk));
    g3d_reader_init(&r, data, size);
    rc = g3d_parse_qkframe_inner(qk, &r, arena);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = r.pos;
    }
    return G3D_OK;
}

static int g3d_write_qkframe_inner(const g3d_qkframe *qk, g3d_writer *w)
{
    g3d_u32 block_size;
    g3d_u32 flags;
    g3d_u32 i;
    if (qk == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    block_size = 8u + (qk->time_value_count * 4u);
    if ((qk->compression & G3D_QK_HINGE_COMPRESSION) != 0u)
    {
        block_size += 12u + (qk->count * 4u);
    }
    else
    {
        block_size += qk->count * 16u;
    }
    flags = ((g3d_u32)qk->interpolation << 16) |
            ((g3d_u32)qk->compression << 8) |
            ((g3d_u32)qk->looping & 0x1u);

    if (g3d_wr_u32le(w, block_size) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, flags) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, qk->count) != G3D_OK) return G3D_ERR_NO_SPACE;

    for (i = 0u; i < qk->time_value_count; ++i)
    {
        if (g3d_wr_u32le(w, qk->time_bits[i]) != G3D_OK) return G3D_ERR_NO_SPACE;
    }

    if ((qk->compression & G3D_QK_HINGE_COMPRESSION) != 0u)
    {
        if (g3d_wr_u32le(w, qk->hinge_axis.x_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
        if (g3d_wr_u32le(w, qk->hinge_axis.y_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
        if (g3d_wr_u32le(w, qk->hinge_axis.z_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
        for (i = 0u; i < qk->count; ++i)
        {
            if (g3d_wr_u32le(w, qk->angle_bits[i]) != G3D_OK) return G3D_ERR_NO_SPACE;
        }
    }
    else
    {
        for (i = 0u; i < qk->count; ++i)
        {
            if (g3d_wr_u32le(w, qk->quats[i].w_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
            if (g3d_wr_u32le(w, qk->quats[i].x_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
            if (g3d_wr_u32le(w, qk->quats[i].y_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
            if (g3d_wr_u32le(w, qk->quats[i].z_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
        }
    }
    return G3D_OK;
}

int g3d_qkframe_write_binary(const g3d_qkframe *qk, void *dst, g3d_u32 cap, g3d_u32 *used_bytes)
{
    g3d_writer w;
    int rc;
    g3d_writer_init(&w, dst, cap);
    rc = g3d_write_qkframe_inner(qk, &w);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = w.pos;
    }
    return G3D_OK;
}

int g3d_qkframe_measure_binary(const g3d_qkframe *qk, g3d_u32 *size_out)
{
    g3d_u32 used;
    int rc;
    rc = g3d_qkframe_write_binary(qk, NULL, 0u, &used);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *size_out = used;
    return G3D_OK;
}

static int g3d_parse_vkframe_inner(g3d_vkframe *vk, g3d_reader *r, g3d_arena *arena)
{
    g3d_u32 block_size;
    g3d_u32 flags;
    g3d_reader sub;
    g3d_u32 i;
    int rc;
    rc = g3d_rd_u32le(r, &block_size);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (block_size > (r->size - r->pos))
    {
        return G3D_ERR_EOF;
    }
    g3d_reader_init(&sub, r->buf + r->pos, block_size);
    rc = g3d_rd_u32le(&sub, &flags);
    if (rc != G3D_OK)
    {
        return rc;
    }
    vk->interpolation = (g3d_u8)((flags >> 16) & 0xffu);
    vk->compression = (g3d_u8)((flags >> 8) & 0xffu);
    vk->looping = (g3d_u8)(flags & 0x1u);
    rc = g3d_rd_u32le(&sub, &vk->count);
    if (rc != G3D_OK)
    {
        return rc;
    }
    vk->time_value_count = (vk->compression & G3D_VK_LINEARTIME_COMPRESSION) ? 2u : vk->count;
    vk->time_bits = NULL;
    vk->vectors = NULL;

    if (vk->time_value_count != 0u)
    {
        vk->time_bits = (g3d_u32 *)g3d_arena_alloc(arena, vk->time_value_count * 4u, 4u);
        if (vk->time_bits == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        for (i = 0u; i < vk->time_value_count; ++i)
        {
            rc = g3d_rd_u32le(&sub, &vk->time_bits[i]);
            if (rc != G3D_OK)
            {
                return rc;
            }
        }
    }

    if (vk->count != 0u)
    {
        vk->vectors = (g3d_fvec3 *)g3d_arena_alloc(arena, vk->count * (g3d_u32)sizeof(g3d_fvec3), 4u);
        if (vk->vectors == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        for (i = 0u; i < vk->count; ++i)
        {
            rc = g3d_rd_u32le(&sub, &vk->vectors[i].x_bits);
            if (rc != G3D_OK) return rc;
            rc = g3d_rd_u32le(&sub, &vk->vectors[i].y_bits);
            if (rc != G3D_OK) return rc;
            rc = g3d_rd_u32le(&sub, &vk->vectors[i].z_bits);
            if (rc != G3D_OK) return rc;
        }
    }

    if (sub.pos != block_size)
    {
        return G3D_ERR_CORRUPT;
    }
    r->pos += block_size;
    return G3D_OK;
}

int g3d_vkframe_parse_binary(g3d_vkframe *vk, const void *data, g3d_u32 size, g3d_arena *arena, g3d_u32 *used_bytes)
{
    g3d_reader r;
    int rc;
    if (vk == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    g3d_zero(vk, (g3d_u32)sizeof(*vk));
    g3d_reader_init(&r, data, size);
    rc = g3d_parse_vkframe_inner(vk, &r, arena);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = r.pos;
    }
    return G3D_OK;
}

static int g3d_write_vkframe_inner(const g3d_vkframe *vk, g3d_writer *w)
{
    g3d_u32 block_size;
    g3d_u32 flags;
    g3d_u32 i;
    if (vk == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    block_size = 8u + (vk->time_value_count * 4u) + (vk->count * 12u);
    flags = ((g3d_u32)vk->interpolation << 16) |
            ((g3d_u32)vk->compression << 8) |
            ((g3d_u32)vk->looping & 0x1u);
    if (g3d_wr_u32le(w, block_size) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, flags) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, vk->count) != G3D_OK) return G3D_ERR_NO_SPACE;
    for (i = 0u; i < vk->time_value_count; ++i)
    {
        if (g3d_wr_u32le(w, vk->time_bits[i]) != G3D_OK) return G3D_ERR_NO_SPACE;
    }
    for (i = 0u; i < vk->count; ++i)
    {
        if (g3d_wr_u32le(w, vk->vectors[i].x_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
        if (g3d_wr_u32le(w, vk->vectors[i].y_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
        if (g3d_wr_u32le(w, vk->vectors[i].z_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
    }
    return G3D_OK;
}

int g3d_vkframe_write_binary(const g3d_vkframe *vk, void *dst, g3d_u32 cap, g3d_u32 *used_bytes)
{
    g3d_writer w;
    int rc;
    g3d_writer_init(&w, dst, cap);
    rc = g3d_write_vkframe_inner(vk, &w);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = w.pos;
    }
    return G3D_OK;
}

int g3d_vkframe_measure_binary(const g3d_vkframe *vk, g3d_u32 *size_out)
{
    g3d_u32 used;
    int rc;
    rc = g3d_vkframe_write_binary(vk, NULL, 0u, &used);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *size_out = used;
    return G3D_OK;
}


static g3d_u8 g3d_qk_interp_to_path(int interp)
{
    if (interp == G3D_QK_SLERP)
    {
        return (g3d_u8)G3D_PATH_QK_SLERP;
    }
    if (interp == G3D_QK_SQUAD)
    {
        return (g3d_u8)G3D_PATH_QK_SQUAD;
    }
    return (g3d_u8)G3D_PATH_QK_LINEAR;
}

static g3d_u8 g3d_vk_interp_to_path(int interp)
{
    if (interp == G3D_VK_HERMITE)
    {
        return (g3d_u8)G3D_PATH_VK_HERMITE;
    }
    if (interp == G3D_VK_HERMITE_ZERO_DERIV)
    {
        return (g3d_u8)G3D_PATH_VK_HERMITE_ZERO_DERIV;
    }
    return (g3d_u8)G3D_PATH_VK_LINEAR;
}

static int g3d_parse_qkframe_ascii(g3d_qkframe *qk, g3d_reader *r, g3d_arena *arena)
{
    char line[512];
    const char *args;
    int count_i;
    int interp_i;
    int comp_i;
    int loop_i;
    g3d_u32 bytes;
    g3d_u32 i;
    int rc;

    if (qk == NULL || r == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    g3d_zero(qk, (g3d_u32)sizeof(*qk));

    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    args = g3d_line_after_tag(line, "Keys");
    if (args == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    if (g3d_parse_s32_4(args, &count_i, &interp_i, &comp_i, &loop_i) != G3D_OK)
    {
        return G3D_ERR_CORRUPT;
    }
    if (count_i < 0 || interp_i < G3D_QK_LINEAR || interp_i > G3D_QK_SQUAD || comp_i < 0)
    {
        return G3D_ERR_CORRUPT;
    }
    if (((g3d_u32)comp_i & ~(G3D_QK_HINGE_COMPRESSION | G3D_QK_LINEARTIME_COMPRESSION)) != 0u)
    {
        return G3D_ERR_UNSUPPORTED;
    }

    qk->interpolation = (g3d_u8)interp_i;
    qk->compression = (g3d_u8)comp_i;
    qk->looping = (g3d_u8)((loop_i != 0) ? 1u : 0u);
    qk->count = (g3d_u32)count_i;
    qk->time_value_count = ((qk->compression & G3D_QK_LINEARTIME_COMPRESSION) != 0u) ? 2u : qk->count;

    if (qk->time_value_count != 0u)
    {
        rc = g3d_mul_u32_checked(qk->time_value_count, 4u, &bytes);
        if (rc != G3D_OK)
        {
            return rc;
        }
        qk->time_bits = (g3d_u32 *)g3d_arena_alloc(arena, bytes, 4u);
        if (qk->time_bits == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        g3d_zero(qk->time_bits, bytes);
    }

    if ((qk->compression & G3D_QK_LINEARTIME_COMPRESSION) != 0u)
    {
        g3d_u32 vals[2];
        rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
        if (rc != G3D_OK)
        {
            return rc;
        }
        if (g3d_parse_fbits_list(line, vals, 2u) != G3D_OK)
        {
            return G3D_ERR_CORRUPT;
        }
        qk->time_bits[0] = vals[0];
        qk->time_bits[1] = vals[1];
    }

    if ((qk->compression & G3D_QK_HINGE_COMPRESSION) != 0u)
    {
        g3d_u32 axis_vals[3];
        rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
        if (rc != G3D_OK)
        {
            return rc;
        }
        if (g3d_parse_fbits_list(line, axis_vals, 3u) != G3D_OK)
        {
            return G3D_ERR_CORRUPT;
        }
        qk->hinge_axis.x_bits = axis_vals[0];
        qk->hinge_axis.y_bits = axis_vals[1];
        qk->hinge_axis.z_bits = axis_vals[2];
        if (qk->count != 0u)
        {
            rc = g3d_mul_u32_checked(qk->count, 4u, &bytes);
            if (rc != G3D_OK)
            {
                return rc;
            }
            qk->angle_bits = (g3d_u32 *)g3d_arena_alloc(arena, bytes, 4u);
            if (qk->angle_bits == NULL)
            {
                return G3D_ERR_ARENA_EXHAUSTED;
            }
            for (i = 0u; i < qk->count; ++i)
            {
                g3d_u32 vals[2];
                rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
                if (rc != G3D_OK)
                {
                    return rc;
                }
                if ((qk->compression & G3D_QK_LINEARTIME_COMPRESSION) != 0u)
                {
                    if (g3d_parse_fbits_list(line, vals, 1u) != G3D_OK)
                    {
                        return G3D_ERR_CORRUPT;
                    }
                    qk->angle_bits[i] = vals[0];
                }
                else
                {
                    if (g3d_parse_fbits_list(line, vals, 2u) != G3D_OK)
                    {
                        return G3D_ERR_CORRUPT;
                    }
                    qk->time_bits[i] = vals[0];
                    qk->angle_bits[i] = vals[1];
                }
            }
        }
    }
    else
    {
        if (qk->count != 0u)
        {
            rc = g3d_mul_u32_checked(qk->count, (g3d_u32)sizeof(g3d_fquat), &bytes);
            if (rc != G3D_OK)
            {
                return rc;
            }
            qk->quats = (g3d_fquat *)g3d_arena_alloc(arena, bytes, 4u);
            if (qk->quats == NULL)
            {
                return G3D_ERR_ARENA_EXHAUSTED;
            }
            for (i = 0u; i < qk->count; ++i)
            {
                g3d_u32 vals[5];
                rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
                if (rc != G3D_OK)
                {
                    return rc;
                }
                if ((qk->compression & G3D_QK_LINEARTIME_COMPRESSION) != 0u)
                {
                    if (g3d_parse_fbits_list(line, vals, 4u) != G3D_OK)
                    {
                        return G3D_ERR_CORRUPT;
                    }
                    qk->quats[i].w_bits = vals[0];
                    qk->quats[i].x_bits = vals[1];
                    qk->quats[i].y_bits = vals[2];
                    qk->quats[i].z_bits = vals[3];
                }
                else
                {
                    if (g3d_parse_fbits_list(line, vals, 5u) != G3D_OK)
                    {
                        return G3D_ERR_CORRUPT;
                    }
                    qk->time_bits[i] = vals[0];
                    qk->quats[i].w_bits = vals[1];
                    qk->quats[i].x_bits = vals[2];
                    qk->quats[i].y_bits = vals[3];
                    qk->quats[i].z_bits = vals[4];
                }
            }
        }
    }
    return G3D_OK;
}


static int g3d_parse_vkframe_ascii(g3d_vkframe *vk, g3d_reader *r, g3d_arena *arena)
{
    char line[512];
    const char *args;
    int count_i;
    int interp_i;
    int comp_i;
    int loop_i;
    g3d_u32 bytes;
    g3d_u32 i;
    int rc;

    if (vk == NULL || r == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    g3d_zero(vk, (g3d_u32)sizeof(*vk));

    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    args = g3d_line_after_tag(line, "Keys");
    if (args == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    if (g3d_parse_s32_4(args, &count_i, &interp_i, &comp_i, &loop_i) != G3D_OK)
    {
        return G3D_ERR_CORRUPT;
    }
    if (count_i < 0 || interp_i < G3D_VK_LINEAR || interp_i > G3D_VK_HERMITE_ZERO_DERIV || comp_i < 0)
    {
        return G3D_ERR_CORRUPT;
    }
    if (((g3d_u32)comp_i & ~G3D_VK_LINEARTIME_COMPRESSION) != 0u)
    {
        return G3D_ERR_UNSUPPORTED;
    }

    vk->interpolation = (g3d_u8)interp_i;
    vk->compression = (g3d_u8)comp_i;
    vk->looping = (g3d_u8)((loop_i != 0) ? 1u : 0u);
    vk->count = (g3d_u32)count_i;
    vk->time_value_count = ((vk->compression & G3D_VK_LINEARTIME_COMPRESSION) != 0u) ? 2u : vk->count;

    if (vk->time_value_count != 0u)
    {
        rc = g3d_mul_u32_checked(vk->time_value_count, 4u, &bytes);
        if (rc != G3D_OK)
        {
            return rc;
        }
        vk->time_bits = (g3d_u32 *)g3d_arena_alloc(arena, bytes, 4u);
        if (vk->time_bits == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        g3d_zero(vk->time_bits, bytes);
    }

    if ((vk->compression & G3D_VK_LINEARTIME_COMPRESSION) != 0u)
    {
        g3d_u32 vals[2];
        rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
        if (rc != G3D_OK)
        {
            return rc;
        }
        if (g3d_parse_fbits_list(line, vals, 2u) != G3D_OK)
        {
            return G3D_ERR_CORRUPT;
        }
        vk->time_bits[0] = vals[0];
        vk->time_bits[1] = vals[1];
    }

    if (vk->count != 0u)
    {
        rc = g3d_mul_u32_checked(vk->count, (g3d_u32)sizeof(g3d_fvec3), &bytes);
        if (rc != G3D_OK)
        {
            return rc;
        }
        vk->vectors = (g3d_fvec3 *)g3d_arena_alloc(arena, bytes, 4u);
        if (vk->vectors == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        for (i = 0u; i < vk->count; ++i)
        {
            g3d_u32 vals[4];
            rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
            if (rc != G3D_OK)
            {
                return rc;
            }
            if ((vk->compression & G3D_VK_LINEARTIME_COMPRESSION) != 0u)
            {
                if (g3d_parse_fbits_list(line, vals, 3u) != G3D_OK)
                {
                    return G3D_ERR_CORRUPT;
                }
                vk->vectors[i].x_bits = vals[0];
                vk->vectors[i].y_bits = vals[1];
                vk->vectors[i].z_bits = vals[2];
            }
            else
            {
                if (g3d_parse_fbits_list(line, vals, 4u) != G3D_OK)
                {
                    return G3D_ERR_CORRUPT;
                }
                vk->time_bits[i] = vals[0];
                vk->vectors[i].x_bits = vals[1];
                vk->vectors[i].y_bits = vals[2];
                vk->vectors[i].z_bits = vals[3];
            }
        }
    }
    return G3D_OK;
}



static int g3d_parse_path_f0_rotation_key(g3d_qkframe *qk, g3d_u32 index, g3d_reader *r)
{
    char line[512];
    g3d_u32 sig;
    g3d_u32 vals[8];
    int rc;

    if (qk == NULL || r == NULL || index >= qk->count)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_rd_u32le(r, &sig);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (sig != G3D_QKFL_ASCII && sig != G3D_QKFS_ASCII && sig != G3D_QKFQ_ASCII)
    {
        return G3D_ERR_BAD_MAGIC;
    }
    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (sig == G3D_QKFQ_ASCII)
    {
        if (g3d_parse_fbits_list(line, vals, 8u) != G3D_OK)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    else
    {
        if (g3d_parse_fbits_list(line, vals, 4u) != G3D_OK)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    qk->quats[index].w_bits = vals[0];
    qk->quats[index].x_bits = vals[1];
    qk->quats[index].y_bits = vals[2];
    qk->quats[index].z_bits = vals[3];
    return G3D_OK;
}


static int g3d_parse_path_f0_translation_key(g3d_vkframe *vk, g3d_u32 index, g3d_reader *r)
{
    char line[512];
    g3d_u32 sig;
    g3d_u32 vals[9];
    int rc;

    if (vk == NULL || r == NULL || index >= vk->count)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_rd_u32le(r, &sig);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (sig != G3D_VKFL_ASCII && sig != G3D_VKFH_ASCII)
    {
        return G3D_ERR_BAD_MAGIC;
    }
    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (sig == G3D_VKFH_ASCII)
    {
        if (g3d_parse_fbits_list(line, vals, 9u) != G3D_OK)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    else
    {
        if (g3d_parse_fbits_list(line, vals, 3u) != G3D_OK)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    vk->vectors[index].x_bits = vals[0];
    vk->vectors[index].y_bits = vals[1];
    vk->vectors[index].z_bits = vals[2];
    return G3D_OK;
}


static int g3d_parse_path_f0_rotation_channel(g3d_path *path, g3d_reader *r, g3d_arena *arena)
{
    char line[512];
    const char *args;
    g3d_u32 sig;
    g3d_u32 version;
    g3d_u32 bytes;
    g3d_u32 i;
    int rc;
    int interp;
    int count;
    int items;

    if (path == NULL || r == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_rd_u32le(r, &sig);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (sig != G3D_PATH_CHANNEL_ASCII)
    {
        return G3D_ERR_BAD_MAGIC;
    }
    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_parse_version_text(line, &version);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (version < G3D_PATH_ASCII_VER0)
    {
        return G3D_ERR_BAD_VERSION;
    }

    interp = -1;
    count = -1;
    items = 0;

    while (items < 4)
    {
        rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
        if (rc != G3D_OK)
        {
            return rc;
        }
        args = g3d_line_after_tag(line, "InterpolationType");
        if (args != NULL)
        {
            if (g3d_parse_s32_1(args, &interp) != G3D_OK)
            {
                return G3D_ERR_CORRUPT;
            }
            ++items;
            continue;
        }
        args = g3d_line_after_tag(line, "StartTime");
        if (args != NULL)
        {
            g3d_u32 ignored_bits;
            if (g3d_parse_fbits_list(args, &ignored_bits, 1u) != G3D_OK)
            {
                return G3D_ERR_CORRUPT;
            }
            ++items;
            continue;
        }
        args = g3d_line_after_tag(line, "EndTime");
        if (args != NULL)
        {
            g3d_u32 ignored_bits;
            if (g3d_parse_fbits_list(args, &ignored_bits, 1u) != G3D_OK)
            {
                return G3D_ERR_CORRUPT;
            }
            ++items;
            continue;
        }
        args = g3d_line_after_tag(line, "KeyList");
        if (args != NULL)
        {
            if (g3d_parse_s32_1(args, &count) != G3D_OK || count < 0)
            {
                return G3D_ERR_CORRUPT;
            }
            if (interp != G3D_OLD_PATH_INTERP_LINEAR &&
                interp != G3D_OLD_PATH_INTERP_SLERP &&
                interp != G3D_OLD_PATH_INTERP_SQUAD)
            {
                return G3D_ERR_CORRUPT;
            }
            path->has_rotation = 1u;
            if (interp == G3D_OLD_PATH_INTERP_SLERP)
            {
                path->rotation.interpolation = (g3d_u8)G3D_QK_SLERP;
                path->rotation_interp = (g3d_u8)G3D_PATH_QK_SLERP;
            }
            else if (interp == G3D_OLD_PATH_INTERP_SQUAD)
            {
                path->rotation.interpolation = (g3d_u8)G3D_QK_SQUAD;
                path->rotation_interp = (g3d_u8)G3D_PATH_QK_SQUAD;
            }
            else
            {
                path->rotation.interpolation = (g3d_u8)G3D_QK_LINEAR;
                path->rotation_interp = (g3d_u8)G3D_PATH_QK_LINEAR;
            }
            path->rotation.compression = 0u;
            path->rotation.looping = path->looped;
            path->rotation.count = (g3d_u32)count;
            path->rotation.time_value_count = (g3d_u32)count;
            path->rotation.time_bits = NULL;
            path->rotation.angle_bits = NULL;
            path->rotation.quats = NULL;
            if (count != 0)
            {
                rc = g3d_mul_u32_checked((g3d_u32)count, 4u, &bytes);
                if (rc != G3D_OK)
                {
                    return rc;
                }
                path->rotation.time_bits = (g3d_u32 *)g3d_arena_alloc(arena, bytes, 4u);
                if (path->rotation.time_bits == NULL)
                {
                    return G3D_ERR_ARENA_EXHAUSTED;
                }
                rc = g3d_mul_u32_checked((g3d_u32)count, (g3d_u32)sizeof(g3d_fquat), &bytes);
                if (rc != G3D_OK)
                {
                    return rc;
                }
                path->rotation.quats = (g3d_fquat *)g3d_arena_alloc(arena, bytes, 4u);
                if (path->rotation.quats == NULL)
                {
                    return G3D_ERR_ARENA_EXHAUSTED;
                }
                for (i = 0u; i < (g3d_u32)count; ++i)
                {
                    g3d_u32 key_time_bits;
                    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
                    if (rc != G3D_OK)
                    {
                        return rc;
                    }
                    if (g3d_parse_fbits_list(line, &key_time_bits, 1u) != G3D_OK)
                    {
                        return G3D_ERR_CORRUPT;
                    }
                    path->rotation.time_bits[i] = key_time_bits;
                    rc = g3d_parse_path_f0_rotation_key(&path->rotation, i, r);
                    if (rc != G3D_OK)
                    {
                        return rc;
                    }
                }
            }
            ++items;
            continue;
        }
        return G3D_ERR_CORRUPT;
    }
    return G3D_OK;
}


static int g3d_parse_path_f0_translation_channel(g3d_path *path, g3d_reader *r, g3d_arena *arena)
{
    char line[512];
    const char *args;
    g3d_u32 sig;
    g3d_u32 version;
    g3d_u32 bytes;
    g3d_u32 i;
    int rc;
    int interp;
    int count;
    int items;

    if (path == NULL || r == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_rd_u32le(r, &sig);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (sig != G3D_PATH_CHANNEL_ASCII)
    {
        return G3D_ERR_BAD_MAGIC;
    }
    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_parse_version_text(line, &version);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (version < G3D_PATH_ASCII_VER0)
    {
        return G3D_ERR_BAD_VERSION;
    }

    interp = -1;
    count = -1;
    items = 0;

    while (items < 4)
    {
        rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
        if (rc != G3D_OK)
        {
            return rc;
        }
        args = g3d_line_after_tag(line, "InterpolationType");
        if (args != NULL)
        {
            if (g3d_parse_s32_1(args, &interp) != G3D_OK)
            {
                return G3D_ERR_CORRUPT;
            }
            ++items;
            continue;
        }
        args = g3d_line_after_tag(line, "StartTime");
        if (args != NULL)
        {
            g3d_u32 ignored_bits;
            if (g3d_parse_fbits_list(args, &ignored_bits, 1u) != G3D_OK)
            {
                return G3D_ERR_CORRUPT;
            }
            ++items;
            continue;
        }
        args = g3d_line_after_tag(line, "EndTime");
        if (args != NULL)
        {
            g3d_u32 ignored_bits;
            if (g3d_parse_fbits_list(args, &ignored_bits, 1u) != G3D_OK)
            {
                return G3D_ERR_CORRUPT;
            }
            ++items;
            continue;
        }
        args = g3d_line_after_tag(line, "KeyList");
        if (args != NULL)
        {
            if (g3d_parse_s32_1(args, &count) != G3D_OK || count < 0)
            {
                return G3D_ERR_CORRUPT;
            }
            if (interp != G3D_OLD_PATH_INTERP_LINEAR &&
                interp != G3D_OLD_PATH_INTERP_HERMITE &&
                interp != G3D_OLD_PATH_INTERP_HERMITE_ZERO_DERIV)
            {
                return G3D_ERR_CORRUPT;
            }
            path->has_translation = 1u;
            if (interp == G3D_OLD_PATH_INTERP_HERMITE)
            {
                path->translation.interpolation = (g3d_u8)G3D_VK_HERMITE;
                path->translation_interp = (g3d_u8)G3D_PATH_VK_HERMITE;
            }
            else if (interp == G3D_OLD_PATH_INTERP_HERMITE_ZERO_DERIV)
            {
                path->translation.interpolation = (g3d_u8)G3D_VK_HERMITE_ZERO_DERIV;
                path->translation_interp = (g3d_u8)G3D_PATH_VK_HERMITE_ZERO_DERIV;
            }
            else
            {
                path->translation.interpolation = (g3d_u8)G3D_VK_LINEAR;
                path->translation_interp = (g3d_u8)G3D_PATH_VK_LINEAR;
            }
            path->translation.compression = 0u;
            path->translation.looping = path->looped;
            path->translation.count = (g3d_u32)count;
            path->translation.time_value_count = (g3d_u32)count;
            path->translation.time_bits = NULL;
            path->translation.vectors = NULL;
            if (count != 0)
            {
                rc = g3d_mul_u32_checked((g3d_u32)count, 4u, &bytes);
                if (rc != G3D_OK)
                {
                    return rc;
                }
                path->translation.time_bits = (g3d_u32 *)g3d_arena_alloc(arena, bytes, 4u);
                if (path->translation.time_bits == NULL)
                {
                    return G3D_ERR_ARENA_EXHAUSTED;
                }
                rc = g3d_mul_u32_checked((g3d_u32)count, (g3d_u32)sizeof(g3d_fvec3), &bytes);
                if (rc != G3D_OK)
                {
                    return rc;
                }
                path->translation.vectors = (g3d_fvec3 *)g3d_arena_alloc(arena, bytes, 4u);
                if (path->translation.vectors == NULL)
                {
                    return G3D_ERR_ARENA_EXHAUSTED;
                }
                for (i = 0u; i < (g3d_u32)count; ++i)
                {
                    g3d_u32 key_time_bits;
                    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
                    if (rc != G3D_OK)
                    {
                        return rc;
                    }
                    if (g3d_parse_fbits_list(line, &key_time_bits, 1u) != G3D_OK)
                    {
                        return G3D_ERR_CORRUPT;
                    }
                    path->translation.time_bits[i] = key_time_bits;
                    rc = g3d_parse_path_f0_translation_key(&path->translation, i, r);
                    if (rc != G3D_OK)
                    {
                        return rc;
                    }
                }
            }
            ++items;
            continue;
        }
        return G3D_ERR_CORRUPT;
    }
    return G3D_OK;
}


static int g3d_parse_path_ascii_f0_body(g3d_path *path, g3d_reader *r, g3d_arena *arena)
{
    char line[512];
    const char *args;
    int rc;
    int flag;

    if (path == NULL || r == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    path->raw_header = G3D_PATH_ASCII;
    path->has_rotation = 0u;
    path->has_translation = 0u;
    path->rotation_interp = (g3d_u8)G3D_PATH_QK_LINEAR;
    path->translation_interp = (g3d_u8)G3D_PATH_VK_LINEAR;
    path->looped = 0u;

    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    args = g3d_line_after_tag(line, "Looped");
    if (args == NULL || g3d_parse_s32_1(args, &flag) != G3D_OK)
    {
        return G3D_ERR_CORRUPT;
    }
    if (flag != 0)
    {
        path->looped = 1u;
    }

    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    args = g3d_line_after_tag(line, "Rotation");
    if (args == NULL || g3d_parse_s32_1(args, &flag) != G3D_OK)
    {
        return G3D_ERR_CORRUPT;
    }
    if (flag != 0)
    {
        rc = g3d_parse_path_f0_rotation_channel(path, r, arena);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }

    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    args = g3d_line_after_tag(line, "Translation");
    if (args == NULL || g3d_parse_s32_1(args, &flag) != G3D_OK)
    {
        return G3D_ERR_CORRUPT;
    }
    if (flag != 0)
    {
        rc = g3d_parse_path_f0_translation_channel(path, r, arena);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    return G3D_OK;
}

static int g3d_parse_path_ascii_body(g3d_path *path, g3d_reader *r, g3d_arena *arena)
{
    char line[512];
    const char *args;
    g3d_u32 version;
    int flag;
    int interp;
    int rc;

    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_parse_version_text(line, &version);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (version == G3D_PATH_ASCII_VER0)
    {
        return g3d_parse_path_ascii_f0_body(path, r, arena);
    }
    if (version != G3D_PATH_ASCII_VER1 && version != G3D_PATH_ASCII_VER2)
    {
        return G3D_ERR_BAD_VERSION;
    }

    path->raw_header = G3D_PATH_ASCII;
    path->has_rotation = 0u;
    path->has_translation = 0u;
    path->rotation_interp = (g3d_u8)G3D_PATH_QK_LINEAR;
    path->translation_interp = (g3d_u8)G3D_PATH_VK_LINEAR;
    path->looped = 0u;

    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    args = g3d_line_after_tag(line, "Rotation");
    if (args == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    interp = G3D_PATH_QK_LINEAR;
    if (version == G3D_PATH_ASCII_VER2)
    {
        if (g3d_parse_s32_2(args, &flag, &interp) != G3D_OK)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    else
    {
        if (g3d_parse_s32_1(args, &flag) != G3D_OK)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    if (flag != 0)
    {
        path->has_rotation = 1u;
        rc = g3d_parse_qkframe_ascii(&path->rotation, r, arena);
        if (rc != G3D_OK)
        {
            return rc;
        }
        path->rotation_interp = g3d_qk_interp_to_path((int)path->rotation.interpolation);
        if (path->rotation.looping != 0u)
        {
            path->looped = 1u;
        }
    }
    else
    {
        path->rotation_interp = (g3d_u8)interp;
    }

    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    args = g3d_line_after_tag(line, "Translation");
    if (args == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    interp = G3D_PATH_VK_LINEAR;
    if (version == G3D_PATH_ASCII_VER2)
    {
        if (g3d_parse_s32_2(args, &flag, &interp) != G3D_OK)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    else
    {
        if (g3d_parse_s32_1(args, &flag) != G3D_OK)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    if (flag != 0)
    {
        path->has_translation = 1u;
        rc = g3d_parse_vkframe_ascii(&path->translation, r, arena);
        if (rc != G3D_OK)
        {
            return rc;
        }
        path->translation_interp = g3d_vk_interp_to_path((int)path->translation.interpolation);
        if (path->translation.looping != 0u)
        {
            path->looped = 1u;
        }
    }
    else
    {
        path->translation_interp = (g3d_u8)interp;
    }
    return G3D_OK;
}

static int g3d_parse_path_inner(g3d_path *path, g3d_reader *r, g3d_arena *arena)
{
    int rc;
    g3d_u32 hdr;
    rc = g3d_rd_u32le(r, &hdr);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (hdr == G3D_PATH_ASCII)
    {
        return g3d_parse_path_ascii_body(path, r, arena);
    }
    if ((hdr >> 16) != G3D_PATH_BIN_VER)
    {
        return G3D_ERR_BAD_VERSION;
    }
    path->raw_header = hdr;
    path->has_translation = (g3d_u8)((hdr >> 1) & 0x1u);
    path->has_rotation = (g3d_u8)(hdr & 0x1u);
    path->translation_interp = (g3d_u8)((hdr >> G3D_PATH_TRANS_SHIFT) & G3D_PATH_MAX_INT_TYPE_COUNT);
    path->rotation_interp = (g3d_u8)((hdr >> G3D_PATH_ROT_SHIFT) & G3D_PATH_MAX_INT_TYPE_COUNT);
    path->looped = 0u;

    if (path->has_translation)
    {
        rc = g3d_parse_vkframe_inner(&path->translation, r, arena);
        if (rc != G3D_OK)
        {
            return rc;
        }
        if (path->translation.looping != 0u)
        {
            path->looped = 1u;
        }
    }
    if (path->has_rotation)
    {
        rc = g3d_parse_qkframe_inner(&path->rotation, r, arena);
        if (rc != G3D_OK)
        {
            return rc;
        }
        if (path->rotation.looping != 0u)
        {
            path->looped = 1u;
        }
    }
    return G3D_OK;
}

int g3d_path_parse_binary(g3d_path *path, const void *data, g3d_u32 size, g3d_arena *arena, g3d_u32 *used_bytes)
{
    g3d_reader r;
    int rc;
    if (path == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    g3d_zero(path, (g3d_u32)sizeof(*path));
    g3d_reader_init(&r, data, size);
    rc = g3d_parse_path_inner(path, &r, arena);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = r.pos;
    }
    return G3D_OK;
}

static int g3d_write_path_inner(const g3d_path *path, g3d_writer *w)
{
    g3d_u32 header;
    int rc;
    if (path == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    header = ((g3d_u32)G3D_PATH_BIN_VER << 16) |
             (((g3d_u32)path->has_translation & 0x1u) << 1) |
             ((g3d_u32)path->has_rotation & 0x1u) |
             ((g3d_u32)path->translation_interp << G3D_PATH_TRANS_SHIFT) |
             ((g3d_u32)path->rotation_interp << G3D_PATH_ROT_SHIFT);

    if (g3d_wr_u32le(w, header) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (path->has_translation)
    {
        rc = g3d_write_vkframe_inner(&path->translation, w);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    if (path->has_rotation)
    {
        rc = g3d_write_qkframe_inner(&path->rotation, w);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    return G3D_OK;
}

int g3d_path_write_binary(const g3d_path *path, void *dst, g3d_u32 cap, g3d_u32 *used_bytes)
{
    g3d_writer w;
    int rc;
    g3d_writer_init(&w, dst, cap);
    rc = g3d_write_path_inner(path, &w);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = w.pos;
    }
    return G3D_OK;
}

int g3d_path_measure_binary(const g3d_path *path, g3d_u32 *size_out)
{
    g3d_u32 used;
    int rc;
    rc = g3d_path_write_binary(path, NULL, 0u, &used);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *size_out = used;
    return G3D_OK;
}


static int g3d_parse_motion_ascii_body(g3d_motion *motion, g3d_reader *r, g3d_arena *arena)
{
    char line[512];
    const char *args;
    const char *name_text;
    g3d_u32 version;
    g3d_u32 bytes;
    int rc;
    int items;
    int maintain_names;
    int path_count;
    int path_array_count;
    int name_checksum;
    int flag;
    g3d_u32 i;

    if (motion == NULL || r == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }

    rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_parse_version_text(line, &version);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (version != G3D_MOT_VER)
    {
        return G3D_ERR_BAD_VERSION;
    }

    motion->name = NULL;
    motion->name_len = 0u;
    motion->maintain_names = 0u;
    motion->node_type = (g3d_u8)G3D_MOTION_NODE_LEAF;
    motion->leaf.path_count = 0;
    motion->leaf.name_checksum = 0;
    motion->leaf.flags = 0u;
    motion->leaf.events = NULL;
    motion->leaf.name_array = NULL;
    motion->leaf.paths = NULL;

    items = 0;
    maintain_names = 0;
    path_count = -1;
    path_array_count = -1;
    name_checksum = 0;

    while (items < 6)
    {
        rc = g3d_rd_line(r, line, (g3d_u32)sizeof(line));
        if (rc != G3D_OK)
        {
            return rc;
        }

        args = g3d_line_after_tag(line, "NameID");
        if (args != NULL)
        {
            name_text = args;
            if (*name_text != '\0')
            {
                motion->name = g3d_arena_strdup(arena, name_text);
                if (motion->name == NULL)
                {
                    return G3D_ERR_ARENA_EXHAUSTED;
                }
                if (g3d_strlen32(name_text) > 0xffffu)
                {
                    return G3D_ERR_OVERFLOW;
                }
                motion->name_len = (g3d_u16)(g3d_strlen32(name_text) + 1u);
            }
            continue;
        }

        args = g3d_line_after_tag(line, "MaintainNames");
        if (args != NULL)
        {
            if (g3d_parse_s32_1(args, &maintain_names) != G3D_OK)
            {
                return G3D_ERR_CORRUPT;
            }
            motion->maintain_names = (g3d_u8)((maintain_names != 0) ? 1u : 0u);
            ++items;
            continue;
        }

        args = g3d_line_after_tag(line, "PathCount");
        if (args != NULL)
        {
            if (g3d_parse_s32_1(args, &path_count) != G3D_OK || path_count < 0)
            {
                return G3D_ERR_CORRUPT;
            }
            motion->leaf.path_count = (g3d_s32)path_count;
            ++items;
            continue;
        }

        args = g3d_line_after_tag(line, "NameChecksum");
        if (args != NULL)
        {
            if (g3d_parse_s32_1(args, &name_checksum) != G3D_OK)
            {
                return G3D_ERR_CORRUPT;
            }
            motion->leaf.name_checksum = (g3d_s32)name_checksum;
            ++items;
            continue;
        }

        args = g3d_line_after_tag(line, "Events");
        if (args != NULL)
        {
            if (g3d_parse_s32_1(args, &flag) != G3D_OK)
            {
                return G3D_ERR_CORRUPT;
            }
            if (flag != 0)
            {
                motion->leaf.events = (g3d_tkevents *)g3d_arena_alloc(arena, (g3d_u32)sizeof(g3d_tkevents), 4u);
                if (motion->leaf.events == NULL)
                {
                    return G3D_ERR_ARENA_EXHAUSTED;
                }
                g3d_zero(motion->leaf.events, (g3d_u32)sizeof(g3d_tkevents));
                rc = g3d_parse_tkevents_inner(motion->leaf.events, r, arena);
                if (rc != G3D_OK)
                {
                    return rc;
                }
                motion->leaf.flags |= 0x1u;
            }
            ++items;
            continue;
        }

        args = g3d_line_after_tag(line, "NameArray");
        if (args != NULL)
        {
            if (g3d_parse_s32_1(args, &flag) != G3D_OK)
            {
                return G3D_ERR_CORRUPT;
            }
            if (flag != 0)
            {
                motion->leaf.name_array = (g3d_strblock *)g3d_arena_alloc(arena, (g3d_u32)sizeof(g3d_strblock), 4u);
                if (motion->leaf.name_array == NULL)
                {
                    return G3D_ERR_ARENA_EXHAUSTED;
                }
                g3d_zero(motion->leaf.name_array, (g3d_u32)sizeof(g3d_strblock));
                rc = g3d_parse_strblock_inner(motion->leaf.name_array, r, arena);
                if (rc != G3D_OK)
                {
                    return rc;
                }
                motion->leaf.flags |= 0x2u;
            }
            ++items;
            continue;
        }

        args = g3d_line_after_tag(line, "PathArray");
        if (args != NULL)
        {
            if (g3d_parse_s32_1(args, &path_array_count) != G3D_OK || path_array_count < 0)
            {
                return G3D_ERR_CORRUPT;
            }
            if (path_count >= 0 && path_array_count != path_count)
            {
                return G3D_ERR_CORRUPT;
            }
            motion->leaf.path_count = (g3d_s32)path_array_count;
            if (path_array_count != 0)
            {
                rc = g3d_mul_u32_checked((g3d_u32)path_array_count, (g3d_u32)sizeof(g3d_path), &bytes);
                if (rc != G3D_OK)
                {
                    return rc;
                }
                motion->leaf.paths = (g3d_path *)g3d_arena_alloc(arena, bytes, 4u);
                if (motion->leaf.paths == NULL)
                {
                    return G3D_ERR_ARENA_EXHAUSTED;
                }
                g3d_zero(motion->leaf.paths, bytes);
                for (i = 0u; i < (g3d_u32)path_array_count; ++i)
                {
                    rc = g3d_parse_path_inner(&motion->leaf.paths[i], r, arena);
                    if (rc != G3D_OK)
                    {
                        return rc;
                    }
                }
            }
            ++items;
            continue;
        }

        return G3D_ERR_CORRUPT;
    }

    return g3d_motion_validate(motion);
}

static int g3d_parse_motion_inner(g3d_motion *motion, g3d_reader *r, g3d_arena *arena)
{
    g3d_u32 sig;
    g3d_u32 ver;
    g3d_u32 packed;
    int rc;
    g3d_u32 i;

    rc = g3d_rd_u32le(r, &sig);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (sig == G3D_MOT_ASCII)
    {
        return g3d_parse_motion_ascii_body(motion, r, arena);
    }
    if (sig != G3D_MOT_BIN)
    {
        return G3D_ERR_BAD_MAGIC;
    }
    rc = g3d_rd_u32le(r, &ver);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (ver != G3D_MOT_VER)
    {
        return G3D_ERR_BAD_VERSION;
    }
    rc = g3d_rd_u32le(r, &packed);
    if (rc != G3D_OK)
    {
        return rc;
    }

    motion->maintain_names = (g3d_u8)(((packed & (1u << 16)) != 0u) ? 1u : 0u);
    motion->name_len = (g3d_u16)(packed & 0xffffu);
    motion->node_type = (g3d_u8)((packed >> 24) & 0xffu);
    motion->name = NULL;

    if (motion->name_len != 0u)
    {
        motion->name = (char *)g3d_arena_alloc(arena, (g3d_u32)motion->name_len, 1u);
        if (motion->name == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        rc = g3d_rd_bytes(r, motion->name, (g3d_u32)motion->name_len);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }

    if (motion->node_type == G3D_MOTION_NODE_BRANCH)
    {
        return G3D_ERR_UNSUPPORTED;
    }
    if (motion->node_type == G3D_MOTION_NODE_UNDECIDED)
    {
        return G3D_OK;
    }
    if (motion->node_type != G3D_MOTION_NODE_LEAF)
    {
        return G3D_ERR_CORRUPT;
    }

    rc = g3d_rd_s32le(r, &motion->leaf.path_count);
    if (rc != G3D_OK) return rc;
    rc = g3d_rd_s32le(r, &motion->leaf.name_checksum);
    if (rc != G3D_OK) return rc;
    rc = g3d_rd_u32le(r, &motion->leaf.flags);
    if (rc != G3D_OK) return rc;

    motion->leaf.events = NULL;
    motion->leaf.name_array = NULL;
    motion->leaf.paths = NULL;

    if ((motion->leaf.flags & 0x1u) != 0u)
    {
        motion->leaf.events = (g3d_tkevents *)g3d_arena_alloc(arena, (g3d_u32)sizeof(g3d_tkevents), 4u);
        if (motion->leaf.events == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        g3d_zero(motion->leaf.events, (g3d_u32)sizeof(g3d_tkevents));
        rc = g3d_parse_tkevents_inner(motion->leaf.events, r, arena);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }

    if ((motion->leaf.flags & 0x2u) != 0u)
    {
        motion->leaf.name_array = (g3d_strblock *)g3d_arena_alloc(arena, (g3d_u32)sizeof(g3d_strblock), 4u);
        if (motion->leaf.name_array == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        g3d_zero(motion->leaf.name_array, (g3d_u32)sizeof(g3d_strblock));
        rc = g3d_parse_strblock_inner(motion->leaf.name_array, r, arena);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }

    if (motion->leaf.path_count < 0)
    {
        return G3D_ERR_CORRUPT;
    }

    if (motion->leaf.path_count != 0)
    {
        motion->leaf.paths = (g3d_path *)g3d_arena_alloc(arena, (g3d_u32)motion->leaf.path_count * (g3d_u32)sizeof(g3d_path), 4u);
        if (motion->leaf.paths == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        g3d_zero(motion->leaf.paths, (g3d_u32)motion->leaf.path_count * (g3d_u32)sizeof(g3d_path));
        for (i = 0u; i < (g3d_u32)motion->leaf.path_count; ++i)
        {
            rc = g3d_parse_path_inner(&motion->leaf.paths[i], r, arena);
            if (rc != G3D_OK)
            {
                return rc;
            }
        }
    }

    return G3D_OK;
}

int g3d_motion_parse_binary(g3d_motion *motion, const void *data, g3d_u32 size, g3d_arena *arena)
{
    g3d_reader r;
    int rc;
    if (motion == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    g3d_zero(motion, (g3d_u32)sizeof(*motion));
    g3d_reader_init(&r, data, size);
    rc = g3d_parse_motion_inner(motion, &r, arena);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (r.pos != size)
    {
        return G3D_ERR_CORRUPT;
    }
    return G3D_OK;
}


static int g3d_find_zero_before(const g3d_u8 *data, g3d_u32 size)
{
    g3d_u32 i;
    if (data == NULL && size != 0u)
    {
        return G3D_FALSE;
    }
    for (i = 0u; i < size; ++i)
    {
        if (data[i] == 0u)
        {
            return G3D_TRUE;
        }
    }
    return G3D_FALSE;
}

static int g3d_write_strblock_ascii_inner(const g3d_strblock *sb, g3d_writer *w)
{
    g3d_s32 i;
    int rc;
    if (sb == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (g3d_wr_u32le(w, G3D_SB_ASCII) != G3D_OK) return G3D_ERR_NO_SPACE;
    rc = g3d_wr_textf(w, " %X.%.2X\n", (unsigned)((G3D_SB_VER & 0xff00u) >> 8), (unsigned)(G3D_SB_VER & 0xffu));
    if (rc != G3D_OK) return rc;
    rc = g3d_wr_textf(w, "Strings %d\n", (int)sb->count);
    if (rc != G3D_OK) return rc;
    for (i = 0; i < sb->count; ++i)
    {
        const char *text;
        text = g3d_strblock_get(sb, i);
        if (text == NULL)
        {
            return G3D_ERR_CORRUPT;
        }
        rc = g3d_wr_cstr_line(w, text);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    return G3D_OK;
}

static int g3d_write_tkevents_ascii_inner(const g3d_tkevents *ev, g3d_writer *w)
{
    g3d_s32 i;
    int rc;
    if (ev == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (g3d_wr_u32le(w, G3D_TKEV_ASCII) != G3D_OK) return G3D_ERR_NO_SPACE;
    rc = g3d_wr_textf(w, " %X.%.2X\n", (unsigned)((G3D_TKEV_VER & 0xff00u) >> 8), (unsigned)(G3D_TKEV_VER & 0xffu));
    if (rc != G3D_OK) return rc;
    rc = g3d_wr_textf(w, "DataSize %u\n", (unsigned)ev->data_size);
    if (rc != G3D_OK) return rc;
    rc = g3d_wr_textf(w, "TimeKeys %d\n", (int)ev->time_keys.num_elements);
    if (rc != G3D_OK) return rc;
    for (i = 0; i < ev->time_keys.num_elements; ++i)
    {
        g3d_u32 time_bits;
        g3d_u32 offset;
        const char *text;
        rc = g3d_tkevents_get(ev, i, &time_bits, &offset);
        if (rc != G3D_OK)
        {
            return rc;
        }
        if (offset >= ev->data_size)
        {
            return G3D_ERR_CORRUPT;
        }
        if (g3d_find_zero_before(ev->event_data + offset, ev->data_size - offset) == G3D_FALSE)
        {
            return G3D_ERR_CORRUPT;
        }
        text = (const char *)(ev->event_data + offset);
        rc = g3d_wr_bits_dec6_chr(w, time_bits, ' ');
        if (rc != G3D_OK)
        {
            return rc;
        }
        rc = g3d_wr_textf(w, "%u\n", (unsigned)offset);
        if (rc != G3D_OK)
        {
            return rc;
        }
        rc = g3d_wr_cstr_line(w, text);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    return G3D_OK;
}


static int g3d_write_qkframe_ascii_inner(const g3d_qkframe *qk, g3d_writer *w)
{
    g3d_u32 i;
    int rc;
    if (qk == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_wr_textf(w, "Keys %u %u %u %u\n", (unsigned)qk->count,
                      (unsigned)qk->interpolation, (unsigned)qk->compression,
                      (unsigned)(qk->looping != 0u ? 1u : 0u));
    if (rc != G3D_OK) return rc;
    if ((qk->compression & G3D_QK_LINEARTIME_COMPRESSION) != 0u)
    {
        if (qk->time_bits == NULL || qk->time_value_count < 2u)
        {
            return G3D_ERR_CORRUPT;
        }
        rc = g3d_wr_bits_dec6_chr(w, qk->time_bits[0], ' ');
        if (rc != G3D_OK) return rc;
        rc = g3d_wr_bits_dec6(w, qk->time_bits[1]);
        if (rc != G3D_OK) return rc;
        rc = g3d_wr_cstr(w, " Start T,Delta T\n");
        if (rc != G3D_OK) return rc;
    }
    if ((qk->compression & G3D_QK_HINGE_COMPRESSION) != 0u)
    {
        if (qk->count != 0u && qk->angle_bits == NULL)
        {
            return G3D_ERR_CORRUPT;
        }
        rc = g3d_wr_bits_dec6_chr(w, qk->hinge_axis.x_bits, ' ');
        if (rc != G3D_OK) return rc;
        rc = g3d_wr_bits_dec6_chr(w, qk->hinge_axis.y_bits, ' ');
        if (rc != G3D_OK) return rc;
        rc = g3d_wr_bits_dec6(w, qk->hinge_axis.z_bits);
        if (rc != G3D_OK) return rc;
        rc = g3d_wr_cstr(w, " Axis\n");
        if (rc != G3D_OK) return rc;
        for (i = 0u; i < qk->count; ++i)
        {
            if ((qk->compression & G3D_QK_LINEARTIME_COMPRESSION) == 0u)
            {
                if (qk->time_bits == NULL || i >= qk->time_value_count)
                {
                    return G3D_ERR_CORRUPT;
                }
                rc = g3d_wr_bits_dec6_chr(w, qk->time_bits[i], ' ');
                if (rc != G3D_OK) return rc;
            }
            rc = g3d_wr_bits_dec6(w, qk->angle_bits[i]);
            if (rc != G3D_OK) return rc;
            rc = g3d_wr_bytes(w, "\n", 1u);
            if (rc != G3D_OK) return rc;
        }
    }
    else
    {
        if (qk->count != 0u && qk->quats == NULL)
        {
            return G3D_ERR_CORRUPT;
        }
        for (i = 0u; i < qk->count; ++i)
        {
            if ((qk->compression & G3D_QK_LINEARTIME_COMPRESSION) == 0u)
            {
                if (qk->time_bits == NULL || i >= qk->time_value_count)
                {
                    return G3D_ERR_CORRUPT;
                }
                rc = g3d_wr_bits_dec6_chr(w, qk->time_bits[i], ' ');
                if (rc != G3D_OK) return rc;
            }
            rc = g3d_wr_bits_dec6_chr(w, qk->quats[i].w_bits, ' ');
            if (rc != G3D_OK) return rc;
            rc = g3d_wr_bits_dec6_chr(w, qk->quats[i].x_bits, ' ');
            if (rc != G3D_OK) return rc;
            rc = g3d_wr_bits_dec6_chr(w, qk->quats[i].y_bits, ' ');
            if (rc != G3D_OK) return rc;
            rc = g3d_wr_bits_dec6(w, qk->quats[i].z_bits);
            if (rc != G3D_OK) return rc;
            rc = g3d_wr_bytes(w, "\n", 1u);
            if (rc != G3D_OK) return rc;
        }
    }
    return G3D_OK;
}


static int g3d_write_vkframe_ascii_inner(const g3d_vkframe *vk, g3d_writer *w)
{
    g3d_u32 i;
    int rc;
    if (vk == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_wr_textf(w, "Keys %u %u %u %u\n", (unsigned)vk->count,
                      (unsigned)vk->interpolation, (unsigned)vk->compression,
                      (unsigned)(vk->looping != 0u ? 1u : 0u));
    if (rc != G3D_OK) return rc;
    if ((vk->compression & G3D_VK_LINEARTIME_COMPRESSION) != 0u)
    {
        if (vk->time_bits == NULL || vk->time_value_count < 2u)
        {
            return G3D_ERR_CORRUPT;
        }
        rc = g3d_wr_bits_dec6_chr(w, vk->time_bits[0], ' ');
        if (rc != G3D_OK) return rc;
        rc = g3d_wr_bits_dec6(w, vk->time_bits[1]);
        if (rc != G3D_OK) return rc;
        rc = g3d_wr_cstr(w, " Start T,Delta T\n");
        if (rc != G3D_OK) return rc;
    }
    if (vk->count != 0u && vk->vectors == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    for (i = 0u; i < vk->count; ++i)
    {
        if ((vk->compression & G3D_VK_LINEARTIME_COMPRESSION) == 0u)
        {
            if (vk->time_bits == NULL || i >= vk->time_value_count)
            {
                return G3D_ERR_CORRUPT;
            }
            rc = g3d_wr_bits_dec6_chr(w, vk->time_bits[i], ' ');
            if (rc != G3D_OK) return rc;
        }
        rc = g3d_wr_bits_dec6_chr(w, vk->vectors[i].x_bits, ' ');
        if (rc != G3D_OK) return rc;
        rc = g3d_wr_bits_dec6_chr(w, vk->vectors[i].y_bits, ' ');
        if (rc != G3D_OK) return rc;
        rc = g3d_wr_bits_dec6(w, vk->vectors[i].z_bits);
        if (rc != G3D_OK) return rc;
        rc = g3d_wr_bytes(w, "\n", 1u);
        if (rc != G3D_OK) return rc;
    }
    return G3D_OK;
}


static int g3d_write_path_ascii_inner(const g3d_path *path, g3d_writer *w)
{
    int rc;
    if (path == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (g3d_wr_u32le(w, G3D_PATH_ASCII) != G3D_OK) return G3D_ERR_NO_SPACE;
    rc = g3d_wr_textf(w, " %X.%.2X\n", (unsigned)((G3D_PATH_ASCII_VER2 & 0xff00u) >> 8), (unsigned)(G3D_PATH_ASCII_VER2 & 0xffu));
    if (rc != G3D_OK) return rc;
    rc = g3d_wr_textf(w, "Rotation %u %u\n", (unsigned)(path->has_rotation != 0u ? 1u : 0u), (unsigned)path->rotation_interp);
    if (rc != G3D_OK) return rc;
    if (path->has_rotation != 0u)
    {
        rc = g3d_write_qkframe_ascii_inner(&path->rotation, w);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    rc = g3d_wr_textf(w, "Translation %u %u\n", (unsigned)(path->has_translation != 0u ? 1u : 0u), (unsigned)path->translation_interp);
    if (rc != G3D_OK) return rc;
    if (path->has_translation != 0u)
    {
        rc = g3d_write_vkframe_ascii_inner(&path->translation, w);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    return G3D_OK;
}

static int g3d_write_motion_ascii_inner(const g3d_motion *motion, g3d_writer *w)
{
    g3d_u32 i;
    g3d_s32 write_name_checksum;
    int rc;
    int has_events;
    int has_name_array;
    if (motion == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_motion_validate(motion);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (motion->node_type == G3D_MOTION_NODE_BRANCH)
    {
        return G3D_ERR_UNSUPPORTED;
    }
    if (g3d_wr_u32le(w, G3D_MOT_ASCII) != G3D_OK) return G3D_ERR_NO_SPACE;
    rc = g3d_wr_textf(w, " %X.%.2X\n", (unsigned)((G3D_MOT_VER & 0xff00u) >> 8), (unsigned)(G3D_MOT_VER & 0xffu));
    if (rc != G3D_OK) return rc;
    rc = g3d_wr_textf(w, "NameID %s\n", (motion->name != NULL) ? motion->name : "");
    if (rc != G3D_OK) return rc;
    rc = g3d_wr_textf(w, "MaintainNames %u\n", (unsigned)(motion->maintain_names != 0u ? 1u : 0u));
    if (rc != G3D_OK) return rc;
    if (motion->node_type == G3D_MOTION_NODE_UNDECIDED)
    {
        return G3D_OK;
    }
    rc = g3d_wr_textf(w, "PathCount %d\n", (int)motion->leaf.path_count);
    if (rc != G3D_OK) return rc;
    write_name_checksum = motion->leaf.name_checksum;
    if (write_name_checksum == 0 && motion->leaf.name_array != NULL)
    {
        write_name_checksum = g3d_strblock_checksum(motion->leaf.name_array);
    }
    rc = g3d_wr_textf(w, "NameChecksum %d\n", (int)write_name_checksum);
    if (rc != G3D_OK) return rc;
    has_events = (((motion->leaf.flags & 0x1u) != 0u) && motion->leaf.events != NULL) ? 1 : 0;
    rc = g3d_wr_textf(w, "Events %d\n", has_events);
    if (rc != G3D_OK) return rc;
    if (has_events != 0)
    {
        rc = g3d_write_tkevents_ascii_inner(motion->leaf.events, w);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    has_name_array = (((motion->leaf.flags & 0x2u) != 0u) && motion->leaf.name_array != NULL) ? 1 : 0;
    rc = g3d_wr_textf(w, "NameArray %d\n", has_name_array);
    if (rc != G3D_OK) return rc;
    if (has_name_array != 0)
    {
        rc = g3d_write_strblock_ascii_inner(motion->leaf.name_array, w);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    rc = g3d_wr_textf(w, "PathArray %d\n", (int)motion->leaf.path_count);
    if (rc != G3D_OK) return rc;
    for (i = 0u; i < (g3d_u32)motion->leaf.path_count; ++i)
    {
        rc = g3d_write_path_ascii_inner(&motion->leaf.paths[i], w);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    return G3D_OK;
}

static int g3d_write_motion_inner(const g3d_motion *motion, g3d_writer *w)
{
    g3d_u32 packed;
    g3d_u32 i;
    int rc;
    g3d_s32 write_name_checksum;
    g3d_u16 name_len;
    if (motion == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_motion_validate(motion);
    if (rc != G3D_OK)
    {
        return rc;
    }

    if (motion->name != NULL)
    {
        name_len = (g3d_u16)(motion->name_len != 0u ? motion->name_len : (g3d_u16)(g3d_strlen32(motion->name) + 1u));
    }
    else
    {
        name_len = 0u;
    }

    if (motion->node_type == G3D_MOTION_NODE_BRANCH)
    {
        return G3D_ERR_UNSUPPORTED;
    }

    packed = (g3d_u32)name_len;
    if (motion->maintain_names != 0u)
    {
        packed |= (1u << 16);
    }
    packed |= ((g3d_u32)motion->node_type << 24);

    if (g3d_wr_u32le(w, G3D_MOT_BIN) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, G3D_MOT_VER) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, packed) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (name_len != 0u)
    {
        if (g3d_wr_bytes(w, motion->name, (g3d_u32)name_len) != G3D_OK) return G3D_ERR_NO_SPACE;
    }

    if (motion->node_type == G3D_MOTION_NODE_UNDECIDED)
    {
        return G3D_OK;
    }

    write_name_checksum = motion->leaf.name_checksum;
    if (write_name_checksum == 0 && motion->leaf.name_array != NULL)
    {
        write_name_checksum = g3d_strblock_checksum(motion->leaf.name_array);
    }

    if (g3d_wr_s32le(w, motion->leaf.path_count) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_s32le(w, write_name_checksum) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, motion->leaf.flags) != G3D_OK) return G3D_ERR_NO_SPACE;

    if ((motion->leaf.flags & 0x1u) != 0u)
    {
        rc = g3d_write_tkevents_inner(motion->leaf.events, w);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    if ((motion->leaf.flags & 0x2u) != 0u)
    {
        rc = g3d_write_strblock_inner(motion->leaf.name_array, w);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    for (i = 0u; i < (g3d_u32)motion->leaf.path_count; ++i)
    {
        rc = g3d_write_path_inner(&motion->leaf.paths[i], w);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    return G3D_OK;
}

int g3d_motion_write_binary(const g3d_motion *motion, void *dst, g3d_u32 cap, g3d_u32 *used_bytes)
{
    g3d_writer w;
    int rc;
    g3d_writer_init(&w, dst, cap);
    rc = g3d_write_motion_inner(motion, &w);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = w.pos;
    }
    return G3D_OK;
}

int g3d_motion_measure_binary(const g3d_motion *motion, g3d_u32 *size_out)
{
    g3d_u32 used;
    int rc;
    rc = g3d_motion_write_binary(motion, NULL, 0u, &used);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *size_out = used;
    return G3D_OK;
}


int g3d_motion_write_ascii(const g3d_motion *motion, void *dst, g3d_u32 cap, g3d_u32 *used_bytes)
{
    g3d_writer w;
    int rc;
    g3d_writer_init(&w, dst, cap);
    rc = g3d_write_motion_ascii_inner(motion, &w);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = w.pos;
    }
    return G3D_OK;
}

int g3d_motion_measure_ascii(const g3d_motion *motion, g3d_u32 *size_out)
{
    g3d_u32 used;
    int rc;
    if (size_out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_motion_write_ascii(motion, NULL, 0u, &used);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *size_out = used;
    return G3D_OK;
}

static int g3d_body_read_vec3(g3d_reader *r, g3d_fvec3 *v)
{
    int rc;
    rc = g3d_rd_u32le(r, &v->x_bits); if (rc != G3D_OK) return rc;
    rc = g3d_rd_u32le(r, &v->y_bits); if (rc != G3D_OK) return rc;
    rc = g3d_rd_u32le(r, &v->z_bits); if (rc != G3D_OK) return rc;
    return G3D_OK;
}

static int g3d_body_write_vec3(g3d_writer *w, const g3d_fvec3 *v)
{
    if (g3d_wr_u32le(w, v->x_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, v->y_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, v->z_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
    return G3D_OK;
}

static int g3d_body_read_xform(g3d_reader *r, g3d_fxform *xf)
{
    g3d_u32 i;
    int rc;
    for (i = 0u; i < 12u; ++i)
    {
        rc = g3d_rd_u32le(r, &xf->f[i]);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    return G3D_OK;
}

static int g3d_body_write_xform(g3d_writer *w, const g3d_fxform *xf)
{
    g3d_u32 i;
    for (i = 0u; i < 12u; ++i)
    {
        if (g3d_wr_u32le(w, xf->f[i]) != G3D_OK)
        {
            return G3D_ERR_NO_SPACE;
        }
    }
    return G3D_OK;
}

static int g3d_body_parse_geometry_inner(g3d_body *body, g3d_reader *r, g3d_arena *arena)
{
    g3d_u32 sig;
    g3d_u32 ver;
    g3d_u32 i;
    g3d_u32 lod;
    int rc;

    rc = g3d_rd_u32le(r, &sig);
    if (rc != G3D_OK) return rc;
    if (sig != G3D_BODY_SIG) return G3D_ERR_BAD_MAGIC;
    rc = g3d_rd_u32le(r, &ver);
    if (rc != G3D_OK) return rc;
    if (ver != G3D_BODY_VER) return G3D_ERR_BAD_VERSION;

    rc = g3d_body_read_vec3(r, &body->bbox_min); if (rc != G3D_OK) return rc;
    rc = g3d_body_read_vec3(r, &body->bbox_max); if (rc != G3D_OK) return rc;

    rc = g3d_rd_s16le(r, &body->xskin_vertex_count); if (rc != G3D_OK) return rc;
    if (body->xskin_vertex_count < 0) return G3D_ERR_CORRUPT;
    if (body->xskin_vertex_count != 0)
    {
        body->xskin_vertices = (g3d_body_xskin_vertex *)g3d_arena_alloc(arena, (g3d_u32)body->xskin_vertex_count * (g3d_u32)sizeof(g3d_body_xskin_vertex), 4u);
        if (body->xskin_vertices == NULL) return G3D_ERR_ARENA_EXHAUSTED;
        for (i = 0u; i < (g3d_u32)body->xskin_vertex_count; ++i)
        {
            rc = g3d_body_read_vec3(r, &body->xskin_vertices[i].xpoint); if (rc != G3D_OK) return rc;
            rc = g3d_rd_u32le(r, &body->xskin_vertices[i].u_bits); if (rc != G3D_OK) return rc;
            rc = g3d_rd_u32le(r, &body->xskin_vertices[i].v_bits); if (rc != G3D_OK) return rc;
            rc = g3d_rd_u8(r, &body->xskin_vertices[i].lod_mask); if (rc != G3D_OK) return rc;
            rc = g3d_rd_u8(r, &body->xskin_vertices[i].pad0); if (rc != G3D_OK) return rc;
            rc = g3d_rd_s16le(r, &body->xskin_vertices[i].bone_index); if (rc != G3D_OK) return rc;
        }
    }

    rc = g3d_rd_s16le(r, &body->normal_count); if (rc != G3D_OK) return rc;
    if (body->normal_count < 0) return G3D_ERR_CORRUPT;
    if (body->normal_count != 0)
    {
        body->normals = (g3d_body_normal *)g3d_arena_alloc(arena, (g3d_u32)body->normal_count * (g3d_u32)sizeof(g3d_body_normal), 4u);
        if (body->normals == NULL) return G3D_ERR_ARENA_EXHAUSTED;
        for (i = 0u; i < (g3d_u32)body->normal_count; ++i)
        {
            rc = g3d_body_read_vec3(r, &body->normals[i].normal); if (rc != G3D_OK) return rc;
            rc = g3d_rd_u8(r, &body->normals[i].lod_mask); if (rc != G3D_OK) return rc;
            rc = g3d_rd_u8(r, &body->normals[i].pad0); if (rc != G3D_OK) return rc;
            rc = g3d_rd_s16le(r, &body->normals[i].bone_index); if (rc != G3D_OK) return rc;
        }
    }

    rc = g3d_rd_s16le(r, &body->bone_count); if (rc != G3D_OK) return rc;
    if (body->bone_count < 0) return G3D_ERR_CORRUPT;
    if (body->bone_count != 0)
    {
        body->bones = (g3d_body_bone *)g3d_arena_alloc(arena, (g3d_u32)body->bone_count * (g3d_u32)sizeof(g3d_body_bone), 4u);
        if (body->bones == NULL) return G3D_ERR_ARENA_EXHAUSTED;
        for (i = 0u; i < (g3d_u32)body->bone_count; ++i)
        {
            rc = g3d_body_read_vec3(r, &body->bones[i].bbox_min); if (rc != G3D_OK) return rc;
            rc = g3d_body_read_vec3(r, &body->bones[i].bbox_max); if (rc != G3D_OK) return rc;
            rc = g3d_body_read_xform(r, &body->bones[i].attachment); if (rc != G3D_OK) return rc;
            rc = g3d_rd_s16le(r, &body->bones[i].parent_bone_index); if (rc != G3D_OK) return rc;
            rc = g3d_rd_u16le(r, &body->bones[i].pad0); if (rc != G3D_OK) return rc;
        }
    }

    body->bone_names = (g3d_strblock *)g3d_arena_alloc(arena, (g3d_u32)sizeof(g3d_strblock), 4u);
    if (body->bone_names == NULL) return G3D_ERR_ARENA_EXHAUSTED;
    g3d_zero(body->bone_names, (g3d_u32)sizeof(g3d_strblock));
    rc = g3d_parse_strblock_inner(body->bone_names, r, arena); if (rc != G3D_OK) return rc;

    rc = g3d_rd_s16le(r, &body->material_count); if (rc != G3D_OK) return rc;
    if (body->material_count < 0) return G3D_ERR_CORRUPT;
    if (body->material_count != 0)
    {
        body->materials = (g3d_body_material *)g3d_arena_alloc(arena, (g3d_u32)body->material_count * (g3d_u32)sizeof(g3d_body_material), 4u);
        if (body->materials == NULL) return G3D_ERR_ARENA_EXHAUSTED;
        g3d_zero(body->materials, (g3d_u32)body->material_count * (g3d_u32)sizeof(g3d_body_material));
        for (i = 0u; i < (g3d_u32)body->material_count; ++i)
        {
            g3d_u32 bmp_flag;
            rc = g3d_rd_u32le(r, &bmp_flag); if (rc != G3D_OK) return rc;
            body->materials[i].has_bitmap = (g3d_u8)(bmp_flag != 0u ? 1u : 0u);
            rc = g3d_rd_u32le(r, &body->materials[i].red_bits); if (rc != G3D_OK) return rc;
            rc = g3d_rd_u32le(r, &body->materials[i].green_bits); if (rc != G3D_OK) return rc;
            rc = g3d_rd_u32le(r, &body->materials[i].blue_bits); if (rc != G3D_OK) return rc;
        }
    }

    body->material_names = (g3d_strblock *)g3d_arena_alloc(arena, (g3d_u32)sizeof(g3d_strblock), 4u);
    if (body->material_names == NULL) return G3D_ERR_ARENA_EXHAUSTED;
    g3d_zero(body->material_names, (g3d_u32)sizeof(g3d_strblock));
    rc = g3d_parse_strblock_inner(body->material_names, r, arena); if (rc != G3D_OK) return rc;

    rc = g3d_rd_u32le(r, &body->lod_count); if (rc != G3D_OK) return rc;
    if (body->lod_count > G3D_BODY_MAX_LOD) return G3D_ERR_CORRUPT;

    for (lod = 0u; lod < body->lod_count; ++lod)
    {
        rc = g3d_rd_u32le(r, &body->lods[lod].face_count); if (rc != G3D_OK) return rc;
        if (body->lods[lod].face_count != 0u)
        {
            body->lods[lod].faces = (g3d_body_triangle *)g3d_arena_alloc(arena, body->lods[lod].face_count * (g3d_u32)sizeof(g3d_body_triangle), 2u);
            if (body->lods[lod].faces == NULL) return G3D_ERR_ARENA_EXHAUSTED;
            for (i = 0u; i < body->lods[lod].face_count; ++i)
            {
                g3d_u32 j;
                for (j = 0u; j < 3u; ++j)
                {
                    rc = g3d_rd_s16le(r, &body->lods[lod].faces[i].vtx_index[j]); if (rc != G3D_OK) return rc;
                }
                for (j = 0u; j < 3u; ++j)
                {
                    rc = g3d_rd_s16le(r, &body->lods[lod].faces[i].normal_index[j]); if (rc != G3D_OK) return rc;
                }
                rc = g3d_rd_s16le(r, &body->lods[lod].faces[i].material_index); if (rc != G3D_OK) return rc;
            }
        }
    }

    return G3D_OK;
}

static int g3d_write_body_geometry_inner(const g3d_body *body, g3d_writer *w)
{
    g3d_u32 i;
    g3d_u32 lod;
    int rc;

    if (body == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_body_validate(body);
    if (rc != G3D_OK)
    {
        return rc;
    }

    if (g3d_wr_u32le(w, G3D_BODY_SIG) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, G3D_BODY_VER) != G3D_OK) return G3D_ERR_NO_SPACE;

    rc = g3d_body_write_vec3(w, &body->bbox_min); if (rc != G3D_OK) return rc;
    rc = g3d_body_write_vec3(w, &body->bbox_max); if (rc != G3D_OK) return rc;

    if (g3d_wr_s16le(w, body->xskin_vertex_count) != G3D_OK) return G3D_ERR_NO_SPACE;
    for (i = 0u; i < (g3d_u32)body->xskin_vertex_count; ++i)
    {
        rc = g3d_body_write_vec3(w, &body->xskin_vertices[i].xpoint); if (rc != G3D_OK) return rc;
        if (g3d_wr_u32le(w, body->xskin_vertices[i].u_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
        if (g3d_wr_u32le(w, body->xskin_vertices[i].v_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
        if (g3d_wr_u8(w, body->xskin_vertices[i].lod_mask) != G3D_OK) return G3D_ERR_NO_SPACE;
        if (g3d_wr_u8(w, body->xskin_vertices[i].pad0) != G3D_OK) return G3D_ERR_NO_SPACE;
        if (g3d_wr_s16le(w, body->xskin_vertices[i].bone_index) != G3D_OK) return G3D_ERR_NO_SPACE;
    }

    if (g3d_wr_s16le(w, body->normal_count) != G3D_OK) return G3D_ERR_NO_SPACE;
    for (i = 0u; i < (g3d_u32)body->normal_count; ++i)
    {
        rc = g3d_body_write_vec3(w, &body->normals[i].normal); if (rc != G3D_OK) return rc;
        if (g3d_wr_u8(w, body->normals[i].lod_mask) != G3D_OK) return G3D_ERR_NO_SPACE;
        if (g3d_wr_u8(w, body->normals[i].pad0) != G3D_OK) return G3D_ERR_NO_SPACE;
        if (g3d_wr_s16le(w, body->normals[i].bone_index) != G3D_OK) return G3D_ERR_NO_SPACE;
    }

    if (g3d_wr_s16le(w, body->bone_count) != G3D_OK) return G3D_ERR_NO_SPACE;
    for (i = 0u; i < (g3d_u32)body->bone_count; ++i)
    {
        rc = g3d_body_write_vec3(w, &body->bones[i].bbox_min); if (rc != G3D_OK) return rc;
        rc = g3d_body_write_vec3(w, &body->bones[i].bbox_max); if (rc != G3D_OK) return rc;
        rc = g3d_body_write_xform(w, &body->bones[i].attachment); if (rc != G3D_OK) return rc;
        if (g3d_wr_s16le(w, body->bones[i].parent_bone_index) != G3D_OK) return G3D_ERR_NO_SPACE;
        if (g3d_wr_u16le(w, body->bones[i].pad0) != G3D_OK) return G3D_ERR_NO_SPACE;
    }

    rc = g3d_write_strblock_inner(body->bone_names, w); if (rc != G3D_OK) return rc;

    if (g3d_wr_s16le(w, body->material_count) != G3D_OK) return G3D_ERR_NO_SPACE;
    for (i = 0u; i < (g3d_u32)body->material_count; ++i)
    {
        if (g3d_wr_u32le(w, body->materials[i].has_bitmap ? 1u : 0u) != G3D_OK) return G3D_ERR_NO_SPACE;
        if (g3d_wr_u32le(w, body->materials[i].red_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
        if (g3d_wr_u32le(w, body->materials[i].green_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
        if (g3d_wr_u32le(w, body->materials[i].blue_bits) != G3D_OK) return G3D_ERR_NO_SPACE;
    }

    rc = g3d_write_strblock_inner(body->material_names, w); if (rc != G3D_OK) return rc;

    if (g3d_wr_u32le(w, body->lod_count) != G3D_OK) return G3D_ERR_NO_SPACE;
    for (lod = 0u; lod < body->lod_count; ++lod)
    {
        if (g3d_wr_u32le(w, body->lods[lod].face_count) != G3D_OK) return G3D_ERR_NO_SPACE;
        for (i = 0u; i < body->lods[lod].face_count; ++i)
        {
            g3d_u32 j;
            for (j = 0u; j < 3u; ++j)
            {
                if (g3d_wr_s16le(w, body->lods[lod].faces[i].vtx_index[j]) != G3D_OK) return G3D_ERR_NO_SPACE;
            }
            for (j = 0u; j < 3u; ++j)
            {
                if (g3d_wr_s16le(w, body->lods[lod].faces[i].normal_index[j]) != G3D_OK) return G3D_ERR_NO_SPACE;
            }
            if (g3d_wr_s16le(w, body->lods[lod].faces[i].material_index) != G3D_OK) return G3D_ERR_NO_SPACE;
        }
    }

    return G3D_OK;
}

int g3d_body_write_geometry(const g3d_body *body, void *dst, g3d_u32 cap, g3d_u32 *used_bytes)
{
    g3d_writer w;
    int rc;
    g3d_writer_init(&w, dst, cap);
    rc = g3d_write_body_geometry_inner(body, &w);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = w.pos;
    }
    return G3D_OK;
}

int g3d_body_measure_geometry(const g3d_body *body, g3d_u32 *size_out)
{
    g3d_u32 used;
    int rc;
    rc = g3d_body_write_geometry(body, NULL, 0u, &used);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *size_out = used;
    return G3D_OK;
}

static int g3d_dir_begin(g3d_writer *w, g3d_u32 *start_pos)
{
    *start_pos = w->pos;
    if (g3d_wr_u32le(w, 0u) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, 0u) != G3D_OK) return G3D_ERR_NO_SPACE;
    return G3D_OK;
}

static int g3d_dir_finish(g3d_writer *w, g3d_u32 start_pos)
{
    g3d_u32 end_pos;
    if (w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    end_pos = w->pos;
    if (g3d_patch_u32le(w, start_pos + 0u, G3D_DIRTREE_SIG) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_patch_u32le(w, start_pos + 4u, end_pos - start_pos) != G3D_OK) return G3D_ERR_NO_SPACE;
    return G3D_OK;
}

static int g3d_dir_write_term(g3d_writer *w)
{
    return (g3d_wr_u32le(w, G3D_DIRTREE_LIST_TERMINATED) == G3D_OK) ? G3D_OK : G3D_ERR_NO_SPACE;
}

static int g3d_dir_write_node_header(g3d_writer *w,
    const char *name,
    g3d_u32 attributes,
    g3d_u32 size,
    g3d_u32 offset,
    const g3d_u8 *hint,
    g3d_u32 hint_len)
{
    g3d_u32 name_len;
    if (w == NULL || name == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    name_len = g3d_strlen32(name) + 1u;
    if (g3d_wr_u32le(w, G3D_DIRTREE_LIST_NOTTERMINATED) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_s32le(w, (g3d_s32)name_len) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_bytes(w, name, name_len) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, 0u) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, 0u) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, attributes) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_s32le(w, (g3d_s32)size) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_s32le(w, (g3d_s32)offset) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_s32le(w, (g3d_s32)hint_len) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_bytes(w, hint, hint_len) != G3D_OK) return G3D_ERR_NO_SPACE;
    return G3D_OK;
}


static int g3d_body_write_bitmaps_dirtree(g3d_writer *w, const g3d_body *body, g3d_u32 first_bitmap_offset)
{
    g3d_u32 i;
    g3d_u32 offset;
    int found_any;
    offset = first_bitmap_offset;
    found_any = G3D_FALSE;

    for (i = 0u; i < (g3d_u32)body->material_count; ++i)
    {
        char name_buf[32];
        int rc;
        int has_next;
        g3d_u32 j;

        if (body->materials[i].has_bitmap == 0u)
        {
            continue;
        }

        found_any = G3D_TRUE;
        sprintf(name_buf, "%u", (unsigned)i);
        rc = g3d_dir_write_node_header(w, name_buf, 0u,
            body->materials[i].bitmap_size,
            offset,
            NULL, 0u);
        if (rc != G3D_OK)
        {
            return rc;
        }
        rc = g3d_dir_write_term(w);
        if (rc != G3D_OK)
        {
            return rc;
        }

        offset += body->materials[i].bitmap_size;
        has_next = G3D_FALSE;
        for (j = i + 1u; j < (g3d_u32)body->material_count; ++j)
        {
            if (body->materials[j].has_bitmap != 0u)
            {
                has_next = G3D_TRUE;
                break;
            }
        }
        if (has_next == G3D_FALSE)
        {
            rc = g3d_dir_write_term(w);
            if (rc != G3D_OK)
            {
                return rc;
            }
            return G3D_OK;
        }
    }

    if (found_any == G3D_FALSE)
    {
        return g3d_dir_write_term(w);
    }
    return G3D_OK;
}

static int g3d_body_write_file_inner(const g3d_body *body, g3d_writer *w, g3d_u8 xor_mask)
{
    g3d_u32 vfs_header_pos;
    g3d_u32 geometry_offset;
    g3d_u32 geometry_size;
    g3d_u32 bitmaps_offset;
    g3d_u32 bitmap_pos;
    g3d_u32 data_length;
    g3d_u32 dir_start;
    g3d_u32 end_pos;
    g3d_u32 base_pos;
    g3d_u32 i;
    int rc;

    if (body == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }

    if (g3d_body_measure_geometry(body, &geometry_size) != G3D_OK)
    {
        return G3D_ERR_CORRUPT;
    }

    vfs_header_pos = w->pos;
    base_pos = vfs_header_pos;
    if (g3d_wr_bytes(w, NULL, 24u) != G3D_OK) return G3D_ERR_NO_SPACE;

    geometry_offset = w->pos - base_pos;
    rc = g3d_write_body_geometry_inner(body, w);
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_xor_writer_range(w, base_pos + geometry_offset, geometry_size, xor_mask);
    if (rc != G3D_OK)
    {
        return rc;
    }

    bitmaps_offset = w->pos - base_pos;

    for (i = 0u; i < (g3d_u32)body->material_count; ++i)
    {
        if (body->materials[i].has_bitmap != 0u)
        {
            bitmap_pos = w->pos;
            if (g3d_wr_bytes(w, body->materials[i].bitmap_data, body->materials[i].bitmap_size) != G3D_OK)
            {
                return G3D_ERR_NO_SPACE;
            }
            rc = g3d_xor_writer_range(w, bitmap_pos, body->materials[i].bitmap_size, xor_mask);
            if (rc != G3D_OK)
            {
                return rc;
            }
        }
    }

    data_length = w->pos - base_pos;
    rc = g3d_dir_begin(w, &dir_start);
    if (rc != G3D_OK) return rc;

    /* Genesis3D DirTree serializes one explicit empty-name root directory.
       Its children are the public VFS entries (Geometry/Bitmaps). */
    rc = g3d_dir_write_node_header(w, "", G3D_VFILE_ATTRIB_DIRECTORY, 0u, 0u, NULL, 0u);
    if (rc != G3D_OK) return rc;

    rc = g3d_dir_write_node_header(w, G3D_BODY_GEOM, 0u, geometry_size, geometry_offset, NULL, 0u);
    if (rc != G3D_OK) return rc;
    rc = g3d_dir_write_term(w);
    if (rc != G3D_OK) return rc;

    rc = g3d_dir_write_node_header(w, G3D_BODY_BITMAPS, G3D_VFILE_ATTRIB_DIRECTORY, 0u, 0u, NULL, 0u);
    if (rc != G3D_OK) return rc;
    rc = g3d_body_write_bitmaps_dirtree(w, body, bitmaps_offset);
    if (rc != G3D_OK) return rc;
    rc = g3d_dir_write_term(w);
    if (rc != G3D_OK) return rc;

    /* terminator for the explicit root's sibling list */
    rc = g3d_dir_write_term(w);
    if (rc != G3D_OK) return rc;

    rc = g3d_dir_finish(w, dir_start);
    if (rc != G3D_OK) return rc;

    end_pos = w->pos - base_pos;

    if (g3d_patch_u32le(w, vfs_header_pos + 0u, (xor_mask != 0u) ? G3D_VFS_SIG_CF00 : G3D_VFS_SIG_VF00) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (w->measure_only == 0)
    {
        w->buf[vfs_header_pos + 4u] = 0u;
        w->buf[vfs_header_pos + 5u] = 0u;
        w->buf[vfs_header_pos + 6u] = 0u;
        w->buf[vfs_header_pos + 7u] = 0u;
    }
    if (g3d_patch_u32le(w, vfs_header_pos + 8u, 0u) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_patch_u32le(w, vfs_header_pos + 12u, data_length) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_patch_u32le(w, vfs_header_pos + 16u, data_length) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_patch_u32le(w, vfs_header_pos + 20u, end_pos) != G3D_OK) return G3D_ERR_NO_SPACE;

    return G3D_OK;
}

int g3d_body_write_file(const g3d_body *body, void *dst, g3d_u32 cap, g3d_u32 *used_bytes)
{
    return g3d_body_write_file_with_mask(body, dst, cap, used_bytes, 0u);
}

int g3d_body_write_file_with_mask(const g3d_body *body, void *dst, g3d_u32 cap, g3d_u32 *used_bytes, g3d_u8 xor_mask)
{
    g3d_writer w;
    int rc;
    g3d_writer_init(&w, dst, cap);
    rc = g3d_body_write_file_inner(body, &w, xor_mask);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = w.pos;
    }
    return G3D_OK;
}

int g3d_body_measure_file(const g3d_body *body, g3d_u32 *size_out)
{
    g3d_u32 used;
    int rc;
    rc = g3d_body_write_file(body, NULL, 0u, &used);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *size_out = used;
    return G3D_OK;
}

static int g3d_vfs_parse_entry(g3d_vfs *vfs, g3d_reader *r, g3d_arena *arena, g3d_vfs_entry **out_entry)
{
    g3d_u32 term;
    g3d_vfs_entry *e;
    int rc;
    g3d_s32 name_len;
    g3d_s32 size;
    g3d_s32 offset;
    g3d_s32 hint_len;

    rc = g3d_rd_u32le(r, &term);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (term == G3D_DIRTREE_LIST_TERMINATED)
    {
        *out_entry = NULL;
        return G3D_OK;
    }
    if (term != G3D_DIRTREE_LIST_NOTTERMINATED)
    {
        return G3D_ERR_CORRUPT;
    }

    e = (g3d_vfs_entry *)g3d_arena_alloc(arena, (g3d_u32)sizeof(g3d_vfs_entry), 4u);
    if (e == NULL)
    {
        return G3D_ERR_ARENA_EXHAUSTED;
    }
    g3d_zero(e, (g3d_u32)sizeof(*e));

    rc = g3d_rd_s32le(r, &name_len);
    if (rc != G3D_OK) return rc;
    if (name_len <= 0) return G3D_ERR_CORRUPT;
    e->name_len = (g3d_u32)name_len;
    e->name = (char *)g3d_arena_alloc(arena, e->name_len, 1u);
    if (e->name == NULL) return G3D_ERR_ARENA_EXHAUSTED;
    rc = g3d_rd_bytes(r, e->name, e->name_len);
    if (rc != G3D_OK) return rc;

    rc = g3d_rd_u32le(r, &e->time1); if (rc != G3D_OK) return rc;
    rc = g3d_rd_u32le(r, &e->time2); if (rc != G3D_OK) return rc;
    rc = g3d_rd_u32le(r, &e->attributes); if (rc != G3D_OK) return rc;
    rc = g3d_rd_s32le(r, &size); if (rc != G3D_OK) return rc;
    rc = g3d_rd_s32le(r, &offset); if (rc != G3D_OK) return rc;
    rc = g3d_rd_s32le(r, &hint_len); if (rc != G3D_OK) return rc;

    e->size = size;
    e->offset = offset;
    e->hint_len = (hint_len < 0) ? 0u : (g3d_u32)hint_len;

    if (hint_len < 0)
    {
        return G3D_ERR_CORRUPT;
    }
    if (e->hint_len != 0u)
    {
        if (e->hint_len > (r->size - r->pos))
        {
            return G3D_ERR_EOF;
        }
        e->hint_data = (g3d_u8 *)g3d_arena_alloc(arena, e->hint_len, 1u);
        if (e->hint_data == NULL) return G3D_ERR_ARENA_EXHAUSTED;
        rc = g3d_rd_bytes(r, e->hint_data, e->hint_len);
        if (rc != G3D_OK) return rc;
    }

    rc = g3d_vfs_parse_entry(vfs, r, arena, &e->child);
    if (rc != G3D_OK) return rc;
    rc = g3d_vfs_parse_entry(vfs, r, arena, &e->sibling);
    if (rc != G3D_OK) return rc;

    if ((e->attributes & G3D_VFILE_ATTRIB_DIRECTORY) == 0u)
    {
        if (e->size < 0 || e->offset < 0)
        {
            return G3D_ERR_CORRUPT;
        }
        if ((g3d_u32)e->offset > vfs->data_length || (g3d_u32)e->size > (vfs->data_length - (g3d_u32)e->offset))
        {
            return G3D_ERR_CORRUPT;
        }
        if (vfs->encrypted != 0u)
        {
            g3d_u8 *copy;
            g3d_u32 i;
            copy = (g3d_u8 *)g3d_arena_alloc(arena, (g3d_u32)e->size, 1u);
            if (copy == NULL && e->size != 0)
            {
                return G3D_ERR_ARENA_EXHAUSTED;
            }
            for (i = 0u; i < (g3d_u32)e->size; ++i)
            {
                copy[i] = (g3d_u8)(vfs->bytes[(g3d_u32)e->offset + i] ^ vfs->xor_mask);
            }
            e->payload = copy;
        }
        else
        {
            e->payload = vfs->bytes + (g3d_u32)e->offset;
        }
    }

    *out_entry = e;
    return G3D_OK;
}

static int g3d_vfs_parse_inner(g3d_vfs *vfs, void *bytes, g3d_u32 size, g3d_arena *arena, g3d_u8 xor_mask)
{
    g3d_reader r;
    g3d_u16 version16;
    g3d_u16 pad16;
    int rc;
    g3d_u32 dirtree_sig;
    g3d_u32 dirtree_size;

    if (vfs == NULL || bytes == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }

    g3d_zero(vfs, (g3d_u32)sizeof(*vfs));
    vfs->bytes = (g3d_u8 *)bytes;
    vfs->size = size;

    g3d_reader_init(&r, bytes, size);
    rc = g3d_rd_u32le(&r, &vfs->signature);
    if (rc != G3D_OK) return rc;
    if (vfs->signature == G3D_VFS_SIG_CF00)
    {
        if (xor_mask == 0u)
        {
            return G3D_ERR_UNSUPPORTED;
        }
        vfs->encrypted = 1u;
        vfs->xor_mask = xor_mask;
    }
    else if (vfs->signature == G3D_VFS_SIG_VF00)
    {
        vfs->encrypted = 0u;
        vfs->xor_mask = 0u;
    }
    else
    {
        return G3D_ERR_BAD_MAGIC;
    }
    rc = g3d_rd_u16le(&r, &version16); if (rc != G3D_OK) return rc;
    rc = g3d_rd_u16le(&r, &pad16); if (rc != G3D_OK) return rc;
    (void)pad16;
    vfs->version = version16;
    if (vfs->version != 0u)
    {
        return G3D_ERR_BAD_VERSION;
    }
    rc = g3d_rd_u32le(&r, &vfs->dispersed); if (rc != G3D_OK) return rc;
    if (vfs->dispersed != 0u)
    {
        return G3D_ERR_UNSUPPORTED;
    }
    rc = g3d_rd_u32le(&r, &vfs->directory_offset); if (rc != G3D_OK) return rc;
    rc = g3d_rd_u32le(&r, &vfs->data_length); if (rc != G3D_OK) return rc;
    rc = g3d_rd_u32le(&r, &vfs->end_position); if (rc != G3D_OK) return rc;

    if (vfs->directory_offset > size || vfs->end_position > size)
    {
        return G3D_ERR_CORRUPT;
    }

    g3d_reader_init(&r, bytes, size);
    r.pos = vfs->directory_offset;

    rc = g3d_rd_u32le(&r, &dirtree_sig); if (rc != G3D_OK) return rc;
    rc = g3d_rd_u32le(&r, &dirtree_size); if (rc != G3D_OK) return rc;
    if (dirtree_sig != G3D_DIRTREE_SIG)
    {
        return G3D_ERR_BAD_MAGIC;
    }
    if (vfs->directory_offset + dirtree_size > size)
    {
        return G3D_ERR_CORRUPT;
    }

    rc = g3d_vfs_parse_entry(vfs, &r, arena, &vfs->root);
    if (rc != G3D_OK)
    {
        return rc;
    }
    return G3D_OK;
}

int g3d_vfs_parse(g3d_vfs *vfs, void *bytes, g3d_u32 size, g3d_arena *arena)
{
    return g3d_vfs_parse_inner(vfs, bytes, size, arena, 0u);
}

int g3d_vfs_parse_with_mask(g3d_vfs *vfs, void *bytes, g3d_u32 size, g3d_arena *arena, g3d_u8 xor_mask)
{
    return g3d_vfs_parse_inner(vfs, bytes, size, arena, xor_mask);
}

static g3d_vfs_entry *g3d_vfs_find_in_list(g3d_vfs_entry *entry, const char *name)
{
    while (entry != NULL)
    {
        /* Genesis3D DirTree_FindExact uses stricmp(), so VFS lookups are
           intentionally ASCII case-insensitive here too. */
        if (g3d_str_ieq(entry->name, name) == G3D_TRUE)
        {
            return entry;
        }
        entry = entry->sibling;
    }
    return NULL;
}

g3d_vfs_entry *g3d_vfs_find(g3d_vfs *vfs, const char *path)
{
    char part[256];
    g3d_u32 idx;
    const char *p;
    g3d_vfs_entry *cur;
    if (vfs == NULL || path == NULL)
    {
        return NULL;
    }
    p = path;
    cur = vfs->root;
    if (cur != NULL && cur->name != NULL && cur->name[0] == '\0' &&
        (cur->attributes & G3D_VFILE_ATTRIB_DIRECTORY) != 0u)
    {
        cur = cur->child;
    }
    while (*p != '\0')
    {
        idx = 0u;
        while (*p == '/' || *p == '\\')
        {
            ++p;
        }
        while (*p != '\0' && *p != '/' && *p != '\\')
        {
            if (idx + 1u >= sizeof(part))
            {
                return NULL;
            }
            part[idx++] = *p++;
        }
        part[idx] = '\0';
        if (idx == 0u)
        {
            break;
        }
        cur = g3d_vfs_find_in_list(cur, part);
        if (cur == NULL)
        {
            return NULL;
        }
        if (*p == '/' || *p == '\\')
        {
            cur = cur->child;
        }
    }
    return cur;
}

static g3d_u32 g3d_vfs_count_entry_list(const g3d_vfs_entry *entry, int include_directories)
{
    g3d_u32 count;
    count = 0u;
    while (entry != NULL)
    {
        int is_dir;
        is_dir = ((entry->attributes & G3D_VFILE_ATTRIB_DIRECTORY) != 0u) ? G3D_TRUE : G3D_FALSE;
        if (include_directories != 0 || is_dir == G3D_FALSE)
        {
            ++count;
        }
        if (entry->child != NULL)
        {
            count += g3d_vfs_count_entry_list(entry->child, include_directories);
        }
        entry = entry->sibling;
    }
    return count;
}

g3d_u32 g3d_vfs_count_entries(const g3d_vfs *vfs, int include_directories)
{
    if (vfs == NULL || vfs->root == NULL)
    {
        return 0u;
    }
    return g3d_vfs_count_entry_list(vfs->root, include_directories);
}

g3d_u32 g3d_vfs_count_files(const g3d_vfs *vfs)
{
    return g3d_vfs_count_entries(vfs, 0);
}

int g3d_body_parse_geometry(g3d_body *body, const void *data, g3d_u32 size, g3d_arena *arena, g3d_u32 *used_bytes)
{
    g3d_reader r;
    int rc;
    if (body == NULL || data == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    g3d_zero(body, (g3d_u32)sizeof(*body));
    g3d_reader_init(&r, data, size);
    rc = g3d_body_parse_geometry_inner(body, &r, arena);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = r.pos;
    }
    return G3D_OK;
}

int g3d_body_parse_file(g3d_body *body, void *data, g3d_u32 size, g3d_arena *arena)
{
    return g3d_body_parse_file_with_mask(body, data, size, arena, 0u);
}

int g3d_body_parse_file_with_mask(g3d_body *body, void *data, g3d_u32 size, g3d_arena *arena, g3d_u8 xor_mask)
{
    g3d_vfs vfs;
    g3d_vfs_entry *geom;
    g3d_u32 i;
    int rc;

    if (body == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    g3d_zero(body, (g3d_u32)sizeof(*body));

    rc = g3d_vfs_parse_with_mask(&vfs, data, size, arena, xor_mask);
    if (rc != G3D_OK)
    {
        return rc;
    }

    geom = g3d_vfs_find(&vfs, G3D_BODY_GEOM);
    if (geom == NULL || (geom->attributes & G3D_VFILE_ATTRIB_DIRECTORY) != 0u)
    {
        return G3D_ERR_NOT_FOUND;
    }

    {
        g3d_reader r;
        g3d_reader_init(&r, geom->payload, (g3d_u32)geom->size);
        rc = g3d_body_parse_geometry_inner(body, &r, arena);
        if (rc != G3D_OK)
        {
            return rc;
        }
        if (r.pos != (g3d_u32)geom->size)
        {
            return G3D_ERR_CORRUPT;
        }
    }

    for (i = 0u; i < (g3d_u32)body->material_count; ++i)
    {
        if (body->materials[i].has_bitmap != 0u)
        {
            char path[64];
            g3d_vfs_entry *bmp;
            sprintf(path, "%s/%u", G3D_BODY_BITMAPS, (unsigned)i);
            bmp = g3d_vfs_find(&vfs, path);
            if (bmp == NULL || (bmp->attributes & G3D_VFILE_ATTRIB_DIRECTORY) != 0u)
            {
                return G3D_ERR_NOT_FOUND;
            }
            body->materials[i].bitmap_size = (g3d_u32)bmp->size;
            body->materials[i].bitmap_data = (g3d_u8 *)g3d_arena_alloc(arena, body->materials[i].bitmap_size, 1u);
            if (body->materials[i].bitmap_data == NULL)
            {
                return G3D_ERR_ARENA_EXHAUSTED;
            }
            memcpy(body->materials[i].bitmap_data, bmp->payload, (size_t)body->materials[i].bitmap_size);
        }
    }
    return G3D_OK;
}

static int g3d_actor_write_header_payload(g3d_writer *w, const g3d_actor *actor)
{
    if (g3d_wr_u32le(w, G3D_ACTOR_SIG) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, G3D_ACTOR_VER) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_u32le(w, actor->has_body != 0u ? 1u : 0u) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_wr_s32le(w, actor->motion_count) != G3D_OK) return G3D_ERR_NO_SPACE;
    return G3D_OK;
}


static int g3d_actor_write_motion_nodes(g3d_writer *w, const g3d_actor *actor, g3d_u32 first_motion_offset)
{
    g3d_u32 i;
    g3d_u32 offset;
    int rc;
    offset = first_motion_offset;

    for (i = 0u; i < (g3d_u32)actor->motion_count; ++i)
    {
        char name_buf[32];
        g3d_u32 motion_size;
        sprintf(name_buf, "%u", (unsigned)i);

        rc = g3d_motion_measure_binary(&actor->motions[i], &motion_size);
        if (rc != G3D_OK)
        {
            return rc;
        }

        rc = g3d_dir_write_node_header(w, name_buf, 0u, motion_size, offset, NULL, 0u);
        if (rc != G3D_OK)
        {
            return rc;
        }
        rc = g3d_dir_write_term(w);
        if (rc != G3D_OK)
        {
            return rc;
        }

        offset += motion_size;
        if (i + 1u == (g3d_u32)actor->motion_count)
        {
            rc = g3d_dir_write_term(w);
            if (rc != G3D_OK)
            {
                return rc;
            }
        }
    }

    if (actor->motion_count == 0)
    {
        return g3d_dir_write_term(w);
    }
    return G3D_OK;
}

static int g3d_actor_write_file_inner(const g3d_actor *actor, g3d_writer *w, g3d_u8 xor_mask)
{
    int validate_rc;
    g3d_u32 vfs_header_pos;
    g3d_u32 header_offset;
    g3d_u32 header_size;
    g3d_u32 body_offset;
    g3d_u32 body_size;
    g3d_u32 motions_offset;
    g3d_u32 motion_pos;
    g3d_u32 data_length;
    g3d_u32 dir_start;
    g3d_u32 end_pos;
    g3d_u32 base_pos;
    g3d_u32 i;
    int rc;

    if (actor == NULL || w == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    validate_rc = g3d_actor_validate(actor);
    if (validate_rc != G3D_OK)
    {
        return validate_rc;
    }

    header_size = 16u;
    body_offset = 0u;
    body_size = 0u;

    vfs_header_pos = w->pos;
    base_pos = vfs_header_pos;
    if (g3d_wr_bytes(w, NULL, 24u) != G3D_OK) return G3D_ERR_NO_SPACE;

    header_offset = w->pos - base_pos;
    rc = g3d_actor_write_header_payload(w, actor);
    if (rc != G3D_OK) return rc;
    rc = g3d_xor_writer_range(w, base_pos + header_offset, header_size, xor_mask);
    if (rc != G3D_OK) return rc;

    if (actor->has_body != 0u && actor->body != NULL)
    {
        body_offset = w->pos - base_pos;
        rc = g3d_body_write_file_inner(actor->body, w, 0u);
        if (rc != G3D_OK)
        {
            return rc;
        }
        body_size = (w->pos - base_pos) - body_offset;
        rc = g3d_xor_writer_range(w, base_pos + body_offset, body_size, xor_mask);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }

    motions_offset = w->pos - base_pos;
    for (i = 0u; i < (g3d_u32)actor->motion_count; ++i)
    {
        motion_pos = w->pos;
        rc = g3d_write_motion_inner(&actor->motions[i], w);
        if (rc != G3D_OK)
        {
            return rc;
        }
        rc = g3d_xor_writer_range(w, motion_pos, w->pos - motion_pos, xor_mask);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }

    data_length = w->pos - base_pos;
    rc = g3d_dir_begin(w, &dir_start);
    if (rc != G3D_OK) return rc;

    /* Genesis3D DirTree serializes one explicit empty-name root directory.
       Its children are the public VFS entries (Header/Body/Motions). */
    rc = g3d_dir_write_node_header(w, "", G3D_VFILE_ATTRIB_DIRECTORY, 0u, 0u, NULL, 0u);
    if (rc != G3D_OK) return rc;

    rc = g3d_dir_write_node_header(w, G3D_ACTOR_HEADER, 0u, header_size, header_offset, NULL, 0u);
    if (rc != G3D_OK) return rc;
    rc = g3d_dir_write_term(w);
    if (rc != G3D_OK) return rc;

    if (actor->has_body != 0u && actor->body != NULL)
    {
        rc = g3d_dir_write_node_header(w, G3D_ACTOR_BODY, 0u, body_size, body_offset, NULL, 0u);
        if (rc != G3D_OK) return rc;
        rc = g3d_dir_write_term(w);
        if (rc != G3D_OK) return rc;

        rc = g3d_dir_write_node_header(w, G3D_ACTOR_MOTDIR, G3D_VFILE_ATTRIB_DIRECTORY, 0u, 0u, NULL, 0u);
        if (rc != G3D_OK) return rc;
        rc = g3d_actor_write_motion_nodes(w, actor, motions_offset);
        if (rc != G3D_OK) return rc;
        rc = g3d_dir_write_term(w);
        if (rc != G3D_OK) return rc;
    }
    else
    {
        rc = g3d_dir_write_node_header(w, G3D_ACTOR_MOTDIR, G3D_VFILE_ATTRIB_DIRECTORY, 0u, 0u, NULL, 0u);
        if (rc != G3D_OK) return rc;
        rc = g3d_actor_write_motion_nodes(w, actor, motions_offset);
        if (rc != G3D_OK) return rc;
        rc = g3d_dir_write_term(w);
        if (rc != G3D_OK) return rc;
    }

    /* terminator for the explicit root's sibling list */
    rc = g3d_dir_write_term(w);
    if (rc != G3D_OK) return rc;

    rc = g3d_dir_finish(w, dir_start);
    if (rc != G3D_OK) return rc;

    end_pos = w->pos;
    if (g3d_patch_u32le(w, vfs_header_pos + 0u, (xor_mask != 0u) ? G3D_VFS_SIG_CF00 : G3D_VFS_SIG_VF00) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (w->measure_only == 0)
    {
        w->buf[vfs_header_pos + 4u] = 0u;
        w->buf[vfs_header_pos + 5u] = 0u;
        w->buf[vfs_header_pos + 6u] = 0u;
        w->buf[vfs_header_pos + 7u] = 0u;
    }
    if (g3d_patch_u32le(w, vfs_header_pos + 8u, 0u) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_patch_u32le(w, vfs_header_pos + 12u, data_length) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_patch_u32le(w, vfs_header_pos + 16u, data_length) != G3D_OK) return G3D_ERR_NO_SPACE;
    if (g3d_patch_u32le(w, vfs_header_pos + 20u, end_pos) != G3D_OK) return G3D_ERR_NO_SPACE;

    return G3D_OK;
}

int g3d_actor_write_file(const g3d_actor *actor, void *dst, g3d_u32 cap, g3d_u32 *used_bytes)
{
    return g3d_actor_write_file_with_mask(actor, dst, cap, used_bytes, 0u);
}

int g3d_actor_write_file_with_mask(const g3d_actor *actor, void *dst, g3d_u32 cap, g3d_u32 *used_bytes, g3d_u8 xor_mask)
{
    g3d_writer w;
    int rc;
    g3d_writer_init(&w, dst, cap);
    rc = g3d_actor_write_file_inner(actor, &w, xor_mask);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (used_bytes != NULL)
    {
        *used_bytes = w.pos;
    }
    return G3D_OK;
}

int g3d_actor_measure_file(const g3d_actor *actor, g3d_u32 *size_out)
{
    g3d_u32 used;
    int rc;
    rc = g3d_actor_write_file(actor, NULL, 0u, &used);
    if (rc != G3D_OK)
    {
        return rc;
    }
    *size_out = used;
    return G3D_OK;
}

int g3d_actor_parse_file(g3d_actor *actor, void *data, g3d_u32 size, g3d_arena *arena)
{
    return g3d_actor_parse_file_with_mask(actor, data, size, arena, 0u);
}

int g3d_actor_parse_file_with_mask(g3d_actor *actor, void *data, g3d_u32 size, g3d_arena *arena, g3d_u8 xor_mask)
{
    g3d_vfs vfs;
    g3d_vfs_entry *header;
    g3d_reader r;
    int rc;
    g3d_u32 sig;
    g3d_u32 ver;
    g3d_u32 has_body;
    g3d_s32 motion_count;
    g3d_u32 i;

    if (actor == NULL || arena == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    g3d_zero(actor, (g3d_u32)sizeof(*actor));

    rc = g3d_vfs_parse_with_mask(&vfs, data, size, arena, xor_mask);
    if (rc != G3D_OK)
    {
        return rc;
    }

    header = g3d_vfs_find(&vfs, G3D_ACTOR_HEADER);
    if (header == NULL)
    {
        return G3D_ERR_NOT_FOUND;
    }

    g3d_reader_init(&r, header->payload, (g3d_u32)header->size);
    rc = g3d_rd_u32le(&r, &sig); if (rc != G3D_OK) return rc;
    if (sig != G3D_ACTOR_SIG) return G3D_ERR_BAD_MAGIC;
    rc = g3d_rd_u32le(&r, &ver); if (rc != G3D_OK) return rc;
    if (ver != G3D_ACTOR_VER) return G3D_ERR_BAD_VERSION;
    rc = g3d_rd_u32le(&r, &has_body); if (rc != G3D_OK) return rc;
    rc = g3d_rd_s32le(&r, &motion_count); if (rc != G3D_OK) return rc;
    if (motion_count < 0)
    {
        return G3D_ERR_CORRUPT;
    }
    actor->has_body = has_body;
    actor->motion_count = motion_count;

    if (actor->has_body != 0u)
    {
        g3d_vfs_entry *body_entry;
        actor->body = (g3d_body *)g3d_arena_alloc(arena, (g3d_u32)sizeof(g3d_body), 4u);
        if (actor->body == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        g3d_zero(actor->body, (g3d_u32)sizeof(g3d_body));
        body_entry = g3d_vfs_find(&vfs, G3D_ACTOR_BODY);
        if (body_entry == NULL)
        {
            return G3D_ERR_NOT_FOUND;
        }
        rc = g3d_body_parse_file_with_mask(actor->body, body_entry->payload, (g3d_u32)body_entry->size, arena, xor_mask);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }

    if (actor->motion_count > 0)
    {
        actor->motions = (g3d_motion *)g3d_arena_alloc(arena, (g3d_u32)actor->motion_count * (g3d_u32)sizeof(g3d_motion), 4u);
        if (actor->motions == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        g3d_zero(actor->motions, (g3d_u32)actor->motion_count * (g3d_u32)sizeof(g3d_motion));
        for (i = 0u; i < (g3d_u32)actor->motion_count; ++i)
        {
            char path[64];
            g3d_vfs_entry *me;
            sprintf(path, "%s/%u", G3D_ACTOR_MOTDIR, (unsigned)i);
            me = g3d_vfs_find(&vfs, path);
            if (me == NULL)
            {
                return G3D_ERR_NOT_FOUND;
            }
            rc = g3d_motion_parse_binary(&actor->motions[i], me->payload, (g3d_u32)me->size, arena);
            if (rc != G3D_OK)
            {
                return rc;
            }
        }
    }

    return G3D_OK;
}


static int g3d_validate_cstr_in_block(const g3d_u8 *base, g3d_s32 size, g3d_s32 off)
{
    g3d_s32 i;
    if (base == NULL || size < 0 || off < 0 || off >= size)
    {
        return G3D_ERR_CORRUPT;
    }
    for (i = off; i < size; ++i)
    {
        if (base[i] == 0u)
        {
            return G3D_OK;
        }
    }
    return G3D_ERR_CORRUPT;
}

static int g3d_validate_strblock_inner(const g3d_strblock *sb)
{
    g3d_u32 i;
    if (sb == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (sb->count < 0 || sb->size < 0)
    {
        return G3D_ERR_CORRUPT;
    }
    if (sb->size == 0)
    {
        return (sb->count == 0) ? G3D_OK : G3D_ERR_CORRUPT;
    }
    if (sb->data == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    if ((g3d_u32)sb->size < ((g3d_u32)sb->count * 4u))
    {
        return G3D_ERR_CORRUPT;
    }
    for (i = 0u; i < (g3d_u32)sb->count; ++i)
    {
        g3d_s32 off;
        off = g3d_rd_s32ptr(sb->data + (i * 4u));
        if (g3d_validate_cstr_in_block(sb->data, sb->size, off) != G3D_OK)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    return G3D_OK;
}

static int g3d_validate_tkarray_inner(const g3d_tkarray *arr)
{
    if (arr == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (arr->num_elements < 0 || arr->element_size < 0)
    {
        return G3D_ERR_CORRUPT;
    }
    if (arr->num_elements == 0)
    {
        return G3D_OK;
    }
    if (arr->element_size <= 0 || arr->elements == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    return G3D_OK;
}

static int g3d_validate_explicit_time_bits(const g3d_u32 *time_bits, g3d_u32 count)
{
    g3d_u32 i;
    g3d_fix16 prev_time;
    g3d_fix16 cur_time;
    if (count <= 1u)
    {
        return G3D_OK;
    }
    if (time_bits == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    prev_time = g3d_f32bits_to_fix16(time_bits[0]);
    for (i = 1u; i < count; ++i)
    {
        cur_time = g3d_f32bits_to_fix16(time_bits[i]);
        if ((g3d_s32)cur_time <= (g3d_s32)prev_time)
        {
            return G3D_ERR_CORRUPT;
        }
        prev_time = cur_time;
    }
    return G3D_OK;
}

static int g3d_validate_lineartime_bits(const g3d_u32 *time_bits, g3d_u32 key_count, g3d_u32 time_value_count)
{
    g3d_fix16 delta_time;
    if (key_count <= 1u)
    {
        return G3D_OK;
    }
    if (time_bits == NULL || time_value_count < 2u)
    {
        return G3D_ERR_CORRUPT;
    }
    delta_time = g3d_f32bits_to_fix16(time_bits[1]);
    if ((g3d_s32)delta_time <= 0)
    {
        return G3D_ERR_CORRUPT;
    }
    (void)g3d_fix16_add_indexed_delta(g3d_f32bits_to_fix16(time_bits[0]), delta_time, key_count - 1u);
    return G3D_OK;
}

static int g3d_validate_tkevents_inner(const g3d_tkevents *ev)
{
    g3d_u32 i;
    g3d_fix16 prev_time;
    int have_prev;
    int rc;
    if (ev == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (ev->data_size != 0u && ev->event_data == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    rc = g3d_validate_tkarray_inner(&ev->time_keys);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (ev->time_keys.num_elements != 0 && ev->time_keys.element_size < 8)
    {
        return G3D_ERR_CORRUPT;
    }
    have_prev = G3D_FALSE;
    prev_time = 0;
    for (i = 0u; i < (g3d_u32)ev->time_keys.num_elements; ++i)
    {
        g3d_u32 time_bits;
        g3d_u32 offset;
        g3d_fix16 cur_time;
        rc = g3d_tkevents_get(ev, (g3d_s32)i, &time_bits, &offset);
        if (rc != G3D_OK)
        {
            return rc;
        }
        cur_time = g3d_f32bits_to_fix16(time_bits);
        if (have_prev != G3D_FALSE && (g3d_s32)cur_time <= (g3d_s32)prev_time)
        {
            return G3D_ERR_CORRUPT;
        }
        have_prev = G3D_TRUE;
        prev_time = cur_time;
        if (offset >= ev->data_size || ev->event_data == NULL)
        {
            return G3D_ERR_CORRUPT;
        }
        if (g3d_strn_has_zero(ev->event_data + offset, ev->data_size - offset) == G3D_FALSE)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    return G3D_OK;
}

static int g3d_validate_qkframe_inner(const g3d_qkframe *qk)
{
    if (qk == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (qk->interpolation != G3D_QK_LINEAR &&
        qk->interpolation != G3D_QK_SLERP &&
        qk->interpolation != G3D_QK_SQUAD)
    {
        return G3D_ERR_CORRUPT;
    }
    if ((qk->compression & ~(G3D_QK_HINGE_COMPRESSION | G3D_QK_LINEARTIME_COMPRESSION)) != 0u)
    {
        return G3D_ERR_CORRUPT;
    }
    if ((qk->compression & G3D_QK_LINEARTIME_COMPRESSION) != 0u)
    {
        if (qk->time_value_count != 2u && qk->count != 0u)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    else
    {
        if (qk->time_value_count != qk->count)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    if (qk->time_value_count != 0u && qk->time_bits == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    if ((qk->compression & G3D_QK_LINEARTIME_COMPRESSION) != 0u)
    {
        int rc_time;
        rc_time = g3d_validate_lineartime_bits(qk->time_bits, qk->count, qk->time_value_count);
        if (rc_time != G3D_OK)
        {
            return rc_time;
        }
    }
    else
    {
        int rc_time;
        rc_time = g3d_validate_explicit_time_bits(qk->time_bits, qk->count);
        if (rc_time != G3D_OK)
        {
            return rc_time;
        }
    }
    if ((qk->compression & G3D_QK_HINGE_COMPRESSION) != 0u)
    {
        if (qk->count != 0u && qk->angle_bits == NULL)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    else
    {
        if (qk->count != 0u && qk->quats == NULL)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    return G3D_OK;
}

static int g3d_validate_vkframe_inner(const g3d_vkframe *vk)
{
    if (vk == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (vk->interpolation != G3D_VK_LINEAR &&
        vk->interpolation != G3D_VK_HERMITE &&
        vk->interpolation != G3D_VK_HERMITE_ZERO_DERIV)
    {
        return G3D_ERR_CORRUPT;
    }
    if ((vk->compression & ~G3D_VK_LINEARTIME_COMPRESSION) != 0u)
    {
        return G3D_ERR_CORRUPT;
    }
    if ((vk->compression & G3D_VK_LINEARTIME_COMPRESSION) != 0u)
    {
        if (vk->time_value_count != 2u && vk->count != 0u)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    else
    {
        if (vk->time_value_count != vk->count)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    if (vk->time_value_count != 0u && vk->time_bits == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    if ((vk->compression & G3D_VK_LINEARTIME_COMPRESSION) != 0u)
    {
        int rc_time;
        rc_time = g3d_validate_lineartime_bits(vk->time_bits, vk->count, vk->time_value_count);
        if (rc_time != G3D_OK)
        {
            return rc_time;
        }
    }
    else
    {
        int rc_time;
        rc_time = g3d_validate_explicit_time_bits(vk->time_bits, vk->count);
        if (rc_time != G3D_OK)
        {
            return rc_time;
        }
    }
    if (vk->count != 0u && vk->vectors == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    return G3D_OK;
}

int g3d_path_validate(const g3d_path *path)
{
    int rc;
    if (path == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (path->has_translation == 0u && path->has_rotation == 0u)
    {
        return G3D_ERR_CORRUPT;
    }
    if (path->has_translation != 0u)
    {
        if (path->translation_interp != G3D_PATH_VK_LINEAR &&
            path->translation_interp != G3D_PATH_VK_HERMITE &&
            path->translation_interp != G3D_PATH_VK_HERMITE_ZERO_DERIV)
        {
            return G3D_ERR_CORRUPT;
        }
        rc = g3d_validate_vkframe_inner(&path->translation);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    if (path->has_rotation != 0u)
    {
        if (path->rotation_interp != G3D_PATH_QK_LINEAR &&
            path->rotation_interp != G3D_PATH_QK_SLERP &&
            path->rotation_interp != G3D_PATH_QK_SQUAD)
        {
            return G3D_ERR_CORRUPT;
        }
        rc = g3d_validate_qkframe_inner(&path->rotation);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    return G3D_OK;
}

int g3d_motion_validate(const g3d_motion *motion)
{
    g3d_u32 i;
    int rc;
    if (motion == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (motion->node_type == G3D_MOTION_NODE_UNDECIDED)
    {
        return G3D_OK;
    }
    if (motion->node_type != G3D_MOTION_NODE_LEAF)
    {
        return G3D_ERR_UNSUPPORTED;
    }
    if (motion->leaf.path_count < 0)
    {
        return G3D_ERR_CORRUPT;
    }
    if (motion->leaf.path_count != 0 && motion->leaf.paths == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    if (motion->leaf.events != NULL)
    {
        rc = g3d_validate_tkevents_inner(motion->leaf.events);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    if (motion->leaf.name_array != NULL)
    {
        rc = g3d_validate_strblock_inner(motion->leaf.name_array);
        if (rc != G3D_OK)
        {
            return rc;
        }
        if (motion->maintain_names != 0u &&
            motion->leaf.name_array->count != motion->leaf.path_count)
        {
            return G3D_ERR_CORRUPT;
        }
        if (motion->leaf.name_checksum != 0 &&
            g3d_strblock_checksum(motion->leaf.name_array) != motion->leaf.name_checksum)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    else if (motion->maintain_names != 0u && motion->leaf.path_count != 0)
    {
        return G3D_ERR_CORRUPT;
    }
    for (i = 0u; i < (g3d_u32)motion->leaf.path_count; ++i)
    {
        rc = g3d_path_validate(&motion->leaf.paths[i]);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    return G3D_OK;
}

int g3d_body_validate(const g3d_body *body)
{
    g3d_u32 i;
    g3d_u32 lod;
    int rc;
    if (body == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (body->xskin_vertex_count < 0 || body->normal_count < 0 ||
        body->bone_count < 0 || body->material_count < 0)
    {
        return G3D_ERR_CORRUPT;
    }
    if (body->xskin_vertex_count != 0 && body->xskin_vertices == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    if (body->normal_count != 0 && body->normals == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    if (body->bone_count != 0 && body->bones == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    if (body->material_count != 0 && body->materials == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    if (body->lod_count > G3D_BODY_MAX_LOD)
    {
        return G3D_ERR_CORRUPT;
    }
    if (body->bone_names == NULL || body->material_names == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    rc = g3d_validate_strblock_inner(body->bone_names);
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_validate_strblock_inner(body->material_names);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (body->bone_names->count != body->bone_count)
    {
        return G3D_ERR_CORRUPT;
    }
    if (body->material_names->count != body->material_count)
    {
        return G3D_ERR_CORRUPT;
    }
    for (i = 0u; i < (g3d_u32)body->xskin_vertex_count; ++i)
    {
        if (body->xskin_vertices[i].bone_index < -1 ||
            body->xskin_vertices[i].bone_index >= body->bone_count)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    for (i = 0u; i < (g3d_u32)body->normal_count; ++i)
    {
        if (body->normals[i].bone_index < -1 ||
            body->normals[i].bone_index >= body->bone_count)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    for (i = 0u; i < (g3d_u32)body->bone_count; ++i)
    {
        if (body->bones[i].parent_bone_index < -1 ||
            body->bones[i].parent_bone_index >= body->bone_count)
        {
            return G3D_ERR_CORRUPT;
        }
        if (body->bones[i].parent_bone_index >= 0 &&
            (g3d_u32)body->bones[i].parent_bone_index >= i)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    for (i = 0u; i < (g3d_u32)body->material_count; ++i)
    {
        if (body->materials[i].has_bitmap != 0u &&
            body->materials[i].bitmap_size != 0u &&
            body->materials[i].bitmap_data == NULL)
        {
            return G3D_ERR_CORRUPT;
        }
    }
    for (lod = 0u; lod < body->lod_count; ++lod)
    {
        if (body->lods[lod].face_count != 0u && body->lods[lod].faces == NULL)
        {
            return G3D_ERR_CORRUPT;
        }
        for (i = 0u; i < body->lods[lod].face_count; ++i)
        {
            g3d_u32 j;
            for (j = 0u; j < 3u; ++j)
            {
                if (body->lods[lod].faces[i].vtx_index[j] < 0 ||
                    body->lods[lod].faces[i].vtx_index[j] >= body->xskin_vertex_count)
                {
                    return G3D_ERR_CORRUPT;
                }
                if (body->lods[lod].faces[i].normal_index[j] < 0 ||
                    body->lods[lod].faces[i].normal_index[j] >= body->normal_count)
                {
                    return G3D_ERR_CORRUPT;
                }
            }
            if (body->lods[lod].faces[i].material_index < 0 ||
                body->lods[lod].faces[i].material_index >= body->material_count)
            {
                return G3D_ERR_CORRUPT;
            }
        }
    }
    return G3D_OK;
}

int g3d_actor_validate(const g3d_actor *actor)
{
    g3d_u32 i;
    int rc;
    if (actor == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (actor->motion_count < 0)
    {
        return G3D_ERR_CORRUPT;
    }
    if (actor->has_body != 0u)
    {
        if (actor->body == NULL)
        {
            return G3D_ERR_CORRUPT;
        }
        rc = g3d_body_validate(actor->body);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    if (actor->motion_count != 0 && actor->motions == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    for (i = 0u; i < (g3d_u32)actor->motion_count; ++i)
    {
        rc = g3d_motion_validate(&actor->motions[i]);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    return G3D_OK;
}

int g3d_motion_find_path_index(const g3d_motion *motion, const char *path_name, g3d_s32 *out_index)
{
    g3d_u32 i;
    if (motion == NULL || path_name == NULL || out_index == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (motion->node_type != G3D_MOTION_NODE_LEAF || motion->leaf.name_array == NULL)
    {
        return G3D_ERR_NOT_FOUND;
    }
    for (i = 0u; i < (g3d_u32)motion->leaf.path_count; ++i)
    {
        const char *name;
        name = g3d_strblock_get(motion->leaf.name_array, (g3d_s32)i);
        if (name != NULL && g3d_str_eq(name, path_name) == G3D_TRUE)
        {
            *out_index = (g3d_s32)i;
            return G3D_OK;
        }
    }
    return G3D_ERR_NOT_FOUND;
}

const char *g3d_motion_get_path_name(const g3d_motion *motion, g3d_s32 path_index)
{
    if (motion == NULL || motion->node_type != G3D_MOTION_NODE_LEAF ||
        motion->leaf.name_array == NULL || path_index < 0 ||
        path_index >= motion->leaf.path_count)
    {
        return NULL;
    }
    return g3d_strblock_get(motion->leaf.name_array, path_index);
}

g3d_s32 g3d_motion_name_checksum(const g3d_motion *motion)
{
    if (motion == NULL || motion->node_type != G3D_MOTION_NODE_LEAF ||
        motion->leaf.name_array == NULL)
    {
        return 0;
    }
    return g3d_strblock_checksum(motion->leaf.name_array);
}

int g3d_body_find_bone_index(const g3d_body *body, const char *bone_name, g3d_s32 *out_index)
{
    g3d_u32 i;
    if (body == NULL || bone_name == NULL || out_index == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (body->bone_names == NULL)
    {
        return G3D_ERR_NOT_FOUND;
    }
    for (i = 0u; i < (g3d_u32)body->bone_count; ++i)
    {
        const char *name;
        name = g3d_strblock_get(body->bone_names, (g3d_s32)i);
        if (name != NULL && g3d_str_eq(name, bone_name) == G3D_TRUE)
        {
            *out_index = (g3d_s32)i;
            return G3D_OK;
        }
    }
    return G3D_ERR_NOT_FOUND;
}

const char *g3d_body_get_bone_name(const g3d_body *body, g3d_s32 bone_index)
{
    if (body == NULL || body->bone_names == NULL || bone_index < 0 || bone_index >= body->bone_count)
    {
        return NULL;
    }
    return g3d_strblock_get(body->bone_names, bone_index);
}

g3d_s32 g3d_body_bone_name_checksum(const g3d_body *body)
{
    if (body == NULL || body->bone_names == NULL)
    {
        return 0;
    }
    return g3d_strblock_checksum(body->bone_names);
}

int g3d_body_compute_bbox_fix16(const g3d_body *body, g3d_fixvec3 *min_out, g3d_fixvec3 *max_out)
{
    g3d_u32 i;
    g3d_fixvec3 mn;
    g3d_fixvec3 mx;
    if (body == NULL || min_out == NULL || max_out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (body->xskin_vertex_count <= 0 || body->xskin_vertices == NULL)
    {
        min_out->x = 0;
        min_out->y = 0;
        min_out->z = 0;
        max_out->x = 0;
        max_out->y = 0;
        max_out->z = 0;
        return G3D_ERR_NOT_FOUND;
    }
    mn.x = g3d_f32bits_to_fix16(body->xskin_vertices[0].xpoint.x_bits);
    mn.y = g3d_f32bits_to_fix16(body->xskin_vertices[0].xpoint.y_bits);
    mn.z = g3d_f32bits_to_fix16(body->xskin_vertices[0].xpoint.z_bits);
    mx = mn;
    for (i = 1u; i < (g3d_u32)body->xskin_vertex_count; ++i)
    {
        g3d_fixvec3 v;
        v.x = g3d_f32bits_to_fix16(body->xskin_vertices[i].xpoint.x_bits);
        v.y = g3d_f32bits_to_fix16(body->xskin_vertices[i].xpoint.y_bits);
        v.z = g3d_f32bits_to_fix16(body->xskin_vertices[i].xpoint.z_bits);
        if ((g3d_s32)v.x < (g3d_s32)mn.x) mn.x = v.x;
        if ((g3d_s32)v.y < (g3d_s32)mn.y) mn.y = v.y;
        if ((g3d_s32)v.z < (g3d_s32)mn.z) mn.z = v.z;
        if ((g3d_s32)v.x > (g3d_s32)mx.x) mx.x = v.x;
        if ((g3d_s32)v.y > (g3d_s32)mx.y) mx.y = v.y;
        if ((g3d_s32)v.z > (g3d_s32)mx.z) mx.z = v.z;
    }
    *min_out = mn;
    *max_out = mx;
    return G3D_OK;
}


int g3d_body_find_material_index(const g3d_body *body, const char *material_name, g3d_s32 *out_index)
{
    g3d_u32 i;
    if (body == NULL || material_name == NULL || out_index == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (body->material_names == NULL)
    {
        return G3D_ERR_NOT_FOUND;
    }
    for (i = 0u; i < (g3d_u32)body->material_count; ++i)
    {
        const char *name;
        name = g3d_strblock_get(body->material_names, (g3d_s32)i);
        if (name != NULL && g3d_str_eq(name, material_name) == G3D_TRUE)
        {
            *out_index = (g3d_s32)i;
            return G3D_OK;
        }
    }
    return G3D_ERR_NOT_FOUND;
}

int g3d_body_find_material_index_prefix(const g3d_body *body, const char *material_prefix, g3d_s32 *out_index)
{
    g3d_u32 i;
    g3d_u32 n;
    if (body == NULL || material_prefix == NULL || out_index == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (body->material_names == NULL)
    {
        return G3D_ERR_NOT_FOUND;
    }
    n = g3d_strlen32(material_prefix);
    for (i = 0u; i < (g3d_u32)body->material_count; ++i)
    {
        const char *name;
        name = g3d_strblock_get(body->material_names, (g3d_s32)i);
        if (name != NULL && strncmp(name, material_prefix, (size_t)n) == 0)
        {
            *out_index = (g3d_s32)i;
            return G3D_OK;
        }
    }
    return G3D_ERR_NOT_FOUND;
}

const char *g3d_body_get_material_name(const g3d_body *body, g3d_s32 material_index)
{
    if (body == NULL || body->material_names == NULL || material_index < 0 || material_index >= body->material_count)
    {
        return NULL;
    }
    return g3d_strblock_get(body->material_names, material_index);
}

g3d_u32 g3d_body_count_textured_materials(const g3d_body *body)
{
    g3d_u32 i;
    g3d_u32 count;
    if (body == NULL || body->materials == NULL || body->material_count <= 0)
    {
        return 0u;
    }
    count = 0u;
    for (i = 0u; i < (g3d_u32)body->material_count; ++i)
    {
        if (body->materials[i].has_bitmap != 0u)
        {
            ++count;
        }
    }
    return count;
}

int g3d_body_get_material_bitmap_info(const g3d_body *body, g3d_s32 material_index, g3d_bitmap_info *info)
{
    const g3d_body_material *mat;
    if (body == NULL || info == NULL || material_index < 0 || material_index >= body->material_count)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (body->materials == NULL)
    {
        return G3D_ERR_NOT_FOUND;
    }
    mat = &body->materials[(g3d_u32)material_index];
    if (mat->has_bitmap == 0u)
    {
        return G3D_ERR_NOT_FOUND;
    }
    if (mat->bitmap_data == NULL || mat->bitmap_size == 0u)
    {
        return G3D_ERR_NOT_FOUND;
    }
    return g3d_bitmap_probe(mat->bitmap_data, mat->bitmap_size, info);
}

int g3d_body_validate_bitmaps(const g3d_body *body, g3d_s32 *bad_material_index)
{
    g3d_u32 i;
    if (bad_material_index != NULL)
    {
        *bad_material_index = -1;
    }
    if (body == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (body->material_count <= 0)
    {
        return G3D_OK;
    }
    if (body->materials == NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    for (i = 0u; i < (g3d_u32)body->material_count; ++i)
    {
        const g3d_body_material *mat;
        mat = &body->materials[i];
        if (mat->has_bitmap != 0u)
        {
            g3d_bitmap_info info;
            int rc;
            if (mat->bitmap_data == NULL || mat->bitmap_size == 0u)
            {
                if (bad_material_index != NULL)
                {
                    *bad_material_index = (g3d_s32)i;
                }
                return G3D_ERR_NOT_FOUND;
            }
            rc = g3d_bitmap_probe(mat->bitmap_data, mat->bitmap_size, &info);
            if (rc != G3D_OK)
            {
                if (bad_material_index != NULL)
                {
                    *bad_material_index = (g3d_s32)i;
                }
                return rc;
            }
        }
    }
    return G3D_OK;
}

int g3d_body_count_vertices_for_bone(const g3d_body *body, g3d_s32 bone_index, g3d_u32 *out_count)
{
    g3d_u32 i;
    g3d_u32 count;
    if (body == NULL || out_count == NULL || bone_index < 0 || bone_index >= body->bone_count)
    {
        return G3D_ERR_BAD_ARG;
    }
    count = 0u;
    if (body->xskin_vertices != NULL && body->xskin_vertex_count > 0)
    {
        for (i = 0u; i < (g3d_u32)body->xskin_vertex_count; ++i)
        {
            if (body->xskin_vertices[i].bone_index == bone_index)
            {
                ++count;
            }
        }
    }
    *out_count = count;
    return G3D_OK;
}

int g3d_body_compute_bone_bbox_fix16(const g3d_body *body, g3d_s32 bone_index, g3d_fixvec3 *min_out, g3d_fixvec3 *max_out)
{
    g3d_u32 i;
    g3d_fixvec3 mn;
    g3d_fixvec3 mx;
    int have;
    if (body == NULL || min_out == NULL || max_out == NULL || bone_index < 0 || bone_index >= body->bone_count)
    {
        return G3D_ERR_BAD_ARG;
    }
    have = G3D_FALSE;
    mn.x = mn.y = mn.z = 0;
    mx.x = mx.y = mx.z = 0;
    if (body->xskin_vertices != NULL && body->xskin_vertex_count > 0)
    {
        for (i = 0u; i < (g3d_u32)body->xskin_vertex_count; ++i)
        {
            if (body->xskin_vertices[i].bone_index == bone_index)
            {
                g3d_fixvec3 v;
                v.x = g3d_f32bits_to_fix16(body->xskin_vertices[i].xpoint.x_bits);
                v.y = g3d_f32bits_to_fix16(body->xskin_vertices[i].xpoint.y_bits);
                v.z = g3d_f32bits_to_fix16(body->xskin_vertices[i].xpoint.z_bits);
                if (have == G3D_FALSE)
                {
                    mn = v;
                    mx = v;
                    have = G3D_TRUE;
                }
                else
                {
                    if ((g3d_s32)v.x < (g3d_s32)mn.x) mn.x = v.x;
                    if ((g3d_s32)v.y < (g3d_s32)mn.y) mn.y = v.y;
                    if ((g3d_s32)v.z < (g3d_s32)mn.z) mn.z = v.z;
                    if ((g3d_s32)v.x > (g3d_s32)mx.x) mx.x = v.x;
                    if ((g3d_s32)v.y > (g3d_s32)mx.y) mx.y = v.y;
                    if ((g3d_s32)v.z > (g3d_s32)mx.z) mx.z = v.z;
                }
            }
        }
    }
    *min_out = mn;
    *max_out = mx;
    return (have == G3D_FALSE) ? G3D_ERR_NOT_FOUND : G3D_OK;
}

g3d_u32 g3d_body_lod_face_count(const g3d_body *body, g3d_u32 lod_index)
{
    if (body == NULL || lod_index >= G3D_BODY_MAX_LOD || lod_index >= body->lod_count)
    {
        return 0u;
    }
    return body->lods[lod_index].face_count;
}

int g3d_actor_find_motion_index(const g3d_actor *actor, const char *motion_name, g3d_s32 *out_index)
{
    g3d_u32 i;
    if (actor == NULL || motion_name == NULL || out_index == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (actor->motion_count <= 0 || actor->motions == NULL)
    {
        return G3D_ERR_NOT_FOUND;
    }
    for (i = 0u; i < (g3d_u32)actor->motion_count; ++i)
    {
        const char *name;
        name = actor->motions[i].name;
        if (name != NULL && g3d_str_eq(name, motion_name) == G3D_TRUE)
        {
            *out_index = (g3d_s32)i;
            return G3D_OK;
        }
    }
    return G3D_ERR_NOT_FOUND;
}

const char *g3d_actor_get_motion_name(const g3d_actor *actor, g3d_s32 motion_index)
{
    if (actor == NULL || motion_index < 0 || motion_index >= actor->motion_count || actor->motions == NULL)
    {
        return NULL;
    }
    return actor->motions[(g3d_u32)motion_index].name;
}

g3d_s32 g3d_motion_count_events_between_fix16(const g3d_motion *motion, g3d_fix16 start_time, g3d_fix16 end_time)
{
    if (motion == NULL || motion->node_type != G3D_MOTION_NODE_LEAF || motion->leaf.events == NULL)
    {
        return 0;
    }
    return g3d_tkevents_count_between_fix16(motion->leaf.events, start_time, end_time);
}

int g3d_motion_find_next_event_between_fix16(const g3d_motion *motion, g3d_fix16 start_time, g3d_fix16 end_time,
                                             g3d_s32 after_index, g3d_s32 *out_index,
                                             g3d_u32 *time_bits, const char **text)
{
    if (motion == NULL || out_index == NULL || time_bits == NULL || text == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (motion->node_type != G3D_MOTION_NODE_LEAF || motion->leaf.events == NULL)
    {
        return G3D_ERR_NOT_FOUND;
    }
    return g3d_tkevents_find_next_between_fix16(motion->leaf.events, start_time, end_time,
                                                after_index, out_index, time_bits, text);
}


void g3d_fixvec3_zero(g3d_fixvec3 *v)
{
    if (v != NULL)
    {
        v->x = 0;
        v->y = 0;
        v->z = 0;
    }
}

void g3d_q15quat_identity(g3d_q15quat *q)
{
    if (q != NULL)
    {
        q->x = 0;
        q->y = 0;
        q->z = 0;
        q->w = G3D_Q15_ONE;
    }
}

void g3d_sample_xform_identity(g3d_sample_xform *xf)
{
    if (xf != NULL)
    {
        g3d_q15quat_identity(&xf->rotation);
        g3d_fixvec3_zero(&xf->translation);
    }
}

void g3d_fixmat34_identity(g3d_fixmat34 *m)
{
    g3d_u32 i;
    if (m == NULL)
    {
        return;
    }
    for (i = 0u; i < 12u; ++i)
    {
        m->m[i] = 0;
    }
    m->m[0] = (g3d_fix16)0x00010000L;
    m->m[5] = (g3d_fix16)0x00010000L;
    m->m[10] = (g3d_fix16)0x00010000L;
}

static g3d_s32 g3d_bits_to_q15(g3d_u32 bits)
{
    g3d_fix16 fx;
    fx = g3d_f32bits_to_fix16(bits);
    return (g3d_s32)(fx >> 1);
}

static g3d_u32 g3d_abs_u32_from_s32(g3d_s32 v)
{
    if (v < 0)
    {
        return (g3d_u32)(-(v + 1)) + 1u;
    }
    return (g3d_u32)v;
}

static g3d_u32 g3d_isqrt_u32(g3d_u32 x)
{
    g3d_u32 op;
    g3d_u32 res;
    g3d_u32 one;
    op = x;
    res = 0u;
    one = 1u << 30;
    while (one > op)
    {
        one >>= 2;
    }
    while (one != 0u)
    {
        if (op >= res + one)
        {
            op -= res + one;
            res = (res >> 1) + one;
        }
        else
        {
            res >>= 1;
        }
        one >>= 2;
    }
    return res;
}

static g3d_s32 g3d_q15_mul(g3d_s32 a, g3d_s32 b)
{
    int neg;
    g3d_u32 ua;
    g3d_u32 ub;
    g3d_u32 prod;
    g3d_s32 out;

    neg = 0;
    if (a < 0)
    {
        ua = g3d_abs_u32_from_s32(a);
        neg = !neg;
    }
    else
    {
        ua = (g3d_u32)a;
    }
    if (b < 0)
    {
        ub = g3d_abs_u32_from_s32(b);
        neg = !neg;
    }
    else
    {
        ub = (g3d_u32)b;
    }

    prod = ua * ub;
    out = (g3d_s32)((prod + (g3d_u32)G3D_Q15_HALF) / (g3d_u32)G3D_Q15_ONE);
    if (neg)
    {
        out = -out;
    }
    return out;
}

static g3d_s32 g3d_mul_s32_q15(g3d_s32 value, g3d_s32 weight)
{
    int neg;
    g3d_u32 mag;
    g3d_u32 w;
    g3d_u32 hi;
    g3d_u32 lo;
    g3d_s32 out;

    neg = 0;
    if (value < 0)
    {
        mag = g3d_abs_u32_from_s32(value);
        neg = !neg;
    }
    else
    {
        mag = (g3d_u32)value;
    }
    if (weight < 0)
    {
        w = g3d_abs_u32_from_s32(weight);
        neg = !neg;
    }
    else
    {
        w = (g3d_u32)weight;
    }

    hi = (mag >> 15) * w;
    lo = ((mag & 0x7fffu) * w + (g3d_u32)G3D_Q15_HALF) >> 15;
    out = (g3d_s32)(hi + lo);
    if (neg)
    {
        out = -out;
    }
    return out;
}

static g3d_s32 g3d_ratio_u32_to_q15(g3d_u32 num, g3d_u32 den)
{
    g3d_u32 acc;
    g3d_u32 rem;
    int i;
    if (den == 0u)
    {
        return 0;
    }
    if (num >= den)
    {
        return G3D_Q15_ONE;
    }
    acc = 0u;
    rem = num;
    for (i = 0; i < 15; ++i)
    {
        rem <<= 1;
        acc <<= 1;
        if (rem >= den)
        {
            rem -= den;
            acc |= 1u;
        }
    }
    return (g3d_s32)acc;
}

static g3d_fix16 g3d_lerp_fix16_q15(g3d_fix16 a, g3d_fix16 b, g3d_s32 t)
{
    g3d_s32 delta;
    delta = (g3d_s32)b - (g3d_s32)a;
    return (g3d_fix16)((g3d_s32)a + g3d_mul_s32_q15(delta, t));
}

static g3d_s32 g3d_s32_add_sat(g3d_s32 a, g3d_s32 b)
{
    if (b > 0 && a > (g3d_s32)INT_MAX - b)
    {
        return (g3d_s32)INT_MAX;
    }
    if (b < 0 && a < (g3d_s32)INT_MIN - b)
    {
        return (g3d_s32)INT_MIN;
    }
    return (g3d_s32)(a + b);
}

static g3d_fix16 g3d_fix16_add_indexed_delta(g3d_fix16 start, g3d_fix16 delta, g3d_u32 index)
{
    g3d_s32 acc;
    g3d_s32 step;
    g3d_u32 n;
    acc = (g3d_s32)start;
    step = (g3d_s32)delta;
    n = index;
    while (n != 0u)
    {
        if ((n & 1u) != 0u)
        {
            acc = g3d_s32_add_sat(acc, step);
        }
        n >>= 1;
        if (n != 0u)
        {
            step = g3d_s32_add_sat(step, step);
        }
    }
    return (g3d_fix16)acc;
}

static g3d_fix16 g3d_wrap_fix16_time(g3d_fix16 time, g3d_fix16 start, g3d_fix16 end)
{
    g3d_s32 span;
    g3d_s32 rel;
    g3d_s32 mod;
    span = (g3d_s32)end - (g3d_s32)start;
    if (span <= 0)
    {
        return start;
    }
    rel = (g3d_s32)time - (g3d_s32)start;
    if (rel >= 0)
    {
        mod = rel % span;
        return (g3d_fix16)((g3d_s32)start + mod);
    }
    rel = -rel;
    mod = rel % span;
    if (mod == 0)
    {
        return start;
    }
    return (g3d_fix16)((g3d_s32)end - mod);
}

static void g3d_fvec3_to_fixvec3(const g3d_fvec3 *src, g3d_fixvec3 *dst)
{
    if (dst == NULL)
    {
        return;
    }
    if (src == NULL)
    {
        g3d_fixvec3_zero(dst);
        return;
    }
    dst->x = g3d_f32bits_to_fix16(src->x_bits);
    dst->y = g3d_f32bits_to_fix16(src->y_bits);
    dst->z = g3d_f32bits_to_fix16(src->z_bits);
}

static void g3d_fquat_to_q15quat(const g3d_fquat *src, g3d_q15quat *dst)
{
    if (dst == NULL)
    {
        return;
    }
    if (src == NULL)
    {
        g3d_q15quat_identity(dst);
        return;
    }
    dst->x = g3d_bits_to_q15(src->x_bits);
    dst->y = g3d_bits_to_q15(src->y_bits);
    dst->z = g3d_bits_to_q15(src->z_bits);
    dst->w = g3d_bits_to_q15(src->w_bits);
}

static void g3d_q15quat_normalize(g3d_q15quat *q)
{
    g3d_u32 sum;
    g3d_u32 mag;
    if (q == NULL)
    {
        return;
    }
    sum = 0u;
    sum += g3d_abs_u32_from_s32(q->x) * g3d_abs_u32_from_s32(q->x);
    sum += g3d_abs_u32_from_s32(q->y) * g3d_abs_u32_from_s32(q->y);
    sum += g3d_abs_u32_from_s32(q->z) * g3d_abs_u32_from_s32(q->z);
    sum += g3d_abs_u32_from_s32(q->w) * g3d_abs_u32_from_s32(q->w);
    if (sum == 0u)
    {
        g3d_q15quat_identity(q);
        return;
    }
    mag = g3d_isqrt_u32(sum);
    if (mag == 0u)
    {
        g3d_q15quat_identity(q);
        return;
    }
    q->x = (g3d_s32)(((g3d_s32)q->x * (g3d_s32)G3D_Q15_ONE) / (g3d_s32)mag);
    q->y = (g3d_s32)(((g3d_s32)q->y * (g3d_s32)G3D_Q15_ONE) / (g3d_s32)mag);
    q->z = (g3d_s32)(((g3d_s32)q->z * (g3d_s32)G3D_Q15_ONE) / (g3d_s32)mag);
    q->w = (g3d_s32)(((g3d_s32)q->w * (g3d_s32)G3D_Q15_ONE) / (g3d_s32)mag);
}

static g3d_s32 g3d_q15quat_dot(const g3d_q15quat *a, const g3d_q15quat *b)
{
    return g3d_q15_mul(a->x, b->x) +
           g3d_q15_mul(a->y, b->y) +
           g3d_q15_mul(a->z, b->z) +
           g3d_q15_mul(a->w, b->w);
}

static void g3d_q15quat_nlerp(const g3d_q15quat *a, const g3d_q15quat *b, g3d_s32 t, g3d_q15quat *out)
{
    g3d_q15quat bb;
    g3d_s32 it;
    bb = *b;
    if (g3d_q15quat_dot(a, &bb) < 0)
    {
        bb.x = -bb.x;
        bb.y = -bb.y;
        bb.z = -bb.z;
        bb.w = -bb.w;
    }
    it = G3D_Q15_ONE - t;
    out->x = g3d_mul_s32_q15(a->x, it) + g3d_mul_s32_q15(bb.x, t);
    out->y = g3d_mul_s32_q15(a->y, it) + g3d_mul_s32_q15(bb.y, t);
    out->z = g3d_mul_s32_q15(a->z, it) + g3d_mul_s32_q15(bb.z, t);
    out->w = g3d_mul_s32_q15(a->w, it) + g3d_mul_s32_q15(bb.w, t);
    g3d_q15quat_normalize(out);
}

#define G3D_FIX16_PI      205887L
#define G3D_FIX16_HALF_PI 102943L
#define G3D_FIX16_TWO_PI  411775L

static g3d_s32 g3d_q15_clamp_unit(g3d_s32 v)
{
    if (v > G3D_Q15_ONE)
    {
        return G3D_Q15_ONE;
    }
    if (v < -G3D_Q15_ONE)
    {
        return -G3D_Q15_ONE;
    }
    return v;
}

static g3d_fix16 g3d_wrap_radians_to_pi_fix16(g3d_fix16 angle)
{
    g3d_s32 a;
    a = (g3d_s32)angle;
    if (a > (g3d_s32)G3D_FIX16_TWO_PI || a < -(g3d_s32)G3D_FIX16_TWO_PI)
    {
        a %= (g3d_s32)G3D_FIX16_TWO_PI;
    }
    while (a > (g3d_s32)G3D_FIX16_PI)
    {
        a -= (g3d_s32)G3D_FIX16_TWO_PI;
    }
    while (a < -(g3d_s32)G3D_FIX16_PI)
    {
        a += (g3d_s32)G3D_FIX16_TWO_PI;
    }
    return (g3d_fix16)a;
}

static g3d_s32 g3d_sin_q15_from_fix16(g3d_fix16 angle)
{
    g3d_s32 x;
    g3d_s32 x2;
    g3d_s32 x3;
    g3d_s32 x5;
    g3d_s32 x7;
    g3d_s32 y;

    angle = g3d_wrap_radians_to_pi_fix16(angle);
    if ((g3d_s32)angle > (g3d_s32)G3D_FIX16_HALF_PI)
    {
        angle = (g3d_fix16)((g3d_s32)G3D_FIX16_PI - (g3d_s32)angle);
    }
    else if ((g3d_s32)angle < -(g3d_s32)G3D_FIX16_HALF_PI)
    {
        angle = (g3d_fix16)(-(g3d_s32)G3D_FIX16_PI - (g3d_s32)angle);
    }

    /* Convert radians in fix16 to signed q15 radians.  The Taylor series is
       evaluated after range reduction to [-pi/2, pi/2]. */
    x = (g3d_s32)angle / 2;
    x2 = g3d_q15_mul(x, x);
    x3 = g3d_q15_mul(x2, x);
    x5 = g3d_q15_mul(x3, x2);
    x7 = g3d_q15_mul(x5, x2);
    y = x - (x3 / 6) + (x5 / 120) - (x7 / 5040);
    return g3d_q15_clamp_unit(y);
}

static g3d_s32 g3d_cos_q15_from_fix16(g3d_fix16 angle)
{
    return g3d_sin_q15_from_fix16((g3d_fix16)((g3d_s32)G3D_FIX16_HALF_PI - (g3d_s32)angle));
}

static void g3d_fvec3_axis_to_q15(const g3d_fvec3 *axis, g3d_s32 *x, g3d_s32 *y, g3d_s32 *z)
{
    g3d_u32 sum;
    g3d_u32 mag;
    g3d_s32 ax;
    g3d_s32 ay;
    g3d_s32 az;

    ax = (axis != NULL) ? g3d_bits_to_q15(axis->x_bits) : 0;
    ay = (axis != NULL) ? g3d_bits_to_q15(axis->y_bits) : 0;
    az = (axis != NULL) ? g3d_bits_to_q15(axis->z_bits) : 0;

    sum = 0u;
    sum += g3d_abs_u32_from_s32(ax) * g3d_abs_u32_from_s32(ax);
    sum += g3d_abs_u32_from_s32(ay) * g3d_abs_u32_from_s32(ay);
    sum += g3d_abs_u32_from_s32(az) * g3d_abs_u32_from_s32(az);
    mag = g3d_isqrt_u32(sum);
    if (mag == 0u)
    {
        *x = 0;
        *y = 0;
        *z = G3D_Q15_ONE;
        return;
    }
    *x = (g3d_s32)(((g3d_s32)ax * (g3d_s32)G3D_Q15_ONE) / (g3d_s32)mag);
    *y = (g3d_s32)(((g3d_s32)ay * (g3d_s32)G3D_Q15_ONE) / (g3d_s32)mag);
    *z = (g3d_s32)(((g3d_s32)az * (g3d_s32)G3D_Q15_ONE) / (g3d_s32)mag);
}

static void g3d_qk_hinge_key_to_q15quat(const g3d_qkframe *qk, g3d_u32 index, g3d_q15quat *out)
{
    g3d_s32 ax;
    g3d_s32 ay;
    g3d_s32 az;
    g3d_fix16 angle;
    g3d_fix16 half_angle;
    g3d_s32 s;
    g3d_s32 c;

    if (out == NULL)
    {
        return;
    }
    if (qk == NULL || qk->angle_bits == NULL || index >= qk->count)
    {
        g3d_q15quat_identity(out);
        return;
    }

    g3d_fvec3_axis_to_q15(&qk->hinge_axis, &ax, &ay, &az);
    angle = g3d_f32bits_to_fix16(qk->angle_bits[index]);
    half_angle = (g3d_fix16)((g3d_s32)angle / 2);
    s = g3d_sin_q15_from_fix16(half_angle);
    c = g3d_cos_q15_from_fix16(half_angle);

    out->x = g3d_q15_mul(ax, s);
    out->y = g3d_q15_mul(ay, s);
    out->z = g3d_q15_mul(az, s);
    out->w = c;
    g3d_q15quat_normalize(out);
}

static int g3d_vk_key_time_fix16(const g3d_vkframe *vk, g3d_u32 index, g3d_fix16 *out_time)
{
    g3d_fix16 start_time;
    if (vk == NULL || out_time == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (index >= vk->count)
    {
        return G3D_ERR_NOT_FOUND;
    }
    if ((vk->compression & G3D_VK_LINEARTIME_COMPRESSION) != 0u)
    {
        g3d_fix16 delta_time;
        if (vk->count == 0u)
        {
            return G3D_ERR_NOT_FOUND;
        }
        start_time = g3d_f32bits_to_fix16(vk->time_bits[0]);
        delta_time = (vk->time_value_count > 1u) ? g3d_f32bits_to_fix16(vk->time_bits[1]) : 0;
        *out_time = g3d_fix16_add_indexed_delta(start_time, delta_time, index);
        return G3D_OK;
    }
    *out_time = g3d_f32bits_to_fix16(vk->time_bits[index]);
    return G3D_OK;
}

static int g3d_qk_key_time_fix16(const g3d_qkframe *qk, g3d_u32 index, g3d_fix16 *out_time)
{
    g3d_fix16 start_time;
    if (qk == NULL || out_time == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (index >= qk->count)
    {
        return G3D_ERR_NOT_FOUND;
    }
    if ((qk->compression & G3D_QK_LINEARTIME_COMPRESSION) != 0u)
    {
        g3d_fix16 delta_time;
        if (qk->count == 0u)
        {
            return G3D_ERR_NOT_FOUND;
        }
        start_time = g3d_f32bits_to_fix16(qk->time_bits[0]);
        delta_time = (qk->time_value_count > 1u) ? g3d_f32bits_to_fix16(qk->time_bits[1]) : 0;
        *out_time = g3d_fix16_add_indexed_delta(start_time, delta_time, index);
        return G3D_OK;
    }
    *out_time = g3d_f32bits_to_fix16(qk->time_bits[index]);
    return G3D_OK;
}

static int g3d_vk_get_extents_fix16(const g3d_vkframe *vk, g3d_fix16 *start_time, g3d_fix16 *end_time)
{
    int rc;
    if (vk == NULL || start_time == NULL || end_time == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (vk->count == 0u)
    {
        return G3D_ERR_NOT_FOUND;
    }
    rc = g3d_vk_key_time_fix16(vk, 0u, start_time);
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_vk_key_time_fix16(vk, vk->count - 1u, end_time);
    return rc;
}

static int g3d_qk_get_extents_fix16(const g3d_qkframe *qk, g3d_fix16 *start_time, g3d_fix16 *end_time)
{
    int rc;
    if (qk == NULL || start_time == NULL || end_time == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (qk->count == 0u)
    {
        return G3D_ERR_NOT_FOUND;
    }
    rc = g3d_qk_key_time_fix16(qk, 0u, start_time);
    if (rc != G3D_OK)
    {
        return rc;
    }
    rc = g3d_qk_key_time_fix16(qk, qk->count - 1u, end_time);
    return rc;
}

static int g3d_vk_find_segment(const g3d_vkframe *vk, g3d_fix16 sample_time,
                               g3d_u32 *out_i0, g3d_u32 *out_i1, g3d_s32 *out_t)
{
    g3d_u32 i;
    g3d_fix16 prev_time;
    g3d_fix16 cur_time;
    int rc;
    if (vk == NULL || out_i0 == NULL || out_i1 == NULL || out_t == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (vk->count == 0u)
    {
        return G3D_ERR_NOT_FOUND;
    }
    if (vk->count == 1u)
    {
        *out_i0 = 0u;
        *out_i1 = 0u;
        *out_t = 0;
        return G3D_OK;
    }
    rc = g3d_vk_key_time_fix16(vk, 0u, &prev_time);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if ((g3d_s32)sample_time <= (g3d_s32)prev_time)
    {
        *out_i0 = 0u;
        *out_i1 = 0u;
        *out_t = 0;
        return G3D_OK;
    }
    for (i = 1u; i < vk->count; ++i)
    {
        rc = g3d_vk_key_time_fix16(vk, i, &cur_time);
        if (rc != G3D_OK)
        {
            return rc;
        }
        if ((g3d_s32)sample_time <= (g3d_s32)cur_time)
        {
            if ((g3d_s32)cur_time <= (g3d_s32)prev_time)
            {
                *out_i0 = i;
                *out_i1 = i;
                *out_t = 0;
                return G3D_OK;
            }
            *out_i0 = i - 1u;
            *out_i1 = i;
            *out_t = g3d_ratio_u32_to_q15((g3d_u32)((g3d_s32)sample_time - (g3d_s32)prev_time),
                                          (g3d_u32)((g3d_s32)cur_time - (g3d_s32)prev_time));
            return G3D_OK;
        }
        prev_time = cur_time;
    }
    *out_i0 = vk->count - 1u;
    *out_i1 = vk->count - 1u;
    *out_t = 0;
    return G3D_OK;
}

static int g3d_qk_find_segment(const g3d_qkframe *qk, g3d_fix16 sample_time,
                               g3d_u32 *out_i0, g3d_u32 *out_i1, g3d_s32 *out_t)
{
    g3d_u32 i;
    g3d_fix16 prev_time;
    g3d_fix16 cur_time;
    int rc;
    if (qk == NULL || out_i0 == NULL || out_i1 == NULL || out_t == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (qk->count == 0u)
    {
        return G3D_ERR_NOT_FOUND;
    }
    if (qk->count == 1u)
    {
        *out_i0 = 0u;
        *out_i1 = 0u;
        *out_t = 0;
        return G3D_OK;
    }
    rc = g3d_qk_key_time_fix16(qk, 0u, &prev_time);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if ((g3d_s32)sample_time <= (g3d_s32)prev_time)
    {
        *out_i0 = 0u;
        *out_i1 = 0u;
        *out_t = 0;
        return G3D_OK;
    }
    for (i = 1u; i < qk->count; ++i)
    {
        rc = g3d_qk_key_time_fix16(qk, i, &cur_time);
        if (rc != G3D_OK)
        {
            return rc;
        }
        if ((g3d_s32)sample_time <= (g3d_s32)cur_time)
        {
            if ((g3d_s32)cur_time <= (g3d_s32)prev_time)
            {
                *out_i0 = i;
                *out_i1 = i;
                *out_t = 0;
                return G3D_OK;
            }
            *out_i0 = i - 1u;
            *out_i1 = i;
            *out_t = g3d_ratio_u32_to_q15((g3d_u32)((g3d_s32)sample_time - (g3d_s32)prev_time),
                                          (g3d_u32)((g3d_s32)cur_time - (g3d_s32)prev_time));
            return G3D_OK;
        }
        prev_time = cur_time;
    }
    *out_i0 = qk->count - 1u;
    *out_i1 = qk->count - 1u;
    *out_t = 0;
    return G3D_OK;
}

static void g3d_fixvec3_lerp(const g3d_fixvec3 *a, const g3d_fixvec3 *b, g3d_s32 t, g3d_fixvec3 *out)
{
    out->x = g3d_lerp_fix16_q15(a->x, b->x, t);
    out->y = g3d_lerp_fix16_q15(a->y, b->y, t);
    out->z = g3d_lerp_fix16_q15(a->z, b->z, t);
}

static void g3d_fixvec3_hermite(const g3d_fixvec3 *pprev, const g3d_fixvec3 *p0,
                                const g3d_fixvec3 *p1, const g3d_fixvec3 *pnext,
                                g3d_s32 t, int zero_deriv, g3d_fixvec3 *out)
{
    g3d_s32 t2;
    g3d_s32 t3;
    g3d_s32 h00;
    g3d_s32 h10;
    g3d_s32 h01;
    g3d_s32 h11;
    g3d_fixvec3 m0;
    g3d_fixvec3 m1;

    t2 = g3d_q15_mul(t, t);
    t3 = g3d_q15_mul(t2, t);

    h00 = (2 * t3) - (3 * t2) + G3D_Q15_ONE;
    h10 = t3 - (2 * t2) + t;
    h01 = (-2 * t3) + (3 * t2);
    h11 = t3 - t2;

    if (zero_deriv)
    {
        g3d_fixvec3_zero(&m0);
        g3d_fixvec3_zero(&m1);
    }
    else
    {
        m0.x = (g3d_fix16)(((g3d_s32)p1->x - (g3d_s32)pprev->x) / 2);
        m0.y = (g3d_fix16)(((g3d_s32)p1->y - (g3d_s32)pprev->y) / 2);
        m0.z = (g3d_fix16)(((g3d_s32)p1->z - (g3d_s32)pprev->z) / 2);
        m1.x = (g3d_fix16)(((g3d_s32)pnext->x - (g3d_s32)p0->x) / 2);
        m1.y = (g3d_fix16)(((g3d_s32)pnext->y - (g3d_s32)p0->y) / 2);
        m1.z = (g3d_fix16)(((g3d_s32)pnext->z - (g3d_s32)p0->z) / 2);
    }

    out->x = (g3d_fix16)(g3d_mul_s32_q15(p0->x, h00) + g3d_mul_s32_q15(m0.x, h10) +
                         g3d_mul_s32_q15(p1->x, h01) + g3d_mul_s32_q15(m1.x, h11));
    out->y = (g3d_fix16)(g3d_mul_s32_q15(p0->y, h00) + g3d_mul_s32_q15(m0.y, h10) +
                         g3d_mul_s32_q15(p1->y, h01) + g3d_mul_s32_q15(m1.y, h11));
    out->z = (g3d_fix16)(g3d_mul_s32_q15(p0->z, h00) + g3d_mul_s32_q15(m0.z, h10) +
                         g3d_mul_s32_q15(p1->z, h01) + g3d_mul_s32_q15(m1.z, h11));
}

static int g3d_vk_sample_fix16(const g3d_vkframe *vk, g3d_u8 looped, g3d_u8 interpolation,
                               g3d_fix16 time, g3d_fixvec3 *out)
{
    g3d_fix16 start_time;
    g3d_fix16 end_time;
    g3d_fix16 sample_time;
    g3d_u32 i0;
    g3d_u32 i1;
    g3d_s32 t;
    g3d_fixvec3 pprev;
    g3d_fixvec3 p0;
    g3d_fixvec3 p1;
    g3d_fixvec3 pnext;
    int rc;

    if (vk == NULL || out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (vk->count == 0u)
    {
        g3d_fixvec3_zero(out);
        return G3D_OK;
    }
    rc = g3d_vk_get_extents_fix16(vk, &start_time, &end_time);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (looped != 0u)
    {
        sample_time = g3d_wrap_fix16_time(time, start_time, end_time);
    }
    else
    {
        sample_time = time;
        if ((g3d_s32)sample_time < (g3d_s32)start_time)
        {
            sample_time = start_time;
        }
        if ((g3d_s32)sample_time > (g3d_s32)end_time)
        {
            sample_time = end_time;
        }
    }

    rc = g3d_vk_find_segment(vk, sample_time, &i0, &i1, &t);
    if (rc != G3D_OK)
    {
        return rc;
    }

    g3d_fvec3_to_fixvec3(&vk->vectors[i0], &p0);
    if (i1 == i0)
    {
        *out = p0;
        return G3D_OK;
    }
    g3d_fvec3_to_fixvec3(&vk->vectors[i1], &p1);

    if (interpolation == G3D_PATH_VK_LINEAR)
    {
        g3d_fixvec3_lerp(&p0, &p1, t, out);
        return G3D_OK;
    }

    if (i0 == 0u)
    {
        pprev = p0;
    }
    else
    {
        g3d_fvec3_to_fixvec3(&vk->vectors[i0 - 1u], &pprev);
    }
    if (i1 + 1u >= vk->count)
    {
        pnext = p1;
    }
    else
    {
        g3d_fvec3_to_fixvec3(&vk->vectors[i1 + 1u], &pnext);
    }

    g3d_fixvec3_hermite(&pprev, &p0, &p1, &pnext, t,
                        (interpolation == G3D_PATH_VK_HERMITE_ZERO_DERIV) ? G3D_TRUE : G3D_FALSE,
                        out);
    return G3D_OK;
}

static int g3d_qk_sample_fix16(const g3d_qkframe *qk, g3d_u8 looped, g3d_u8 interpolation,
                               g3d_fix16 time, g3d_q15quat *out)
{
    g3d_fix16 start_time;
    g3d_fix16 end_time;
    g3d_fix16 sample_time;
    g3d_u32 i0;
    g3d_u32 i1;
    g3d_s32 t;
    g3d_q15quat q0;
    g3d_q15quat q1;
    int rc;

    if (qk == NULL || out == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (qk->count == 0u)
    {
        g3d_q15quat_identity(out);
        return G3D_OK;
    }
    rc = g3d_qk_get_extents_fix16(qk, &start_time, &end_time);
    if (rc != G3D_OK)
    {
        return rc;
    }
    if (looped != 0u)
    {
        sample_time = g3d_wrap_fix16_time(time, start_time, end_time);
    }
    else
    {
        sample_time = time;
        if ((g3d_s32)sample_time < (g3d_s32)start_time)
        {
            sample_time = start_time;
        }
        if ((g3d_s32)sample_time > (g3d_s32)end_time)
        {
            sample_time = end_time;
        }
    }

    rc = g3d_qk_find_segment(qk, sample_time, &i0, &i1, &t);
    if (rc != G3D_OK)
    {
        return rc;
    }

    if ((qk->compression & G3D_QK_HINGE_COMPRESSION) != 0u)
    {
        g3d_qk_hinge_key_to_q15quat(qk, i0, &q0);
    }
    else
    {
        g3d_fquat_to_q15quat(&qk->quats[i0], &q0);
        g3d_q15quat_normalize(&q0);
    }
    if (i1 == i0)
    {
        *out = q0;
        return G3D_OK;
    }
    if ((qk->compression & G3D_QK_HINGE_COMPRESSION) != 0u)
    {
        g3d_qk_hinge_key_to_q15quat(qk, i1, &q1);
    }
    else
    {
        g3d_fquat_to_q15quat(&qk->quats[i1], &q1);
        g3d_q15quat_normalize(&q1);
    }

    (void)interpolation;
    g3d_q15quat_nlerp(&q0, &q1, t, out);
    return G3D_OK;
}

int g3d_path_get_time_extents_fix16(const g3d_path *path, g3d_fix16 *start_time, g3d_fix16 *end_time)
{
    g3d_fix16 s;
    g3d_fix16 e;
    int have_any;
    int rc;
    if (path == NULL || start_time == NULL || end_time == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    have_any = G3D_FALSE;
    if (path->has_translation != 0u)
    {
        rc = g3d_vk_get_extents_fix16(&path->translation, &s, &e);
        if (rc == G3D_OK)
        {
            *start_time = s;
            *end_time = e;
            have_any = G3D_TRUE;
        }
    }
    if (path->has_rotation != 0u)
    {
        rc = g3d_qk_get_extents_fix16(&path->rotation, &s, &e);
        if (rc == G3D_OK)
        {
            if (!have_any || (g3d_s32)s < (g3d_s32)*start_time)
            {
                *start_time = s;
            }
            if (!have_any || (g3d_s32)e > (g3d_s32)*end_time)
            {
                *end_time = e;
            }
            have_any = G3D_TRUE;
        }
    }
    if (!have_any)
    {
        return G3D_ERR_NOT_FOUND;
    }
    return G3D_OK;
}

int g3d_motion_get_time_extents_fix16(const g3d_motion *motion, g3d_fix16 *start_time, g3d_fix16 *end_time)
{
    g3d_u32 i;
    g3d_fix16 s;
    g3d_fix16 e;
    int have_any;
    int rc;
    if (motion == NULL || start_time == NULL || end_time == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (motion->node_type != G3D_MOTION_NODE_LEAF)
    {
        return G3D_ERR_UNSUPPORTED;
    }
    have_any = G3D_FALSE;
    for (i = 0u; i < (g3d_u32)motion->leaf.path_count; ++i)
    {
        rc = g3d_path_get_time_extents_fix16(&motion->leaf.paths[i], &s, &e);
        if (rc == G3D_OK)
        {
            if (!have_any || (g3d_s32)s < (g3d_s32)*start_time)
            {
                *start_time = s;
            }
            if (!have_any || (g3d_s32)e > (g3d_s32)*end_time)
            {
                *end_time = e;
            }
            have_any = G3D_TRUE;
        }
    }
    if (!have_any)
    {
        return G3D_ERR_NOT_FOUND;
    }
    return G3D_OK;
}

int g3d_path_sample_channels_fix16(const g3d_path *path, g3d_fix16 time, g3d_q15quat *rotation, g3d_fixvec3 *translation)
{
    int rc;
    if (path == NULL || (rotation == NULL && translation == NULL))
    {
        return G3D_ERR_BAD_ARG;
    }
    if (translation != NULL)
    {
        if (path->has_translation != 0u)
        {
            rc = g3d_vk_sample_fix16(&path->translation, path->looped, path->translation_interp, time, translation);
            if (rc != G3D_OK)
            {
                return rc;
            }
        }
        else
        {
            g3d_fixvec3_zero(translation);
        }
    }
    if (rotation != NULL)
    {
        if (path->has_rotation != 0u)
        {
            rc = g3d_qk_sample_fix16(&path->rotation, path->looped, path->rotation_interp, time, rotation);
            if (rc != G3D_OK)
            {
                return rc;
            }
        }
        else
        {
            g3d_q15quat_identity(rotation);
        }
    }
    return G3D_OK;
}

int g3d_motion_sample_channels_fix16(const g3d_motion *motion, g3d_s32 path_index, g3d_fix16 time,
                                     g3d_q15quat *rotation, g3d_fixvec3 *translation)
{
    if (motion == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (motion->node_type != G3D_MOTION_NODE_LEAF)
    {
        return G3D_ERR_UNSUPPORTED;
    }
    if (path_index < 0 || path_index >= motion->leaf.path_count)
    {
        return G3D_ERR_NOT_FOUND;
    }
    return g3d_path_sample_channels_fix16(&motion->leaf.paths[(g3d_u32)path_index], time, rotation, translation);
}

int g3d_motion_sample_channels_named_fix16(const g3d_motion *motion, const char *path_name, g3d_fix16 time,
                                           g3d_q15quat *rotation, g3d_fixvec3 *translation)
{
    g3d_s32 index;
    int rc;
    if (motion == NULL || path_name == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    rc = g3d_motion_find_path_index(motion, path_name, &index);
    if (rc != G3D_OK)
    {
        return rc;
    }
    return g3d_motion_sample_channels_fix16(motion, index, time, rotation, translation);
}

void g3d_sample_xform_to_matrix(const g3d_q15quat *rotation, const g3d_fixvec3 *translation, g3d_fixmat34 *out_matrix)
{
    g3d_q15quat q;
    g3d_fixvec3 t;
    g3d_s32 xx;
    g3d_s32 yy;
    g3d_s32 zz;
    g3d_s32 xy;
    g3d_s32 xz;
    g3d_s32 yz;
    g3d_s32 wx;
    g3d_s32 wy;
    g3d_s32 wz;

    if (out_matrix == NULL)
    {
        return;
    }

    if (rotation != NULL)
    {
        q = *rotation;
        g3d_q15quat_normalize(&q);
    }
    else
    {
        g3d_q15quat_identity(&q);
    }

    if (translation != NULL)
    {
        t = *translation;
    }
    else
    {
        g3d_fixvec3_zero(&t);
    }

    xx = g3d_q15_mul(q.x, q.x);
    yy = g3d_q15_mul(q.y, q.y);
    zz = g3d_q15_mul(q.z, q.z);
    xy = g3d_q15_mul(q.x, q.y);
    xz = g3d_q15_mul(q.x, q.z);
    yz = g3d_q15_mul(q.y, q.z);
    wx = g3d_q15_mul(q.w, q.x);
    wy = g3d_q15_mul(q.w, q.y);
    wz = g3d_q15_mul(q.w, q.z);

    out_matrix->m[0] = (g3d_fix16)((G3D_Q15_ONE - (yy << 1) - (zz << 1)) << 1);
    out_matrix->m[1] = (g3d_fix16)(((xy << 1) - (wz << 1)) << 1);
    out_matrix->m[2] = (g3d_fix16)(((xz << 1) + (wy << 1)) << 1);
    out_matrix->m[3] = t.x;

    out_matrix->m[4] = (g3d_fix16)(((xy << 1) + (wz << 1)) << 1);
    out_matrix->m[5] = (g3d_fix16)((G3D_Q15_ONE - (xx << 1) - (zz << 1)) << 1);
    out_matrix->m[6] = (g3d_fix16)(((yz << 1) - (wx << 1)) << 1);
    out_matrix->m[7] = t.y;

    out_matrix->m[8] = (g3d_fix16)(((xz << 1) - (wy << 1)) << 1);
    out_matrix->m[9] = (g3d_fix16)(((yz << 1) + (wx << 1)) << 1);
    out_matrix->m[10] = (g3d_fix16)((G3D_Q15_ONE - (xx << 1) - (yy << 1)) << 1);
    out_matrix->m[11] = t.z;
}


static g3d_s32 g3d_mul_s32_fix16(g3d_s32 value, g3d_s32 weight)
{
    int neg;
    g3d_u32 mag;
    g3d_u32 w;
    g3d_u32 ah;
    g3d_u32 al;
    g3d_u32 bh;
    g3d_u32 bl;
    g3d_u32 out_mag;
    g3d_s32 out;

    neg = 0;
    if (value < 0)
    {
        mag = g3d_abs_u32_from_s32(value);
        neg = !neg;
    }
    else
    {
        mag = (g3d_u32)value;
    }
    if (weight < 0)
    {
        w = g3d_abs_u32_from_s32(weight);
        neg = !neg;
    }
    else
    {
        w = (g3d_u32)weight;
    }

    /* 16.16 multiply without long long and without overflow-prone
       (hi * full_operand) intermediates.  Split both operands into
       16-bit halves:
         (ah*65536 + al) * (bh*65536 + bl) / 65536
       = (ah*bh)<<16 + ah*bl + bh*al + (al*bl)>>16.
       This fixes Genesis3D bone-chain accumulation for real ACT files. */
    ah = (mag >> 16);
    al = (mag & 0xffffu);
    bh = (w >> 16);
    bl = (w & 0xffffu);

    out_mag = (ah * bh) << 16;
    out_mag += (ah * bl);
    out_mag += (bh * al);
    out_mag += ((al * bl) + 0x8000u) >> 16;

    out = (g3d_s32)out_mag;
    if (neg)
    {
        out = -out;
    }
    return out;
}

static g3d_fix16 g3d_fix16_mul(g3d_fix16 a, g3d_fix16 b)
{
    return (g3d_fix16)g3d_mul_s32_fix16((g3d_s32)a, (g3d_s32)b);
}

static g3d_s32 g3d_q15_clamp(g3d_s32 v)
{
    if (v < 0)
    {
        return 0;
    }
    if (v > G3D_Q15_ONE)
    {
        return G3D_Q15_ONE;
    }
    return v;
}

static g3d_s32 g3d_q15_blend_curve(g3d_s32 t, g3d_u32 blend_type)
{
    g3d_s32 t2;
    g3d_s32 t3;
    t = g3d_q15_clamp(t);
    if (blend_type == G3D_POSE_BLEND_HERMITE)
    {
        t2 = g3d_q15_mul(t, t);
        t3 = g3d_q15_mul(t2, t);
        t = (t2 * 3) - (t3 * 2);
        t = g3d_q15_clamp(t);
    }
    return t;
}

static void g3d_fixmat34_from_fxform(const g3d_fxform *src, g3d_fixmat34 *dst)
{
    if (dst == NULL)
    {
        return;
    }
    if (src == NULL)
    {
        g3d_fixmat34_identity(dst);
        return;
    }

    /* Genesis3D geXForm3d is laid out as:
       AX AY AZ  BX BY BZ  CX CY CZ  Translation.X/Y/Z.
       The internal g3d_fixmat34 is row-major 3x4:
       AX AY AZ TX / BX BY BZ TY / CX CY CZ TZ. */
    dst->m[0]  = g3d_f32bits_to_fix16(src->f[0]);
    dst->m[1]  = g3d_f32bits_to_fix16(src->f[1]);
    dst->m[2]  = g3d_f32bits_to_fix16(src->f[2]);
    dst->m[3]  = g3d_f32bits_to_fix16(src->f[9]);

    dst->m[4]  = g3d_f32bits_to_fix16(src->f[3]);
    dst->m[5]  = g3d_f32bits_to_fix16(src->f[4]);
    dst->m[6]  = g3d_f32bits_to_fix16(src->f[5]);
    dst->m[7]  = g3d_f32bits_to_fix16(src->f[10]);

    dst->m[8]  = g3d_f32bits_to_fix16(src->f[6]);
    dst->m[9]  = g3d_f32bits_to_fix16(src->f[7]);
    dst->m[10] = g3d_f32bits_to_fix16(src->f[8]);
    dst->m[11] = g3d_f32bits_to_fix16(src->f[11]);
}

static void g3d_fixmat34_mul(const g3d_fixmat34 *a, const g3d_fixmat34 *b, g3d_fixmat34 *out)
{
    g3d_fixmat34 t;
    if (a == NULL || b == NULL || out == NULL)
    {
        return;
    }

    t.m[0] = (g3d_fix16)(g3d_fix16_mul(a->m[0], b->m[0]) + g3d_fix16_mul(a->m[1], b->m[4]) + g3d_fix16_mul(a->m[2], b->m[8]));
    t.m[1] = (g3d_fix16)(g3d_fix16_mul(a->m[0], b->m[1]) + g3d_fix16_mul(a->m[1], b->m[5]) + g3d_fix16_mul(a->m[2], b->m[9]));
    t.m[2] = (g3d_fix16)(g3d_fix16_mul(a->m[0], b->m[2]) + g3d_fix16_mul(a->m[1], b->m[6]) + g3d_fix16_mul(a->m[2], b->m[10]));
    t.m[3] = (g3d_fix16)(g3d_fix16_mul(a->m[0], b->m[3]) + g3d_fix16_mul(a->m[1], b->m[7]) + g3d_fix16_mul(a->m[2], b->m[11]) + a->m[3]);

    t.m[4] = (g3d_fix16)(g3d_fix16_mul(a->m[4], b->m[0]) + g3d_fix16_mul(a->m[5], b->m[4]) + g3d_fix16_mul(a->m[6], b->m[8]));
    t.m[5] = (g3d_fix16)(g3d_fix16_mul(a->m[4], b->m[1]) + g3d_fix16_mul(a->m[5], b->m[5]) + g3d_fix16_mul(a->m[6], b->m[9]));
    t.m[6] = (g3d_fix16)(g3d_fix16_mul(a->m[4], b->m[2]) + g3d_fix16_mul(a->m[5], b->m[6]) + g3d_fix16_mul(a->m[6], b->m[10]));
    t.m[7] = (g3d_fix16)(g3d_fix16_mul(a->m[4], b->m[3]) + g3d_fix16_mul(a->m[5], b->m[7]) + g3d_fix16_mul(a->m[6], b->m[11]) + a->m[7]);

    t.m[8] = (g3d_fix16)(g3d_fix16_mul(a->m[8], b->m[0]) + g3d_fix16_mul(a->m[9], b->m[4]) + g3d_fix16_mul(a->m[10], b->m[8]));
    t.m[9] = (g3d_fix16)(g3d_fix16_mul(a->m[8], b->m[1]) + g3d_fix16_mul(a->m[9], b->m[5]) + g3d_fix16_mul(a->m[10], b->m[9]));
    t.m[10] = (g3d_fix16)(g3d_fix16_mul(a->m[8], b->m[2]) + g3d_fix16_mul(a->m[9], b->m[6]) + g3d_fix16_mul(a->m[10], b->m[10]));
    t.m[11] = (g3d_fix16)(g3d_fix16_mul(a->m[8], b->m[3]) + g3d_fix16_mul(a->m[9], b->m[7]) + g3d_fix16_mul(a->m[10], b->m[11]) + a->m[11]);

    *out = t;
}

static void g3d_fixmat34_transform_point(const g3d_fixmat34 *m, const g3d_fixvec3 *v, g3d_fixvec3 *out)
{
    if (m == NULL || v == NULL || out == NULL)
    {
        return;
    }
    out->x = (g3d_fix16)(g3d_fix16_mul(m->m[0], v->x) + g3d_fix16_mul(m->m[1], v->y) + g3d_fix16_mul(m->m[2], v->z) + m->m[3]);
    out->y = (g3d_fix16)(g3d_fix16_mul(m->m[4], v->x) + g3d_fix16_mul(m->m[5], v->y) + g3d_fix16_mul(m->m[6], v->z) + m->m[7]);
    out->z = (g3d_fix16)(g3d_fix16_mul(m->m[8], v->x) + g3d_fix16_mul(m->m[9], v->y) + g3d_fix16_mul(m->m[10], v->z) + m->m[11]);
}

static void g3d_fixmat34_transform_vector(const g3d_fixmat34 *m, const g3d_fixvec3 *v, g3d_fixvec3 *out)
{
    if (m == NULL || v == NULL || out == NULL)
    {
        return;
    }
    out->x = (g3d_fix16)(g3d_fix16_mul(m->m[0], v->x) + g3d_fix16_mul(m->m[1], v->y) + g3d_fix16_mul(m->m[2], v->z));
    out->y = (g3d_fix16)(g3d_fix16_mul(m->m[4], v->x) + g3d_fix16_mul(m->m[5], v->y) + g3d_fix16_mul(m->m[6], v->z));
    out->z = (g3d_fix16)(g3d_fix16_mul(m->m[8], v->x) + g3d_fix16_mul(m->m[9], v->y) + g3d_fix16_mul(m->m[10], v->z));
}

static int g3d_body_motion_exact_match(const g3d_body *body, const g3d_motion *motion)
{
    g3d_u32 i;
    if (body == NULL || motion == NULL)
    {
        return G3D_FALSE;
    }
    if (motion->node_type != G3D_MOTION_NODE_LEAF)
    {
        return G3D_FALSE;
    }
    if ((g3d_u32)body->bone_count != (g3d_u32)motion->leaf.path_count)
    {
        return G3D_FALSE;
    }
    if (body->bone_names == NULL || motion->leaf.name_array == NULL)
    {
        return G3D_TRUE;
    }
    for (i = 0u; i < (g3d_u32)body->bone_count; ++i)
    {
        const char *bn;
        const char *mn;
        bn = g3d_strblock_get(body->bone_names, (g3d_s32)i);
        mn = g3d_strblock_get(motion->leaf.name_array, (g3d_s32)i);
        if (bn == NULL || mn == NULL)
        {
            return G3D_FALSE;
        }
        if (!g3d_str_eq(bn, mn))
        {
            return G3D_FALSE;
        }
    }
    return G3D_TRUE;
}

static int g3d_body_sample_motion_bone_local(const g3d_body *body, const g3d_motion *motion,
                                             g3d_u32 bone_index, g3d_fix16 time,
                                             g3d_sample_xform *out_pose)
{
    int rc;
    const char *bone_name;

    if (body == NULL || out_pose == NULL || bone_index >= (g3d_u32)body->bone_count)
    {
        return G3D_ERR_BAD_ARG;
    }

    g3d_sample_xform_identity(out_pose);

    if (motion == NULL)
    {
        return G3D_OK;
    }
    if (motion->node_type != G3D_MOTION_NODE_LEAF)
    {
        return G3D_ERR_UNSUPPORTED;
    }

    if (g3d_body_motion_exact_match(body, motion) != G3D_FALSE)
    {
        return g3d_motion_sample_channels_fix16(motion, (g3d_s32)bone_index, time,
                                                &out_pose->rotation, &out_pose->translation);
    }

    bone_name = (body->bone_names != NULL) ? g3d_strblock_get(body->bone_names, (g3d_s32)bone_index) : NULL;
    if (bone_name != NULL && motion->leaf.name_array != NULL)
    {
        rc = g3d_motion_sample_channels_named_fix16(motion, bone_name, time,
                                                    &out_pose->rotation, &out_pose->translation);
        if (rc == G3D_OK)
        {
            return G3D_OK;
        }
        if (rc != G3D_ERR_NOT_FOUND)
        {
            return rc;
        }
        g3d_sample_xform_identity(out_pose);
    }

    if (bone_index < (g3d_u32)motion->leaf.path_count)
    {
        rc = g3d_motion_sample_channels_fix16(motion, (g3d_s32)bone_index, time,
                                              &out_pose->rotation, &out_pose->translation);
        if (rc == G3D_OK || rc == G3D_ERR_NOT_FOUND)
        {
            return G3D_OK;
        }
        return rc;
    }

    return G3D_OK;
}

static void g3d_sample_xform_blend(g3d_sample_xform *dst, const g3d_sample_xform *src, g3d_s32 t)
{
    if (dst == NULL || src == NULL)
    {
        return;
    }
    t = g3d_q15_clamp(t);
    g3d_q15quat_nlerp(&dst->rotation, &src->rotation, t, &dst->rotation);
    dst->translation.x = g3d_lerp_fix16_q15(dst->translation.x, src->translation.x, t);
    dst->translation.y = g3d_lerp_fix16_q15(dst->translation.y, src->translation.y, t);
    dst->translation.z = g3d_lerp_fix16_q15(dst->translation.z, src->translation.z, t);
}

int g3d_body_sample_local_pose_fix16(const g3d_body *body, const g3d_motion *motion, g3d_fix16 time,
                                     g3d_sample_xform *out_pose, g3d_u32 pose_count)
{
    g3d_u32 i;
    int rc;

    if (body == NULL || out_pose == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (pose_count < (g3d_u32)body->bone_count)
    {
        return G3D_ERR_NO_SPACE;
    }

    for (i = 0u; i < (g3d_u32)body->bone_count; ++i)
    {
        rc = g3d_body_sample_motion_bone_local(body, motion, i, time, &out_pose[i]);
        if (rc != G3D_OK)
        {
            return rc;
        }
    }
    return G3D_OK;
}

int g3d_body_blend_local_pose_fix16(const g3d_body *body,
                                    const g3d_motion *base_motion, g3d_fix16 base_time,
                                    const g3d_motion *blend_motion, g3d_fix16 blend_time,
                                    g3d_s32 blend_amount_q15, g3d_u32 blend_type,
                                    g3d_sample_xform *out_pose, g3d_u32 pose_count)
{
    g3d_u32 i;
    int rc;
    g3d_s32 blend_amount;

    if (blend_type != G3D_POSE_BLEND_LINEAR && blend_type != G3D_POSE_BLEND_HERMITE)
    {
        return G3D_ERR_BAD_ARG;
    }

    rc = g3d_body_sample_local_pose_fix16(body, base_motion, base_time, out_pose, pose_count);
    if (rc != G3D_OK)
    {
        return rc;
    }

    if (body == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (blend_motion == NULL)
    {
        return G3D_OK;
    }

    blend_amount = g3d_q15_blend_curve(blend_amount_q15, blend_type);
    if (blend_amount == 0)
    {
        return G3D_OK;
    }

    for (i = 0u; i < (g3d_u32)body->bone_count; ++i)
    {
        g3d_sample_xform blend_pose;
        rc = g3d_body_sample_motion_bone_local(body, blend_motion, i, blend_time, &blend_pose);
        if (rc != G3D_OK)
        {
            return rc;
        }
        g3d_sample_xform_blend(&out_pose[i], &blend_pose, blend_amount);
    }
    return G3D_OK;
}

int g3d_body_local_pose_to_matrices_fix16(const g3d_body *body,
                                          const g3d_sample_xform *local_pose, g3d_u32 pose_count,
                                          g3d_fixmat34 *out_matrices, g3d_u32 matrix_count)
{
    g3d_u32 i;
    if (body == NULL || local_pose == NULL || out_matrices == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (pose_count < (g3d_u32)body->bone_count || matrix_count < (g3d_u32)body->bone_count)
    {
        return G3D_ERR_NO_SPACE;
    }

    for (i = 0u; i < (g3d_u32)body->bone_count; ++i)
    {
        g3d_fixmat34 attachment;
        g3d_fixmat34 local;
        g3d_fixmat34 bone_local;
        g3d_s16 parent;

        g3d_fixmat34_from_fxform(&body->bones[i].attachment, &attachment);
        g3d_sample_xform_to_matrix(&local_pose[i].rotation, &local_pose[i].translation, &local);
        g3d_fixmat34_mul(&attachment, &local, &bone_local);

        parent = body->bones[i].parent_bone_index;
        if (parent < 0)
        {
            out_matrices[i] = bone_local;
        }
        else
        {
            if ((g3d_u32)parent >= i)
            {
                return G3D_ERR_CORRUPT;
            }
            g3d_fixmat34_mul(&out_matrices[(g3d_u32)parent], &bone_local, &out_matrices[i]);
        }
    }
    return G3D_OK;
}

int g3d_body_skin_vertices_fix16(const g3d_body *body,
                                 const g3d_fixmat34 *bone_matrices, g3d_u32 matrix_count,
                                 g3d_fixvec3 *out_vertices, g3d_u32 vertex_count,
                                 g3d_fixvec3 *out_normals, g3d_u32 normal_count)
{
    g3d_u32 i;
    if (body == NULL || bone_matrices == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (vertex_count < (g3d_u32)body->xskin_vertex_count || normal_count < (g3d_u32)body->normal_count)
    {
        return G3D_ERR_NO_SPACE;
    }

    if (out_vertices != NULL)
    {
        for (i = 0u; i < (g3d_u32)body->xskin_vertex_count; ++i)
        {
            g3d_fixvec3 local_v;
            g3d_s16 bone_index;
            bone_index = body->xskin_vertices[i].bone_index;
            if (bone_index < 0 || (g3d_u32)bone_index >= matrix_count)
            {
                return G3D_ERR_CORRUPT;
            }
            g3d_fvec3_to_fixvec3(&body->xskin_vertices[i].xpoint, &local_v);
            g3d_fixmat34_transform_point(&bone_matrices[(g3d_u32)bone_index], &local_v, &out_vertices[i]);
        }
    }

    if (out_normals != NULL)
    {
        for (i = 0u; i < (g3d_u32)body->normal_count; ++i)
        {
            g3d_fixvec3 local_n;
            g3d_s16 bone_index;
            bone_index = body->normals[i].bone_index;
            if (bone_index < 0 || (g3d_u32)bone_index >= matrix_count)
            {
                return G3D_ERR_CORRUPT;
            }
            g3d_fvec3_to_fixvec3(&body->normals[i].normal, &local_n);
            g3d_fixmat34_transform_vector(&bone_matrices[(g3d_u32)bone_index], &local_n, &out_normals[i]);
        }
    }

    return G3D_OK;
}

int g3d_body_alloc_skinned_mesh(g3d_arena *arena, const g3d_body *body, g3d_skinned_mesh *mesh)
{
    if (arena == NULL || body == NULL || mesh == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }

    mesh->bone_count = (g3d_u32)body->bone_count;
    mesh->vertex_count = (g3d_u32)body->xskin_vertex_count;
    mesh->normal_count = (g3d_u32)body->normal_count;
    mesh->local_pose = NULL;
    mesh->bone_matrices = NULL;
    mesh->vertices = NULL;
    mesh->normals = NULL;

    if (mesh->bone_count != 0u)
    {
        mesh->local_pose = (g3d_sample_xform *)g3d_arena_alloc(arena,
            (g3d_u32)(sizeof(g3d_sample_xform) * mesh->bone_count), 4u);
        if (mesh->local_pose == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
        mesh->bone_matrices = (g3d_fixmat34 *)g3d_arena_alloc(arena,
            (g3d_u32)(sizeof(g3d_fixmat34) * mesh->bone_count), 4u);
        if (mesh->bone_matrices == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
    }
    if (mesh->vertex_count != 0u)
    {
        mesh->vertices = (g3d_fixvec3 *)g3d_arena_alloc(arena,
            (g3d_u32)(sizeof(g3d_fixvec3) * mesh->vertex_count), 4u);
        if (mesh->vertices == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
    }
    if (mesh->normal_count != 0u)
    {
        mesh->normals = (g3d_fixvec3 *)g3d_arena_alloc(arena,
            (g3d_u32)(sizeof(g3d_fixvec3) * mesh->normal_count), 4u);
        if (mesh->normals == NULL)
        {
            return G3D_ERR_ARENA_EXHAUSTED;
        }
    }
    return G3D_OK;
}

static int g3d_body_sample_skinned_common(const g3d_body *body,
                                          const g3d_motion *base_motion, g3d_fix16 base_time,
                                          const g3d_motion *blend_motion, g3d_fix16 blend_time,
                                          g3d_s32 blend_amount_q15, g3d_u32 blend_type,
                                          int do_blend,
                                          g3d_skinned_mesh *mesh)
{
    int rc;
    if (body == NULL || mesh == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (mesh->bone_count < (g3d_u32)body->bone_count || mesh->vertex_count < (g3d_u32)body->xskin_vertex_count ||
        mesh->normal_count < (g3d_u32)body->normal_count)
    {
        return G3D_ERR_NO_SPACE;
    }
    if (((g3d_u32)body->bone_count != 0u) && (mesh->local_pose == NULL || mesh->bone_matrices == NULL))
    {
        return G3D_ERR_BAD_ARG;
    }
    if (((g3d_u32)body->xskin_vertex_count != 0u) && mesh->vertices == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (((g3d_u32)body->normal_count != 0u) && mesh->normals == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }

    if (do_blend)
    {
        rc = g3d_body_blend_local_pose_fix16(body, base_motion, base_time, blend_motion, blend_time,
                                             blend_amount_q15, blend_type,
                                             mesh->local_pose, mesh->bone_count);
    }
    else
    {
        rc = g3d_body_sample_local_pose_fix16(body, base_motion, base_time,
                                              mesh->local_pose, mesh->bone_count);
    }
    if (rc != G3D_OK)
    {
        return rc;
    }

    rc = g3d_body_local_pose_to_matrices_fix16(body, mesh->local_pose, mesh->bone_count,
                                               mesh->bone_matrices, mesh->bone_count);
    if (rc != G3D_OK)
    {
        return rc;
    }

    return g3d_body_skin_vertices_fix16(body, mesh->bone_matrices, mesh->bone_count,
                                        mesh->vertices, mesh->vertex_count,
                                        mesh->normals, mesh->normal_count);
}

int g3d_body_sample_skinned_fix16(const g3d_body *body, const g3d_motion *motion, g3d_fix16 time,
                                  g3d_skinned_mesh *mesh)
{
    return g3d_body_sample_skinned_common(body, motion, time, NULL, 0,
                                          0, G3D_POSE_BLEND_LINEAR, G3D_FALSE, mesh);
}

int g3d_body_sample_skinned_blend_fix16(const g3d_body *body,
                                        const g3d_motion *base_motion, g3d_fix16 base_time,
                                        const g3d_motion *blend_motion, g3d_fix16 blend_time,
                                        g3d_s32 blend_amount_q15, g3d_u32 blend_type,
                                        g3d_skinned_mesh *mesh)
{
    return g3d_body_sample_skinned_common(body, base_motion, base_time,
                                          blend_motion, blend_time,
                                          blend_amount_q15, blend_type,
                                          G3D_TRUE, mesh);
}

int g3d_actor_sample_skinned_fix16(const g3d_actor *actor, g3d_s32 motion_index, g3d_fix16 time,
                                   g3d_skinned_mesh *mesh)
{
    const g3d_motion *motion;
    if (actor == NULL || mesh == NULL || actor->has_body == 0u || actor->body == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    motion = NULL;
    if (motion_index >= 0)
    {
        if (motion_index >= actor->motion_count || actor->motions == NULL)
        {
            return G3D_ERR_NOT_FOUND;
        }
        motion = &actor->motions[(g3d_u32)motion_index];
    }
    return g3d_body_sample_skinned_fix16(actor->body, motion, time, mesh);
}

int g3d_actor_sample_skinned_blend_fix16(const g3d_actor *actor,
                                         g3d_s32 base_motion_index, g3d_fix16 base_time,
                                         g3d_s32 blend_motion_index, g3d_fix16 blend_time,
                                         g3d_s32 blend_amount_q15, g3d_u32 blend_type,
                                         g3d_skinned_mesh *mesh)
{
    const g3d_motion *base_motion;
    const g3d_motion *blend_motion;
    if (actor == NULL || mesh == NULL || actor->has_body == 0u || actor->body == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    base_motion = NULL;
    blend_motion = NULL;
    if (base_motion_index >= 0)
    {
        if (base_motion_index >= actor->motion_count || actor->motions == NULL)
        {
            return G3D_ERR_NOT_FOUND;
        }
        base_motion = &actor->motions[(g3d_u32)base_motion_index];
    }
    if (blend_motion_index >= 0)
    {
        if (blend_motion_index >= actor->motion_count || actor->motions == NULL)
        {
            return G3D_ERR_NOT_FOUND;
        }
        blend_motion = &actor->motions[(g3d_u32)blend_motion_index];
    }
    return g3d_body_sample_skinned_blend_fix16(actor->body,
                                               base_motion, base_time,
                                               blend_motion, blend_time,
                                               blend_amount_q15, blend_type,
                                               mesh);
}


void g3d_asset_manager_init(g3d_asset_manager *mgr, g3d_asset_slot *slots, g3d_u32 capacity)
{
    g3d_u32 i;
    if (mgr != NULL)
    {
        mgr->slots = slots;
        mgr->count = 0u;
        mgr->capacity = capacity;
        if (slots != NULL)
        {
            for (i = 0u; i < capacity; ++i)
            {
                slots[i].name = NULL;
                slots[i].type = 0u;
                slots[i].object = NULL;
            }
        }
    }
}

int g3d_asset_manager_add(g3d_asset_manager *mgr, g3d_arena *arena, const char *name, g3d_u32 type, void *object)
{
    if (mgr == NULL || arena == NULL || name == NULL || object == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    if (g3d_asset_manager_find(mgr, name, type) != NULL)
    {
        return G3D_ERR_CORRUPT;
    }
    if (mgr->count >= mgr->capacity)
    {
        return G3D_ERR_NO_SPACE;
    }
    mgr->slots[mgr->count].name = g3d_arena_strdup(arena, name);
    if (mgr->slots[mgr->count].name == NULL)
    {
        return G3D_ERR_ARENA_EXHAUSTED;
    }
    mgr->slots[mgr->count].type = type;
    mgr->slots[mgr->count].object = object;
    ++mgr->count;
    return G3D_OK;
}

int g3d_asset_manager_upsert(g3d_asset_manager *mgr, g3d_arena *arena, const char *name, g3d_u32 type, void *object)
{
    g3d_u32 i;
    if (mgr == NULL || name == NULL || object == NULL)
    {
        return G3D_ERR_BAD_ARG;
    }
    for (i = 0u; i < mgr->count; ++i)
    {
        if (mgr->slots[i].type == type && g3d_str_eq(mgr->slots[i].name, name) == G3D_TRUE)
        {
            mgr->slots[i].object = object;
            return G3D_OK;
        }
    }
    return g3d_asset_manager_add(mgr, arena, name, type, object);
}

g3d_u32 g3d_asset_manager_count_type(const g3d_asset_manager *mgr, g3d_u32 type)
{
    g3d_u32 i;
    g3d_u32 count;
    if (mgr == NULL)
    {
        return 0u;
    }
    count = 0u;
    for (i = 0u; i < mgr->count; ++i)
    {
        if (mgr->slots[i].type == type)
        {
            ++count;
        }
    }
    return count;
}

void *g3d_asset_manager_find(const g3d_asset_manager *mgr, const char *name, g3d_u32 type)
{
    g3d_u32 i;
    if (mgr == NULL || name == NULL)
    {
        return NULL;
    }
    for (i = 0u; i < mgr->count; ++i)
    {
        if (mgr->slots[i].type == type && g3d_str_eq(mgr->slots[i].name, name) == G3D_TRUE)
        {
            return mgr->slots[i].object;
        }
    }
    return NULL;
}
