#include "ir_fx.h"

#include <ctype.h>
#include <string.h>

static const unsigned long ir_pow10_tbl[] = {
    1UL,
    10UL,
    100UL,
    1000UL,
    10000UL,
    100000UL,
    1000000UL,
    10000000UL,
    100000000UL,
    1000000000UL
};

static const char* ir_skip_ws_local(const char* s)
{
    while (*s && isspace((unsigned char)*s)) {
        ++s;
    }
    return s;
}

static int ir_is_sep(int ch)
{
    return ch == ',' || isspace((unsigned char)ch);
}

static int ir_fx_has_exp_marker(const char* s);
static int ir_fx_normalize_exponent(const char* s, char* out, size_t cap);

int ir_i32_from_str(const char* s, ir_i32* out)
{
    int neg;
    unsigned long value;
    const char* p;
    if (!s || !out) {
        return IR_ERR_ARGUMENT;
    }
    p = ir_skip_ws_local(s);
    neg = 0;
    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        ++p;
    }
    if (!isdigit((unsigned char)*p)) {
        return IR_ERR_FORMAT;
    }
    value = 0UL;
    while (isdigit((unsigned char)*p)) {
        value = value * 10UL + (unsigned long)(*p - '0');
        if ((!neg && value > 2147483647UL) || (neg && value > 2147483648UL)) {
            return IR_ERR_OVERFLOW;
        }
        ++p;
    }
    p = ir_skip_ws_local(p);
    if (*p != '\0') {
        return IR_ERR_FORMAT;
    }
    if (neg) {
        if (value == 2147483648UL) {
            *out = (-2147483647L - 1L);
        } else {
            *out = -(ir_i32)value;
        }
    } else {
        *out = (ir_i32)value;
    }
    return IR_OK;
}

int ir_u32_from_str(const char* s, ir_u32* out)
{
    unsigned long value;
    const char* p;
    if (!s || !out) {
        return IR_ERR_ARGUMENT;
    }
    p = ir_skip_ws_local(s);
    if (*p == '+') {
        ++p;
    }
    if (!isdigit((unsigned char)*p)) {
        return IR_ERR_FORMAT;
    }
    value = 0UL;
    while (isdigit((unsigned char)*p)) {
        if (value > (4294967295UL / 10UL)) {
            return IR_ERR_OVERFLOW;
        }
        value = value * 10UL + (unsigned long)(*p - '0');
        if (value > 4294967295UL) {
            return IR_ERR_OVERFLOW;
        }
        ++p;
    }
    p = ir_skip_ws_local(p);
    if (*p != '\0') {
        return IR_ERR_FORMAT;
    }
    *out = (ir_u32)value;
    return IR_OK;
}


int ir_u32_from_irr_index_str(const char* s, ir_u32* out)
{
    unsigned long value;
    const char* p;
    int had_digits;
    int had_frac_digits;
    int neg;
    int rc;
    if (!s || !out) {
        return IR_ERR_ARGUMENT;
    }
    rc = ir_u32_from_str(s, out);
    if (rc == IR_OK) {
        return IR_OK;
    }
    if (ir_fx_has_exp_marker(s) != 0) {
        char normalized[128];
        rc = ir_fx_normalize_exponent(s, normalized, sizeof(normalized));
        if (rc != IR_OK) {
            return rc;
        }
        return ir_u32_from_irr_index_str(normalized, out);
    }
    p = ir_skip_ws_local(s);
    neg = 0;
    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        ++p;
    }
    value = 0UL;
    had_digits = 0;
    while (isdigit((unsigned char)*p)) {
        if (value > (4294967295UL / 10UL)) {
            return IR_ERR_OVERFLOW;
        }
        value = value * 10UL + (unsigned long)(*p - '0');
        if (value > 4294967295UL) {
            return IR_ERR_OVERFLOW;
        }
        had_digits = 1;
        ++p;
    }
    had_frac_digits = 0;
    if (*p == '.') {
        ++p;
        while (isdigit((unsigned char)*p)) {
            had_frac_digits = 1;
            ++p;
        }
    }
    if (!had_digits && !had_frac_digits) {
        return IR_ERR_FORMAT;
    }
    p = ir_skip_ws_local(p);
    if (*p != '\0') {
        return IR_ERR_FORMAT;
    }
    if (neg) {
        return IR_ERR_RANGE;
    }
    *out = (ir_u32)value;
    return IR_OK;
}

int ir_bool_from_str(const char* s, int* out)
{
    const char* p;
    size_t n;
    if (!s || !out) {
        return IR_ERR_ARGUMENT;
    }
    p = ir_skip_ws_local(s);
    n = strlen(p);
    while (n > 0U && isspace((unsigned char)p[n - 1U])) {
        --n;
    }
    if (n == 4U && (p[0] == 't' || p[0] == 'T') && (p[1] == 'r' || p[1] == 'R') && (p[2] == 'u' || p[2] == 'U') && (p[3] == 'e' || p[3] == 'E')) {
        *out = 1;
        return IR_OK;
    }
    if (n == 5U && (p[0] == 'f' || p[0] == 'F') && (p[1] == 'a' || p[1] == 'A') && (p[2] == 'l' || p[2] == 'L') && (p[3] == 's' || p[3] == 'S') && (p[4] == 'e' || p[4] == 'E')) {
        *out = 0;
        return IR_OK;
    }
    if (n == 1U && p[0] == '1') {
        *out = 1;
        return IR_OK;
    }
    if (n == 1U && p[0] == '0') {
        *out = 0;
        return IR_OK;
    }
    return IR_ERR_FORMAT;
}

static int ir_fx_from_plain_str(const char* s, ir_fx16* out)
{
    int neg;
    unsigned long integer_part;
    unsigned long frac_part;
    unsigned long scale;
    unsigned long frac_fixed;
    const char* p;
    int frac_digits;
    if (!s || !out) {
        return IR_ERR_ARGUMENT;
    }
    p = ir_skip_ws_local(s);
    neg = 0;
    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        ++p;
    }
    if (!isdigit((unsigned char)*p) && *p != '.') {
        return IR_ERR_FORMAT;
    }
    integer_part = 0UL;
    while (isdigit((unsigned char)*p)) {
        integer_part = integer_part * 10UL + (unsigned long)(*p - '0');
        if (integer_part > 32767UL) {
            return IR_ERR_OVERFLOW;
        }
        ++p;
    }
    frac_part = 0UL;
    frac_digits = 0;
    if (*p == '.') {
        ++p;
        while (isdigit((unsigned char)*p)) {
            if (frac_digits < 9) {
                frac_part = frac_part * 10UL + (unsigned long)(*p - '0');
                ++frac_digits;
            }
            ++p;
        }
    }
    p = ir_skip_ws_local(p);
    if (*p != '\0') {
        return IR_ERR_FORMAT;
    }
    scale = ir_pow10_tbl[frac_digits];
    frac_fixed = 0UL;
    if (frac_digits > 0) {
        frac_fixed = (frac_part * 65536UL + (scale / 2UL)) / scale;
    }
    *out = (ir_fx16)((long)integer_part << 16);
    *out += (ir_fx16)frac_fixed;
    if (neg) {
        *out = -*out;
    }
    return IR_OK;
}

static int ir_fx_has_exp_marker(const char* s)
{
    const char* p;
    if (!s) {
        return 0;
    }
    p = s;
    while (*p) {
        if (*p == 'e' || *p == 'E') {
            return 1;
        }
        ++p;
    }
    return 0;
}

static int ir_fx_append_char_local(char* out, size_t cap, size_t* len, int ch)
{
    if (!out || !len || *len + 1U >= cap) {
        return IR_ERR_CAPACITY;
    }
    out[*len] = (char)ch;
    *len += 1U;
    out[*len] = '\0';
    return IR_OK;
}

static int ir_fx_normalize_exponent(const char* s, char* out, size_t cap)
{
    const char* p;
    char digits[96];
    size_t dlen;
    size_t int_digits;
    long exp;
    long decimal_pos;
    int neg;
    int exp_neg;
    int saw_digit;
    int exp_saw_digit;
    size_t len;
    size_t i;
    int rc;

    if (!s || !out || cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    p = ir_skip_ws_local(s);
    neg = 0;
    if (*p == '+' || *p == '-') {
        neg = (*p == '-');
        ++p;
    }

    dlen = 0U;
    int_digits = 0U;
    saw_digit = 0;
    while (isdigit((unsigned char)*p)) {
        if (dlen + 1U >= sizeof(digits)) {
            return IR_ERR_OVERFLOW;
        }
        digits[dlen++] = *p++;
        ++int_digits;
        saw_digit = 1;
    }
    if (*p == '.') {
        ++p;
        while (isdigit((unsigned char)*p)) {
            if (dlen + 1U >= sizeof(digits)) {
                return IR_ERR_OVERFLOW;
            }
            digits[dlen++] = *p++;
            saw_digit = 1;
        }
    }
    if (!saw_digit) {
        return IR_ERR_FORMAT;
    }
    if (*p != 'e' && *p != 'E') {
        return IR_ERR_FORMAT;
    }
    ++p;
    exp_neg = 0;
    if (*p == '+' || *p == '-') {
        exp_neg = (*p == '-');
        ++p;
    }
    exp = 0L;
    exp_saw_digit = 0;
    while (isdigit((unsigned char)*p)) {
        if (exp < 1000L) {
            exp = exp * 10L + (long)(*p - '0');
        }
        ++p;
        exp_saw_digit = 1;
    }
    if (!exp_saw_digit) {
        return IR_ERR_FORMAT;
    }
    p = ir_skip_ws_local(p);
    if (*p != '\0') {
        return IR_ERR_FORMAT;
    }
    if (exp_neg) {
        exp = -exp;
    }
    decimal_pos = (long)int_digits + exp;

    len = 0U;
    out[0] = '\0';
    if (neg) {
        rc = ir_fx_append_char_local(out, cap, &len, '-');
        if (rc != IR_OK) return rc;
    }
    if (decimal_pos <= 0L) {
        long zeros;
        rc = ir_fx_append_char_local(out, cap, &len, '0'); if (rc != IR_OK) return rc;
        rc = ir_fx_append_char_local(out, cap, &len, '.'); if (rc != IR_OK) return rc;
        zeros = -decimal_pos;
        while (zeros > 0L) {
            rc = ir_fx_append_char_local(out, cap, &len, '0');
            if (rc != IR_OK) return rc;
            --zeros;
        }
        for (i = 0U; i < dlen; ++i) {
            rc = ir_fx_append_char_local(out, cap, &len, digits[i]);
            if (rc != IR_OK) return rc;
        }
    } else if ((size_t)decimal_pos >= dlen) {
        long zeros;
        for (i = 0U; i < dlen; ++i) {
            rc = ir_fx_append_char_local(out, cap, &len, digits[i]);
            if (rc != IR_OK) return rc;
        }
        zeros = decimal_pos - (long)dlen;
        while (zeros > 0L) {
            rc = ir_fx_append_char_local(out, cap, &len, '0');
            if (rc != IR_OK) return rc;
            --zeros;
        }
    } else {
        for (i = 0U; i < (size_t)decimal_pos; ++i) {
            rc = ir_fx_append_char_local(out, cap, &len, digits[i]);
            if (rc != IR_OK) return rc;
        }
        rc = ir_fx_append_char_local(out, cap, &len, '.');
        if (rc != IR_OK) return rc;
        for (i = (size_t)decimal_pos; i < dlen; ++i) {
            rc = ir_fx_append_char_local(out, cap, &len, digits[i]);
            if (rc != IR_OK) return rc;
        }
    }
    return IR_OK;
}

int ir_fx_from_str(const char* s, ir_fx16* out)
{
    int rc;
    char normalized[128];
    rc = ir_fx_from_plain_str(s, out);
    if (rc == IR_OK || !ir_fx_has_exp_marker(s)) {
        return rc;
    }
    rc = ir_fx_normalize_exponent(s, normalized, sizeof(normalized));
    if (rc != IR_OK) {
        return rc;
    }
    return ir_fx_from_plain_str(normalized, out);
}

int ir_fx_parse_list(const char* s, ir_fx16* out, int count)
{
    int i;
    const char* p;
    char buf[64];
    int bi;
    int rc;
    if (!s || !out || count <= 0) {
        return IR_ERR_ARGUMENT;
    }
    p = s;
    for (i = 0; i < count; ++i) {
        while (*p && ir_is_sep((unsigned char)*p)) {
            ++p;
        }
        if (!*p) {
            return IR_ERR_FORMAT;
        }
        bi = 0;
        while (*p && !ir_is_sep((unsigned char)*p)) {
            if (bi + 1 >= (int)sizeof(buf)) {
                return IR_ERR_OVERFLOW;
            }
            buf[bi++] = *p++;
        }
        buf[bi] = '\0';
        rc = ir_fx_from_str(buf, &out[i]);
        if (rc != IR_OK) {
            return rc;
        }
    }
    while (*p && ir_is_sep((unsigned char)*p)) {
        ++p;
    }
    if (*p != '\0') {
        return IR_ERR_FORMAT;
    }
    return IR_OK;
}

int ir_vec2fx_from_str(const char* s, ir_vec2fx* out)
{
    ir_fx16 tmp[2];
    int rc;
    if (!out) {
        return IR_ERR_ARGUMENT;
    }
    rc = ir_fx_parse_list(s, tmp, 2);
    if (rc != IR_OK) {
        return rc;
    }
    out->x = tmp[0];
    out->y = tmp[1];
    return IR_OK;
}

int ir_vec3fx_from_str(const char* s, ir_vec3fx* out)
{
    ir_fx16 tmp[3];
    int rc;
    if (!out) {
        return IR_ERR_ARGUMENT;
    }
    rc = ir_fx_parse_list(s, tmp, 3);
    if (rc != IR_OK) {
        return rc;
    }
    out->x = tmp[0];
    out->y = tmp[1];
    out->z = tmp[2];
    return IR_OK;
}

int ir_vec4fx_from_str(const char* s, ir_vec4fx* out)
{
    ir_fx16 tmp[4];
    int rc;
    if (!out) {
        return IR_ERR_ARGUMENT;
    }
    rc = ir_fx_parse_list(s, tmp, 4);
    if (rc != IR_OK) {
        return rc;
    }
    out->x = tmp[0];
    out->y = tmp[1];
    out->z = tmp[2];
    out->w = tmp[3];
    return IR_OK;
}

int ir_box3fx_from_attrs(const char* min_edge, const char* max_edge, ir_box3fx* out)
{
    int rc;
    if (!min_edge || !max_edge || !out) {
        return IR_ERR_ARGUMENT;
    }
    rc = ir_vec3fx_from_str(min_edge, &out->min_edge);
    if (rc != IR_OK) {
        return rc;
    }
    rc = ir_vec3fx_from_str(max_edge, &out->max_edge);
    if (rc != IR_OK) {
        return rc;
    }
    return IR_OK;
}

int ir_color_argb_from_hex(const char* s, ir_u32* out)
{
    const char* p;
    int digits;
    unsigned long value;
    int ch;
    unsigned long v;
    if (!s || !out) {
        return IR_ERR_ARGUMENT;
    }
    p = ir_skip_ws_local(s);
    if (*p == '#') {
        ++p;
    }
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }
    value = 0UL;
    digits = 0;
    while (*p && !isspace((unsigned char)*p)) {
        ch = (unsigned char)*p;
        if (ch >= '0' && ch <= '9') {
            v = (unsigned long)(ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            v = (unsigned long)(10 + ch - 'a');
        } else if (ch >= 'A' && ch <= 'F') {
            v = (unsigned long)(10 + ch - 'A');
        } else {
            return IR_ERR_FORMAT;
        }
        if (digits >= 8) {
            return IR_ERR_OVERFLOW;
        }
        value = (value << 4) | v;
        ++digits;
        ++p;
    }
    p = ir_skip_ws_local(p);
    if (*p != '\0' || digits == 0) {
        return IR_ERR_FORMAT;
    }
    *out = (ir_u32)value;
    return IR_OK;
}

static int ir_reverse_copy(char* dst, size_t cap, const char* src, size_t n)
{
    size_t i;
    if (!dst || cap <= n) {
        return IR_ERR_CAPACITY;
    }
    for (i = 0; i < n; ++i) {
        dst[i] = src[n - 1U - i];
    }
    dst[n] = '\0';
    return IR_OK;
}

int ir_u32_to_str(ir_u32 value, char* out, size_t cap)
{
    char tmp[32];
    size_t n;
    if (!out || cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    n = 0U;
    do {
        tmp[n++] = (char)('0' + (value % 10UL));
        value /= 10UL;
    } while (value && n < sizeof(tmp));
    return ir_reverse_copy(out, cap, tmp, n);
}

int ir_i32_to_str(ir_i32 value, char* out, size_t cap)
{
    unsigned long mag;
    int rc;
    char tmp[32];
    size_t n;
    if (!out || cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    if (value < 0) {
        if (cap < 2U) {
            return IR_ERR_CAPACITY;
        }
        out[0] = '-';
        if (value == (-2147483647L - 1L)) {
            mag = 2147483648UL;
        } else {
            mag = (unsigned long)(-value);
        }
        rc = ir_u32_to_str((ir_u32)mag, tmp, sizeof(tmp));
        if (rc != IR_OK) {
            return rc;
        }
        if (strlen(tmp) + 1U >= cap) {
            return IR_ERR_CAPACITY;
        }
        strcpy(out + 1, tmp);
        return IR_OK;
    }
    n = 0U;
    do {
        tmp[n++] = (char)('0' + ((unsigned long)value % 10UL));
        value = (ir_i32)((unsigned long)value / 10UL);
    } while (value && n < sizeof(tmp));
    return ir_reverse_copy(out, cap, tmp, n);
}

int ir_fx_to_str(ir_fx16 value, char* out, size_t cap)
{
    char ibuf[32];
    unsigned long frac;
    unsigned long dec;
    int rc;
    ir_i32 integer_part;
    size_t olen;
    if (!out || cap == 0U) {
        return IR_ERR_ARGUMENT;
    }
    if (value < 0) {
        if (cap < 2U) {
            return IR_ERR_CAPACITY;
        }
        *out++ = '-';
        --cap;
        value = -value;
    }
    integer_part = (ir_i32)(value >> 16);
    frac = (unsigned long)(value & 0xFFFFL);
    dec = (frac * 1000000UL + 32768UL) >> 16;
    if (dec >= 1000000UL) {
        ++integer_part;
        dec -= 1000000UL;
    }
    rc = ir_i32_to_str(integer_part, ibuf, sizeof(ibuf));
    if (rc != IR_OK) {
        return rc;
    }
    olen = strlen(ibuf);
    if (olen + 8U >= cap) {
        return IR_ERR_CAPACITY;
    }
    strcpy(out, ibuf);
    out += olen;
    *out++ = '.';
    out[5] = (char)('0' + (dec % 10UL)); dec /= 10UL;
    out[4] = (char)('0' + (dec % 10UL)); dec /= 10UL;
    out[3] = (char)('0' + (dec % 10UL)); dec /= 10UL;
    out[2] = (char)('0' + (dec % 10UL)); dec /= 10UL;
    out[1] = (char)('0' + (dec % 10UL)); dec /= 10UL;
    out[0] = (char)('0' + (dec % 10UL));
    out[6] = '\0';
    return IR_OK;
}
