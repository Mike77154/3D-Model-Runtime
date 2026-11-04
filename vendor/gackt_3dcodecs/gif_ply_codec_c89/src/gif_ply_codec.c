#include "gif_ply_codec.h"
#include <string.h>
#include <limits.h>

#define GIF_PLY_TYPE_INVALID 0
#define GIF_PLY_TYPE_I8      1
#define GIF_PLY_TYPE_U8      2
#define GIF_PLY_TYPE_I16     3
#define GIF_PLY_TYPE_U16     4
#define GIF_PLY_TYPE_I32     5
#define GIF_PLY_TYPE_U32     6
#define GIF_PLY_TYPE_F32     7
#define GIF_PLY_TYPE_F64     8

#define GIF_PLY_PROP_SCALAR 0
#define GIF_PLY_PROP_LIST   1

#define GIF_PLY_READ_ASCII 1
#define GIF_PLY_READ_BIN_LE 2
#define GIF_PLY_READ_BIN_BE 3

typedef struct GifPlyPropDef {
    char name[GIF_PLY_NAME_MAX];
    int kind;
    int type;
    int count_type;
    int item_type;
} GifPlyPropDef;

typedef struct GifPlyElemDef {
    char name[GIF_PLY_NAME_MAX];
    unsigned long count;
    int prop_count;
    GifPlyPropDef props[GIF_PLY_MAX_PROPERTIES];
} GifPlyElemDef;

typedef struct GifPlyHeader {
    int format;
    int elem_count;
    GifPlyElemDef elems[GIF_PLY_MAX_ELEMENTS];
    unsigned long body_offset;
    unsigned long comments;
    unsigned long obj_info_lines;
    unsigned long texture_file_comments;
    unsigned long declared_properties;
} GifPlyHeader;

typedef struct GifPlyReader {
    const unsigned char *data;
    unsigned long size;
    unsigned long pos;
    int mode;
    int endian;
} GifPlyReader;

typedef struct GifPlyFaceTmp {
    unsigned long indices[GIF_PLY_MAX_FACE_VERTS];
    gif_ply_fx u[GIF_PLY_MAX_FACE_VERTS];
    gif_ply_fx v[GIF_PLY_MAX_FACE_VERTS];
    unsigned long index_count;
    unsigned long uv_pair_count;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    unsigned char flags;
    unsigned char material_index;
    int has_indices;
    int has_uv;
    int has_color;
} GifPlyFaceTmp;

static int gif_ply_streq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) return 0;
        ++a;
        ++b;
    }
    return (*a == '\0' && *b == '\0');
}

static int gif_ply_tolower_ascii(int c)
{
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int gif_ply_contains_ci(const char *s, const char *needle)
{
    const char *p;
    const char *a;
    const char *b;

    p = s;
    while (*p != '\0') {
        a = p;
        b = needle;
        while (*a != '\0' && *b != '\0' && gif_ply_tolower_ascii((unsigned char)*a) == gif_ply_tolower_ascii((unsigned char)*b)) {
            ++a;
            ++b;
        }
        if (*b == '\0') return 1;
        ++p;
    }
    return 0;
}

static int gif_ply_is_space(int c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static int gif_ply_is_digit(int c)
{
    return c >= '0' && c <= '9';
}

static void gif_ply_copy_name(char *dst, const char *src)
{
    int i;
    i = 0;
    while (i < (GIF_PLY_NAME_MAX - 1) && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static int gif_ply_read_line(const unsigned char *data,
                             unsigned long size,
                             unsigned long *pos,
                             char *line,
                             int line_cap)
{
    int n;
    unsigned char c;

    if (*pos >= size) return 0;
    n = 0;
    while (*pos < size) {
        c = data[*pos];
        *pos = *pos + 1u;
        if (c == '\n') break;
        if (c == '\r') {
            if (*pos < size && data[*pos] == '\n') *pos = *pos + 1u;
            break;
        }
        if (n < (line_cap - 1)) {
            line[n] = (char)c;
            ++n;
        }
    }
    line[n] = '\0';
    return 1;
}

static int gif_ply_next_token_line(char **cursor, char *tok, int tok_cap)
{
    char *p;
    int n;

    p = *cursor;
    while (*p != '\0' && gif_ply_is_space((unsigned char)*p)) ++p;
    if (*p == '\0') {
        *cursor = p;
        return 0;
    }
    n = 0;
    while (*p != '\0' && !gif_ply_is_space((unsigned char)*p)) {
        if (n < (tok_cap - 1)) {
            tok[n] = *p;
            ++n;
        }
        ++p;
    }
    tok[n] = '\0';
    *cursor = p;
    return 1;
}

static int gif_ply_type_from_name(const char *s)
{
    if (gif_ply_streq(s, "char") || gif_ply_streq(s, "int8")) return GIF_PLY_TYPE_I8;
    if (gif_ply_streq(s, "uchar") || gif_ply_streq(s, "uint8") || gif_ply_streq(s, "unsigned_char")) return GIF_PLY_TYPE_U8;
    if (gif_ply_streq(s, "short") || gif_ply_streq(s, "int16")) return GIF_PLY_TYPE_I16;
    if (gif_ply_streq(s, "ushort") || gif_ply_streq(s, "uint16") || gif_ply_streq(s, "unsigned_short")) return GIF_PLY_TYPE_U16;
    if (gif_ply_streq(s, "int") || gif_ply_streq(s, "int32")) return GIF_PLY_TYPE_I32;
    if (gif_ply_streq(s, "uint") || gif_ply_streq(s, "uint32") || gif_ply_streq(s, "unsigned_int")) return GIF_PLY_TYPE_U32;
    if (gif_ply_streq(s, "float") || gif_ply_streq(s, "float32")) return GIF_PLY_TYPE_F32;
    if (gif_ply_streq(s, "double") || gif_ply_streq(s, "float64")) return GIF_PLY_TYPE_F64;
    return GIF_PLY_TYPE_INVALID;
}

static int gif_ply_type_size(int t)
{
    switch (t) {
        case GIF_PLY_TYPE_I8:
        case GIF_PLY_TYPE_U8:
            return 1;
        case GIF_PLY_TYPE_I16:
        case GIF_PLY_TYPE_U16:
            return 2;
        case GIF_PLY_TYPE_I32:
        case GIF_PLY_TYPE_U32:
        case GIF_PLY_TYPE_F32:
            return 4;
        case GIF_PLY_TYPE_F64:
            return 8;
        default:
            return 0;
    }
}

static long gif_ply_sat_left_shift(long v, int sh)
{
    int i;
    if (sh <= 0) return v;
    i = 0;
    while (i < sh) {
        if (v > (LONG_MAX / 2L)) return LONG_MAX;
        if (v < (LONG_MIN / 2L)) return LONG_MIN;
        v = v * 2L;
        ++i;
    }
    return v;
}

gif_ply_fx gif_ply_fx_from_int(long v)
{
    if (v > (LONG_MAX >> GIF_PLY_FX_SHIFT)) return LONG_MAX;
    if (v < (LONG_MIN >> GIF_PLY_FX_SHIFT)) return LONG_MIN;
    return (gif_ply_fx)(v << GIF_PLY_FX_SHIFT);
}

long gif_ply_fx_to_int(gif_ply_fx v)
{
    return (long)(v >> GIF_PLY_FX_SHIFT);
}

long gif_ply_fx_to_milli(gif_ply_fx v)
{
    long sign;
    long abs_v;
    long whole;
    long frac;

    sign = 1L;
    if (v < 0) {
        sign = -1L;
        if (v == LONG_MIN) abs_v = LONG_MAX;
        else abs_v = -v;
    } else {
        abs_v = v;
    }
    whole = abs_v >> GIF_PLY_FX_SHIFT;
    frac = abs_v & (GIF_PLY_FX_ONE - 1L);
    if (whole > (LONG_MAX / 1000L)) return sign > 0 ? LONG_MAX : LONG_MIN;
    return sign * ((whole * 1000L) + ((frac * 1000L) >> GIF_PLY_FX_SHIFT));
}

static int gif_ply_parse_ulong_token(const char *s, unsigned long *out)
{
    unsigned long v;
    int saw;

    v = 0u;
    saw = 0;
    while (*s != '\0') {
        if (!gif_ply_is_digit((unsigned char)*s)) return 0;
        if (v > ((~0ul) / 10ul)) return 0;
        v = (v * 10ul) + (unsigned long)(*s - '0');
        saw = 1;
        ++s;
    }
    if (!saw) return 0;
    *out = v;
    return 1;
}

static int gif_ply_parse_long_token(const char *s, long *out)
{
    int neg;
    unsigned long v;
    unsigned long lim;
    int saw;

    neg = 0;
    if (*s == '-' || *s == '+') {
        neg = (*s == '-');
        ++s;
    }
    v = 0u;
    saw = 0;
    lim = neg ? (unsigned long)LONG_MAX + 1ul : (unsigned long)LONG_MAX;
    while (*s != '\0') {
        if (!gif_ply_is_digit((unsigned char)*s)) return 0;
        if (v > (lim / 10ul)) return 0;
        v = (v * 10ul) + (unsigned long)(*s - '0');
        if (v > lim) return 0;
        saw = 1;
        ++s;
    }
    if (!saw) return 0;
    if (neg) {
        if (v == ((unsigned long)LONG_MAX + 1ul)) *out = LONG_MIN;
        else *out = -(long)v;
    } else {
        *out = (long)v;
    }
    return 1;
}

static long gif_ply_mul10_sat(long v)
{
    if (v > (LONG_MAX / 10L)) return LONG_MAX;
    if (v < (LONG_MIN / 10L)) return LONG_MIN;
    return v * 10L;
}

static int gif_ply_parse_fixed_token(const char *s, gif_ply_fx *out)
{
    int neg;
    long int_part;
    long frac_part;
    long frac_den;
    int frac_digits;
    int saw;
    long result;
    int exp_neg;
    long exp_val;
    int i;

    neg = 0;
    if (*s == '-' || *s == '+') {
        neg = (*s == '-');
        ++s;
    }

    int_part = 0L;
    saw = 0;
    while (gif_ply_is_digit((unsigned char)*s)) {
        saw = 1;
        if (int_part > (LONG_MAX / 10L)) int_part = LONG_MAX;
        else int_part = (int_part * 10L) + (long)(*s - '0');
        ++s;
    }

    frac_part = 0L;
    frac_den = 1L;
    frac_digits = 0;
    if (*s == '.') {
        ++s;
        while (gif_ply_is_digit((unsigned char)*s)) {
            saw = 1;
            if (frac_digits < 6) {
                frac_part = (frac_part * 10L) + (long)(*s - '0');
                frac_den = frac_den * 10L;
                ++frac_digits;
            }
            ++s;
        }
    }

    if (!saw) return 0;
    if (int_part > (LONG_MAX >> GIF_PLY_FX_SHIFT)) result = LONG_MAX;
    else result = int_part << GIF_PLY_FX_SHIFT;
    if (frac_den > 1L && result != LONG_MAX) {
        result += (long)(((unsigned long)frac_part << GIF_PLY_FX_SHIFT) / (unsigned long)frac_den);
    }

    if (*s == 'e' || *s == 'E') {
        ++s;
        exp_neg = 0;
        if (*s == '-' || *s == '+') {
            exp_neg = (*s == '-');
            ++s;
        }
        exp_val = 0L;
        if (!gif_ply_is_digit((unsigned char)*s)) return 0;
        while (gif_ply_is_digit((unsigned char)*s)) {
            if (exp_val < 64L) exp_val = (exp_val * 10L) + (long)(*s - '0');
            ++s;
        }
        if (exp_val > 64L) exp_val = 64L;
        i = 0;
        while (i < exp_val) {
            if (exp_neg) result = result / 10L;
            else result = gif_ply_mul10_sat(result);
            ++i;
        }
    }

    if (*s != '\0') return 0;
    if (neg) result = -result;
    *out = result;
    return 1;
}

static void gif_ply_reader_skip_ws(GifPlyReader *r)
{
    while (r->pos < r->size && gif_ply_is_space(r->data[r->pos])) r->pos = r->pos + 1u;
}

static int gif_ply_reader_next_ascii_token(GifPlyReader *r, char *tok, int cap)
{
    int n;

    gif_ply_reader_skip_ws(r);
    if (r->pos >= r->size) return 0;
    n = 0;
    while (r->pos < r->size && !gif_ply_is_space(r->data[r->pos])) {
        if (n < (cap - 1)) {
            tok[n] = (char)r->data[r->pos];
            ++n;
        }
        r->pos = r->pos + 1u;
    }
    tok[n] = '\0';
    return 1;
}

static unsigned long gif_ply_read_u16_endian(const unsigned char *p, int endian)
{
    if (endian == GIF_PLY_READ_BIN_LE) return ((unsigned long)p[0]) | (((unsigned long)p[1]) << 8);
    return ((unsigned long)p[1]) | (((unsigned long)p[0]) << 8);
}

static unsigned long gif_ply_read_u32_endian(const unsigned char *p, int endian)
{
    if (endian == GIF_PLY_READ_BIN_LE) {
        return ((unsigned long)p[0]) |
               (((unsigned long)p[1]) << 8) |
               (((unsigned long)p[2]) << 16) |
               (((unsigned long)p[3]) << 24);
    }
    return ((unsigned long)p[3]) |
           (((unsigned long)p[2]) << 8) |
           (((unsigned long)p[1]) << 16) |
           (((unsigned long)p[0]) << 24);
}

static long gif_ply_sign_extend8(unsigned long v)
{
    v &= 0xfful;
    if (v & 0x80ul) return -1L - (long)((~v) & 0xfful);
    return (long)v;
}

static long gif_ply_sign_extend16(unsigned long v)
{
    v &= 0xfffful;
    if (v & 0x8000ul) return -1L - (long)((~v) & 0xfffful);
    return (long)v;
}

static long gif_ply_sign_extend32(unsigned long v)
{
    v &= 0xfffffffful;
    if (v & 0x80000000ul) return -1L - (long)((~v) & 0xfffffffful);
    return (long)v;
}

static gif_ply_fx gif_ply_fx_from_f32_bits(unsigned long bits)
{
    int sign;
    int exp;
    unsigned long mant;
    int shift;
    long value;

    sign = (int)((bits >> 31) & 1ul);
    exp = (int)((bits >> 23) & 0xfful);
    mant = bits & 0x7ffffful;

    if (exp == 255) return sign ? LONG_MIN : LONG_MAX;
    if (exp == 0) return 0L;

    mant |= 0x800000ul;
    shift = exp - 134;

    if (shift >= 0) {
        if (shift > 7) value = LONG_MAX;
        else value = gif_ply_sat_left_shift((long)mant, shift);
    } else {
        if (-shift >= 31) value = 0L;
        else value = (long)(mant >> (-shift));
    }

    if (sign) value = -value;
    return value;
}

static gif_ply_fx gif_ply_fx_from_f64_words(unsigned long hi, unsigned long lo)
{
    int sign;
    int exp;
    unsigned long mant_hi;
    unsigned long frac31;
    long base;
    int e;
    long result;

    sign = (int)((hi >> 31) & 1ul);
    exp = (int)((hi >> 20) & 0x7fful);
    mant_hi = hi & 0xffffful;

    if (exp == 2047) return sign ? LONG_MIN : LONG_MAX;
    if (exp == 0) return 0L;

    e = exp - 1023;
    if (e > 14) return sign ? LONG_MIN : LONG_MAX;
    if (e < -30) return 0L;

    frac31 = (mant_hi << 11) | ((lo >> 21) & 0x7fful);
    base = GIF_PLY_FX_ONE + (long)(frac31 >> 15);

    if (e >= 0) result = gif_ply_sat_left_shift(base, e);
    else result = base >> (-e);

    if (sign) result = -result;
    return result;
}

static int gif_ply_read_scalar_int(GifPlyReader *r, int type, long *out)
{
    char tok[96];
    unsigned long u;
    gif_ply_fx fx;
    int sz;

    if (r->mode == GIF_PLY_READ_ASCII) {
        if (!gif_ply_reader_next_ascii_token(r, tok, (int)sizeof(tok))) return GIF_PLY_ERR_TRUNCATED;
        if (type == GIF_PLY_TYPE_F32 || type == GIF_PLY_TYPE_F64) {
            if (!gif_ply_parse_fixed_token(tok, &fx)) return GIF_PLY_ERR_PARSE;
            *out = fx >> GIF_PLY_FX_SHIFT;
            return GIF_PLY_OK;
        }
        if (!gif_ply_parse_long_token(tok, out)) return GIF_PLY_ERR_PARSE;
        return GIF_PLY_OK;
    }

    sz = gif_ply_type_size(type);
    if (sz <= 0) return GIF_PLY_ERR_UNSUPPORTED;
    if ((r->size - r->pos) < (unsigned long)sz) return GIF_PLY_ERR_TRUNCATED;

    if (type == GIF_PLY_TYPE_I8) {
        *out = gif_ply_sign_extend8((unsigned long)r->data[r->pos]);
    } else if (type == GIF_PLY_TYPE_U8) {
        *out = (long)r->data[r->pos];
    } else if (type == GIF_PLY_TYPE_I16) {
        u = gif_ply_read_u16_endian(r->data + r->pos, r->endian);
        *out = gif_ply_sign_extend16(u);
    } else if (type == GIF_PLY_TYPE_U16) {
        u = gif_ply_read_u16_endian(r->data + r->pos, r->endian);
        *out = (long)u;
    } else if (type == GIF_PLY_TYPE_I32) {
        u = gif_ply_read_u32_endian(r->data + r->pos, r->endian);
        *out = gif_ply_sign_extend32(u);
    } else if (type == GIF_PLY_TYPE_U32) {
        u = gif_ply_read_u32_endian(r->data + r->pos, r->endian);
        if (u > (unsigned long)LONG_MAX) return GIF_PLY_ERR_OVERFLOW;
        *out = (long)u;
    } else if (type == GIF_PLY_TYPE_F32) {
        u = gif_ply_read_u32_endian(r->data + r->pos, r->endian);
        fx = gif_ply_fx_from_f32_bits(u);
        *out = fx >> GIF_PLY_FX_SHIFT;
    } else if (type == GIF_PLY_TYPE_F64) {
        unsigned long hi;
        unsigned long lo;
        if (r->endian == GIF_PLY_READ_BIN_LE) {
            lo = gif_ply_read_u32_endian(r->data + r->pos, r->endian);
            hi = gif_ply_read_u32_endian(r->data + r->pos + 4u, r->endian);
        } else {
            hi = gif_ply_read_u32_endian(r->data + r->pos, r->endian);
            lo = gif_ply_read_u32_endian(r->data + r->pos + 4u, r->endian);
        }
        fx = gif_ply_fx_from_f64_words(hi, lo);
        *out = fx >> GIF_PLY_FX_SHIFT;
    } else {
        return GIF_PLY_ERR_UNSUPPORTED;
    }

    r->pos = r->pos + (unsigned long)sz;
    return GIF_PLY_OK;
}

static int gif_ply_read_scalar_fixed(GifPlyReader *r, int type, gif_ply_fx *out)
{
    char tok[96];
    long v;
    unsigned long u;
    int sz;
    int rc;

    if (r->mode == GIF_PLY_READ_ASCII) {
        if (!gif_ply_reader_next_ascii_token(r, tok, (int)sizeof(tok))) return GIF_PLY_ERR_TRUNCATED;
        if (!gif_ply_parse_fixed_token(tok, out)) return GIF_PLY_ERR_PARSE;
        return GIF_PLY_OK;
    }

    if (type == GIF_PLY_TYPE_F32) {
        if ((r->size - r->pos) < 4u) return GIF_PLY_ERR_TRUNCATED;
        u = gif_ply_read_u32_endian(r->data + r->pos, r->endian);
        r->pos = r->pos + 4u;
        *out = gif_ply_fx_from_f32_bits(u);
        return GIF_PLY_OK;
    }

    if (type == GIF_PLY_TYPE_F64) {
        unsigned long hi;
        unsigned long lo;
        if ((r->size - r->pos) < 8u) return GIF_PLY_ERR_TRUNCATED;
        if (r->endian == GIF_PLY_READ_BIN_LE) {
            lo = gif_ply_read_u32_endian(r->data + r->pos, r->endian);
            hi = gif_ply_read_u32_endian(r->data + r->pos + 4u, r->endian);
        } else {
            hi = gif_ply_read_u32_endian(r->data + r->pos, r->endian);
            lo = gif_ply_read_u32_endian(r->data + r->pos + 4u, r->endian);
        }
        r->pos = r->pos + 8u;
        *out = gif_ply_fx_from_f64_words(hi, lo);
        return GIF_PLY_OK;
    }

    if (type == GIF_PLY_TYPE_U32) {
        sz = gif_ply_type_size(type);
        if ((r->size - r->pos) < (unsigned long)sz) return GIF_PLY_ERR_TRUNCATED;
        u = gif_ply_read_u32_endian(r->data + r->pos, r->endian);
        r->pos = r->pos + (unsigned long)sz;
        if (u > (unsigned long)(LONG_MAX >> GIF_PLY_FX_SHIFT)) return GIF_PLY_ERR_OVERFLOW;
        *out = (long)u << GIF_PLY_FX_SHIFT;
        return GIF_PLY_OK;
    }

    if (type == GIF_PLY_TYPE_I8 || type == GIF_PLY_TYPE_U8 ||
        type == GIF_PLY_TYPE_I16 || type == GIF_PLY_TYPE_U16 ||
        type == GIF_PLY_TYPE_I32) {
        rc = gif_ply_read_scalar_int(r, type, &v);
        if (rc != GIF_PLY_OK) return rc;
        *out = gif_ply_fx_from_int(v);
        return GIF_PLY_OK;
    }

    return GIF_PLY_ERR_UNSUPPORTED;
}

static int gif_ply_skip_scalar(GifPlyReader *r, int type)
{
    char tok[96];
    int sz;

    if (r->mode == GIF_PLY_READ_ASCII) {
        if (!gif_ply_reader_next_ascii_token(r, tok, (int)sizeof(tok))) return GIF_PLY_ERR_TRUNCATED;
        return GIF_PLY_OK;
    }
    sz = gif_ply_type_size(type);
    if (sz <= 0) return GIF_PLY_ERR_UNSUPPORTED;
    if ((r->size - r->pos) < (unsigned long)sz) return GIF_PLY_ERR_TRUNCATED;
    r->pos = r->pos + (unsigned long)sz;
    return GIF_PLY_OK;
}

static int gif_ply_skip_list(GifPlyReader *r, const GifPlyPropDef *p)
{
    long count;
    long i;
    int rc;

    rc = gif_ply_read_scalar_int(r, p->count_type, &count);
    if (rc != GIF_PLY_OK) return rc;
    if (count < 0L) return GIF_PLY_ERR_PARSE;
    i = 0L;
    while (i < count) {
        rc = gif_ply_skip_scalar(r, p->item_type);
        if (rc != GIF_PLY_OK) return rc;
        ++i;
    }
    return GIF_PLY_OK;
}

static unsigned char gif_ply_clamp_u8(long v)
{
    if (v < 0L) return 0u;
    if (v > 255L) return 255u;
    return (unsigned char)v;
}

static int gif_ply_name_is_x(const char *s) { return gif_ply_streq(s, "x"); }
static int gif_ply_name_is_y(const char *s) { return gif_ply_streq(s, "y"); }
static int gif_ply_name_is_z(const char *s) { return gif_ply_streq(s, "z"); }
static int gif_ply_name_is_nx(const char *s) { return gif_ply_streq(s, "nx") || gif_ply_streq(s, "normal_x"); }
static int gif_ply_name_is_ny(const char *s) { return gif_ply_streq(s, "ny") || gif_ply_streq(s, "normal_y"); }
static int gif_ply_name_is_nz(const char *s) { return gif_ply_streq(s, "nz") || gif_ply_streq(s, "normal_z"); }

static int gif_ply_name_is_u(const char *s)
{
    return gif_ply_streq(s, "u") || gif_ply_streq(s, "s") ||
           gif_ply_streq(s, "texture_u") || gif_ply_streq(s, "texcoord_u") ||
           gif_ply_streq(s, "texture_s") || gif_ply_streq(s, "tu");
}

static int gif_ply_name_is_v(const char *s)
{
    return gif_ply_streq(s, "v") || gif_ply_streq(s, "t") ||
           gif_ply_streq(s, "texture_v") || gif_ply_streq(s, "texcoord_v") ||
           gif_ply_streq(s, "texture_t") || gif_ply_streq(s, "tv");
}

static int gif_ply_name_is_red(const char *s)
{
    return gif_ply_streq(s, "red") || gif_ply_streq(s, "r") ||
           gif_ply_streq(s, "diffuse_red") || gif_ply_streq(s, "ambient_red");
}

static int gif_ply_name_is_green(const char *s)
{
    return gif_ply_streq(s, "green") || gif_ply_streq(s, "g") ||
           gif_ply_streq(s, "diffuse_green") || gif_ply_streq(s, "ambient_green");
}

static int gif_ply_name_is_blue(const char *s)
{
    return gif_ply_streq(s, "blue") || gif_ply_streq(s, "b") ||
           gif_ply_streq(s, "diffuse_blue") || gif_ply_streq(s, "ambient_blue");
}

static int gif_ply_name_is_alpha(const char *s)
{
    return gif_ply_streq(s, "alpha") || gif_ply_streq(s, "a") ||
           gif_ply_streq(s, "diffuse_alpha") || gif_ply_streq(s, "opacity");
}

static int gif_ply_name_is_indices(const char *s)
{
    return gif_ply_streq(s, "vertex_indices") || gif_ply_streq(s, "vertex_index") ||
           gif_ply_streq(s, "vertex_indexes") || gif_ply_streq(s, "indices");
}

static int gif_ply_name_is_face_texcoord(const char *s)
{
    return gif_ply_streq(s, "texcoord") || gif_ply_streq(s, "texcoords") ||
           gif_ply_streq(s, "texture_coordinates") || gif_ply_streq(s, "texture_coordinate") ||
           gif_ply_streq(s, "uv") || gif_ply_streq(s, "uvs");
}

static int gif_ply_name_is_intensity(const char *s)
{
    return gif_ply_streq(s, "intensity") || gif_ply_streq(s, "scalar") || gif_ply_streq(s, "value");
}

static int gif_ply_name_is_confidence(const char *s)
{
    return gif_ply_streq(s, "confidence") || gif_ply_streq(s, "quality") || gif_ply_streq(s, "conf");
}

static int gif_ply_name_is_material_index(const char *s)
{
    return gif_ply_streq(s, "material_index") || gif_ply_streq(s, "material") || gif_ply_streq(s, "mat_index");
}

static int gif_ply_name_is_edge_a(const char *s)
{
    return gif_ply_streq(s, "vertex1") || gif_ply_streq(s, "v1") || gif_ply_streq(s, "vertex_a") || gif_ply_streq(s, "a");
}

static int gif_ply_name_is_edge_b(const char *s)
{
    return gif_ply_streq(s, "vertex2") || gif_ply_streq(s, "v2") || gif_ply_streq(s, "vertex_b") || gif_ply_streq(s, "b");
}

static int gif_ply_name_is_crease(const char *s)
{
    return gif_ply_streq(s, "crease") || gif_ply_streq(s, "sharpness") || gif_ply_streq(s, "weight");
}

static int gif_ply_parse_header(const unsigned char *data, unsigned long size, GifPlyHeader *h)
{
    unsigned long pos;
    char line[512];
    char tok0[64];
    char tok1[64];
    char tok2[64];
    char tok3[64];
    char *cur;
    GifPlyElemDef *cur_elem;
    GifPlyPropDef *prop;
    unsigned long n;
    int type;
    int count_type;
    int item_type;

    memset(h, 0, sizeof(*h));
    pos = 0u;
    if (!gif_ply_read_line(data, size, &pos, line, (int)sizeof(line))) return GIF_PLY_ERR_BAD_MAGIC;
    if (!gif_ply_streq(line, "ply")) return GIF_PLY_ERR_BAD_MAGIC;

    cur_elem = (GifPlyElemDef *)0;
    while (gif_ply_read_line(data, size, &pos, line, (int)sizeof(line))) {
        cur = line;
        if (!gif_ply_next_token_line(&cur, tok0, (int)sizeof(tok0))) continue;

        if (gif_ply_streq(tok0, "comment")) {
            h->comments = h->comments + 1u;
            if (gif_ply_contains_ci(cur, "texturefile")) h->texture_file_comments = h->texture_file_comments + 1u;
            continue;
        }

        if (gif_ply_streq(tok0, "obj_info")) {
            h->obj_info_lines = h->obj_info_lines + 1u;
            continue;
        }

        if (gif_ply_streq(tok0, "format")) {
            if (!gif_ply_next_token_line(&cur, tok1, (int)sizeof(tok1))) return GIF_PLY_ERR_BAD_HEADER;
            if (!gif_ply_next_token_line(&cur, tok2, (int)sizeof(tok2))) return GIF_PLY_ERR_BAD_HEADER;
            if (!gif_ply_streq(tok2, "1.0")) return GIF_PLY_ERR_UNSUPPORTED;
            if (gif_ply_streq(tok1, "ascii")) h->format = GIF_PLY_FORMAT_ASCII;
            else if (gif_ply_streq(tok1, "binary_little_endian")) h->format = GIF_PLY_FORMAT_BINARY_LE;
            else if (gif_ply_streq(tok1, "binary_big_endian")) h->format = GIF_PLY_FORMAT_BINARY_BE;
            else return GIF_PLY_ERR_UNSUPPORTED;
            continue;
        }

        if (gif_ply_streq(tok0, "element")) {
            if (!gif_ply_next_token_line(&cur, tok1, (int)sizeof(tok1))) return GIF_PLY_ERR_BAD_HEADER;
            if (!gif_ply_next_token_line(&cur, tok2, (int)sizeof(tok2))) return GIF_PLY_ERR_BAD_HEADER;
            if (!gif_ply_parse_ulong_token(tok2, &n)) return GIF_PLY_ERR_BAD_HEADER;
            if (h->elem_count >= GIF_PLY_MAX_ELEMENTS) return GIF_PLY_ERR_CAPACITY;
            cur_elem = &h->elems[h->elem_count];
            memset(cur_elem, 0, sizeof(*cur_elem));
            gif_ply_copy_name(cur_elem->name, tok1);
            cur_elem->count = n;
            h->elem_count = h->elem_count + 1;
            continue;
        }

        if (gif_ply_streq(tok0, "property")) {
            if (cur_elem == (GifPlyElemDef *)0) return GIF_PLY_ERR_BAD_HEADER;
            if (cur_elem->prop_count >= GIF_PLY_MAX_PROPERTIES) return GIF_PLY_ERR_CAPACITY;
            if (!gif_ply_next_token_line(&cur, tok1, (int)sizeof(tok1))) return GIF_PLY_ERR_BAD_HEADER;
            prop = &cur_elem->props[cur_elem->prop_count];
            memset(prop, 0, sizeof(*prop));
            if (gif_ply_streq(tok1, "list")) {
                if (!gif_ply_next_token_line(&cur, tok2, (int)sizeof(tok2))) return GIF_PLY_ERR_BAD_HEADER;
                if (!gif_ply_next_token_line(&cur, tok3, (int)sizeof(tok3))) return GIF_PLY_ERR_BAD_HEADER;
                if (!gif_ply_next_token_line(&cur, tok1, (int)sizeof(tok1))) return GIF_PLY_ERR_BAD_HEADER;
                count_type = gif_ply_type_from_name(tok2);
                item_type = gif_ply_type_from_name(tok3);
                if (count_type == GIF_PLY_TYPE_INVALID || item_type == GIF_PLY_TYPE_INVALID) return GIF_PLY_ERR_UNSUPPORTED;
                prop->kind = GIF_PLY_PROP_LIST;
                prop->count_type = count_type;
                prop->item_type = item_type;
                prop->type = item_type;
                gif_ply_copy_name(prop->name, tok1);
            } else {
                type = gif_ply_type_from_name(tok1);
                if (type == GIF_PLY_TYPE_INVALID) return GIF_PLY_ERR_UNSUPPORTED;
                if (!gif_ply_next_token_line(&cur, tok2, (int)sizeof(tok2))) return GIF_PLY_ERR_BAD_HEADER;
                prop->kind = GIF_PLY_PROP_SCALAR;
                prop->type = type;
                prop->count_type = GIF_PLY_TYPE_INVALID;
                prop->item_type = GIF_PLY_TYPE_INVALID;
                gif_ply_copy_name(prop->name, tok2);
            }
            cur_elem->prop_count = cur_elem->prop_count + 1;
            h->declared_properties = h->declared_properties + 1u;
            continue;
        }

        if (gif_ply_streq(tok0, "end_header")) {
            if (h->format == 0) return GIF_PLY_ERR_BAD_HEADER;
            h->body_offset = pos;
            return GIF_PLY_OK;
        }

        return GIF_PLY_ERR_BAD_HEADER;
    }

    return GIF_PLY_ERR_BAD_HEADER;
}

static void gif_ply_info_from_header(const GifPlyHeader *h, GifPlyInfo *info)
{
    int i;
    const GifPlyElemDef *e;

    memset(info, 0, sizeof(*info));
    info->format = h->format;
    info->declared_elements = (unsigned long)h->elem_count;
    info->declared_properties = h->declared_properties;
    info->comments = h->comments;
    info->obj_info_lines = h->obj_info_lines;
    info->texture_file_comments = h->texture_file_comments;

    i = 0;
    while (i < h->elem_count) {
        e = &h->elems[i];
        if (gif_ply_streq(e->name, "vertex")) {
            info->saw_vertex = 1;
            info->declared_vertices = e->count;
        } else if (gif_ply_streq(e->name, "face")) {
            info->saw_face = 1;
            info->declared_faces = e->count;
        } else if (gif_ply_streq(e->name, "edge")) {
            info->saw_edge = 1;
            info->declared_edges = e->count;
        }
        ++i;
    }
}

int gif_ply_probe_memory(const unsigned char *data, unsigned long size, GifPlyInfo *out_info)
{
    GifPlyHeader h;
    GifPlyInfo info;
    int rc;

    if (data == (const unsigned char *)0 || out_info == (GifPlyInfo *)0) return GIF_PLY_ERR_NULL;
    rc = gif_ply_parse_header(data, size, &h);
    if (rc != GIF_PLY_OK) return rc;
    gif_ply_info_from_header(&h, &info);
    *out_info = info;
    return GIF_PLY_OK;
}

static int gif_ply_import_vertex(GifPlyReader *r, const GifPlyElemDef *e, GifPlyMesh *mesh)
{
    GifPlyVertex *v;
    int i;
    int rc;
    long iv;
    const GifPlyPropDef *p;

    if (mesh->vertex_count >= mesh->vertex_capacity) return GIF_PLY_ERR_CAPACITY;
    v = &mesh->vertices[mesh->vertex_count];
    memset(v, 0, sizeof(*v));
    v->a = 255u;

    i = 0;
    while (i < e->prop_count) {
        p = &e->props[i];
        if (p->kind == GIF_PLY_PROP_LIST) {
            rc = gif_ply_skip_list(r, p);
            if (rc != GIF_PLY_OK) return rc;
        } else if (gif_ply_name_is_x(p->name)) {
            rc = gif_ply_read_scalar_fixed(r, p->type, &v->x);
            if (rc != GIF_PLY_OK) return rc;
        } else if (gif_ply_name_is_y(p->name)) {
            rc = gif_ply_read_scalar_fixed(r, p->type, &v->y);
            if (rc != GIF_PLY_OK) return rc;
        } else if (gif_ply_name_is_z(p->name)) {
            rc = gif_ply_read_scalar_fixed(r, p->type, &v->z);
            if (rc != GIF_PLY_OK) return rc;
        } else if (gif_ply_name_is_nx(p->name)) {
            rc = gif_ply_read_scalar_fixed(r, p->type, &v->nx);
            if (rc != GIF_PLY_OK) return rc;
            v->flags = (unsigned char)(v->flags | GIF_PLY_VERTEX_HAS_NORMAL);
        } else if (gif_ply_name_is_ny(p->name)) {
            rc = gif_ply_read_scalar_fixed(r, p->type, &v->ny);
            if (rc != GIF_PLY_OK) return rc;
            v->flags = (unsigned char)(v->flags | GIF_PLY_VERTEX_HAS_NORMAL);
        } else if (gif_ply_name_is_nz(p->name)) {
            rc = gif_ply_read_scalar_fixed(r, p->type, &v->nz);
            if (rc != GIF_PLY_OK) return rc;
            v->flags = (unsigned char)(v->flags | GIF_PLY_VERTEX_HAS_NORMAL);
        } else if (gif_ply_name_is_u(p->name)) {
            rc = gif_ply_read_scalar_fixed(r, p->type, &v->u);
            if (rc != GIF_PLY_OK) return rc;
            v->flags = (unsigned char)(v->flags | GIF_PLY_VERTEX_HAS_UV);
        } else if (gif_ply_name_is_v(p->name)) {
            rc = gif_ply_read_scalar_fixed(r, p->type, &v->v);
            if (rc != GIF_PLY_OK) return rc;
            v->flags = (unsigned char)(v->flags | GIF_PLY_VERTEX_HAS_UV);
        } else if (gif_ply_name_is_intensity(p->name)) {
            rc = gif_ply_read_scalar_fixed(r, p->type, &v->intensity);
            if (rc != GIF_PLY_OK) return rc;
            v->flags = (unsigned char)(v->flags | GIF_PLY_VERTEX_HAS_INTENSITY);
        } else if (gif_ply_name_is_confidence(p->name)) {
            rc = gif_ply_read_scalar_fixed(r, p->type, &v->confidence);
            if (rc != GIF_PLY_OK) return rc;
            v->flags = (unsigned char)(v->flags | GIF_PLY_VERTEX_HAS_CONFIDENCE);
        } else if (gif_ply_name_is_red(p->name)) {
            rc = gif_ply_read_scalar_int(r, p->type, &iv);
            if (rc != GIF_PLY_OK) return rc;
            v->r = gif_ply_clamp_u8(iv);
            v->flags = (unsigned char)(v->flags | GIF_PLY_VERTEX_HAS_COLOR);
        } else if (gif_ply_name_is_green(p->name)) {
            rc = gif_ply_read_scalar_int(r, p->type, &iv);
            if (rc != GIF_PLY_OK) return rc;
            v->g = gif_ply_clamp_u8(iv);
            v->flags = (unsigned char)(v->flags | GIF_PLY_VERTEX_HAS_COLOR);
        } else if (gif_ply_name_is_blue(p->name)) {
            rc = gif_ply_read_scalar_int(r, p->type, &iv);
            if (rc != GIF_PLY_OK) return rc;
            v->b = gif_ply_clamp_u8(iv);
            v->flags = (unsigned char)(v->flags | GIF_PLY_VERTEX_HAS_COLOR);
        } else if (gif_ply_name_is_alpha(p->name)) {
            rc = gif_ply_read_scalar_int(r, p->type, &iv);
            if (rc != GIF_PLY_OK) return rc;
            v->a = gif_ply_clamp_u8(iv);
            v->flags = (unsigned char)(v->flags | GIF_PLY_VERTEX_HAS_COLOR);
        } else {
            rc = gif_ply_skip_scalar(r, p->type);
            if (rc != GIF_PLY_OK) return rc;
        }
        ++i;
    }

    mesh->vertex_count = mesh->vertex_count + 1u;
    return GIF_PLY_OK;
}

static int gif_ply_read_index_list(GifPlyReader *r, const GifPlyPropDef *p, GifPlyFaceTmp *face)
{
    long count;
    long i;
    long iv;
    int rc;

    rc = gif_ply_read_scalar_int(r, p->count_type, &count);
    if (rc != GIF_PLY_OK) return rc;
    if (count < 0L) return GIF_PLY_ERR_PARSE;
    if ((unsigned long)count > (unsigned long)GIF_PLY_MAX_FACE_VERTS) return GIF_PLY_ERR_FACE_TOO_LARGE;

    i = 0L;
    while (i < count) {
        rc = gif_ply_read_scalar_int(r, p->item_type, &iv);
        if (rc != GIF_PLY_OK) return rc;
        if (iv < 0L) return GIF_PLY_ERR_PARSE;
        face->indices[i] = (unsigned long)iv;
        ++i;
    }
    face->index_count = (unsigned long)count;
    face->has_indices = 1;
    return GIF_PLY_OK;
}

static int gif_ply_read_face_uv_list(GifPlyReader *r, const GifPlyPropDef *p, GifPlyFaceTmp *face)
{
    long count;
    long i;
    gif_ply_fx val;
    int rc;
    unsigned long pair;

    rc = gif_ply_read_scalar_int(r, p->count_type, &count);
    if (rc != GIF_PLY_OK) return rc;
    if (count < 0L) return GIF_PLY_ERR_PARSE;
    if ((unsigned long)count > ((unsigned long)GIF_PLY_MAX_FACE_VERTS * 2ul)) return GIF_PLY_ERR_FACE_TOO_LARGE;

    i = 0L;
    while (i < count) {
        rc = gif_ply_read_scalar_fixed(r, p->item_type, &val);
        if (rc != GIF_PLY_OK) return rc;
        pair = (unsigned long)i / 2ul;
        if (((unsigned long)i & 1ul) == 0ul) face->u[pair] = val;
        else face->v[pair] = val;
        ++i;
    }
    face->uv_pair_count = (unsigned long)count / 2ul;
    face->has_uv = (face->uv_pair_count > 0u) ? 1 : 0;
    return GIF_PLY_OK;
}

static int gif_ply_store_tri_meta(GifPlyMesh *mesh,
                                  const GifPlyFaceTmp *face,
                                  unsigned long source_face,
                                  unsigned long ia,
                                  unsigned long ib,
                                  unsigned long ic)
{
    GifPlyTriangle *t;
    GifPlyTriUV *uv;
    unsigned char flags;

    flags = face->flags;
    if (face->has_color) flags = (unsigned char)(flags | GIF_PLY_TRI_HAS_FACE_COLOR);
    if (face->index_count > 3u) flags = (unsigned char)(flags | GIF_PLY_TRI_FROM_NGON);
    if (face->has_uv && face->uv_pair_count >= face->index_count) flags = (unsigned char)(flags | GIF_PLY_TRI_HAS_FACE_UV);

    if (mesh->triangles != (GifPlyTriangle *)0) {
        if (mesh->triangle_count >= mesh->triangle_capacity) return GIF_PLY_ERR_CAPACITY;
        t = &mesh->triangles[mesh->triangle_count];
        memset(t, 0, sizeof(*t));
        t->source_face = source_face;
        t->source_vertex_count = (face->index_count > 255u) ? 255u : (unsigned char)face->index_count;
        t->r = face->r;
        t->g = face->g;
        t->b = face->b;
        t->a = face->a;
        t->flags = flags;
        t->material_index = face->material_index;
    }

    if (mesh->tri_uvs != (GifPlyTriUV *)0) {
        if (mesh->tri_uv_count >= mesh->tri_uv_capacity) return GIF_PLY_ERR_CAPACITY;
        uv = &mesh->tri_uvs[mesh->tri_uv_count];
        memset(uv, 0, sizeof(*uv));
        if (face->has_uv && face->uv_pair_count >= face->index_count) {
            uv->u0 = face->u[ia];
            uv->v0 = face->v[ia];
            uv->u1 = face->u[ib];
            uv->v1 = face->v[ib];
            uv->u2 = face->u[ic];
            uv->v2 = face->v[ic];
        }
        mesh->tri_uv_count = mesh->tri_uv_count + 1u;
    }

    mesh->triangle_count = mesh->triangle_count + 1u;
    return GIF_PLY_OK;
}

static int gif_ply_emit_tri(GifPlyMesh *mesh,
                            const GifPlyFaceTmp *face,
                            unsigned long source_face,
                            unsigned long ia,
                            unsigned long ib,
                            unsigned long ic)
{
    unsigned long a;
    unsigned long b;
    unsigned long c;
    int rc;

    if ((mesh->index_capacity - mesh->index_count) < 3u) return GIF_PLY_ERR_CAPACITY;
    if (ia >= face->index_count || ib >= face->index_count || ic >= face->index_count) return GIF_PLY_ERR_PARSE;
    a = face->indices[ia];
    b = face->indices[ib];
    c = face->indices[ic];
    if (a >= mesh->vertex_count || b >= mesh->vertex_count || c >= mesh->vertex_count) return GIF_PLY_ERR_PARSE;

    rc = gif_ply_store_tri_meta(mesh, face, source_face, ia, ib, ic);
    if (rc != GIF_PLY_OK) return rc;

    mesh->indices[mesh->index_count] = a;
    mesh->indices[mesh->index_count + 1u] = b;
    mesh->indices[mesh->index_count + 2u] = c;
    mesh->index_count = mesh->index_count + 3u;
    return GIF_PLY_OK;
}

static int gif_ply_emit_face(GifPlyMesh *mesh, GifPlyFaceTmp *face, unsigned long source_face, GifPlyInfo *info)
{
    unsigned long i;
    int rc;

    if (!face->has_indices) return GIF_PLY_OK;
    if (face->index_count < 3u) return GIF_PLY_OK;
    i = 2u;
    while (i < face->index_count) {
        rc = gif_ply_emit_tri(mesh, face, source_face, 0u, i - 1u, i);
        if (rc != GIF_PLY_OK) return rc;
        info->emitted_triangles = info->emitted_triangles + 1u;
        ++i;
    }
    return GIF_PLY_OK;
}

static int gif_ply_import_face(GifPlyReader *r,
                               const GifPlyElemDef *e,
                               GifPlyMesh *mesh,
                               GifPlyInfo *info,
                               unsigned long source_face)
{
    int i;
    int rc;
    long iv;
    const GifPlyPropDef *p;
    GifPlyFaceTmp face;

    memset(&face, 0, sizeof(face));
    face.a = 255u;

    i = 0;
    while (i < e->prop_count) {
        p = &e->props[i];
        if (p->kind == GIF_PLY_PROP_LIST) {
            if (!face.has_indices && gif_ply_name_is_indices(p->name)) {
                rc = gif_ply_read_index_list(r, p, &face);
                if (rc != GIF_PLY_OK) return rc;
            } else if (!face.has_uv && gif_ply_name_is_face_texcoord(p->name)) {
                rc = gif_ply_read_face_uv_list(r, p, &face);
                if (rc != GIF_PLY_OK) return rc;
            } else {
                rc = gif_ply_skip_list(r, p);
                if (rc != GIF_PLY_OK) return rc;
                info->skipped_properties = info->skipped_properties + 1u;
            }
        } else if (gif_ply_name_is_red(p->name)) {
            rc = gif_ply_read_scalar_int(r, p->type, &iv);
            if (rc != GIF_PLY_OK) return rc;
            face.r = gif_ply_clamp_u8(iv);
            face.has_color = 1;
        } else if (gif_ply_name_is_green(p->name)) {
            rc = gif_ply_read_scalar_int(r, p->type, &iv);
            if (rc != GIF_PLY_OK) return rc;
            face.g = gif_ply_clamp_u8(iv);
            face.has_color = 1;
        } else if (gif_ply_name_is_blue(p->name)) {
            rc = gif_ply_read_scalar_int(r, p->type, &iv);
            if (rc != GIF_PLY_OK) return rc;
            face.b = gif_ply_clamp_u8(iv);
            face.has_color = 1;
        } else if (gif_ply_name_is_alpha(p->name)) {
            rc = gif_ply_read_scalar_int(r, p->type, &iv);
            if (rc != GIF_PLY_OK) return rc;
            face.a = gif_ply_clamp_u8(iv);
            face.has_color = 1;
        } else if (gif_ply_name_is_material_index(p->name)) {
            rc = gif_ply_read_scalar_int(r, p->type, &iv);
            if (rc != GIF_PLY_OK) return rc;
            face.material_index = gif_ply_clamp_u8(iv);
        } else {
            rc = gif_ply_skip_scalar(r, p->type);
            if (rc != GIF_PLY_OK) return rc;
            info->skipped_properties = info->skipped_properties + 1u;
        }
        ++i;
    }

    return gif_ply_emit_face(mesh, &face, source_face, info);
}

static int gif_ply_import_edge(GifPlyReader *r, const GifPlyElemDef *e, GifPlyMesh *mesh, GifPlyInfo *info)
{
    GifPlyEdge edge;
    int i;
    int rc;
    long iv;
    const GifPlyPropDef *p;
    int has_a;
    int has_b;

    memset(&edge, 0, sizeof(edge));
    has_a = 0;
    has_b = 0;

    i = 0;
    while (i < e->prop_count) {
        p = &e->props[i];
        if (p->kind == GIF_PLY_PROP_LIST) {
            rc = gif_ply_skip_list(r, p);
            if (rc != GIF_PLY_OK) return rc;
            info->skipped_properties = info->skipped_properties + 1u;
        } else if (gif_ply_name_is_edge_a(p->name)) {
            rc = gif_ply_read_scalar_int(r, p->type, &iv);
            if (rc != GIF_PLY_OK) return rc;
            if (iv < 0L) return GIF_PLY_ERR_PARSE;
            edge.a = (unsigned long)iv;
            has_a = 1;
        } else if (gif_ply_name_is_edge_b(p->name)) {
            rc = gif_ply_read_scalar_int(r, p->type, &iv);
            if (rc != GIF_PLY_OK) return rc;
            if (iv < 0L) return GIF_PLY_ERR_PARSE;
            edge.b = (unsigned long)iv;
            has_b = 1;
        } else if (gif_ply_name_is_crease(p->name)) {
            rc = gif_ply_read_scalar_fixed(r, p->type, &edge.crease);
            if (rc != GIF_PLY_OK) return rc;
            edge.flags = (unsigned char)(edge.flags | GIF_PLY_EDGE_HAS_CREASE);
        } else {
            rc = gif_ply_skip_scalar(r, p->type);
            if (rc != GIF_PLY_OK) return rc;
            info->skipped_properties = info->skipped_properties + 1u;
        }
        ++i;
    }

    if (!has_a || !has_b || edge.a >= mesh->vertex_count || edge.b >= mesh->vertex_count) {
        info->skipped_edges = info->skipped_edges + 1u;
        return GIF_PLY_OK;
    }

    if (mesh->edges == (GifPlyEdge *)0 || mesh->edge_capacity == 0u) {
        info->skipped_edges = info->skipped_edges + 1u;
        return GIF_PLY_OK;
    }

    if (mesh->edge_count >= mesh->edge_capacity) return GIF_PLY_ERR_CAPACITY;
    mesh->edges[mesh->edge_count] = edge;
    mesh->edge_count = mesh->edge_count + 1u;
    return GIF_PLY_OK;
}

static int gif_ply_skip_element_instance(GifPlyReader *r, const GifPlyElemDef *e, GifPlyInfo *info)
{
    int i;
    int rc;
    const GifPlyPropDef *p;

    i = 0;
    while (i < e->prop_count) {
        p = &e->props[i];
        if (p->kind == GIF_PLY_PROP_LIST) rc = gif_ply_skip_list(r, p);
        else rc = gif_ply_skip_scalar(r, p->type);
        if (rc != GIF_PLY_OK) return rc;
        info->skipped_properties = info->skipped_properties + 1u;
        ++i;
    }
    return GIF_PLY_OK;
}

int gif_ply_import_memory(const unsigned char *data,
                          unsigned long size,
                          GifPlyMesh *mesh,
                          GifPlyInfo *out_info)
{
    GifPlyHeader h;
    GifPlyReader r;
    GifPlyInfo info;
    int rc;
    int ei;
    unsigned long j;
    const GifPlyElemDef *e;

    if (data == (const unsigned char *)0 || mesh == (GifPlyMesh *)0) return GIF_PLY_ERR_NULL;
    if (mesh->vertices == (GifPlyVertex *)0 || mesh->indices == (unsigned long *)0) return GIF_PLY_ERR_NULL;

    rc = gif_ply_parse_header(data, size, &h);
    if (rc != GIF_PLY_OK) return rc;
    gif_ply_info_from_header(&h, &info);

    mesh->vertex_count = 0u;
    mesh->index_count = 0u;
    mesh->edge_count = 0u;
    mesh->triangle_count = 0u;
    mesh->tri_uv_count = 0u;

    r.data = data;
    r.size = size;
    r.pos = h.body_offset;
    r.mode = (h.format == GIF_PLY_FORMAT_ASCII) ? GIF_PLY_READ_ASCII : h.format;
    r.endian = (h.format == GIF_PLY_FORMAT_BINARY_BE) ? GIF_PLY_READ_BIN_BE : GIF_PLY_READ_BIN_LE;

    ei = 0;
    while (ei < h.elem_count) {
        e = &h.elems[ei];
        if (gif_ply_streq(e->name, "vertex")) {
            if (e->count > mesh->vertex_capacity) return GIF_PLY_ERR_CAPACITY;
            j = 0u;
            while (j < e->count) {
                rc = gif_ply_import_vertex(&r, e, mesh);
                if (rc != GIF_PLY_OK) return rc;
                ++j;
            }
        } else if (gif_ply_streq(e->name, "face")) {
            j = 0u;
            while (j < e->count) {
                rc = gif_ply_import_face(&r, e, mesh, &info, j);
                if (rc != GIF_PLY_OK) return rc;
                ++j;
            }
        } else if (gif_ply_streq(e->name, "edge")) {
            j = 0u;
            while (j < e->count) {
                rc = gif_ply_import_edge(&r, e, mesh, &info);
                if (rc != GIF_PLY_OK) return rc;
                ++j;
            }
        } else {
            info.skipped_elements = info.skipped_elements + 1u;
            j = 0u;
            while (j < e->count) {
                rc = gif_ply_skip_element_instance(&r, e, &info);
                if (rc != GIF_PLY_OK) return rc;
                ++j;
            }
        }
        ++ei;
    }

    if (!info.saw_vertex) return GIF_PLY_ERR_BAD_HEADER;
    if (out_info != (GifPlyInfo *)0) *out_info = info;
    return GIF_PLY_OK;
}

const char *gif_ply_error_string(int code)
{
    switch (code) {
        case GIF_PLY_OK: return "ok";
        case GIF_PLY_ERR_NULL: return "null argument";
        case GIF_PLY_ERR_BAD_MAGIC: return "bad PLY magic";
        case GIF_PLY_ERR_BAD_HEADER: return "bad PLY header";
        case GIF_PLY_ERR_UNSUPPORTED: return "unsupported PLY feature";
        case GIF_PLY_ERR_CAPACITY: return "caller buffer capacity exceeded";
        case GIF_PLY_ERR_TRUNCATED: return "truncated input";
        case GIF_PLY_ERR_PARSE: return "parse error";
        case GIF_PLY_ERR_OVERFLOW: return "integer/fixed overflow";
        case GIF_PLY_ERR_FACE_TOO_LARGE: return "face/list exceeds GIF_PLY_MAX_FACE_VERTS";
        default: return "unknown error";
    }
}
