#include "mot89.h"

#include <stddef.h>

#define MOT89_I32_MAX ((long)2147483647L)
#define MOT89_I32_MIN ((long)(-2147483647L - 1L))

static void mot89_zero_bytes(void *ptr, size_t n)
{
    unsigned char *p;
    p = (unsigned char *)ptr;
    while (n > 0U) {
        *p = (unsigned char)0;
        p++;
        n--;
    }
}

static int mot89_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static int mot89_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int mot89_streq4(const char *a, const char *b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3] && a[4] == (char)0 && b[4] == (char)0;
}

static unsigned long mot89_abs_long_to_ulong(long v)
{
    if (v == MOT89_I32_MIN) {
        return (unsigned long)2147483648UL;
    }
    if (v < 0) {
        return (unsigned long)(-v);
    }
    return (unsigned long)v;
}

static long mot89_sat_add(long a, long b)
{
    if (b > 0 && a > MOT89_I32_MAX - b) {
        return MOT89_I32_MAX;
    }
    if (b < 0 && a < MOT89_I32_MIN - b) {
        return MOT89_I32_MIN;
    }
    return a + b;
}

static long mot89_sat_sub(long a, long b)
{
    if (b == MOT89_I32_MIN) {
        return MOT89_I32_MAX;
    }
    return mot89_sat_add(a, -b);
}

static long mot89_sat_mul_int(long a, long b)
{
    int neg;
    unsigned long ua;
    unsigned long ub;
    unsigned long cap;
    unsigned long r;
    unsigned long q;
    unsigned long d;

    if (a == 0 || b == 0) {
        return 0;
    }
    neg = ((a < 0) ? 1 : 0) ^ ((b < 0) ? 1 : 0);
    ua = mot89_abs_long_to_ulong(a);
    ub = mot89_abs_long_to_ulong(b);
    cap = neg ? 2147483648UL : 2147483647UL;
    d = 1UL;

    q = 0UL;
    r = 0UL;
    {
        int bit;
        for (bit = 31; bit >= 0; bit--) {
            unsigned long qextra;
            qextra = 0UL;
            if (q > cap / 2UL) {
                q = cap;
            } else {
                q = q * 2UL;
            }
            if (r >= d - r) {
                r = r - (d - r);
                qextra++;
            } else {
                r = r + r;
            }
            if (((ub >> bit) & 1UL) != 0UL) {
                unsigned long addq;
                unsigned long addr;
                addq = ua / d;
                addr = ua % d;
                if (qextra > cap - addq) {
                    qextra = cap;
                } else {
                    qextra += addq;
                }
                if (addr != 0UL) {
                    if (r >= d - addr) {
                        r = r - (d - addr);
                        if (qextra < cap) {
                            qextra++;
                        }
                    } else {
                        r += addr;
                    }
                }
            }
            if (q > cap - qextra) {
                q = cap;
            } else {
                q += qextra;
            }
        }
    }

    if (neg) {
        if (q >= 2147483648UL) {
            return MOT89_I32_MIN;
        }
        return -(long)q;
    }
    if (q > 2147483647UL) {
        return MOT89_I32_MAX;
    }
    return (long)q;
}

static unsigned long mot89_umuldiv_sat(unsigned long a, unsigned long b, unsigned long d, unsigned long cap)
{
    unsigned long q;
    unsigned long r;
    int bit;

    if (d == 0UL) {
        return cap;
    }
    q = 0UL;
    r = 0UL;
    for (bit = 31; bit >= 0; bit--) {
        unsigned long qextra;
        qextra = 0UL;

        if (q > cap / 2UL) {
            q = cap;
        } else {
            q = q * 2UL;
        }

        if (r >= d - r) {
            r = r - (d - r);
            qextra++;
        } else {
            r = r + r;
        }

        if (((b >> bit) & 1UL) != 0UL) {
            unsigned long addq;
            unsigned long addr;
            addq = a / d;
            addr = a % d;
            if (qextra > cap - addq) {
                qextra = cap;
            } else {
                qextra += addq;
            }
            if (addr != 0UL) {
                if (r >= d - addr) {
                    r = r - (d - addr);
                    if (qextra < cap) {
                        qextra++;
                    }
                } else {
                    r += addr;
                }
            }
        }

        if (q > cap - qextra) {
            q = cap;
        } else {
            q += qextra;
        }
    }
    return q;
}

static long mot89_mul_div_s32(long a, long b, long d)
{
    int neg;
    unsigned long ua;
    unsigned long ub;
    unsigned long ud;
    unsigned long cap;
    unsigned long q;

    if (d == 0) {
        if (((a < 0) ? 1 : 0) ^ ((b < 0) ? 1 : 0)) {
            return MOT89_I32_MIN;
        }
        return MOT89_I32_MAX;
    }
    neg = 0;
    if (a < 0) {
        neg = !neg;
    }
    if (b < 0) {
        neg = !neg;
    }
    if (d < 0) {
        neg = !neg;
    }

    ua = mot89_abs_long_to_ulong(a);
    ub = mot89_abs_long_to_ulong(b);
    ud = mot89_abs_long_to_ulong(d);
    cap = neg ? 2147483648UL : 2147483647UL;
    q = mot89_umuldiv_sat(ua, ub, ud, cap);

    if (neg) {
        if (q >= 2147483648UL) {
            return MOT89_I32_MIN;
        }
        return -(long)q;
    }
    if (q > 2147483647UL) {
        return MOT89_I32_MAX;
    }
    return (long)q;
}

static long mot89_fmul(long a, long b)
{
    return mot89_mul_div_s32(a, b, MOT89_ONE);
}

static long mot89_div2(long a)
{
    return a / 2L;
}

void mot89_arena_init(mot89_arena *arena, void *buffer, size_t buffer_size)
{
    if (arena == (mot89_arena *)0) {
        return;
    }
    arena->base = (unsigned char *)buffer;
    arena->size = buffer_size;
    arena->used = 0U;
    arena->overflowed = 0;
}

void mot89_arena_reset(mot89_arena *arena)
{
    if (arena == (mot89_arena *)0) {
        return;
    }
    arena->used = 0U;
    arena->overflowed = 0;
}

void *mot89_arena_push(mot89_arena *arena, size_t size, size_t align_bytes)
{
    size_t align_mask;
    size_t off;
    void *result;

    if (arena == (mot89_arena *)0 || arena->base == (unsigned char *)0) {
        return (void *)0;
    }
    if (align_bytes == 0U) {
        align_bytes = 1U;
    }
    align_mask = align_bytes - 1U;
    off = (arena->used + align_mask) & ~align_mask;
    if (off > arena->size || size > arena->size - off) {
        arena->overflowed = 1;
        return (void *)0;
    }
    result = (void *)(arena->base + off);
    arena->used = off + size;
    mot89_zero_bytes(result, size);
    return result;
}

const char *mot89_error_string(int code)
{
    switch (code) {
    case MOT89_OK: return "ok";
    case MOT89_E_NULL: return "null argument";
    case MOT89_E_MAGIC: return "missing LWMO/LWEN marker";
    case MOT89_E_VERSION: return "unsupported version";
    case MOT89_E_CHANNELS: return "unsupported channel count";
    case MOT89_E_KEYS: return "bad key count";
    case MOT89_E_NUMBER: return "bad number";
    case MOT89_E_ARENA: return "arena too small";
    case MOT89_E_RANGE: return "value out of supported range";
    case MOT89_E_OVERFLOW: return "output buffer too small";
    case MOT89_E_INTERNAL: return "internal error";
    default: return "unknown error";
    }
}

const char *mot89_channel_name(int channel_index)
{
    switch (channel_index) {
    case MOT89_CHAN_X: return "x";
    case MOT89_CHAN_Y: return "y";
    case MOT89_CHAN_Z: return "z";
    case MOT89_CHAN_HEADING: return "heading";
    case MOT89_CHAN_PITCH: return "pitch";
    case MOT89_CHAN_BANK: return "bank";
    case MOT89_CHAN_SCALE_X: return "scale_x";
    case MOT89_CHAN_SCALE_Y: return "scale_y";
    case MOT89_CHAN_SCALE_Z: return "scale_z";
    default: return "unknown";
    }
}

long mot89_fixed_from_int(long value)
{
    if (value > MOT89_I32_MAX / MOT89_ONE) {
        return MOT89_I32_MAX;
    }
    if (value < MOT89_I32_MIN / MOT89_ONE) {
        return MOT89_I32_MIN;
    }
    return value * MOT89_ONE;
}

long mot89_fixed_from_ratio(long numerator, long denominator)
{
    if (denominator == 0) {
        if (numerator < 0) {
            return MOT89_I32_MIN;
        }
        return MOT89_I32_MAX;
    }
    return mot89_mul_div_s32(numerator, MOT89_ONE, denominator);
}

typedef struct mot89_reader {
    const char *cur;
    const char *end;
} mot89_reader;

static void mot89_reader_skip_ws(mot89_reader *r)
{
    int comment;
    for (;;) {
        comment = 0;
        while (r->cur < r->end && mot89_is_space(*r->cur)) {
            r->cur++;
        }
        if (r->cur < r->end && (*r->cur == '#' || *r->cur == ';')) {
            comment = 1;
            while (r->cur < r->end && *r->cur != '\n') {
                r->cur++;
            }
        }
        if (!comment) {
            break;
        }
    }
}

static int mot89_reader_word(mot89_reader *r, char *out, size_t out_cap)
{
    size_t n;
    mot89_reader_skip_ws(r);
    if (r->cur >= r->end) {
        return 0;
    }
    n = 0U;
    while (r->cur < r->end && !mot89_is_space(*r->cur)) {
        if (n + 1U < out_cap) {
            out[n] = *r->cur;
            n++;
        }
        r->cur++;
    }
    if (out_cap > 0U) {
        out[n] = (char)0;
    }
    return 1;
}

static int mot89_reader_fixed(mot89_reader *r, long *out)
{
    int neg;
    int got;
    long ip;
    long fp;
    long factor;
    long v;

    mot89_reader_skip_ws(r);
    if (r->cur >= r->end) {
        return 0;
    }

    neg = 0;
    if (*r->cur == '-' || *r->cur == '+') {
        if (*r->cur == '-') {
            neg = 1;
        }
        r->cur++;
    }

    got = 0;
    ip = 0;
    while (r->cur < r->end && mot89_is_digit(*r->cur)) {
        int d;
        d = (int)(*r->cur - '0');
        got = 1;
        if (ip <= (MOT89_I32_MAX / MOT89_ONE - (long)d) / 10L) {
            ip = ip * 10L + (long)d;
        } else {
            ip = MOT89_I32_MAX / MOT89_ONE;
        }
        r->cur++;
    }

    fp = 0;
    factor = MOT89_ONE / 10L;
    if (r->cur < r->end && *r->cur == '.') {
        r->cur++;
        while (r->cur < r->end && mot89_is_digit(*r->cur)) {
            int d2;
            d2 = (int)(*r->cur - '0');
            got = 1;
            if (factor > 0L) {
                fp += (long)d2 * factor;
                factor /= 10L;
            }
            r->cur++;
        }
    }

    if (!got) {
        return 0;
    }

    v = mot89_sat_add(mot89_fixed_from_int(ip), fp);
    if (neg) {
        if (v == MOT89_I32_MIN) {
            *out = MOT89_I32_MIN;
        } else {
            *out = -v;
        }
    } else {
        *out = v;
    }
    return 1;
}

static int mot89_reader_int(mot89_reader *r, int *out)
{
    long v;
    if (!mot89_reader_fixed(r, &v)) {
        return 0;
    }
    if (v >= 0) {
        *out = (int)((v + MOT89_HALF) / MOT89_ONE);
    } else {
        *out = (int)((v - MOT89_HALF) / MOT89_ONE);
    }
    return 1;
}

static int mot89_find_magic(mot89_reader *r, char magic[5])
{
    char word[32];
    while (mot89_reader_word(r, word, sizeof(word))) {
        if (mot89_streq4(word, "LWMO") || mot89_streq4(word, "LWEN")) {
            magic[0] = word[0];
            magic[1] = word[1];
            magic[2] = word[2];
            magic[3] = word[3];
            magic[4] = (char)0;
            return 1;
        }
    }
    return 0;
}

static void mot89_sort_keys(mot89_motion *m)
{
    int i;
    int sorted_input;
    sorted_input = 1;
    for (i = 1; i < m->key_count; i++) {
        if (m->keys[i - 1].time > m->keys[i].time) {
            sorted_input = 0;
            break;
        }
    }
    if (sorted_input) {
        m->flags |= MOT89_FLAG_SORTED_INPUT;
        return;
    }
    for (i = 1; i < m->key_count; i++) {
        mot89_key k;
        int j;
        k = m->keys[i];
        j = i - 1;
        while (j >= 0 && m->keys[j].time > k.time) {
            m->keys[j + 1] = m->keys[j];
            j--;
        }
        m->keys[j + 1] = k;
    }
    m->flags |= MOT89_FLAG_WAS_SORTED;
}

int mot89_parse(const char *text, size_t text_size, mot89_arena *arena, mot89_motion **out_motion)
{
    mot89_reader r;
    mot89_motion *m;
    char magic[5];
    int i;
    int ch;

    if (text == (const char *)0 || arena == (mot89_arena *)0 || out_motion == (mot89_motion **)0) {
        return MOT89_E_NULL;
    }
    *out_motion = (mot89_motion *)0;
    r.cur = text;
    r.end = text + text_size;

    if (!mot89_find_magic(&r, magic)) {
        return MOT89_E_MAGIC;
    }

    m = (mot89_motion *)mot89_arena_push(arena, sizeof(mot89_motion), sizeof(long));
    if (m == (mot89_motion *)0) {
        return MOT89_E_ARENA;
    }

    if (mot89_streq4(magic, "LWMO")) {
        m->type = MOT89_TYPE_MOT;
        m->flags |= MOT89_FLAG_MOT;
    } else if (mot89_streq4(magic, "LWEN")) {
        m->type = MOT89_TYPE_ENV;
        m->flags |= MOT89_FLAG_ENV;
    } else {
        return MOT89_E_MAGIC;
    }

    if (!mot89_reader_int(&r, &m->version)) {
        return MOT89_E_NUMBER;
    }
    if (m->version != 1) {
        return MOT89_E_VERSION;
    }
    if (!mot89_reader_int(&r, &m->channels)) {
        return MOT89_E_NUMBER;
    }
    if (m->channels < 1 || m->channels > MOT89_MAX_CHANNELS) {
        return MOT89_E_CHANNELS;
    }
    if (m->type == MOT89_TYPE_MOT && m->channels != MOT89_MAX_CHANNELS) {
        return MOT89_E_CHANNELS;
    }
    if (!mot89_reader_int(&r, &m->key_count)) {
        return MOT89_E_NUMBER;
    }
    if (m->key_count < 1) {
        return MOT89_E_KEYS;
    }
    if ((size_t)m->key_count > ((size_t)-1) / sizeof(mot89_key)) {
        return MOT89_E_RANGE;
    }
    m->keys = (mot89_key *)mot89_arena_push(arena, sizeof(mot89_key) * (size_t)m->key_count, sizeof(long));
    if (m->keys == (mot89_key *)0) {
        return MOT89_E_ARENA;
    }

    for (i = 0; i < m->key_count; i++) {
        for (ch = 0; ch < MOT89_MAX_CHANNELS; ch++) {
            m->keys[i].values[ch] = (ch >= MOT89_CHAN_SCALE_X) ? MOT89_ONE : 0L;
        }
        for (ch = 0; ch < m->channels; ch++) {
            if (!mot89_reader_fixed(&r, &m->keys[i].values[ch])) {
                return MOT89_E_NUMBER;
            }
        }
        if (!mot89_reader_fixed(&r, &m->keys[i].time)) {
            return MOT89_E_NUMBER;
        }
        if (!mot89_reader_int(&r, &m->keys[i].linear_flag)) {
            return MOT89_E_NUMBER;
        }
        if (!mot89_reader_fixed(&r, &m->keys[i].tension)) {
            return MOT89_E_NUMBER;
        }
        if (!mot89_reader_fixed(&r, &m->keys[i].continuity)) {
            return MOT89_E_NUMBER;
        }
        if (!mot89_reader_fixed(&r, &m->keys[i].bias)) {
            return MOT89_E_NUMBER;
        }
        if (m->keys[i].linear_flag == 0) {
            m->flags |= MOT89_FLAG_NONLINEAR_KEYS;
        }
    }

    mot89_sort_keys(m);
    *out_motion = m;
    return MOT89_OK;
}

int mot89_validate(const mot89_motion *motion, mot89_report *out_report)
{
    int i;
    int nonlinear;
    int sorted;

    if (motion == (const mot89_motion *)0 || out_report == (mot89_report *)0) {
        return MOT89_E_NULL;
    }
    mot89_zero_bytes(out_report, sizeof(*out_report));
    out_report->type = motion->type;
    out_report->version = motion->version;
    out_report->channels = motion->channels;
    out_report->key_count = motion->key_count;

    if (motion->key_count < 1 || motion->keys == (mot89_key *)0) {
        return MOT89_E_KEYS;
    }

    sorted = 1;
    nonlinear = 0;
    for (i = 0; i < motion->key_count; i++) {
        if (i > 0 && motion->keys[i - 1].time > motion->keys[i].time) {
            sorted = 0;
        }
        if (motion->keys[i].linear_flag == 0) {
            nonlinear = 1;
        }
    }
    out_report->sorted = sorted;
    out_report->nonlinear_keys = nonlinear;
    out_report->first_time = motion->keys[0].time;
    out_report->last_time = motion->keys[motion->key_count - 1].time;
    return MOT89_OK;
}

static long mot89_key_value(const mot89_motion *m, int key_index, int ch)
{
    if (ch < 0 || ch >= MOT89_MAX_CHANNELS) {
        return 0L;
    }
    if (ch >= m->channels) {
        return (ch >= MOT89_CHAN_SCALE_X) ? MOT89_ONE : 0L;
    }
    return m->keys[key_index].values[ch];
}

static long mot89_tcb_outgoing(const mot89_motion *m, int i, int ch)
{
    long p0;
    long p1;
    long p2;
    long t;
    long c;
    long b;
    long one_t;
    long coeff_a;
    long coeff_b;
    long d0;
    long d1;
    long a;
    long r;

    p0 = mot89_key_value(m, (i > 0) ? i - 1 : i, ch);
    p1 = mot89_key_value(m, i, ch);
    p2 = mot89_key_value(m, (i + 1 < m->key_count) ? i + 1 : i, ch);
    t = m->keys[i].tension;
    c = m->keys[i].continuity;
    b = m->keys[i].bias;
    one_t = mot89_sat_sub(MOT89_ONE, t);

    coeff_a = mot89_fmul(mot89_fmul(one_t, mot89_sat_add(MOT89_ONE, c)), mot89_sat_add(MOT89_ONE, b));
    coeff_b = mot89_fmul(mot89_fmul(one_t, mot89_sat_sub(MOT89_ONE, c)), mot89_sat_sub(MOT89_ONE, b));
    coeff_a = mot89_div2(coeff_a);
    coeff_b = mot89_div2(coeff_b);

    d0 = mot89_sat_sub(p1, p0);
    d1 = mot89_sat_sub(p2, p1);
    a = mot89_fmul(d0, coeff_a);
    r = mot89_sat_add(a, mot89_fmul(d1, coeff_b));
    return r;
}

static long mot89_tcb_incoming(const mot89_motion *m, int i, int ch)
{
    long p0;
    long p1;
    long p2;
    long t;
    long c;
    long b;
    long one_t;
    long coeff_a;
    long coeff_b;
    long d0;
    long d1;
    long a;
    long r;

    p0 = mot89_key_value(m, (i > 0) ? i - 1 : i, ch);
    p1 = mot89_key_value(m, i, ch);
    p2 = mot89_key_value(m, (i + 1 < m->key_count) ? i + 1 : i, ch);
    t = m->keys[i].tension;
    c = m->keys[i].continuity;
    b = m->keys[i].bias;
    one_t = mot89_sat_sub(MOT89_ONE, t);

    coeff_a = mot89_fmul(mot89_fmul(one_t, mot89_sat_sub(MOT89_ONE, c)), mot89_sat_add(MOT89_ONE, b));
    coeff_b = mot89_fmul(mot89_fmul(one_t, mot89_sat_add(MOT89_ONE, c)), mot89_sat_sub(MOT89_ONE, b));
    coeff_a = mot89_div2(coeff_a);
    coeff_b = mot89_div2(coeff_b);

    d0 = mot89_sat_sub(p1, p0);
    d1 = mot89_sat_sub(p2, p1);
    a = mot89_fmul(d0, coeff_a);
    r = mot89_sat_add(a, mot89_fmul(d1, coeff_b));
    return r;
}

static long mot89_lerp(long a, long b, long t, long t0, long t1)
{
    long span;
    long dt;
    long delta;
    span = mot89_sat_sub(t1, t0);
    if (span == 0L) {
        return b;
    }
    dt = mot89_sat_sub(t, t0);
    delta = mot89_sat_sub(b, a);
    return mot89_sat_add(a, mot89_mul_div_s32(delta, dt, span));
}

static long mot89_hermite(long p0, long p1, long m0, long m1, long u)
{
    long u2;
    long u3;
    long h00;
    long h10;
    long h01;
    long h11;
    long r;

    u2 = mot89_fmul(u, u);
    u3 = mot89_fmul(u2, u);

    h00 = mot89_sat_add(mot89_sat_sub(mot89_sat_mul_int(u3, 2L), mot89_sat_mul_int(u2, 3L)), MOT89_ONE);
    h10 = mot89_sat_add(mot89_sat_sub(u3, mot89_sat_mul_int(u2, 2L)), u);
    h01 = mot89_sat_add(mot89_sat_mul_int(u3, -2L), mot89_sat_mul_int(u2, 3L));
    h11 = mot89_sat_sub(u3, u2);

    r = 0L;
    r = mot89_sat_add(r, mot89_fmul(p0, h00));
    r = mot89_sat_add(r, mot89_fmul(m0, h10));
    r = mot89_sat_add(r, mot89_fmul(p1, h01));
    r = mot89_sat_add(r, mot89_fmul(m1, h11));
    return r;
}

int mot89_sample_channel(const mot89_motion *motion, int channel_index, long time, long *out_value)
{
    int i;
    long p0;
    long p1;
    long u;

    if (motion == (const mot89_motion *)0 || out_value == (long *)0) {
        return MOT89_E_NULL;
    }
    if (motion->keys == (mot89_key *)0 || motion->key_count < 1) {
        return MOT89_E_KEYS;
    }
    if (channel_index < 0 || channel_index >= MOT89_MAX_CHANNELS) {
        return MOT89_E_CHANNELS;
    }
    if (time <= motion->keys[0].time) {
        *out_value = mot89_key_value(motion, 0, channel_index);
        return MOT89_OK;
    }
    if (time >= motion->keys[motion->key_count - 1].time) {
        *out_value = mot89_key_value(motion, motion->key_count - 1, channel_index);
        return MOT89_OK;
    }

    for (i = 0; i < motion->key_count - 1; i++) {
        if (time >= motion->keys[i].time && time <= motion->keys[i + 1].time) {
            p0 = mot89_key_value(motion, i, channel_index);
            p1 = mot89_key_value(motion, i + 1, channel_index);
            if (motion->keys[i + 1].linear_flag != 0) {
                *out_value = mot89_lerp(p0, p1, time, motion->keys[i].time, motion->keys[i + 1].time);
                return MOT89_OK;
            }
            u = mot89_fixed_from_ratio(mot89_sat_sub(time, motion->keys[i].time), mot89_sat_sub(motion->keys[i + 1].time, motion->keys[i].time));
            *out_value = mot89_hermite(p0, p1, mot89_tcb_outgoing(motion, i, channel_index), mot89_tcb_incoming(motion, i + 1, channel_index), u);
            return MOT89_OK;
        }
    }

    *out_value = mot89_key_value(motion, motion->key_count - 1, channel_index);
    return MOT89_OK;
}

int mot89_sample_all(const mot89_motion *motion, long time, long out_values[MOT89_MAX_CHANNELS])
{
    int ch;
    int rc;
    if (motion == (const mot89_motion *)0 || out_values == (long *)0) {
        return MOT89_E_NULL;
    }
    for (ch = 0; ch < MOT89_MAX_CHANNELS; ch++) {
        rc = mot89_sample_channel(motion, ch, time, &out_values[ch]);
        if (rc != MOT89_OK) {
            return rc;
        }
    }
    return MOT89_OK;
}

static void mot89_put_char(char *out, size_t cap, size_t *used, int *bad, char c)
{
    if (*used + 1U < cap) {
        out[*used] = c;
    } else {
        *bad = 1;
    }
    (*used)++;
}

static void mot89_put_str(char *out, size_t cap, size_t *used, int *bad, const char *s)
{
    while (*s != (char)0) {
        mot89_put_char(out, cap, used, bad, *s);
        s++;
    }
}

static void mot89_put_uint(char *out, size_t cap, size_t *used, int *bad, unsigned long v)
{
    char tmp[16];
    int n;
    n = 0;
    if (v == 0UL) {
        mot89_put_char(out, cap, used, bad, '0');
        return;
    }
    while (v > 0UL && n < (int)sizeof(tmp)) {
        tmp[n] = (char)('0' + (char)(v % 10UL));
        v /= 10UL;
        n++;
    }
    while (n > 0) {
        n--;
        mot89_put_char(out, cap, used, bad, tmp[n]);
    }
}

static void mot89_put_int(char *out, size_t cap, size_t *used, int *bad, long v)
{
    unsigned long uv;
    if (v < 0) {
        mot89_put_char(out, cap, used, bad, '-');
        uv = mot89_abs_long_to_ulong(v);
    } else {
        uv = (unsigned long)v;
    }
    mot89_put_uint(out, cap, used, bad, uv);
}

static void mot89_put_fixed(char *out, size_t cap, size_t *used, int *bad, long v)
{
    unsigned long uv;
    unsigned long ip;
    unsigned long frac;
    unsigned long frac4;
    if (v < 0) {
        mot89_put_char(out, cap, used, bad, '-');
        uv = mot89_abs_long_to_ulong(v);
    } else {
        uv = (unsigned long)v;
    }
    ip = uv / (unsigned long)MOT89_ONE;
    frac = uv % (unsigned long)MOT89_ONE;
    frac4 = (frac * 10000UL + 32768UL) / 65536UL;
    if (frac4 >= 10000UL) {
        ip++;
        frac4 -= 10000UL;
    }
    mot89_put_uint(out, cap, used, bad, ip);
    if (frac4 != 0UL) {
        unsigned long d;
        mot89_put_char(out, cap, used, bad, '.');
        d = 1000UL;
        while (d > 0UL) {
            mot89_put_char(out, cap, used, bad, (char)('0' + (char)(frac4 / d)));
            frac4 %= d;
            d /= 10UL;
        }
    }
}

int mot89_fixed_to_dec4(long value, char *out, size_t out_cap)
{
    size_t used;
    int bad;
    if (out == (char *)0 || out_cap == 0U) {
        return MOT89_E_NULL;
    }
    used = 0U;
    bad = 0;
    mot89_put_fixed(out, out_cap, &used, &bad, value);
    if (used < out_cap) {
        out[used] = (char)0;
    } else {
        out[out_cap - 1U] = (char)0;
        bad = 1;
    }
    return bad ? MOT89_E_OVERFLOW : MOT89_OK;
}

int mot89_emit(const mot89_motion *motion, char *out_text, size_t out_cap, size_t *out_written)
{
    size_t used;
    int bad;
    int i;
    int ch;

    if (motion == (const mot89_motion *)0 || out_text == (char *)0) {
        return MOT89_E_NULL;
    }
    if (out_cap == 0U) {
        return MOT89_E_OVERFLOW;
    }
    used = 0U;
    bad = 0;

    if (motion->type == MOT89_TYPE_ENV) {
        mot89_put_str(out_text, out_cap, &used, &bad, "LWEN\n");
    } else {
        mot89_put_str(out_text, out_cap, &used, &bad, "LWMO\n");
    }
    mot89_put_int(out_text, out_cap, &used, &bad, (long)motion->version);
    mot89_put_char(out_text, out_cap, &used, &bad, '\n');
    mot89_put_int(out_text, out_cap, &used, &bad, (long)motion->channels);
    mot89_put_char(out_text, out_cap, &used, &bad, '\n');
    mot89_put_int(out_text, out_cap, &used, &bad, (long)motion->key_count);
    mot89_put_char(out_text, out_cap, &used, &bad, '\n');

    for (i = 0; i < motion->key_count; i++) {
        for (ch = 0; ch < motion->channels; ch++) {
            if (ch > 0) {
                mot89_put_char(out_text, out_cap, &used, &bad, ' ');
            }
            mot89_put_fixed(out_text, out_cap, &used, &bad, motion->keys[i].values[ch]);
        }
        mot89_put_char(out_text, out_cap, &used, &bad, '\n');
        mot89_put_fixed(out_text, out_cap, &used, &bad, motion->keys[i].time);
        mot89_put_char(out_text, out_cap, &used, &bad, ' ');
        mot89_put_int(out_text, out_cap, &used, &bad, (long)motion->keys[i].linear_flag);
        mot89_put_char(out_text, out_cap, &used, &bad, ' ');
        mot89_put_fixed(out_text, out_cap, &used, &bad, motion->keys[i].tension);
        mot89_put_char(out_text, out_cap, &used, &bad, ' ');
        mot89_put_fixed(out_text, out_cap, &used, &bad, motion->keys[i].continuity);
        mot89_put_char(out_text, out_cap, &used, &bad, ' ');
        mot89_put_fixed(out_text, out_cap, &used, &bad, motion->keys[i].bias);
        mot89_put_char(out_text, out_cap, &used, &bad, '\n');
    }

    if (used < out_cap) {
        out_text[used] = (char)0;
    } else {
        out_text[out_cap - 1U] = (char)0;
        bad = 1;
    }
    if (out_written != (size_t *)0) {
        *out_written = used;
    }
    return bad ? MOT89_E_OVERFLOW : MOT89_OK;
}
