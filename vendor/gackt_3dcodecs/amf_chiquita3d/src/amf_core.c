#include "chiqui_amf.h"
#include <string.h>

static int amf_is_space_c(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static amf_slice amf_trim_slice(amf_slice s)
{
    while (s.len > 0u && amf_is_space_c(s.ptr[0])) {
        s.ptr++;
        s.len--;
    }
    while (s.len > 0u && amf_is_space_c(s.ptr[s.len - 1u])) {
        s.len--;
    }
    return s;
}

static int amf_slice_eq_lit(amf_slice s, const char *lit)
{
    size_t n;
    n = 0u;
    while (lit[n] != '\0') {
        n++;
    }
    if (s.len != n) {
        return 0;
    }
    return memcmp(s.ptr, lit, n) == 0;
}

void amf_arena_init(amf_arena *arena, void *memory, size_t size)
{
    if (arena != 0) {
        arena->base = (amf_u8 *)memory;
        arena->size = size;
        arena->used = 0u;
    }
}

void amf_arena_reset(amf_arena *arena)
{
    if (arena != 0) {
        arena->used = 0u;
    }
}

void *amf_arena_alloc(amf_arena *arena, size_t size, size_t align)
{
    size_t p;
    size_t aligned;
    size_t mask;
    if (arena == 0 || arena->base == 0) {
        return 0;
    }
    if (align == 0u) {
        align = 1u;
    }
    mask = align - 1u;
    p = arena->used;
    aligned = (p + mask) & ~mask;
    if (aligned < p) {
        return 0;
    }
    if (size > arena->size || aligned > arena->size - size) {
        return 0;
    }
    arena->used = aligned + size;
    return (void *)(arena->base + aligned);
}

void *amf_arena_calloc(amf_arena *arena, size_t count, size_t size, size_t align)
{
    void *p;
    size_t total;
    if (size != 0u && count > ((size_t)-1) / size) {
        return 0;
    }
    total = count * size;
    p = amf_arena_alloc(arena, total, align);
    if (p != 0) {
        memset(p, 0, total);
    }
    return p;
}

amf_slice amf_arena_copy_slice(amf_arena *arena, const char *ptr, size_t len)
{
    amf_slice out;
    char *dst;
    out.ptr = "";
    out.len = 0u;
    if (len == 0u) {
        return out;
    }
    dst = (char *)amf_arena_alloc(arena, len + 1u, 1u);
    if (dst == 0) {
        return out;
    }
    memcpy(dst, ptr, len);
    dst[len] = '\0';
    out.ptr = dst;
    out.len = len;
    return out;
}

void amf_diag_clear(amf_diag *diag)
{
    if (diag != 0) {
        diag->code = AMF_OK;
        diag->line = 1u;
        diag->column = 1u;
        diag->warnings = 0u;
        diag->message[0] = '\0';
    }
}

void amf_diag_set(amf_diag *diag, int code, size_t line, size_t col, const char *msg)
{
    size_t i;
    if (diag == 0) {
        return;
    }
    diag->code = code;
    diag->line = line;
    diag->column = col;
    if (msg == 0) {
        diag->message[0] = '\0';
        return;
    }
    i = 0u;
    while (msg[i] != '\0' && i + 1u < AMF_MAX_DIAG_TEXT) {
        diag->message[i] = msg[i];
        i++;
    }
    diag->message[i] = '\0';
}

const char *amf_error_name(int code)
{
    switch (code) {
    case AMF_OK: return "AMF_OK";
    case AMF_ERR_BAD_ARG: return "AMF_ERR_BAD_ARG";
    case AMF_ERR_ARENA_FULL: return "AMF_ERR_ARENA_FULL";
    case AMF_ERR_BAD_XML: return "AMF_ERR_BAD_XML";
    case AMF_ERR_BAD_NUMBER: return "AMF_ERR_BAD_NUMBER";
    case AMF_ERR_RANGE: return "AMF_ERR_RANGE";
    case AMF_ERR_REQUIRED: return "AMF_ERR_REQUIRED";
    case AMF_ERR_UNSUPPORTED_ZIP: return "AMF_ERR_UNSUPPORTED_ZIP";
    case AMF_ERR_ZIP_NEEDS_OUTPUT: return "AMF_ERR_ZIP_NEEDS_OUTPUT";
    case AMF_ERR_BAD_ZIP: return "AMF_ERR_BAD_ZIP";
    case AMF_ERR_WRITE_FULL: return "AMF_ERR_WRITE_FULL";
    case AMF_ERR_VALIDATE: return "AMF_ERR_VALIDATE";
    default: return "AMF_ERR_UNKNOWN";
    }
}

amf_fx amf_fx_from_i32(amf_i32 v)
{
    return (amf_fx)(v << AMF_FX_SHIFT);
}

amf_i32 amf_fx_to_i32(amf_fx v)
{
    return (amf_i32)(v >> AMF_FX_SHIFT);
}

static amf_fx amf_fx_abs_i(amf_fx v)
{
    if (v < 0) {
        return (amf_fx)(-v);
    }
    return v;
}

amf_fx amf_fx_mul(amf_fx a, amf_fx b)
{
#if defined(_MSC_VER)
    __int64 aa;
    __int64 bb;
    aa = (__int64)a;
    bb = (__int64)b;
    return (amf_fx)((aa * bb) >> AMF_FX_SHIFT);
#elif !defined(AMF_STRICT_C89_NO_LONGLONG) && (defined(__GNUC__) || defined(__clang__))
    long long aa;
    long long bb;
    aa = (long long)a;
    bb = (long long)b;
    return (amf_fx)((aa * bb) >> AMF_FX_SHIFT);
#else
    int neg;
    amf_u32 ua;
    amf_u32 ub;
    amf_u32 ai;
    amf_u32 af;
    amf_u32 bi;
    amf_u32 bf;
    amf_u32 mask;
    amf_u32 r;
    neg = 0;
    if (a < 0) { neg = !neg; ua = (amf_u32)(-a); } else { ua = (amf_u32)a; }
    if (b < 0) { neg = !neg; ub = (amf_u32)(-b); } else { ub = (amf_u32)b; }
    mask = (amf_u32)((1UL << AMF_FX_SHIFT) - 1UL);
    ai = ua >> AMF_FX_SHIFT;
    af = ua & mask;
    bi = ub >> AMF_FX_SHIFT;
    bf = ub & mask;
    r = (amf_u32)((ai * bi) << AMF_FX_SHIFT);
    r += ai * bf;
    r += bi * af;
    r += (af * bf) >> AMF_FX_SHIFT;
    if (neg) { return (amf_fx)(-(amf_i32)r); }
    return (amf_fx)r;
#endif
}

amf_fx amf_fx_div(amf_fx a, amf_fx b)
{
#if defined(_MSC_VER)
    __int64 aa;
    if (b == 0) { return 0; }
    aa = (__int64)a;
    return (amf_fx)((aa << AMF_FX_SHIFT) / (__int64)b);
#elif !defined(AMF_STRICT_C89_NO_LONGLONG) && (defined(__GNUC__) || defined(__clang__))
    long long aa;
    if (b == 0) { return 0; }
    aa = (long long)a;
    return (amf_fx)((aa << AMF_FX_SHIFT) / (long long)b);
#else
    int neg;
    amf_u32 ua;
    amf_u32 ub;
    amf_u32 q;
    amf_u32 r;
    amf_u32 out;
    if (b == 0) { return 0; }
    neg = 0;
    if (a < 0) { neg = !neg; ua = (amf_u32)(-a); } else { ua = (amf_u32)a; }
    if (b < 0) { neg = !neg; ub = (amf_u32)(-b); } else { ub = (amf_u32)b; }
    q = ua / ub;
    r = ua % ub;
    out = (q << AMF_FX_SHIFT) + ((r << AMF_FX_SHIFT) / ub);
    if (neg) { return (amf_fx)(-(amf_i32)out); }
    return (amf_fx)out;
#endif
}

int amf_u32_parse(amf_slice s, amf_u32 *out)
{
    amf_u32 v;
    size_t i;
    int any;
    s = amf_trim_slice(s);
    if (s.len == 0u || out == 0) {
        return AMF_ERR_BAD_NUMBER;
    }
    v = 0u;
    any = 0;
    i = 0u;
    if (s.ptr[i] == '+') {
        i++;
    }
    while (i < s.len) {
        char c;
        c = s.ptr[i];
        if (c < '0' || c > '9') {
            return AMF_ERR_BAD_NUMBER;
        }
        if (v > 429496729u) {
            return AMF_ERR_RANGE;
        }
        v = (amf_u32)(v * 10u + (amf_u32)(c - '0'));
        any = 1;
        i++;
    }
    if (!any) {
        return AMF_ERR_BAD_NUMBER;
    }
    *out = v;
    return AMF_OK;
}

int amf_fx_parse(amf_slice s, amf_fx *out)
{
    size_t i;
    int neg;
    int any;
    amf_i32 int_part;
    amf_i32 frac_part;
    amf_i32 place;
    int exp_neg;
    int exp_val;
    amf_fx val;
    s = amf_trim_slice(s);
    if (s.len == 0u || out == 0) {
        return AMF_ERR_BAD_NUMBER;
    }
    i = 0u;
    neg = 0;
    if (s.ptr[i] == '-' || s.ptr[i] == '+') {
        neg = (s.ptr[i] == '-');
        i++;
    }
    int_part = 0;
    frac_part = 0;
    place = AMF_FX_ONE / 10;
    any = 0;
    while (i < s.len && s.ptr[i] >= '0' && s.ptr[i] <= '9') {
        if (int_part > ((amf_i32)0x7fffffff >> AMF_FX_SHIFT) / 10) {
            return AMF_ERR_RANGE;
        }
        int_part = (amf_i32)(int_part * 10 + (s.ptr[i] - '0'));
        any = 1;
        i++;
    }
    if (i < s.len && s.ptr[i] == '.') {
        i++;
        while (i < s.len && s.ptr[i] >= '0' && s.ptr[i] <= '9') {
            if (place > 0) {
                frac_part = (amf_i32)(frac_part + (s.ptr[i] - '0') * place);
                place = place / 10;
            }
            any = 1;
            i++;
        }
    }
    val = (amf_fx)((int_part << AMF_FX_SHIFT) + frac_part);
    if (i < s.len && (s.ptr[i] == 'e' || s.ptr[i] == 'E')) {
        i++;
        exp_neg = 0;
        exp_val = 0;
        if (i < s.len && (s.ptr[i] == '-' || s.ptr[i] == '+')) {
            exp_neg = (s.ptr[i] == '-');
            i++;
        }
        if (i >= s.len || s.ptr[i] < '0' || s.ptr[i] > '9') {
            return AMF_ERR_BAD_NUMBER;
        }
        while (i < s.len && s.ptr[i] >= '0' && s.ptr[i] <= '9') {
            if (exp_val < 64) {
                exp_val = exp_val * 10 + (s.ptr[i] - '0');
            }
            i++;
        }
        while (exp_val > 0) {
            if (exp_neg) {
                val = amf_fx_div(val, amf_fx_from_i32(10));
            } else {
                val = amf_fx_mul(val, amf_fx_from_i32(10));
            }
            exp_val--;
        }
    }
    if (!any || i != s.len) {
        return AMF_ERR_BAD_NUMBER;
    }
    if (neg) {
        val = (amf_fx)(-val);
    }
    *out = val;
    return AMF_OK;
}

static int amf_append_char(char *dst, size_t cap, size_t *pos, char c)
{
    if (*pos + 1u >= cap) {
        return AMF_ERR_WRITE_FULL;
    }
    dst[*pos] = c;
    *pos = *pos + 1u;
    dst[*pos] = '\0';
    return AMF_OK;
}

static int amf_append_u32(char *dst, size_t cap, size_t *pos, amf_u32 v)
{
    char tmp[16];
    size_t n;
    size_t i;
    if (v == 0u) {
        return amf_append_char(dst, cap, pos, '0');
    }
    n = 0u;
    while (v > 0u && n < sizeof(tmp)) {
        tmp[n] = (char)('0' + (v % 10u));
        v = v / 10u;
        n++;
    }
    i = 0u;
    while (i < n) {
        int r;
        r = amf_append_char(dst, cap, pos, tmp[n - 1u - i]);
        if (r != AMF_OK) { return r; }
        i++;
    }
    return AMF_OK;
}

int amf_fx_to_text(amf_fx v, char *dst, size_t cap, size_t *out_len)
{
    size_t pos;
    amf_u32 whole;
    amf_u32 frac;
    amf_fx av;
    int r;
    int digits;
    if (dst == 0 || cap == 0u) {
        return AMF_ERR_BAD_ARG;
    }
    pos = 0u;
    dst[0] = '\0';
    if (v < 0) {
        r = amf_append_char(dst, cap, &pos, '-');
        if (r != AMF_OK) { return r; }
    }
    av = amf_fx_abs_i(v);
    whole = (amf_u32)(av >> AMF_FX_SHIFT);
    frac = (amf_u32)(av & ((1UL << AMF_FX_SHIFT) - 1UL));
    r = amf_append_u32(dst, cap, &pos, whole);
    if (r != AMF_OK) { return r; }
    if (frac != 0u) {
        r = amf_append_char(dst, cap, &pos, '.');
        if (r != AMF_OK) { return r; }
        digits = 0;
        while (frac != 0u && digits < 5) {
            amf_u32 d;
            frac = frac * 10u;
            d = frac >> AMF_FX_SHIFT;
            frac = frac & ((1UL << AMF_FX_SHIFT) - 1UL);
            r = amf_append_char(dst, cap, &pos, (char)('0' + d));
            if (r != AMF_OK) { return r; }
            digits++;
        }
    }
    if (out_len != 0) {
        *out_len = pos;
    }
    return AMF_OK;
}

amf_unit amf_unit_from_slice(amf_slice s)
{
    s = amf_trim_slice(s);
    if (amf_slice_eq_lit(s, "millimeter") || amf_slice_eq_lit(s, "millimeters") || amf_slice_eq_lit(s, "mm")) {
        return AMF_UNIT_MM;
    }
    if (amf_slice_eq_lit(s, "inch") || amf_slice_eq_lit(s, "inches") || amf_slice_eq_lit(s, "in")) {
        return AMF_UNIT_INCH;
    }
    if (amf_slice_eq_lit(s, "feet") || amf_slice_eq_lit(s, "foot") || amf_slice_eq_lit(s, "ft")) {
        return AMF_UNIT_FEET;
    }
    if (amf_slice_eq_lit(s, "meter") || amf_slice_eq_lit(s, "meters") || amf_slice_eq_lit(s, "m")) {
        return AMF_UNIT_METER;
    }
    if (amf_slice_eq_lit(s, "micron") || amf_slice_eq_lit(s, "microns") || amf_slice_eq_lit(s, "um") || amf_slice_eq_lit(s, "micrometer") || amf_slice_eq_lit(s, "micrometers")) {
        return AMF_UNIT_MICRON;
    }
    return AMF_UNIT_UNKNOWN;
}

const char *amf_unit_name(amf_unit unit)
{
    switch (unit) {
    case AMF_UNIT_MM: return "millimeter";
    case AMF_UNIT_INCH: return "inch";
    case AMF_UNIT_FEET: return "feet";
    case AMF_UNIT_METER: return "meter";
    case AMF_UNIT_MICRON: return "micron";
    default: return "unknown";
    }
}

amf_fx amf_unit_to_mm_factor(amf_unit unit)
{
    switch (unit) {
    case AMF_UNIT_MM: return amf_fx_from_i32(1);
    case AMF_UNIT_INCH: return (amf_fx)(25 * AMF_FX_ONE + (AMF_FX_ONE * 4) / 10);
    case AMF_UNIT_FEET: return (amf_fx)(304 * AMF_FX_ONE + (AMF_FX_ONE * 8) / 10);
    case AMF_UNIT_METER: return amf_fx_from_i32(1000);
    case AMF_UNIT_MICRON: return amf_fx_div(amf_fx_from_i32(1), amf_fx_from_i32(1000));
    default: return amf_fx_from_i32(1);
    }
}

void amf_document_zero(amf_document *doc)
{
    if (doc != 0) {
        memset(doc, 0, sizeof(*doc));
        doc->unit = AMF_UNIT_MM;
    }
}

static int amf_b64_val(char c)
{
    if (c >= 'A' && c <= 'Z') { return c - 'A'; }
    if (c >= 'a' && c <= 'z') { return c - 'a' + 26; }
    if (c >= '0' && c <= '9') { return c - '0' + 52; }
    if (c == '+') { return 62; }
    if (c == '/') { return 63; }
    if (c == '=') { return -2; }
    if (amf_is_space_c(c)) { return -3; }
    return -1;
}

int amf_base64_decode(const char *src, size_t src_len, amf_u8 *dst, size_t dst_cap, size_t *out_len)
{
    int q[4];
    int qn;
    size_t i;
    size_t op;
    qn = 0;
    op = 0u;
    i = 0u;
    while (i < src_len) {
        int v;
        v = amf_b64_val(src[i]);
        if (v == -3) {
            i++;
            continue;
        }
        if (v == -1) {
            return AMF_ERR_BAD_NUMBER;
        }
        q[qn] = v;
        qn++;
        if (qn == 4) {
            amf_u32 triple;
            if (q[0] < 0 || q[1] < 0) {
                return AMF_ERR_BAD_NUMBER;
            }
            triple = ((amf_u32)q[0] << 18) | ((amf_u32)q[1] << 12);
            if (q[2] >= 0) { triple |= ((amf_u32)q[2] << 6); }
            if (q[3] >= 0) { triple |= (amf_u32)q[3]; }
            if (op >= dst_cap) { return AMF_ERR_WRITE_FULL; }
            dst[op++] = (amf_u8)((triple >> 16) & 255u);
            if (q[2] >= 0) {
                if (op >= dst_cap) { return AMF_ERR_WRITE_FULL; }
                dst[op++] = (amf_u8)((triple >> 8) & 255u);
            }
            if (q[3] >= 0) {
                if (op >= dst_cap) { return AMF_ERR_WRITE_FULL; }
                dst[op++] = (amf_u8)(triple & 255u);
            }
            qn = 0;
        }
        i++;
    }
    if (qn != 0) {
        return AMF_ERR_BAD_NUMBER;
    }
    if (out_len != 0) {
        *out_len = op;
    }
    return AMF_OK;
}
