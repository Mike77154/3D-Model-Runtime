#include "hlfp_internal.h"

#include <stdio.h>

#if CHAR_BIT != 8
#error "hlfp requires 8-bit bytes"
#endif

typedef char hlfp__check_short_is_16[(sizeof(short) == 2) ? 1 : -1];
typedef char hlfp__check_int_is_32[(sizeof(int) == 4) ? 1 : -1];

void hlfp_error_clear(hlfp_error *err) {
    if (err == 0) {
        return;
    }
    err->code = HLFP_OK;
    err->line = 0;
    err->column = 0;
    err->message[0] = '\0';
}

const char *hlfp_strerror(int code) {
    switch (code) {
        case HLFP_OK: return "ok";
        case HLFP_ERR_BAD_ARGUMENT: return "bad argument";
        case HLFP_ERR_BAD_FORMAT: return "bad format";
        case HLFP_ERR_BAD_MAGIC: return "bad magic";
        case HLFP_ERR_BAD_VERSION: return "bad version";
        case HLFP_ERR_OUT_OF_RANGE: return "out of range";
        case HLFP_ERR_CAPACITY: return "capacity exceeded";
        case HLFP_ERR_MISSING_DATA: return "missing data";
        case HLFP_ERR_UNSUPPORTED: return "unsupported";
        case HLFP_ERR_DIV_BY_ZERO: return "division by zero";
        case HLFP_ERR_OVERFLOW: return "overflow";
        default: return "unknown";
    }
}

int hlfp__set_error(hlfp_error *err, int code, int line, int column, const char *msg) {
    if (err != 0) {
        err->code = code;
        err->line = line;
        err->column = column;
        if (msg != 0) {
            hlfp__copy_cstr(err->message, (int)sizeof(err->message), msg, (int)strlen(msg));
        } else {
            err->message[0] = '\0';
        }
    }
    return code;
}

int hlfp__set_errorf(hlfp_error *err, int code, int line, int column, const char *prefix, int value) {
    if (err != 0) {
        char tmp[160];
        sprintf(tmp, "%s%d", prefix != 0 ? prefix : "", value);
        err->code = code;
        err->line = line;
        err->column = column;
        hlfp__copy_cstr(err->message, (int)sizeof(err->message), tmp, (int)strlen(tmp));
    }
    return code;
}

void hlfp__zero(void *ptr, size_t size) {
    if (ptr != 0 && size != 0) {
        memset(ptr, 0, size);
    }
}

void hlfp__copy_cstr(char *dst, int dst_cap, const char *src, int src_len) {
    int n;
    if (dst == 0 || dst_cap <= 0) {
        return;
    }
    if (src == 0 || src_len <= 0) {
        dst[0] = '\0';
        return;
    }
    n = src_len;
    if (n >= dst_cap) {
        n = dst_cap - 1;
    }
    if (n > 0) {
        memcpy(dst, src, (size_t)n);
    }
    dst[n] = '\0';
}

int hlfp__is_space(int c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

int hlfp__is_digit(int c) {
    return c >= '0' && c <= '9';
}

hlfp_fixed hlfp_fixed_from_int(int value) {
    if (value > (INT_MAX >> HLFP_FIXED_FRAC_BITS)) {
        return INT_MAX;
    }
    if (value < (INT_MIN >> HLFP_FIXED_FRAC_BITS)) {
        return INT_MIN;
    }
    return (hlfp_fixed)(value << HLFP_FIXED_FRAC_BITS);
}

int hlfp_fixed_to_int_trunc(hlfp_fixed value) {
    return (int)(value >> HLFP_FIXED_FRAC_BITS);
}

int hlfp_fixed_to_int_round(hlfp_fixed value) {
    if (value >= 0) {
        return (int)((value + HLFP_FIXED_HALF) >> HLFP_FIXED_FRAC_BITS);
    }
    return (int)(-(((-value) + HLFP_FIXED_HALF) >> HLFP_FIXED_FRAC_BITS));
}

const hlfp_u8 *hlfp__checked_range(const hlfp_buffer *buf, size_t off, size_t bytes, hlfp_error *err, const char *what) {
    if (buf == 0 || buf->data == 0) {
        hlfp__set_error(err, HLFP_ERR_BAD_ARGUMENT, 0, 0, "null buffer");
        return 0;
    }
    if (off > buf->size) {
        hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, what != 0 ? what : "offset out of range");
        return 0;
    }
    if (bytes > (buf->size - off)) {
        hlfp__set_error(err, HLFP_ERR_OUT_OF_RANGE, 0, 0, what != 0 ? what : "range out of range");
        return 0;
    }
    return buf->data + off;
}

int hlfp__read_line(hlfp_text_cursor *cur, const char **line_start, int *line_len) {
    size_t start;
    size_t p;
    if (cur == 0 || line_start == 0 || line_len == 0) {
        return 0;
    }
    if (cur->pos >= cur->size) {
        return 0;
    }
    start = cur->pos;
    p = start;
    while (p < cur->size && cur->data[p] != '\n' && cur->data[p] != '\r') {
        ++p;
    }
    *line_start = cur->data + start;
    *line_len = (int)(p - start);
    if (p < cur->size) {
        if (cur->data[p] == '\r') {
            ++p;
            if (p < cur->size && cur->data[p] == '\n') {
                ++p;
            }
        } else {
            ++p;
        }
    }
    cur->pos = p;
    ++cur->line;
    return 1;
}

void hlfp__trim_line(const char **start, int *len) {
    int i;
    int q;
    const char *s;
    int n;
    if (start == 0 || len == 0 || *start == 0) {
        return;
    }
    s = *start;
    n = *len;
    while (n > 0 && hlfp__is_space((unsigned char)*s)) {
        ++s;
        --n;
    }
    while (n > 0 && hlfp__is_space((unsigned char)s[n - 1])) {
        --n;
    }
    q = 0;
    for (i = 0; i + 1 < n; ++i) {
        if (s[i] == '"') {
            q = !q;
        }
        if (!q && s[i] == '/' && s[i + 1] == '/') {
            n = i;
            while (n > 0 && hlfp__is_space((unsigned char)s[n - 1])) {
                --n;
            }
            break;
        }
    }
    *start = s;
    *len = n;
}

int hlfp__parse_i32_token(const char *s, int len, int *out) {
    int sign;
    int i;
    unsigned int v;
    int digits;
    if (s == 0 || out == 0 || len <= 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }
    sign = 1;
    i = 0;
    if (s[i] == '+' || s[i] == '-') {
        if (s[i] == '-') {
            sign = -1;
        }
        ++i;
    }
    v = 0U;
    digits = 0;
    for (; i < len; ++i) {
        unsigned int d;
        if (!hlfp__is_digit((unsigned char)s[i])) {
            return HLFP_ERR_BAD_FORMAT;
        }
        d = (unsigned int)(s[i] - '0');
        if (v > (unsigned int)(INT_MAX / 10)) {
            return HLFP_ERR_OVERFLOW;
        }
        v *= 10U;
        if (v > (unsigned int)INT_MAX - d - (sign < 0 ? 1U : 0U)) {
            return HLFP_ERR_OVERFLOW;
        }
        v += d;
        ++digits;
    }
    if (digits == 0) {
        return HLFP_ERR_BAD_FORMAT;
    }
    if (sign < 0) {
        if (v == (unsigned int)INT_MAX + 1U) {
            *out = INT_MIN;
        } else {
            *out = -(int)v;
        }
    } else {
        *out = (int)v;
    }
    return HLFP_OK;
}

int hlfp__parse_fixed_token(const char *s, int len, hlfp_fixed *out) {
    int sign;
    int i;
    int digits_before;
    int dot_seen;
    int whole;
    char frac_digits[32];
    int frac_count;
    hlfp_fixed frac_bits;
    int bit;
    if (s == 0 || out == 0 || len <= 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }
    sign = 1;
    i = 0;
    if (s[i] == '+' || s[i] == '-') {
        if (s[i] == '-') {
            sign = -1;
        }
        ++i;
    }
    whole = 0;
    digits_before = 0;
    while (i < len && hlfp__is_digit((unsigned char)s[i])) {
        int d;
        d = s[i] - '0';
        if (whole > INT_MAX / 10) {
            return HLFP_ERR_OVERFLOW;
        }
        whole *= 10;
        if (whole > INT_MAX - d) {
            return HLFP_ERR_OVERFLOW;
        }
        whole += d;
        ++i;
        ++digits_before;
    }
    dot_seen = 0;
    frac_count = 0;
    if (i < len && s[i] == '.') {
        dot_seen = 1;
        ++i;
        while (i < len) {
            if (!hlfp__is_digit((unsigned char)s[i])) {
                return HLFP_ERR_BAD_FORMAT;
            }
            if (frac_count < (int)sizeof(frac_digits)) {
                frac_digits[frac_count++] = (char)(s[i] - '0');
            }
            ++i;
        }
    }
    if (digits_before == 0 && !dot_seen) {
        return HLFP_ERR_BAD_FORMAT;
    }
    if (digits_before == 0 && frac_count == 0) {
        return HLFP_ERR_BAD_FORMAT;
    }
    if (whole > (INT_MAX >> HLFP_FIXED_FRAC_BITS)) {
        return HLFP_ERR_OVERFLOW;
    }
    frac_bits = 0;
    for (bit = 0; bit < HLFP_FIXED_FRAC_BITS; ++bit) {
        int carry;
        int j;
        carry = 0;
        for (j = frac_count - 1; j >= 0; --j) {
            int value;
            value = frac_digits[j] * 2 + carry;
            frac_digits[j] = (char)(value % 10);
            carry = value / 10;
        }
        frac_bits = (hlfp_fixed)((frac_bits << 1) | carry);
    }
    *out = (hlfp_fixed)((whole << HLFP_FIXED_FRAC_BITS) | frac_bits);
    if (sign < 0) {
        *out = (hlfp_fixed)(-*out);
    }
    return HLFP_OK;
}

int hlfp__parse_quoted_string(const char *s, int len, int *used, char *dst, int dst_cap) {
    int i;
    int start;
    int end;
    if (s == 0 || len <= 0 || used == 0 || dst == 0 || dst_cap <= 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }
    i = 0;
    while (i < len && hlfp__is_space((unsigned char)s[i])) {
        ++i;
    }
    if (i >= len || s[i] != '"') {
        return HLFP_ERR_BAD_FORMAT;
    }
    start = ++i;
    while (i < len && s[i] != '"') {
        ++i;
    }
    if (i >= len) {
        return HLFP_ERR_BAD_FORMAT;
    }
    end = i;
    ++i;
    hlfp__copy_cstr(dst, dst_cap, s + start, end - start);
    *used = i;
    return HLFP_OK;
}

int hlfp__float_bits_to_fixed(hlfp_u32 bits, hlfp_fixed *out) {
    unsigned int sign;
    unsigned int exp;
    unsigned int frac;
    unsigned int mant;
    int shift;
    hlfp_u32 value;
    if (out == 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }
    sign = (bits >> 31) & 1U;
    exp = (bits >> 23) & 0xFFU;
    frac = bits & 0x7FFFFFU;
    if (exp == 0xFFU) {
        *out = sign ? INT_MIN : INT_MAX;
        return HLFP_ERR_OVERFLOW;
    }
    if (exp == 0U) {
        if (frac == 0U) {
            *out = 0;
            return HLFP_OK;
        }
        mant = frac;
        shift = -133;
    } else {
        mant = frac | 0x800000U;
        shift = (int)exp - 127 - 7;
    }
    if (shift > 7) {
        *out = sign ? INT_MIN : INT_MAX;
        return HLFP_ERR_OVERFLOW;
    }
    if (shift >= 0) {
        value = mant << shift;
    } else {
        int rshift;
        rshift = -shift;
        if (rshift >= 32) {
            value = 0U;
        } else {
            value = (mant + (1U << (rshift - 1))) >> rshift;
        }
    }
    if (value > (hlfp_u32)INT_MAX + (sign ? 1U : 0U)) {
        *out = sign ? INT_MIN : INT_MAX;
        return HLFP_ERR_OVERFLOW;
    }
    if (sign) {
        if (value == (hlfp_u32)INT_MAX + 1U) {
            *out = INT_MIN;
        } else {
            *out = -(hlfp_fixed)value;
        }
    } else {
        *out = (hlfp_fixed)value;
    }
    return HLFP_OK;
}

int hlfp__fixed_mul_i16(hlfp_fixed scale, hlfp_i16 sample, hlfp_fixed *out) {
    int sign;
    unsigned int a;
    unsigned int b;
    if (out == 0) {
        return HLFP_ERR_BAD_ARGUMENT;
    }
    sign = 1;
    if (scale < 0) {
        sign = -sign;
        a = (unsigned int)(-scale);
    } else {
        a = (unsigned int)scale;
    }
    if (sample < 0) {
        sign = -sign;
        b = (unsigned int)(-(int)sample);
    } else {
        b = (unsigned int)sample;
    }
    if (b != 0U && a > ((unsigned int)INT_MAX) / b) {
        *out = sign < 0 ? INT_MIN : INT_MAX;
        return HLFP_ERR_OVERFLOW;
    }
    *out = (hlfp_fixed)(a * b);
    if (sign < 0) {
        *out = (hlfp_fixed)(-*out);
    }
    return HLFP_OK;
}

hlfp_u16 hlfp__read_u16le(const hlfp_u8 *p) {
    return (hlfp_u16)((hlfp_u16)p[0] | ((hlfp_u16)p[1] << 8));
}

hlfp_i16 hlfp__read_i16le(const hlfp_u8 *p) {
    return (hlfp_i16)hlfp__read_u16le(p);
}

hlfp_u32 hlfp__read_u32le(const hlfp_u8 *p) {
    return ((hlfp_u32)p[0]) |
           ((hlfp_u32)p[1] << 8) |
           ((hlfp_u32)p[2] << 16) |
           ((hlfp_u32)p[3] << 24);
}

hlfp_i32 hlfp__read_i32le(const hlfp_u8 *p) {
    return (hlfp_i32)hlfp__read_u32le(p);
}


void hlfp_mem_writer_init(hlfp_mem_writer *w, void *data, size_t capacity) {
    if (w == 0) {
        return;
    }
    w->data = (hlfp_u8 *)data;
    w->capacity = capacity;
    w->size = 0U;
}

int hlfp_mem_writer_write(void *user, const void *data, size_t size) {
    hlfp_mem_writer *w;
    if (user == 0) {
        return 0;
    }
    w = (hlfp_mem_writer *)user;
    if (size > w->capacity - w->size) {
        return 0;
    }
    if (data != 0 && size != 0U) {
        memcpy(w->data + w->size, data, size);
    }
    w->size += size;
    return 1;
}
