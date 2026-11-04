#include "pskpsa3d.h"

#include <string.h>
#include <limits.h>

#define PSK3D_HEADER_BYTES 32u
#define PSK3D_ACTORX_VERSION_LIMIT 1999801u
#define PSK3D_SIN_LUT_COUNT 129u

static void psk3d_zero(void *ptr, psk3d_u32 bytes)
{
    unsigned char *p;
    psk3d_u32 i;
    p = (unsigned char *)ptr;
    for (i = 0u; i < bytes; i++) {
        p[i] = 0u;
    }
}

static int psk3d_check_types(void)
{
    if (sizeof(psk3d_u8) != 1u) {
        return 0;
    }
    if (sizeof(psk3d_u16) != 2u || sizeof(psk3d_i16) != 2u) {
        return 0;
    }
    if (sizeof(psk3d_u32) != 4u || sizeof(psk3d_i32) != 4u) {
        return 0;
    }
    return 1;
}

static void psk3d_copy_fixed_name(char *dst, const psk3d_u8 *src, psk3d_u32 n)
{
    psk3d_u32 i;
    if (dst == (char *)0) {
        return;
    }
    for (i = 0u; i < n; i++) {
        dst[i] = (char)src[i];
        if (src[i] == 0u) {
            break;
        }
    }
    if (i == n) {
        dst[n] = '\0';
    } else {
        while (i < n) {
            dst[i] = '\0';
            i++;
        }
        dst[n] = '\0';
    }
}

static void psk3d_set_error(psk3d_report *report,
                            psk3d_result code,
                            const char *text,
                            const char *chunk_id,
                            psk3d_u32 chunk_index,
                            psk3d_u32 offset)
{
    psk3d_u32 i;
    if (report == (psk3d_report *)0) {
        return;
    }
    report->last_error = code;
    report->last_chunk_index = chunk_index;
    report->last_chunk_offset = offset;
    if (chunk_id != (const char *)0) {
        for (i = 0u; i < PSK3D_CHUNK_ID_BYTES; i++) {
            report->last_chunk_id[i] = chunk_id[i];
            if (chunk_id[i] == '\0') {
                break;
            }
        }
        if (i == PSK3D_CHUNK_ID_BYTES) {
            report->last_chunk_id[PSK3D_CHUNK_ID_BYTES] = '\0';
        } else {
            while (i < PSK3D_CHUNK_ID_BYTES) {
                report->last_chunk_id[i] = '\0';
                i++;
            }
            report->last_chunk_id[PSK3D_CHUNK_ID_BYTES] = '\0';
        }
    }
    if (text != (const char *)0) {
        i = 0u;
        while (i < 95u && text[i] != '\0') {
            report->error_text[i] = text[i];
            i++;
        }
        report->error_text[i] = '\0';
    }
}

static psk3d_u16 psk3d_rd_u16(const psk3d_u8 *p)
{
    psk3d_u16 v;
    v = (psk3d_u16)p[0];
    v = (psk3d_u16)(v | ((psk3d_u16)p[1] << 8));
    return v;
}

static psk3d_u32 psk3d_rd_u32(const psk3d_u8 *p)
{
    psk3d_u32 v;
    v = (psk3d_u32)p[0];
    v |= ((psk3d_u32)p[1]) << 8;
    v |= ((psk3d_u32)p[2]) << 16;
    v |= ((psk3d_u32)p[3]) << 24;
    return v;
}

static psk3d_i32 psk3d_rd_i32(const psk3d_u8 *p)
{
    psk3d_u32 u;
    u = psk3d_rd_u32(p);
    if (u >= 2147483648u) {
        return (psk3d_i32)(u - 2147483648u) - 2147483647 - 1;
    }
    return (psk3d_i32)u;
}

static psk3d_i32 psk3d_rd_f32q16(const psk3d_u8 *p)
{
    return psk3d_f32bits_to_q16(psk3d_rd_u32(p));
}

static int psk3d_id_eq(const char *id, const char *lit)
{
    psk3d_u32 i;
    for (i = 0u; i < PSK3D_CHUNK_ID_BYTES; i++) {
        if (lit[i] == '\0') {
            if (id[i] == '\0') {
                return 1;
            }
            return 0;
        }
        if (id[i] != lit[i]) {
            return 0;
        }
    }
    return 1;
}

static int psk3d_id_starts(const char *id, const char *prefix)
{
    psk3d_u32 i;
    for (i = 0u; i < PSK3D_CHUNK_ID_BYTES; i++) {
        if (prefix[i] == '\0') {
            return 1;
        }
        if (id[i] != prefix[i]) {
            return 0;
        }
    }
    return 1;
}

static char psk3d_ascii_lower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static int psk3d_name_eq(const char *a, const char *b, int insensitive)
{
    psk3d_u32 i;
    char ca;
    char cb;
    if (a == (const char *)0 || b == (const char *)0) {
        return 0;
    }
    for (i = 0u; i < PSK3D_NAME_BYTES + 1u; i++) {
        ca = a[i];
        cb = b[i];
        if (insensitive) {
            ca = psk3d_ascii_lower(ca);
            cb = psk3d_ascii_lower(cb);
        }
        if (ca != cb) {
            return 0;
        }
        if (ca == '\0') {
            return 1;
        }
    }
    return 1;
}

static psk3d_result psk3d_read_chunk(const psk3d_u8 *data,
                                     psk3d_u32 size,
                                     psk3d_u32 offset,
                                     psk3d_chunk *chunk)
{
    psk3d_u32 i;
    if (offset > size) {
        return PSK3D_ERR_EOF;
    }
    if (size - offset < PSK3D_HEADER_BYTES) {
        return PSK3D_ERR_EOF;
    }
    psk3d_zero(chunk, (psk3d_u32)sizeof(*chunk));
    for (i = 0u; i < PSK3D_CHUNK_ID_BYTES; i++) {
        chunk->id[i] = (char)data[offset + i];
        if (data[offset + i] == 0u) {
            break;
        }
    }
    chunk->id[PSK3D_CHUNK_ID_BYTES] = '\0';
    chunk->type_flags = psk3d_rd_u32(data + offset + 20u);
    chunk->data_size = psk3d_rd_u32(data + offset + 24u);
    chunk->data_count = psk3d_rd_u32(data + offset + 28u);
    chunk->payload_offset = offset + PSK3D_HEADER_BYTES;
    return PSK3D_OK;
}

static psk3d_result psk3d_check_payload(const psk3d_chunk *chunk,
                                        psk3d_u32 file_size)
{
    psk3d_u32 payload_bytes;
    if (chunk->data_count != 0u) {
        if (chunk->data_size > 0xffffffffu / chunk->data_count) {
            return PSK3D_ERR_OVERFLOW;
        }
    }
    payload_bytes = chunk->data_size * chunk->data_count;
    if (chunk->payload_offset > file_size) {
        return PSK3D_ERR_EOF;
    }
    if (payload_bytes > file_size - chunk->payload_offset) {
        return PSK3D_ERR_EOF;
    }
    return PSK3D_OK;
}

static psk3d_u32 psk3d_chunk_next(const psk3d_chunk *chunk)
{
    return chunk->payload_offset + chunk->data_size * chunk->data_count;
}

static void psk3d_remember_chunk(psk3d_report *report, const psk3d_chunk *chunk)
{
    psk3d_u32 index;
    if (report == (psk3d_report *)0) {
        return;
    }
    index = report->chunks_seen;
    if (index < PSK3D_MAX_CHUNKS) {
        report->chunks[index] = *chunk;
    }
    report->chunks_seen++;
}

void psk3d_clear_report(psk3d_report *report)
{
    if (report != (psk3d_report *)0) {
        psk3d_zero(report, (psk3d_u32)sizeof(*report));
        report->last_error = PSK3D_OK;
    }
}

void psk3d_clear_model(psk3d_model *model)
{
    if (model != (psk3d_model *)0) {
        psk3d_zero(model, (psk3d_u32)sizeof(*model));
    }
}

void psk3d_clear_animset(psk3d_animset *animset)
{
    if (animset != (psk3d_animset *)0) {
        psk3d_zero(animset, (psk3d_u32)sizeof(*animset));
    }
}

psk3d_i32 psk3d_f32bits_to_q16(psk3d_u32 bits)
{
    psk3d_u32 sign;
    psk3d_u32 exp;
    psk3d_u32 mant;
    psk3d_i32 e;
    psk3d_i32 shift;
    psk3d_u32 mag;
    psk3d_u32 rem;

    sign = (bits >> 31) & 1u;
    exp = (bits >> 23) & 255u;
    mant = bits & 0x7fffffu;

    if (exp == 255u) {
        return sign ? PSK3D_Q16_MIN : PSK3D_Q16_MAX;
    }
    if (exp == 0u) {
        return 0;
    }

    mant |= 0x800000u;
    e = (psk3d_i32)exp - 127;
    shift = e - 7;

    if (shift >= 7) {
        return sign ? PSK3D_Q16_MIN : PSK3D_Q16_MAX;
    }

    if (shift >= 0) {
        mag = mant << (unsigned)shift;
        if (mag > 2147483647u) {
            return sign ? PSK3D_Q16_MIN : PSK3D_Q16_MAX;
        }
    } else {
        psk3d_i32 rshift;
        rshift = -shift;
        if (rshift >= 32) {
            mag = 0u;
        } else {
            rem = 0u;
            if (rshift > 0) {
                rem = (psk3d_u32)1u << (unsigned)(rshift - 1);
            }
            mag = (mant + rem) >> (unsigned)rshift;
        }
    }

    if (sign) {
        if (mag > 2147483647u) {
            return PSK3D_Q16_MIN;
        }
        return -(psk3d_i32)mag;
    }
    return (psk3d_i32)mag;
}

psk3d_u32 psk3d_q16_to_i16_rounded(psk3d_i32 q)
{
    psk3d_i32 v;
    if (q >= 0) {
        v = (q + PSK3D_Q16_HALF) / PSK3D_Q16_ONE;
    } else {
        v = -((-q + PSK3D_Q16_HALF) / PSK3D_Q16_ONE);
    }
    return (psk3d_u32)v;
}


static psk3d_u32 psk3d_abs_i32_as_u32(psk3d_i32 v)
{
    if (v < 0) {
        if (v == PSK3D_Q16_MIN) {
            return 2147483647u;
        }
        return (psk3d_u32)(-v);
    }
    return (psk3d_u32)v;
}

static psk3d_i32 psk3d_apply_sign_sat(psk3d_u32 mag, int neg)
{
    if (mag > 2147483647u) {
        return neg ? PSK3D_Q16_MIN : PSK3D_Q16_MAX;
    }
    if (neg) {
        return -(psk3d_i32)mag;
    }
    return (psk3d_i32)mag;
}

psk3d_i32 psk3d_q16_add_sat(psk3d_i32 a, psk3d_i32 b)
{
    if (b > 0 && a > PSK3D_Q16_MAX - b) {
        return PSK3D_Q16_MAX;
    }
    if (b < 0 && a < PSK3D_Q16_MIN - b) {
        return PSK3D_Q16_MIN;
    }
    return a + b;
}

psk3d_i32 psk3d_q16_sub_sat(psk3d_i32 a, psk3d_i32 b)
{
    if (b == PSK3D_Q16_MIN) {
        return PSK3D_Q16_MAX;
    }
    return psk3d_q16_add_sat(a, -b);
}

static psk3d_u32 psk3d_add_u32_sat(psk3d_u32 a, psk3d_u32 b)
{
    if (a > 2147483647u - b) {
        return 2147483648u;
    }
    return a + b;
}

psk3d_i32 psk3d_q16_mul(psk3d_i32 a, psk3d_i32 b)
{
    psk3d_u32 aa;
    psk3d_u32 bb;
    psk3d_u32 aw;
    psk3d_u32 bw;
    psk3d_u32 af;
    psk3d_u32 bf;
    psk3d_u32 mag;
    psk3d_u32 term;
    int neg;

    if (a == 0 || b == 0) {
        return 0;
    }
    neg = ((a < 0) != (b < 0));
    aa = psk3d_abs_i32_as_u32(a);
    bb = psk3d_abs_i32_as_u32(b);
    aw = aa >> 16;
    bw = bb >> 16;
    af = aa & 65535u;
    bf = bb & 65535u;
    mag = 0u;

    if (aw != 0u && bw != 0u) {
        if (aw > 32767u / bw) {
            return neg ? PSK3D_Q16_MIN : PSK3D_Q16_MAX;
        }
        term = aw * bw;
        term = term << 16;
        mag = psk3d_add_u32_sat(mag, term);
        if (mag > 2147483647u) {
            return neg ? PSK3D_Q16_MIN : PSK3D_Q16_MAX;
        }
    }
    if (aw != 0u && bf != 0u) {
        if (aw > 2147483647u / bf) {
            return neg ? PSK3D_Q16_MIN : PSK3D_Q16_MAX;
        }
        term = aw * bf;
        mag = psk3d_add_u32_sat(mag, term);
        if (mag > 2147483647u) {
            return neg ? PSK3D_Q16_MIN : PSK3D_Q16_MAX;
        }
    }
    if (bw != 0u && af != 0u) {
        if (bw > 2147483647u / af) {
            return neg ? PSK3D_Q16_MIN : PSK3D_Q16_MAX;
        }
        term = bw * af;
        mag = psk3d_add_u32_sat(mag, term);
        if (mag > 2147483647u) {
            return neg ? PSK3D_Q16_MIN : PSK3D_Q16_MAX;
        }
    }
    if (af != 0u && bf != 0u) {
        term = (af * bf + 32768u) >> 16;
        mag = psk3d_add_u32_sat(mag, term);
        if (mag > 2147483647u) {
            return neg ? PSK3D_Q16_MIN : PSK3D_Q16_MAX;
        }
    }
    return psk3d_apply_sign_sat(mag, neg);
}

psk3d_i32 psk3d_q16_div(psk3d_i32 a, psk3d_i32 b)
{
    psk3d_u32 aa;
    psk3d_u32 bb;
    psk3d_u32 whole;
    psk3d_u32 rem;
    psk3d_u32 mag;
    int bit;
    int neg;

    if (b == 0) {
        return a < 0 ? PSK3D_Q16_MIN : PSK3D_Q16_MAX;
    }
    if (a == 0) {
        return 0;
    }
    neg = ((a < 0) != (b < 0));
    aa = psk3d_abs_i32_as_u32(a);
    bb = psk3d_abs_i32_as_u32(b);
    whole = aa / bb;
    if (whole > 32767u) {
        return neg ? PSK3D_Q16_MIN : PSK3D_Q16_MAX;
    }
    mag = whole << 16;
    rem = aa % bb;
    bit = 15;
    while (bit >= 0) {
        if (rem > 2147483647u / 2u) {
            return neg ? PSK3D_Q16_MIN : PSK3D_Q16_MAX;
        }
        rem = rem << 1;
        if (rem >= bb) {
            mag |= (psk3d_u32)1u << (unsigned)bit;
            rem -= bb;
        }
        bit--;
    }
    return psk3d_apply_sign_sat(mag, neg);
}

psk3d_i32 psk3d_q16_sqrt(psk3d_i32 x)
{
    psk3d_i32 lo;
    psk3d_i32 hi;
    psk3d_i32 mid;
    psk3d_i32 sq;
    int i;

    if (x <= 0) {
        return 0;
    }
    lo = 0;
    if (x < PSK3D_Q16_ONE) {
        hi = PSK3D_Q16_ONE;
    } else {
        hi = x;
    }
    if (hi < PSK3D_Q16_ONE) {
        hi = PSK3D_Q16_ONE;
    }
    i = 0;
    while (i < 32) {
        /* Upper midpoint without overflowing signed 32-bit on Q16_MAX. */
        mid = lo + (hi - lo) / 2 + ((hi - lo) & 1);
        sq = psk3d_q16_mul(mid, mid);
        if (sq <= x) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
        i++;
    }
    return lo;
}

psk3d_i32 psk3d_q16_lerp(psk3d_i32 a, psk3d_i32 b, psk3d_i32 t)
{
    psk3d_i32 d;
    d = psk3d_q16_sub_sat(b, a);
    return psk3d_q16_add_sat(a, psk3d_q16_mul(d, t));
}

static const psk3d_i32 psk3d_sin_lut_q16[PSK3D_SIN_LUT_COUNT] = {
    0, 804, 1608, 2412, 3216, 4019, 4821, 5623,
    6424, 7224, 8022, 8820, 9616, 10411, 11204, 11996,
    12785, 13573, 14359, 15143, 15924, 16703, 17479, 18253,
    19024, 19792, 20557, 21320, 22078, 22834, 23586, 24335,
    25080, 25821, 26558, 27291, 28020, 28745, 29466, 30182,
    30893, 31600, 32303, 33000, 33692, 34380, 35062, 35738,
    36410, 37076, 37736, 38391, 39040, 39683, 40320, 40951,
    41576, 42194, 42806, 43412, 44011, 44604, 45190, 45769,
    46341, 46906, 47464, 48015, 48559, 49095, 49624, 50146,
    50660, 51166, 51665, 52156, 52639, 53114, 53581, 54040,
    54491, 54934, 55368, 55794, 56212, 56621, 57022, 57414,
    57798, 58172, 58538, 58896, 59244, 59583, 59914, 60235,
    60547, 60851, 61145, 61429, 61705, 61971, 62228, 62476,
    62714, 62943, 63162, 63372, 63572, 63763, 63944, 64115,
    64277, 64429, 64571, 64704, 64827, 64940, 65043, 65137,
    65220, 65294, 65358, 65413, 65457, 65492, 65516, 65531,
    65536
};


static psk3d_i32 psk3d_q16_clamp(psk3d_i32 v, psk3d_i32 lo, psk3d_i32 hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static psk3d_i32 psk3d_q16_sin_half_pi(psk3d_i32 angle_q16)
{
    psk3d_i32 u;
    psk3d_i32 pos;
    psk3d_u32 idx;
    psk3d_i32 frac;

    angle_q16 = psk3d_q16_clamp(angle_q16, 0, PSK3D_Q16_HALF_PI);
    if (angle_q16 <= 0) {
        return 0;
    }
    if (angle_q16 >= PSK3D_Q16_HALF_PI) {
        return PSK3D_Q16_ONE;
    }

    u = psk3d_q16_div(angle_q16, PSK3D_Q16_HALF_PI);
    pos = psk3d_q16_mul(u, (psk3d_i32)((PSK3D_SIN_LUT_COUNT - 1u) * PSK3D_Q16_ONE));
    idx = (psk3d_u32)(pos >> 16);
    frac = pos & 65535;

    if (idx >= PSK3D_SIN_LUT_COUNT - 1u) {
        return PSK3D_Q16_ONE;
    }
    return psk3d_q16_lerp(psk3d_sin_lut_q16[idx], psk3d_sin_lut_q16[idx + 1u], frac);
}

static psk3d_i32 psk3d_q16_sin_0_pi(psk3d_i32 angle_q16)
{
    angle_q16 = psk3d_q16_clamp(angle_q16, 0, PSK3D_Q16_PI);
    if (angle_q16 <= PSK3D_Q16_HALF_PI) {
        return psk3d_q16_sin_half_pi(angle_q16);
    }
    return psk3d_q16_sin_half_pi(PSK3D_Q16_PI - angle_q16);
}

static psk3d_i32 psk3d_q16_cos_0_pi(psk3d_i32 angle_q16)
{
    angle_q16 = psk3d_q16_clamp(angle_q16, 0, PSK3D_Q16_PI);
    if (angle_q16 <= PSK3D_Q16_HALF_PI) {
        return psk3d_q16_sin_half_pi(PSK3D_Q16_HALF_PI - angle_q16);
    }
    return -psk3d_q16_sin_half_pi(angle_q16 - PSK3D_Q16_HALF_PI);
}

static psk3d_i32 psk3d_q16_acos_unit(psk3d_i32 x)
{
    psk3d_i32 lo;
    psk3d_i32 hi;
    psk3d_i32 mid;
    psk3d_i32 c;
    int i;

    x = psk3d_q16_clamp(x, -PSK3D_Q16_ONE, PSK3D_Q16_ONE);
    if (x >= PSK3D_Q16_ONE) {
        return 0;
    }
    if (x <= -PSK3D_Q16_ONE) {
        return PSK3D_Q16_PI;
    }

    lo = 0;
    hi = PSK3D_Q16_PI;
    for (i = 0; i < 18; i++) {
        mid = lo + (hi - lo) / 2;
        c = psk3d_q16_cos_0_pi(mid);
        if (c > x) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return hi;
}

void psk3d_quat_identity(psk3d_quat *q)
{
    if (q != (psk3d_quat *)0) {
        q->x = 0;
        q->y = 0;
        q->z = 0;
        q->w = PSK3D_Q16_ONE;
    }
}

psk3d_i32 psk3d_quat_dot(const psk3d_quat *a, const psk3d_quat *b)
{
    psk3d_i32 v;
    if (a == (const psk3d_quat *)0 || b == (const psk3d_quat *)0) {
        return 0;
    }
    v = psk3d_q16_mul(a->x, b->x);
    v = psk3d_q16_add_sat(v, psk3d_q16_mul(a->y, b->y));
    v = psk3d_q16_add_sat(v, psk3d_q16_mul(a->z, b->z));
    v = psk3d_q16_add_sat(v, psk3d_q16_mul(a->w, b->w));
    return v;
}

void psk3d_quat_normalize(psk3d_quat *q)
{
    psk3d_i32 n2;
    psk3d_i32 n;
    if (q == (psk3d_quat *)0) {
        return;
    }
    n2 = psk3d_quat_dot(q, q);
    if (n2 <= 16) {
        psk3d_quat_identity(q);
        return;
    }
    n = psk3d_q16_sqrt(n2);
    if (n <= 16) {
        psk3d_quat_identity(q);
        return;
    }
    q->x = psk3d_q16_div(q->x, n);
    q->y = psk3d_q16_div(q->y, n);
    q->z = psk3d_q16_div(q->z, n);
    q->w = psk3d_q16_div(q->w, n);
}

void psk3d_quat_mul(const psk3d_quat *a, const psk3d_quat *b, psk3d_quat *out)
{
    psk3d_quat r;
    if (a == (const psk3d_quat *)0 || b == (const psk3d_quat *)0 || out == (psk3d_quat *)0) {
        return;
    }
    r.x = psk3d_q16_add_sat(psk3d_q16_add_sat(psk3d_q16_mul(a->w, b->x), psk3d_q16_mul(a->x, b->w)),
                            psk3d_q16_sub_sat(psk3d_q16_mul(a->y, b->z), psk3d_q16_mul(a->z, b->y)));
    r.y = psk3d_q16_add_sat(psk3d_q16_sub_sat(psk3d_q16_mul(a->w, b->y), psk3d_q16_mul(a->x, b->z)),
                            psk3d_q16_add_sat(psk3d_q16_mul(a->y, b->w), psk3d_q16_mul(a->z, b->x)));
    r.z = psk3d_q16_add_sat(psk3d_q16_add_sat(psk3d_q16_mul(a->w, b->z), psk3d_q16_mul(a->x, b->y)),
                            psk3d_q16_sub_sat(psk3d_q16_mul(a->z, b->w), psk3d_q16_mul(a->y, b->x)));
    r.w = psk3d_q16_sub_sat(psk3d_q16_sub_sat(psk3d_q16_sub_sat(psk3d_q16_mul(a->w, b->w), psk3d_q16_mul(a->x, b->x)),
                                                psk3d_q16_mul(a->y, b->y)),
                            psk3d_q16_mul(a->z, b->z));
    *out = r;
}

void psk3d_quat_conjugate(const psk3d_quat *q, psk3d_quat *out)
{
    if (q == (const psk3d_quat *)0 || out == (psk3d_quat *)0) {
        return;
    }
    out->x = -q->x;
    out->y = -q->y;
    out->z = -q->z;
    out->w = q->w;
}

void psk3d_quat_nlerp(const psk3d_quat *a, const psk3d_quat *b, psk3d_i32 t, psk3d_quat *out)
{
    psk3d_quat bb;
    psk3d_quat r;
    psk3d_i32 dot;
    if (a == (const psk3d_quat *)0 || b == (const psk3d_quat *)0 || out == (psk3d_quat *)0) {
        return;
    }
    if (t < 0) {
        t = 0;
    }
    if (t > PSK3D_Q16_ONE) {
        t = PSK3D_Q16_ONE;
    }
    bb = *b;
    dot = psk3d_quat_dot(a, b);
    if (dot < 0) {
        bb.x = -bb.x;
        bb.y = -bb.y;
        bb.z = -bb.z;
        bb.w = -bb.w;
    }
    r.x = psk3d_q16_lerp(a->x, bb.x, t);
    r.y = psk3d_q16_lerp(a->y, bb.y, t);
    r.z = psk3d_q16_lerp(a->z, bb.z, t);
    r.w = psk3d_q16_lerp(a->w, bb.w, t);
    psk3d_quat_normalize(&r);
    *out = r;
}


void psk3d_quat_slerp(const psk3d_quat *a, const psk3d_quat *b, psk3d_i32 t, psk3d_quat *out)
{
    psk3d_quat aa;
    psk3d_quat bb;
    psk3d_quat r;
    psk3d_i32 dot;
    psk3d_i32 theta;
    psk3d_i32 sin_theta;
    psk3d_i32 sin_a;
    psk3d_i32 sin_b;
    psk3d_i32 wa;
    psk3d_i32 wb;
    psk3d_i32 one_minus_t;

    if (a == (const psk3d_quat *)0 || b == (const psk3d_quat *)0 || out == (psk3d_quat *)0) {
        return;
    }
    if (t < 0) {
        t = 0;
    }
    if (t > PSK3D_Q16_ONE) {
        t = PSK3D_Q16_ONE;
    }

    aa = *a;
    bb = *b;
    psk3d_quat_normalize(&aa);
    psk3d_quat_normalize(&bb);

    dot = psk3d_quat_dot(&aa, &bb);
    if (dot < 0) {
        bb.x = -bb.x;
        bb.y = -bb.y;
        bb.z = -bb.z;
        bb.w = -bb.w;
        dot = -dot;
    }
    dot = psk3d_q16_clamp(dot, -PSK3D_Q16_ONE, PSK3D_Q16_ONE);

    if (dot > (PSK3D_Q16_ONE - 32)) {
        psk3d_quat_nlerp(&aa, &bb, t, out);
        return;
    }

    theta = psk3d_q16_acos_unit(dot);
    sin_theta = psk3d_q16_sin_0_pi(theta);
    if (sin_theta <= 32) {
        psk3d_quat_nlerp(&aa, &bb, t, out);
        return;
    }

    one_minus_t = PSK3D_Q16_ONE - t;
    sin_a = psk3d_q16_sin_0_pi(psk3d_q16_mul(one_minus_t, theta));
    sin_b = psk3d_q16_sin_0_pi(psk3d_q16_mul(t, theta));
    wa = psk3d_q16_div(sin_a, sin_theta);
    wb = psk3d_q16_div(sin_b, sin_theta);

    r.x = psk3d_q16_add_sat(psk3d_q16_mul(aa.x, wa), psk3d_q16_mul(bb.x, wb));
    r.y = psk3d_q16_add_sat(psk3d_q16_mul(aa.y, wa), psk3d_q16_mul(bb.y, wb));
    r.z = psk3d_q16_add_sat(psk3d_q16_mul(aa.z, wa), psk3d_q16_mul(bb.z, wb));
    r.w = psk3d_q16_add_sat(psk3d_q16_mul(aa.w, wa), psk3d_q16_mul(bb.w, wb));
    psk3d_quat_normalize(&r);
    *out = r;
}

void psk3d_quat_rotate_vec3(const psk3d_quat *q, const psk3d_vec3 *v, psk3d_vec3 *out)
{
    psk3d_quat qq;
    psk3d_quat p;
    psk3d_quat qc;
    psk3d_quat tmp;
    psk3d_quat r;
    if (q == (const psk3d_quat *)0 || v == (const psk3d_vec3 *)0 || out == (psk3d_vec3 *)0) {
        return;
    }
    qq = *q;
    psk3d_quat_normalize(&qq);
    p.x = v->x;
    p.y = v->y;
    p.z = v->z;
    p.w = 0;
    qc.x = -qq.x;
    qc.y = -qq.y;
    qc.z = -qq.z;
    qc.w = qq.w;
    psk3d_quat_mul(&qq, &p, &tmp);
    psk3d_quat_mul(&tmp, &qc, &r);
    out->x = r.x;
    out->y = r.y;
    out->z = r.z;
}

void psk3d_vec3_normalize(psk3d_vec3 *v)
{
    psk3d_i32 len2;
    psk3d_i32 len;
    if (v == (psk3d_vec3 *)0) {
        return;
    }
    len2 = psk3d_q16_add_sat(psk3d_q16_add_sat(psk3d_q16_mul(v->x, v->x),
                                                psk3d_q16_mul(v->y, v->y)),
                              psk3d_q16_mul(v->z, v->z));
    len = psk3d_q16_sqrt(len2);
    if (len <= 0) {
        v->x = 0;
        v->y = 0;
        v->z = PSK3D_Q16_ONE;
        return;
    }
    v->x = psk3d_q16_div(v->x, len);
    v->y = psk3d_q16_div(v->y, len);
    v->z = psk3d_q16_div(v->z, len);
}

static void psk3d_vec3_add(const psk3d_vec3 *a, const psk3d_vec3 *b, psk3d_vec3 *out)
{
    out->x = psk3d_q16_add_sat(a->x, b->x);
    out->y = psk3d_q16_add_sat(a->y, b->y);
    out->z = psk3d_q16_add_sat(a->z, b->z);
}

static void psk3d_vec3_sub(const psk3d_vec3 *a, const psk3d_vec3 *b, psk3d_vec3 *out)
{
    out->x = psk3d_q16_sub_sat(a->x, b->x);
    out->y = psk3d_q16_sub_sat(a->y, b->y);
    out->z = psk3d_q16_sub_sat(a->z, b->z);
}

static void psk3d_vec3_scale_accum(psk3d_vec3 *acc, const psk3d_vec3 *v, psk3d_i32 w)
{
    acc->x = psk3d_q16_add_sat(acc->x, psk3d_q16_mul(v->x, w));
    acc->y = psk3d_q16_add_sat(acc->y, psk3d_q16_mul(v->y, w));
    acc->z = psk3d_q16_add_sat(acc->z, psk3d_q16_mul(v->z, w));
}

static void psk3d_vec3_component_mul(const psk3d_vec3 *a, const psk3d_vec3 *b, psk3d_vec3 *out)
{
    out->x = psk3d_q16_mul(a->x, b->x);
    out->y = psk3d_q16_mul(a->y, b->y);
    out->z = psk3d_q16_mul(a->z, b->z);
}

static psk3d_i32 psk3d_nonzero_scale(psk3d_i32 v)
{
    if (v == 0) {
        return PSK3D_Q16_ONE;
    }
    return v;
}

static void psk3d_vec3_get_scale_or_one(const psk3d_vec3 *src, psk3d_vec3 *out)
{
    if (src == (const psk3d_vec3 *)0) {
        out->x = PSK3D_Q16_ONE;
        out->y = PSK3D_Q16_ONE;
        out->z = PSK3D_Q16_ONE;
        return;
    }
    out->x = psk3d_nonzero_scale(src->x);
    out->y = psk3d_nonzero_scale(src->y);
    out->z = psk3d_nonzero_scale(src->z);
}

static void psk3d_pose_set_identity_scales(psk3d_pose *pose)
{
    psk3d_u32 i;
    if (pose == (psk3d_pose *)0) {
        return;
    }
    for (i = 0u; i < pose->bone_count && i < PSK3D_MAX_BONES; i++) {
        pose->scales[i].x = PSK3D_Q16_ONE;
        pose->scales[i].y = PSK3D_Q16_ONE;
        pose->scales[i].z = PSK3D_Q16_ONE;
    }
}

static void psk3d_transform_point_delta_scaled(const psk3d_pose *bind_global,
                                               const psk3d_pose *current_global,
                                               psk3d_u32 bone_index,
                                               const psk3d_vec3 *p,
                                               psk3d_vec3 *out)
{
    psk3d_vec3 shifted;
    psk3d_vec3 local;
    psk3d_vec3 bind_scale;
    psk3d_vec3 current_scale;
    psk3d_vec3 scaled_local;
    psk3d_vec3 moved;
    psk3d_quat inv_bind;

    psk3d_vec3_sub(p, &bind_global->positions[bone_index], &shifted);
    inv_bind.x = -bind_global->orientations[bone_index].x;
    inv_bind.y = -bind_global->orientations[bone_index].y;
    inv_bind.z = -bind_global->orientations[bone_index].z;
    inv_bind.w = bind_global->orientations[bone_index].w;
    psk3d_quat_rotate_vec3(&inv_bind, &shifted, &local);

    psk3d_vec3_get_scale_or_one(&bind_global->scales[bone_index], &bind_scale);
    psk3d_vec3_get_scale_or_one(&current_global->scales[bone_index], &current_scale);
    local.x = psk3d_q16_div(local.x, bind_scale.x);
    local.y = psk3d_q16_div(local.y, bind_scale.y);
    local.z = psk3d_q16_div(local.z, bind_scale.z);
    psk3d_vec3_component_mul(&local, &current_scale, &scaled_local);

    psk3d_quat_rotate_vec3(&current_global->orientations[bone_index], &scaled_local, &moved);
    psk3d_vec3_add(&moved, &current_global->positions[bone_index], out);
}

static void psk3d_transform_normal_delta(const psk3d_pose *bind_global,
                                         const psk3d_pose *current_global,
                                         psk3d_u32 bone_index,
                                         const psk3d_vec3 *n,
                                         psk3d_vec3 *out)
{
    psk3d_vec3 local;
    psk3d_quat inv_bind;

    inv_bind.x = -bind_global->orientations[bone_index].x;
    inv_bind.y = -bind_global->orientations[bone_index].y;
    inv_bind.z = -bind_global->orientations[bone_index].z;
    inv_bind.w = bind_global->orientations[bone_index].w;
    psk3d_quat_rotate_vec3(&inv_bind, n, &local);
    psk3d_quat_rotate_vec3(&current_global->orientations[bone_index], &local, out);
    psk3d_vec3_normalize(out);
}

static psk3d_result psk3d_read_points(const psk3d_u8 *data,
                                      const psk3d_chunk *chunk,
                                      psk3d_model *model)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    if (chunk->data_size < 12u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_POINTS) {
        return PSK3D_ERR_CAPACITY;
    }
    model->point_count = chunk->data_count;
    for (i = 0u; i < chunk->data_count; i++) {
        pos = chunk->payload_offset + i * chunk->data_size;
        model->points[i].p.x = psk3d_rd_f32q16(data + pos);
        model->points[i].p.y = psk3d_rd_f32q16(data + pos + 4u);
        model->points[i].p.z = psk3d_rd_f32q16(data + pos + 8u);
    }
    return PSK3D_OK;
}

static int psk3d_use_wedge32(const psk3d_u8 *rec, const psk3d_model *model)
{
    psk3d_u32 low;
    psk3d_u32 high;
    psk3d_u32 whole;
    low = (psk3d_u32)psk3d_rd_u16(rec);
    high = (psk3d_u32)psk3d_rd_u16(rec + 2u);
    whole = psk3d_rd_u32(rec);
    if (model->point_count > 65535u) {
        return 1;
    }
    if (high != 0u && whole < model->point_count) {
        return 1;
    }
    if (low >= model->point_count && whole < model->point_count) {
        return 1;
    }
    return 0;
}

static psk3d_result psk3d_read_wedges(const psk3d_u8 *data,
                                      const psk3d_chunk *chunk,
                                      psk3d_model *model)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    int w32;
    if (chunk->data_size < 16u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_WEDGES) {
        return PSK3D_ERR_CAPACITY;
    }
    model->wedge_count = chunk->data_count;
    for (i = 0u; i < chunk->data_count; i++) {
        pos = chunk->payload_offset + i * chunk->data_size;
        w32 = psk3d_use_wedge32(data + pos, model);
        if (w32) {
            model->wedges[i].point_index = psk3d_rd_u32(data + pos);
            model->wedges[i].u = psk3d_rd_f32q16(data + pos + 4u);
            model->wedges[i].v = psk3d_rd_f32q16(data + pos + 8u);
            model->wedges[i].mat_index = data[pos + 12u];
            model->wedges[i].reserved = data[pos + 13u];
        } else {
            model->wedges[i].point_index = (psk3d_u32)psk3d_rd_u16(data + pos);
            model->wedges[i].u = psk3d_rd_f32q16(data + pos + 4u);
            model->wedges[i].v = psk3d_rd_f32q16(data + pos + 8u);
            model->wedges[i].mat_index = data[pos + 12u];
            model->wedges[i].reserved = data[pos + 13u];
        }
    }
    return PSK3D_OK;
}

static psk3d_result psk3d_read_faces(const psk3d_u8 *data,
                                     const psk3d_chunk *chunk,
                                     psk3d_model *model)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    if (chunk->data_size != 12u && chunk->data_size < 18u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_FACES) {
        return PSK3D_ERR_CAPACITY;
    }
    model->face_count = chunk->data_count;
    for (i = 0u; i < chunk->data_count; i++) {
        pos = chunk->payload_offset + i * chunk->data_size;
        if (chunk->data_size == 12u) {
            model->faces[i].wedge[0] = (psk3d_u32)psk3d_rd_u16(data + pos);
            model->faces[i].wedge[1] = (psk3d_u32)psk3d_rd_u16(data + pos + 2u);
            model->faces[i].wedge[2] = (psk3d_u32)psk3d_rd_u16(data + pos + 4u);
            model->faces[i].mat_index = data[pos + 6u];
            model->faces[i].aux_mat_index = data[pos + 7u];
            model->faces[i].smoothing_groups = psk3d_rd_u32(data + pos + 8u);
        } else {
            model->faces[i].wedge[0] = psk3d_rd_u32(data + pos);
            model->faces[i].wedge[1] = psk3d_rd_u32(data + pos + 4u);
            model->faces[i].wedge[2] = psk3d_rd_u32(data + pos + 8u);
            model->faces[i].mat_index = data[pos + 12u];
            model->faces[i].aux_mat_index = data[pos + 13u];
            model->faces[i].smoothing_groups = psk3d_rd_u32(data + pos + 14u);
        }
    }
    return PSK3D_OK;
}

static psk3d_result psk3d_read_materials(const psk3d_u8 *data,
                                         const psk3d_chunk *chunk,
                                         psk3d_model *model)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    if (chunk->data_size < 88u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_MATERIALS) {
        return PSK3D_ERR_CAPACITY;
    }
    model->material_count = chunk->data_count;
    for (i = 0u; i < chunk->data_count; i++) {
        pos = chunk->payload_offset + i * chunk->data_size;
        psk3d_copy_fixed_name(model->materials[i].name, data + pos, PSK3D_NAME_BYTES);
        model->materials[i].texture_index = psk3d_rd_u32(data + pos + 64u);
        model->materials[i].poly_flags = psk3d_rd_u32(data + pos + 68u);
        model->materials[i].aux_material = psk3d_rd_u32(data + pos + 72u);
        model->materials[i].aux_flags = psk3d_rd_u32(data + pos + 76u);
        model->materials[i].lod_bias = psk3d_rd_i32(data + pos + 80u);
        model->materials[i].lod_style = psk3d_rd_i32(data + pos + 84u);
    }
    return PSK3D_OK;
}

static void psk3d_read_bone_one(const psk3d_u8 *data, psk3d_u32 pos, psk3d_u32 bone_index, psk3d_bone *bone)
{
    psk3d_copy_fixed_name(bone->name, data + pos, PSK3D_NAME_BYTES);
    bone->flags = psk3d_rd_u32(data + pos + 64u);
    bone->num_children = psk3d_rd_i32(data + pos + 68u);
    bone->parent_index = psk3d_rd_i32(data + pos + 72u);
    bone->orientation.x = psk3d_rd_f32q16(data + pos + 76u);
    bone->orientation.y = psk3d_rd_f32q16(data + pos + 80u);
    bone->orientation.z = psk3d_rd_f32q16(data + pos + 84u);
    bone->orientation.w = psk3d_rd_f32q16(data + pos + 88u);
    /* ActorX convention for the globalize path: conjugate CHILD local
       orientations only. Root bone 0 can report parent_index == 0, so
       testing parent_index >= 0 incorrectly conjugates the root. */
    if (bone_index != 0u) {
        bone->orientation.x = -bone->orientation.x;
        bone->orientation.y = -bone->orientation.y;
        bone->orientation.z = -bone->orientation.z;
    }
    bone->position.x = psk3d_rd_f32q16(data + pos + 92u);
    bone->position.y = psk3d_rd_f32q16(data + pos + 96u);
    bone->position.z = psk3d_rd_f32q16(data + pos + 100u);
    bone->length = psk3d_rd_f32q16(data + pos + 104u);
    bone->size.x = psk3d_rd_f32q16(data + pos + 108u);
    bone->size.y = psk3d_rd_f32q16(data + pos + 112u);
    bone->size.z = psk3d_rd_f32q16(data + pos + 116u);
}

static psk3d_result psk3d_read_bones_model(const psk3d_u8 *data,
                                           const psk3d_chunk *chunk,
                                           psk3d_model *model)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    if (chunk->data_size < 120u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_BONES) {
        return PSK3D_ERR_CAPACITY;
    }
    model->bone_count = chunk->data_count;
    for (i = 0u; i < chunk->data_count; i++) {
        pos = chunk->payload_offset + i * chunk->data_size;
        psk3d_read_bone_one(data, pos, i, &model->bones[i]);
    }
    return PSK3D_OK;
}

static psk3d_result psk3d_read_bones_anim(const psk3d_u8 *data,
                                          const psk3d_chunk *chunk,
                                          psk3d_animset *animset)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    if (chunk->data_size < 120u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_BONES) {
        return PSK3D_ERR_CAPACITY;
    }
    animset->bone_count = chunk->data_count;
    for (i = 0u; i < chunk->data_count; i++) {
        pos = chunk->payload_offset + i * chunk->data_size;
        psk3d_read_bone_one(data, pos, i, &animset->bones[i]);
    }
    return PSK3D_OK;
}

static psk3d_result psk3d_read_influences(const psk3d_u8 *data,
                                          const psk3d_chunk *chunk,
                                          psk3d_model *model)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    if (chunk->data_size < 12u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_INFLUENCES) {
        return PSK3D_ERR_CAPACITY;
    }
    model->influence_count = chunk->data_count;
    for (i = 0u; i < chunk->data_count; i++) {
        pos = chunk->payload_offset + i * chunk->data_size;
        model->influences[i].weight = psk3d_rd_f32q16(data + pos);
        model->influences[i].point_index = psk3d_rd_u32(data + pos + 4u);
        model->influences[i].bone_index = psk3d_rd_u32(data + pos + 8u);
    }
    return PSK3D_OK;
}


static psk3d_u32 psk3d_extra_uv_slot_from_id(const char *id, const psk3d_model *model)
{
    psk3d_u32 slot;
    slot = model->extra_uv_set_count;
    if (id != (const char *)0) {
        if (id[8] >= '0' && id[8] <= '9') {
            slot = (psk3d_u32)(id[8] - '0');
        }
    }
    return slot;
}

static psk3d_result psk3d_read_extra_uvs(const psk3d_u8 *data,
                                         const psk3d_chunk *chunk,
                                         psk3d_model *model)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    psk3d_u32 slot;
    if (chunk->data_size < 8u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_WEDGES) {
        return PSK3D_ERR_CAPACITY;
    }
    slot = psk3d_extra_uv_slot_from_id(chunk->id, model);
    if (slot >= PSK3D_MAX_EXTRA_UV_SETS) {
        return PSK3D_ERR_CAPACITY;
    }
    model->extra_uv_counts[slot] = chunk->data_count;
    if (slot + 1u > model->extra_uv_set_count) {
        model->extra_uv_set_count = slot + 1u;
    }
    for (i = 0u; i < chunk->data_count; i++) {
        pos = chunk->payload_offset + i * chunk->data_size;
        model->extra_uvs[slot][i].u = psk3d_rd_f32q16(data + pos);
        model->extra_uvs[slot][i].v = psk3d_rd_f32q16(data + pos + 4u);
    }
    return PSK3D_OK;
}

static psk3d_result psk3d_read_vertex_colors(const psk3d_u8 *data,
                                             const psk3d_chunk *chunk,
                                             psk3d_model *model)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    if (chunk->data_size < 4u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_WEDGES) {
        return PSK3D_ERR_CAPACITY;
    }
    model->vertex_color_count = chunk->data_count;
    for (i = 0u; i < chunk->data_count; i++) {
        pos = chunk->payload_offset + i * chunk->data_size;
        model->vertex_colors[i].r = data[pos];
        model->vertex_colors[i].g = data[pos + 1u];
        model->vertex_colors[i].b = data[pos + 2u];
        model->vertex_colors[i].a = data[pos + 3u];
    }
    return PSK3D_OK;
}

static psk3d_result psk3d_read_normals(const psk3d_u8 *data,
                                       const psk3d_chunk *chunk,
                                       psk3d_model *model)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    if (chunk->data_size < 12u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_WEDGES) {
        return PSK3D_ERR_CAPACITY;
    }
    model->normal_count = chunk->data_count;
    for (i = 0u; i < chunk->data_count; i++) {
        pos = chunk->payload_offset + i * chunk->data_size;
        model->normals[i].x = psk3d_rd_f32q16(data + pos);
        model->normals[i].y = psk3d_rd_f32q16(data + pos + 4u);
        model->normals[i].z = psk3d_rd_f32q16(data + pos + 8u);
        psk3d_vec3_normalize(&model->normals[i]);
    }
    return PSK3D_OK;
}

static psk3d_result psk3d_read_morph_infos(const psk3d_u8 *data,
                                           const psk3d_chunk *chunk,
                                           psk3d_model *model)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    psk3d_u32 running;
    psk3d_i32 count;

    if (chunk->data_size < 68u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_MORPH_TARGETS) {
        return PSK3D_ERR_CAPACITY;
    }
    model->morph_target_count = chunk->data_count;
    running = 0u;
    for (i = 0u; i < chunk->data_count; i++) {
        pos = chunk->payload_offset + i * chunk->data_size;
        psk3d_copy_fixed_name(model->morph_targets[i].name, data + pos, PSK3D_NAME_BYTES);
        count = psk3d_rd_i32(data + pos + 64u);
        if (count < 0) {
            return PSK3D_ERR_BAD_SIZE;
        }
        if ((psk3d_u32)count > PSK3D_MAX_MORPH_DELTAS - running) {
            return PSK3D_ERR_CAPACITY;
        }
        model->morph_targets[i].vertex_count = (psk3d_u32)count;
        model->morph_targets[i].data_offset = running;
        running += (psk3d_u32)count;
    }
    return PSK3D_OK;
}

static psk3d_result psk3d_read_morph_deltas(const psk3d_u8 *data,
                                            const psk3d_chunk *chunk,
                                            psk3d_model *model)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    psk3d_u32 point_last;
    psk3d_u32 point_first;

    if (chunk->data_size < 16u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_MORPH_DELTAS) {
        return PSK3D_ERR_CAPACITY;
    }
    model->morph_delta_count = chunk->data_count;
    for (i = 0u; i < chunk->data_count; i++) {
        pos = chunk->payload_offset + i * chunk->data_size;
        point_last = psk3d_rd_u32(data + pos + 12u);
        point_first = psk3d_rd_u32(data + pos);
        if (point_last < PSK3D_MAX_POINTS) {
            model->morph_deltas[i].position_delta.x = psk3d_rd_f32q16(data + pos);
            model->morph_deltas[i].position_delta.y = psk3d_rd_f32q16(data + pos + 4u);
            model->morph_deltas[i].position_delta.z = psk3d_rd_f32q16(data + pos + 8u);
            model->morph_deltas[i].point_index = point_last;
        } else {
            model->morph_deltas[i].point_index = point_first;
            model->morph_deltas[i].position_delta.x = psk3d_rd_f32q16(data + pos + 4u);
            model->morph_deltas[i].position_delta.y = psk3d_rd_f32q16(data + pos + 8u);
            model->morph_deltas[i].position_delta.z = psk3d_rd_f32q16(data + pos + 12u);
        }
    }
    return PSK3D_OK;
}

static psk3d_result psk3d_read_anim_infos(const psk3d_u8 *data,
                                          const psk3d_chunk *chunk,
                                          psk3d_animset *animset)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    psk3d_u32 running;
    if (chunk->data_size < 168u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_SEQUENCES) {
        return PSK3D_ERR_CAPACITY;
    }
    animset->sequence_count = chunk->data_count;
    running = 0u;
    for (i = 0u; i < chunk->data_count; i++) {
        pos = chunk->payload_offset + i * chunk->data_size;
        psk3d_copy_fixed_name(animset->sequences[i].name, data + pos, PSK3D_NAME_BYTES);
        psk3d_copy_fixed_name(animset->sequences[i].group, data + pos + 64u, PSK3D_NAME_BYTES);
        animset->sequences[i].total_bones = psk3d_rd_i32(data + pos + 128u);
        animset->sequences[i].root_include = psk3d_rd_i32(data + pos + 132u);
        animset->sequences[i].key_compression_style = psk3d_rd_i32(data + pos + 136u);
        animset->sequences[i].key_quotum = psk3d_rd_i32(data + pos + 140u);
        animset->sequences[i].key_reduction = psk3d_rd_f32q16(data + pos + 144u);
        animset->sequences[i].track_time = psk3d_rd_f32q16(data + pos + 148u);
        animset->sequences[i].anim_rate = psk3d_rd_f32q16(data + pos + 152u);
        animset->sequences[i].start_bone = psk3d_rd_i32(data + pos + 156u);
        animset->sequences[i].first_raw_frame = psk3d_rd_i32(data + pos + 160u);
        animset->sequences[i].num_raw_frames = psk3d_rd_i32(data + pos + 164u);
        animset->sequences[i].key_offset = running;
        if (animset->sequences[i].num_raw_frames > 0 && animset->sequences[i].total_bones > 0) {
            psk3d_u32 frames;
            psk3d_u32 bones;
            frames = (psk3d_u32)animset->sequences[i].num_raw_frames;
            bones = (psk3d_u32)animset->sequences[i].total_bones;
            if (frames > 0xffffffffu / bones) {
                return PSK3D_ERR_OVERFLOW;
            }
            if (running > 0xffffffffu - frames * bones) {
                return PSK3D_ERR_OVERFLOW;
            }
            running += frames * bones;
        }
    }
    return PSK3D_OK;
}

static psk3d_result psk3d_read_anim_keys(const psk3d_u8 *data,
                                         const psk3d_chunk *chunk,
                                         psk3d_animset *animset)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    if (chunk->data_size < 32u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_KEYS) {
        return PSK3D_ERR_CAPACITY;
    }
    animset->key_count = chunk->data_count;
    for (i = 0u; i < chunk->data_count; i++) {
        psk3d_u32 bone_index;
        pos = chunk->payload_offset + i * chunk->data_size;
        animset->keys[i].position.x = psk3d_rd_f32q16(data + pos);
        animset->keys[i].position.y = psk3d_rd_f32q16(data + pos + 4u);
        animset->keys[i].position.z = psk3d_rd_f32q16(data + pos + 8u);
        animset->keys[i].orientation.x = psk3d_rd_f32q16(data + pos + 12u);
        animset->keys[i].orientation.y = psk3d_rd_f32q16(data + pos + 16u);
        animset->keys[i].orientation.z = psk3d_rd_f32q16(data + pos + 20u);
        animset->keys[i].orientation.w = psk3d_rd_f32q16(data + pos + 24u);
        bone_index = (animset->bone_count > 0u) ? (i % animset->bone_count) : 0u;
        if (animset->bone_count > 0u && bone_index < animset->bone_count && bone_index != 0u) {
            animset->keys[i].orientation.x = -animset->keys[i].orientation.x;
            animset->keys[i].orientation.y = -animset->keys[i].orientation.y;
            animset->keys[i].orientation.z = -animset->keys[i].orientation.z;
        }
        animset->keys[i].time = psk3d_rd_f32q16(data + pos + 28u);
    }
    return PSK3D_OK;
}

static psk3d_result psk3d_read_scale_keys(const psk3d_u8 *data,
                                          const psk3d_chunk *chunk,
                                          psk3d_animset *animset)
{
    psk3d_u32 i;
    psk3d_u32 pos;
    if (chunk->data_size < 16u) {
        return PSK3D_ERR_BAD_SIZE;
    }
    if (chunk->data_count > PSK3D_MAX_SCALE_KEYS) {
        return PSK3D_ERR_CAPACITY;
    }
    animset->scale_key_count = chunk->data_count;
    for (i = 0u; i < chunk->data_count; i++) {
        pos = chunk->payload_offset + i * chunk->data_size;
        animset->scale_keys[i].scale.x = psk3d_rd_f32q16(data + pos);
        animset->scale_keys[i].scale.y = psk3d_rd_f32q16(data + pos + 4u);
        animset->scale_keys[i].scale.z = psk3d_rd_f32q16(data + pos + 8u);
        animset->scale_keys[i].time = psk3d_rd_f32q16(data + pos + 12u);
    }
    return PSK3D_OK;
}

psk3d_result psk3d_parse_psk(const psk3d_u8 *data,
                             psk3d_u32 size,
                             psk3d_model *out_model,
                             psk3d_report *report)
{
    psk3d_u32 offset;
    psk3d_u32 chunk_index;
    psk3d_chunk chunk;
    psk3d_result r;

    if (report != (psk3d_report *)0) {
        psk3d_clear_report(report);
    }
    if (data == (const psk3d_u8 *)0 || out_model == (psk3d_model *)0) {
        psk3d_set_error(report, PSK3D_ERR_NULL, "null input", (const char *)0, 0u, 0u);
        return PSK3D_ERR_NULL;
    }
    if (!psk3d_check_types()) {
        psk3d_set_error(report, PSK3D_ERR_TYPESIZE, "C integer type is too small", (const char *)0, 0u, 0u);
        return PSK3D_ERR_TYPESIZE;
    }

    psk3d_clear_model(out_model);
    r = psk3d_read_chunk(data, size, 0u, &chunk);
    if (r != PSK3D_OK) {
        psk3d_set_error(report, r, "missing ACTRHEAD", (const char *)0, 0u, 0u);
        return r;
    }
    psk3d_remember_chunk(report, &chunk);
    if (!psk3d_id_eq(chunk.id, "ACTRHEAD")) {
        psk3d_set_error(report, PSK3D_ERR_BAD_MAGIC, "not a PSK ACTRHEAD file", chunk.id, 0u, 0u);
        return PSK3D_ERR_BAD_MAGIC;
    }
    out_model->version_flags = chunk.type_flags;
    r = psk3d_check_payload(&chunk, size);
    if (r != PSK3D_OK) {
        psk3d_set_error(report, r, "bad ACTRHEAD payload", chunk.id, 0u, 0u);
        return r;
    }

    offset = psk3d_chunk_next(&chunk);
    chunk_index = 1u;
    while (offset < size) {
        r = psk3d_read_chunk(data, size, offset, &chunk);
        if (r != PSK3D_OK) {
            psk3d_set_error(report, r, "bad chunk header", (const char *)0, chunk_index, offset);
            return r;
        }
        psk3d_remember_chunk(report, &chunk);
        r = psk3d_check_payload(&chunk, size);
        if (r != PSK3D_OK) {
            psk3d_set_error(report, r, "chunk payload outside file", chunk.id, chunk_index, offset);
            return r;
        }

        if (psk3d_id_eq(chunk.id, "PNTS0000")) {
            r = psk3d_read_points(data, &chunk, out_model);
        } else if (psk3d_id_eq(chunk.id, "VTXW0000") || psk3d_id_eq(chunk.id, "VTXW3200")) {
            r = psk3d_read_wedges(data, &chunk, out_model);
        } else if (psk3d_id_eq(chunk.id, "FACE0000") || psk3d_id_eq(chunk.id, "FACE3200")) {
            r = psk3d_read_faces(data, &chunk, out_model);
        } else if (psk3d_id_eq(chunk.id, "MATT0000")) {
            r = psk3d_read_materials(data, &chunk, out_model);
        } else if (psk3d_id_eq(chunk.id, "REFSKELT") || psk3d_id_eq(chunk.id, "REFSKEL0")) {
            r = psk3d_read_bones_model(data, &chunk, out_model);
        } else if (psk3d_id_eq(chunk.id, "RAWWEIGHTS") || psk3d_id_starts(chunk.id, "RAWWEIGH") || psk3d_id_eq(chunk.id, "RAWW0000")) {
            r = psk3d_read_influences(data, &chunk, out_model);
        } else if (psk3d_id_starts(chunk.id, "EXTRAUVS")) {
            r = psk3d_read_extra_uvs(data, &chunk, out_model);
        } else if (psk3d_id_starts(chunk.id, "VERTEXCO")) {
            r = psk3d_read_vertex_colors(data, &chunk, out_model);
        } else if (psk3d_id_eq(chunk.id, "VTXNORMS")) {
            r = psk3d_read_normals(data, &chunk, out_model);
        } else if (psk3d_id_eq(chunk.id, "MRPHINFO") || psk3d_id_starts(chunk.id, "MORPHINF")) {
            r = psk3d_read_morph_infos(data, &chunk, out_model);
        } else if (psk3d_id_eq(chunk.id, "MRPHDATA") || psk3d_id_starts(chunk.id, "MORPHDAT")) {
            r = psk3d_read_morph_deltas(data, &chunk, out_model);
        } else {
            if (report != (psk3d_report *)0) {
                report->unknown_chunks++;
            }
            r = PSK3D_OK;
        }

        if (r != PSK3D_OK) {
            psk3d_set_error(report, r, "failed decoding chunk", chunk.id, chunk_index, offset);
            return r;
        }

        offset = psk3d_chunk_next(&chunk);
        chunk_index++;
    }

    r = psk3d_validate_model_indices(out_model, report);
    if (r != PSK3D_OK) {
        return r;
    }
    if (report != (psk3d_report *)0) {
        report->last_error = PSK3D_OK;
    }
    return PSK3D_OK;
}

psk3d_result psk3d_parse_psa(const psk3d_u8 *data,
                             psk3d_u32 size,
                             psk3d_animset *out_animset,
                             psk3d_report *report)
{
    psk3d_u32 offset;
    psk3d_u32 chunk_index;
    psk3d_chunk chunk;
    psk3d_result r;

    if (report != (psk3d_report *)0) {
        psk3d_clear_report(report);
    }
    if (data == (const psk3d_u8 *)0 || out_animset == (psk3d_animset *)0) {
        psk3d_set_error(report, PSK3D_ERR_NULL, "null input", (const char *)0, 0u, 0u);
        return PSK3D_ERR_NULL;
    }
    if (!psk3d_check_types()) {
        psk3d_set_error(report, PSK3D_ERR_TYPESIZE, "C integer type is too small", (const char *)0, 0u, 0u);
        return PSK3D_ERR_TYPESIZE;
    }

    psk3d_clear_animset(out_animset);
    r = psk3d_read_chunk(data, size, 0u, &chunk);
    if (r != PSK3D_OK) {
        psk3d_set_error(report, r, "missing ANIMHEAD", (const char *)0, 0u, 0u);
        return r;
    }
    psk3d_remember_chunk(report, &chunk);
    if (!psk3d_id_eq(chunk.id, "ANIMHEAD")) {
        psk3d_set_error(report, PSK3D_ERR_BAD_MAGIC, "not a PSA ANIMHEAD file", chunk.id, 0u, 0u);
        return PSK3D_ERR_BAD_MAGIC;
    }
    out_animset->version_flags = chunk.type_flags;
    r = psk3d_check_payload(&chunk, size);
    if (r != PSK3D_OK) {
        psk3d_set_error(report, r, "bad ANIMHEAD payload", chunk.id, 0u, 0u);
        return r;
    }

    offset = psk3d_chunk_next(&chunk);
    chunk_index = 1u;
    while (offset < size) {
        r = psk3d_read_chunk(data, size, offset, &chunk);
        if (r != PSK3D_OK) {
            psk3d_set_error(report, r, "bad chunk header", (const char *)0, chunk_index, offset);
            return r;
        }
        psk3d_remember_chunk(report, &chunk);
        r = psk3d_check_payload(&chunk, size);
        if (r != PSK3D_OK) {
            psk3d_set_error(report, r, "chunk payload outside file", chunk.id, chunk_index, offset);
            return r;
        }

        if (psk3d_id_eq(chunk.id, "BONENAMES")) {
            r = psk3d_read_bones_anim(data, &chunk, out_animset);
        } else if (psk3d_id_eq(chunk.id, "ANIMINFO")) {
            r = psk3d_read_anim_infos(data, &chunk, out_animset);
        } else if (psk3d_id_eq(chunk.id, "ANIMKEYS")) {
            r = psk3d_read_anim_keys(data, &chunk, out_animset);
        } else if (psk3d_id_starts(chunk.id, "SCALE")) {
            r = psk3d_read_scale_keys(data, &chunk, out_animset);
        } else {
            if (report != (psk3d_report *)0) {
                report->unknown_chunks++;
            }
            r = PSK3D_OK;
        }

        if (r != PSK3D_OK) {
            psk3d_set_error(report, r, "failed decoding chunk", chunk.id, chunk_index, offset);
            return r;
        }

        offset = psk3d_chunk_next(&chunk);
        chunk_index++;
    }

    r = psk3d_validate_animset(out_animset, report);
    if (r != PSK3D_OK) {
        return r;
    }
    if (report != (psk3d_report *)0) {
        report->last_error = PSK3D_OK;
    }
    return PSK3D_OK;
}

psk3d_result psk3d_validate_model_indices(const psk3d_model *model,
                                          psk3d_report *report)
{
    psk3d_u32 i;
    if (model == (const psk3d_model *)0) {
        psk3d_set_error(report, PSK3D_ERR_NULL, "null model", (const char *)0, 0u, 0u);
        return PSK3D_ERR_NULL;
    }
    for (i = 0u; i < model->wedge_count; i++) {
        if (model->wedges[i].point_index >= model->point_count) {
            psk3d_set_error(report, PSK3D_ERR_BAD_INDEX, "wedge point index out of range", "VTXW0000", i, 0u);
            return PSK3D_ERR_BAD_INDEX;
        }
    }
    for (i = 0u; i < model->face_count; i++) {
        if (model->faces[i].wedge[0] >= model->wedge_count ||
            model->faces[i].wedge[1] >= model->wedge_count ||
            model->faces[i].wedge[2] >= model->wedge_count) {
            psk3d_set_error(report, PSK3D_ERR_BAD_INDEX, "face wedge index out of range", "FACE0000", i, 0u);
            return PSK3D_ERR_BAD_INDEX;
        }
    }
    for (i = 0u; i < model->influence_count; i++) {
        if (model->influences[i].point_index >= model->point_count) {
            psk3d_set_error(report, PSK3D_ERR_BAD_INDEX, "influence point index out of range", "RAWWEIGHTS", i, 0u);
            return PSK3D_ERR_BAD_INDEX;
        }
        if (model->influences[i].bone_index >= model->bone_count) {
            psk3d_set_error(report, PSK3D_ERR_BAD_INDEX, "influence bone index out of range", "RAWWEIGHTS", i, 0u);
            return PSK3D_ERR_BAD_INDEX;
        }
    }
    for (i = 0u; i < model->bone_count; i++) {
        if (i == 0u) {
            if (!(model->bones[i].parent_index == -1 || model->bones[i].parent_index == 0)) {
                psk3d_set_error(report, PSK3D_ERR_BAD_INDEX, "root parent index is unusual", "REFSKELT", i, 0u);
                return PSK3D_ERR_BAD_INDEX;
            }
        } else {
            if (model->bones[i].parent_index < 0 || (psk3d_u32)model->bones[i].parent_index >= model->bone_count) {
                psk3d_set_error(report, PSK3D_ERR_BAD_INDEX, "bone parent index out of range", "REFSKELT", i, 0u);
                return PSK3D_ERR_BAD_INDEX;
            }
        }
    }
    for (i = 0u; i < model->morph_target_count; i++) {
        if (model->morph_targets[i].data_offset > model->morph_delta_count ||
            model->morph_targets[i].vertex_count > model->morph_delta_count - model->morph_targets[i].data_offset) {
            psk3d_set_error(report, PSK3D_ERR_BAD_INDEX, "morph target delta range out of range", "MRPHINFO", i, 0u);
            return PSK3D_ERR_BAD_INDEX;
        }
    }
    for (i = 0u; i < model->morph_delta_count; i++) {
        if (model->morph_deltas[i].point_index >= model->point_count) {
            psk3d_set_error(report, PSK3D_ERR_BAD_INDEX, "morph delta point index out of range", "MRPHDATA", i, 0u);
            return PSK3D_ERR_BAD_INDEX;
        }
    }
    return PSK3D_OK;
}

psk3d_result psk3d_validate_animset(const psk3d_animset *animset,
                                    psk3d_report *report)
{
    psk3d_u32 i;
    psk3d_u32 needed;
    psk3d_u32 frames;
    psk3d_u32 bones;
    if (animset == (const psk3d_animset *)0) {
        psk3d_set_error(report, PSK3D_ERR_NULL, "null animset", (const char *)0, 0u, 0u);
        return PSK3D_ERR_NULL;
    }
    for (i = 0u; i < animset->bone_count; i++) {
        if (i == 0u) {
            if (!(animset->bones[i].parent_index == -1 || animset->bones[i].parent_index == 0)) {
                psk3d_set_error(report, PSK3D_ERR_BAD_INDEX, "root parent index is unusual", "BONENAMES", i, 0u);
                return PSK3D_ERR_BAD_INDEX;
            }
        } else {
            if (animset->bones[i].parent_index < 0 || (psk3d_u32)animset->bones[i].parent_index >= animset->bone_count) {
                psk3d_set_error(report, PSK3D_ERR_BAD_INDEX, "bone parent index out of range", "BONENAMES", i, 0u);
                return PSK3D_ERR_BAD_INDEX;
            }
        }
    }
    for (i = 0u; i < animset->sequence_count; i++) {
        if (animset->sequences[i].total_bones < 0 ||
            (psk3d_u32)animset->sequences[i].total_bones > animset->bone_count) {
            psk3d_set_error(report, PSK3D_ERR_BAD_INDEX, "sequence bone count out of range", "ANIMINFO", i, 0u);
            return PSK3D_ERR_BAD_INDEX;
        }
        if (animset->sequences[i].num_raw_frames < 0 || animset->sequences[i].first_raw_frame < 0) {
            psk3d_set_error(report, PSK3D_ERR_BAD_INDEX, "negative raw frame data", "ANIMINFO", i, 0u);
            return PSK3D_ERR_BAD_INDEX;
        }
        frames = (psk3d_u32)animset->sequences[i].num_raw_frames;
        bones = (psk3d_u32)animset->sequences[i].total_bones;
        if (bones != 0u && frames > 0xffffffffu / bones) {
            psk3d_set_error(report, PSK3D_ERR_OVERFLOW, "sequence key count overflow", "ANIMINFO", i, 0u);
            return PSK3D_ERR_OVERFLOW;
        }
        needed = ((psk3d_u32)animset->sequences[i].first_raw_frame) * animset->bone_count;
        if (frames != 0u && animset->bone_count > 0u) {
            if ((psk3d_u32)animset->sequences[i].first_raw_frame > 0xffffffffu / animset->bone_count) {
                psk3d_set_error(report, PSK3D_ERR_OVERFLOW, "first frame key index overflow", "ANIMINFO", i, 0u);
                return PSK3D_ERR_OVERFLOW;
            }
            if (frames > (0xffffffffu - (psk3d_u32)animset->sequences[i].first_raw_frame)) {
                psk3d_set_error(report, PSK3D_ERR_OVERFLOW, "frame range overflow", "ANIMINFO", i, 0u);
                return PSK3D_ERR_OVERFLOW;
            }
            if (((psk3d_u32)animset->sequences[i].first_raw_frame + frames) > 0xffffffffu / animset->bone_count) {
                psk3d_set_error(report, PSK3D_ERR_OVERFLOW, "key range overflow", "ANIMINFO", i, 0u);
                return PSK3D_ERR_OVERFLOW;
            }
            needed = ((psk3d_u32)animset->sequences[i].first_raw_frame + frames) * animset->bone_count;
            if (needed > animset->key_count) {
                psk3d_set_error(report, PSK3D_ERR_BAD_INDEX, "sequence references missing animation keys", "ANIMKEYS", i, 0u);
                return PSK3D_ERR_BAD_INDEX;
            }
        }
    }
    return PSK3D_OK;
}

psk3d_i32 psk3d_find_sequence(const psk3d_animset *animset,
                              const char *name)
{
    psk3d_u32 i;
    if (animset == (const psk3d_animset *)0 || name == (const char *)0) {
        return -1;
    }
    for (i = 0u; i < animset->sequence_count; i++) {
        if (strcmp(animset->sequences[i].name, name) == 0) {
            return (psk3d_i32)i;
        }
    }
    return -1;
}

psk3d_result psk3d_get_key(const psk3d_animset *animset,
                           psk3d_u32 sequence_index,
                           psk3d_u32 frame_index,
                           psk3d_u32 bone_index,
                           psk3d_anim_key *out_key)
{
    const psk3d_anim_info *seq;
    psk3d_u32 abs_frame;
    psk3d_u32 key_index;
    if (animset == (const psk3d_animset *)0 || out_key == (psk3d_anim_key *)0) {
        return PSK3D_ERR_NULL;
    }
    if (sequence_index >= animset->sequence_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    seq = &animset->sequences[sequence_index];
    if (seq->num_raw_frames < 0 || seq->first_raw_frame < 0 || seq->total_bones < 0) {
        return PSK3D_ERR_BAD_INDEX;
    }
    if (frame_index >= (psk3d_u32)seq->num_raw_frames) {
        return PSK3D_ERR_BAD_INDEX;
    }
    if (bone_index >= (psk3d_u32)seq->total_bones || bone_index >= animset->bone_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    abs_frame = (psk3d_u32)seq->first_raw_frame + frame_index;
    if (animset->bone_count != 0u && abs_frame > 0xffffffffu / animset->bone_count) {
        return PSK3D_ERR_OVERFLOW;
    }
    key_index = abs_frame * animset->bone_count + bone_index;
    if (key_index >= animset->key_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    *out_key = animset->keys[key_index];
    return PSK3D_OK;
}

static psk3d_result psk3d_get_scale_key(const psk3d_animset *animset,
                                        psk3d_u32 sequence_index,
                                        psk3d_u32 frame_index,
                                        psk3d_u32 bone_index,
                                        psk3d_anim_scale_key *out_key)
{
    const psk3d_anim_info *seq;
    psk3d_u32 abs_frame;
    psk3d_u32 key_index;
    if (animset == (const psk3d_animset *)0 || out_key == (psk3d_anim_scale_key *)0) {
        return PSK3D_ERR_NULL;
    }
    out_key->scale.x = PSK3D_Q16_ONE;
    out_key->scale.y = PSK3D_Q16_ONE;
    out_key->scale.z = PSK3D_Q16_ONE;
    out_key->time = 0;
    if (animset->scale_key_count == 0u) {
        return PSK3D_OK;
    }
    if (sequence_index >= animset->sequence_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    seq = &animset->sequences[sequence_index];
    if (seq->num_raw_frames < 0 || seq->first_raw_frame < 0 || seq->total_bones < 0) {
        return PSK3D_ERR_BAD_INDEX;
    }
    if (frame_index >= (psk3d_u32)seq->num_raw_frames) {
        return PSK3D_ERR_BAD_INDEX;
    }
    if (bone_index >= (psk3d_u32)seq->total_bones || bone_index >= animset->bone_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    abs_frame = (psk3d_u32)seq->first_raw_frame + frame_index;
    if (animset->bone_count != 0u && abs_frame > 0xffffffffu / animset->bone_count) {
        return PSK3D_ERR_OVERFLOW;
    }
    key_index = abs_frame * animset->bone_count + bone_index;
    if (key_index >= animset->scale_key_count) {
        return PSK3D_OK;
    }
    *out_key = animset->scale_keys[key_index];
    return PSK3D_OK;
}

psk3d_result psk3d_extract_sequence_track_range(const psk3d_animset *animset,
                                                psk3d_u32 sequence_index,
                                                psk3d_u32 bone_index,
                                                psk3d_u32 *out_first_key,
                                                psk3d_u32 *out_key_count)
{
    const psk3d_anim_info *seq;
    psk3d_u32 first_frame;
    if (animset == (const psk3d_animset *)0 || out_first_key == (psk3d_u32 *)0 || out_key_count == (psk3d_u32 *)0) {
        return PSK3D_ERR_NULL;
    }
    if (sequence_index >= animset->sequence_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    seq = &animset->sequences[sequence_index];
    if (seq->first_raw_frame < 0 || seq->num_raw_frames < 0 || seq->total_bones < 0) {
        return PSK3D_ERR_BAD_INDEX;
    }
    if (bone_index >= (psk3d_u32)seq->total_bones || bone_index >= animset->bone_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    first_frame = (psk3d_u32)seq->first_raw_frame;
    if (animset->bone_count != 0u && first_frame > 0xffffffffu / animset->bone_count) {
        return PSK3D_ERR_OVERFLOW;
    }
    *out_first_key = first_frame * animset->bone_count + bone_index;
    *out_key_count = (psk3d_u32)seq->num_raw_frames;
    if (*out_first_key >= animset->key_count || *out_key_count > animset->key_count - *out_first_key) {
        return PSK3D_ERR_BAD_INDEX;
    }
    return PSK3D_OK;
}

psk3d_result psk3d_make_bind_pose_from_model(const psk3d_model *model,
                                             psk3d_pose *out_pose)
{
    psk3d_u32 i;
    if (model == (const psk3d_model *)0 || out_pose == (psk3d_pose *)0) {
        return PSK3D_ERR_NULL;
    }
    psk3d_zero(out_pose, (psk3d_u32)sizeof(*out_pose));
    out_pose->bone_count = model->bone_count;
    for (i = 0u; i < model->bone_count; i++) {
        out_pose->positions[i] = model->bones[i].position;
        out_pose->orientations[i] = model->bones[i].orientation;
        out_pose->scales[i].x = PSK3D_Q16_ONE;
        out_pose->scales[i].y = PSK3D_Q16_ONE;
        out_pose->scales[i].z = PSK3D_Q16_ONE;
    }
    return PSK3D_OK;
}

psk3d_result psk3d_make_bind_pose_from_animset(const psk3d_animset *animset,
                                               psk3d_pose *out_pose)
{
    psk3d_u32 i;
    if (animset == (const psk3d_animset *)0 || out_pose == (psk3d_pose *)0) {
        return PSK3D_ERR_NULL;
    }
    psk3d_zero(out_pose, (psk3d_u32)sizeof(*out_pose));
    out_pose->bone_count = animset->bone_count;
    for (i = 0u; i < animset->bone_count; i++) {
        out_pose->positions[i] = animset->bones[i].position;
        out_pose->orientations[i] = animset->bones[i].orientation;
        out_pose->scales[i].x = PSK3D_Q16_ONE;
        out_pose->scales[i].y = PSK3D_Q16_ONE;
        out_pose->scales[i].z = PSK3D_Q16_ONE;
    }
    return PSK3D_OK;
}

psk3d_result psk3d_sample_sequence_nearest(const psk3d_animset *animset,
                                           psk3d_u32 sequence_index,
                                           psk3d_u32 frame_index,
                                           psk3d_pose *out_pose)
{
    psk3d_u32 i;
    psk3d_anim_key key;
    psk3d_anim_scale_key scale_key;
    psk3d_result r;
    const psk3d_anim_info *seq;
    if (animset == (const psk3d_animset *)0 || out_pose == (psk3d_pose *)0) {
        return PSK3D_ERR_NULL;
    }
    if (sequence_index >= animset->sequence_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    seq = &animset->sequences[sequence_index];
    if (seq->total_bones < 0) {
        return PSK3D_ERR_BAD_INDEX;
    }
    psk3d_zero(out_pose, (psk3d_u32)sizeof(*out_pose));
    out_pose->bone_count = (psk3d_u32)seq->total_bones;
    if (out_pose->bone_count > PSK3D_MAX_BONES) {
        return PSK3D_ERR_CAPACITY;
    }
    psk3d_pose_set_identity_scales(out_pose);
    for (i = 0u; i < out_pose->bone_count; i++) {
        r = psk3d_get_key(animset, sequence_index, frame_index, i, &key);
        if (r != PSK3D_OK) {
            return r;
        }
        r = psk3d_get_scale_key(animset, sequence_index, frame_index, i, &scale_key);
        if (r != PSK3D_OK) {
            return r;
        }
        out_pose->positions[i] = key.position;
        out_pose->orientations[i] = key.orientation;
        out_pose->scales[i] = scale_key.scale;
    }
    return PSK3D_OK;
}


psk3d_result psk3d_sample_sequence_linear_ex(const psk3d_animset *animset,
                                             psk3d_u32 sequence_index,
                                             psk3d_u32 frame_index,
                                             psk3d_i32 frame_t_q16,
                                             psk3d_rot_interp rot_interp,
                                             psk3d_pose *out_pose)
{
    psk3d_u32 i;
    psk3d_u32 next_frame;
    psk3d_anim_key a;
    psk3d_anim_key b;
    psk3d_anim_scale_key sa;
    psk3d_anim_scale_key sb;
    psk3d_result r;
    const psk3d_anim_info *seq;

    if (animset == (const psk3d_animset *)0 || out_pose == (psk3d_pose *)0) {
        return PSK3D_ERR_NULL;
    }
    if (sequence_index >= animset->sequence_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    seq = &animset->sequences[sequence_index];
    if (seq->total_bones < 0 || seq->num_raw_frames <= 0) {
        return PSK3D_ERR_BAD_INDEX;
    }
    if (frame_index >= (psk3d_u32)seq->num_raw_frames) {
        frame_index = (psk3d_u32)seq->num_raw_frames - 1u;
    }
    next_frame = frame_index;
    if (frame_index + 1u < (psk3d_u32)seq->num_raw_frames) {
        next_frame = frame_index + 1u;
    }
    if (frame_t_q16 < 0) {
        frame_t_q16 = 0;
    }
    if (frame_t_q16 > PSK3D_Q16_ONE) {
        frame_t_q16 = PSK3D_Q16_ONE;
    }

    psk3d_zero(out_pose, (psk3d_u32)sizeof(*out_pose));
    out_pose->bone_count = (psk3d_u32)seq->total_bones;
    if (out_pose->bone_count > PSK3D_MAX_BONES) {
        return PSK3D_ERR_CAPACITY;
    }
    psk3d_pose_set_identity_scales(out_pose);

    for (i = 0u; i < out_pose->bone_count; i++) {
        r = psk3d_get_key(animset, sequence_index, frame_index, i, &a);
        if (r != PSK3D_OK) {
            return r;
        }
        r = psk3d_get_key(animset, sequence_index, next_frame, i, &b);
        if (r != PSK3D_OK) {
            return r;
        }
        r = psk3d_get_scale_key(animset, sequence_index, frame_index, i, &sa);
        if (r != PSK3D_OK) {
            return r;
        }
        r = psk3d_get_scale_key(animset, sequence_index, next_frame, i, &sb);
        if (r != PSK3D_OK) {
            return r;
        }
        out_pose->positions[i].x = psk3d_q16_lerp(a.position.x, b.position.x, frame_t_q16);
        out_pose->positions[i].y = psk3d_q16_lerp(a.position.y, b.position.y, frame_t_q16);
        out_pose->positions[i].z = psk3d_q16_lerp(a.position.z, b.position.z, frame_t_q16);
        out_pose->scales[i].x = psk3d_q16_lerp(sa.scale.x, sb.scale.x, frame_t_q16);
        out_pose->scales[i].y = psk3d_q16_lerp(sa.scale.y, sb.scale.y, frame_t_q16);
        out_pose->scales[i].z = psk3d_q16_lerp(sa.scale.z, sb.scale.z, frame_t_q16);
        if (rot_interp == PSK3D_ROT_SLERP) {
            psk3d_quat_slerp(&a.orientation, &b.orientation, frame_t_q16, &out_pose->orientations[i]);
        } else {
            psk3d_quat_nlerp(&a.orientation, &b.orientation, frame_t_q16, &out_pose->orientations[i]);
        }
    }
    return PSK3D_OK;
}


psk3d_result psk3d_sample_sequence_linear(const psk3d_animset *animset,
                                          psk3d_u32 sequence_index,
                                          psk3d_u32 frame_index,
                                          psk3d_i32 frame_t_q16,
                                          psk3d_pose *out_pose)
{
    return psk3d_sample_sequence_linear_ex(animset,
                                           sequence_index,
                                           frame_index,
                                           frame_t_q16,
                                           PSK3D_ROT_NLERP,
                                           out_pose);
}

psk3d_result psk3d_sample_sequence_slerp(const psk3d_animset *animset,
                                         psk3d_u32 sequence_index,
                                         psk3d_u32 frame_index,
                                         psk3d_i32 frame_t_q16,
                                         psk3d_pose *out_pose)
{
    return psk3d_sample_sequence_linear_ex(animset,
                                           sequence_index,
                                           frame_index,
                                           frame_t_q16,
                                           PSK3D_ROT_SLERP,
                                           out_pose);
}


static psk3d_u32 psk3d_pose_common_count(const psk3d_pose *a, const psk3d_pose *b)
{
    if (a->bone_count < b->bone_count) {
        return a->bone_count;
    }
    return b->bone_count;
}

psk3d_result psk3d_pose_blend(const psk3d_pose *a,
                              const psk3d_pose *b,
                              psk3d_i32 weight_q16,
                              psk3d_rot_interp rot_interp,
                              psk3d_pose *out_pose)
{
    psk3d_u32 i;
    psk3d_u32 n;
    psk3d_pose rpose;
    if (a == (const psk3d_pose *)0 || b == (const psk3d_pose *)0 || out_pose == (psk3d_pose *)0) {
        return PSK3D_ERR_NULL;
    }
    if (weight_q16 < 0) {
        weight_q16 = 0;
    }
    if (weight_q16 > PSK3D_Q16_ONE) {
        weight_q16 = PSK3D_Q16_ONE;
    }
    n = psk3d_pose_common_count(a, b);
    if (n > PSK3D_MAX_BONES) {
        return PSK3D_ERR_CAPACITY;
    }
    psk3d_zero(&rpose, (psk3d_u32)sizeof(rpose));
    rpose.bone_count = n;
    psk3d_pose_set_identity_scales(&rpose);
    for (i = 0u; i < n; i++) {
        rpose.positions[i].x = psk3d_q16_lerp(a->positions[i].x, b->positions[i].x, weight_q16);
        rpose.positions[i].y = psk3d_q16_lerp(a->positions[i].y, b->positions[i].y, weight_q16);
        rpose.positions[i].z = psk3d_q16_lerp(a->positions[i].z, b->positions[i].z, weight_q16);
        rpose.scales[i].x = psk3d_q16_lerp(a->scales[i].x, b->scales[i].x, weight_q16);
        rpose.scales[i].y = psk3d_q16_lerp(a->scales[i].y, b->scales[i].y, weight_q16);
        rpose.scales[i].z = psk3d_q16_lerp(a->scales[i].z, b->scales[i].z, weight_q16);
        if (rot_interp == PSK3D_ROT_SLERP) {
            psk3d_quat_slerp(&a->orientations[i], &b->orientations[i], weight_q16, &rpose.orientations[i]);
        } else {
            psk3d_quat_nlerp(&a->orientations[i], &b->orientations[i], weight_q16, &rpose.orientations[i]);
        }
    }
    *out_pose = rpose;
    return PSK3D_OK;
}

psk3d_result psk3d_pose_apply_additive(const psk3d_pose *base_pose,
                                       const psk3d_pose *add_pose,
                                       psk3d_i32 weight_q16,
                                       psk3d_rot_interp rot_interp,
                                       psk3d_pose *inout_pose)
{
    psk3d_u32 i;
    psk3d_u32 n;
    psk3d_vec3 dpos;
    psk3d_vec3 dscale;
    psk3d_quat inv_base;
    psk3d_quat delta;
    psk3d_quat ident;
    psk3d_quat weighted;
    psk3d_quat result;
    if (base_pose == (const psk3d_pose *)0 || add_pose == (const psk3d_pose *)0 || inout_pose == (psk3d_pose *)0) {
        return PSK3D_ERR_NULL;
    }
    if (weight_q16 < 0) {
        weight_q16 = 0;
    }
    if (weight_q16 > PSK3D_Q16_ONE) {
        weight_q16 = PSK3D_Q16_ONE;
    }
    n = psk3d_pose_common_count(base_pose, add_pose);
    if (inout_pose->bone_count < n) {
        n = inout_pose->bone_count;
    }
    psk3d_quat_identity(&ident);
    for (i = 0u; i < n; i++) {
        dpos.x = psk3d_q16_sub_sat(add_pose->positions[i].x, base_pose->positions[i].x);
        dpos.y = psk3d_q16_sub_sat(add_pose->positions[i].y, base_pose->positions[i].y);
        dpos.z = psk3d_q16_sub_sat(add_pose->positions[i].z, base_pose->positions[i].z);
        inout_pose->positions[i].x = psk3d_q16_add_sat(inout_pose->positions[i].x, psk3d_q16_mul(dpos.x, weight_q16));
        inout_pose->positions[i].y = psk3d_q16_add_sat(inout_pose->positions[i].y, psk3d_q16_mul(dpos.y, weight_q16));
        inout_pose->positions[i].z = psk3d_q16_add_sat(inout_pose->positions[i].z, psk3d_q16_mul(dpos.z, weight_q16));

        dscale.x = psk3d_q16_sub_sat(add_pose->scales[i].x, base_pose->scales[i].x);
        dscale.y = psk3d_q16_sub_sat(add_pose->scales[i].y, base_pose->scales[i].y);
        dscale.z = psk3d_q16_sub_sat(add_pose->scales[i].z, base_pose->scales[i].z);
        inout_pose->scales[i].x = psk3d_q16_add_sat(inout_pose->scales[i].x, psk3d_q16_mul(dscale.x, weight_q16));
        inout_pose->scales[i].y = psk3d_q16_add_sat(inout_pose->scales[i].y, psk3d_q16_mul(dscale.y, weight_q16));
        inout_pose->scales[i].z = psk3d_q16_add_sat(inout_pose->scales[i].z, psk3d_q16_mul(dscale.z, weight_q16));

        psk3d_quat_conjugate(&base_pose->orientations[i], &inv_base);
        psk3d_quat_mul(&add_pose->orientations[i], &inv_base, &delta);
        psk3d_quat_normalize(&delta);
        if (rot_interp == PSK3D_ROT_SLERP) {
            psk3d_quat_slerp(&ident, &delta, weight_q16, &weighted);
        } else {
            psk3d_quat_nlerp(&ident, &delta, weight_q16, &weighted);
        }
        psk3d_quat_mul(&weighted, &inout_pose->orientations[i], &result);
        psk3d_quat_normalize(&result);
        inout_pose->orientations[i] = result;
    }
    return PSK3D_OK;
}

void psk3d_clear_anim_stack(psk3d_anim_stack *stack)
{
    if (stack != (psk3d_anim_stack *)0) {
        psk3d_zero(stack, (psk3d_u32)sizeof(*stack));
    }
}

psk3d_result psk3d_anim_stack_push(psk3d_anim_stack *stack,
                                   psk3d_u32 sequence_index,
                                   psk3d_u32 frame_index,
                                   psk3d_i32 frame_t_q16,
                                   psk3d_i32 weight_q16,
                                   psk3d_rot_interp rot_interp,
                                   psk3d_blend_mode blend_mode)
{
    psk3d_anim_layer *layer;
    if (stack == (psk3d_anim_stack *)0) {
        return PSK3D_ERR_NULL;
    }
    if (stack->layer_count >= PSK3D_MAX_ANIM_LAYERS) {
        return PSK3D_ERR_CAPACITY;
    }
    if (weight_q16 < 0) {
        weight_q16 = 0;
    }
    if (weight_q16 > PSK3D_Q16_ONE) {
        weight_q16 = PSK3D_Q16_ONE;
    }
    layer = &stack->layers[stack->layer_count];
    layer->enabled = 1u;
    layer->sequence_index = sequence_index;
    layer->frame_index = frame_index;
    layer->frame_t_q16 = frame_t_q16;
    layer->weight_q16 = weight_q16;
    layer->rot_interp = rot_interp;
    layer->blend_mode = blend_mode;
    stack->layer_count++;
    return PSK3D_OK;
}

psk3d_result psk3d_sample_anim_stack(const psk3d_animset *animset,
                                     const psk3d_anim_stack *stack,
                                     const psk3d_pose *base_pose,
                                     psk3d_pose *out_pose)
{
    psk3d_u32 i;
    psk3d_u32 started;
    psk3d_pose layer_pose;
    psk3d_pose blended;
    psk3d_pose identity_base;
    psk3d_result r;
    const psk3d_pose *add_base;
    if (animset == (const psk3d_animset *)0 || stack == (const psk3d_anim_stack *)0 || out_pose == (psk3d_pose *)0) {
        return PSK3D_ERR_NULL;
    }
    psk3d_zero(&identity_base, (psk3d_u32)sizeof(identity_base));
    identity_base.bone_count = animset->bone_count;
    if (identity_base.bone_count > PSK3D_MAX_BONES) {
        return PSK3D_ERR_CAPACITY;
    }
    psk3d_pose_set_identity_scales(&identity_base);
    for (i = 0u; i < identity_base.bone_count; i++) {
        psk3d_quat_identity(&identity_base.orientations[i]);
    }
    add_base = base_pose;
    if (add_base == (const psk3d_pose *)0) {
        add_base = &identity_base;
    }
    started = 0u;
    psk3d_zero(out_pose, (psk3d_u32)sizeof(*out_pose));
    for (i = 0u; i < stack->layer_count; i++) {
        if (stack->layers[i].enabled == 0u || stack->layers[i].weight_q16 == 0) {
            continue;
        }
        r = psk3d_sample_sequence_linear_ex(animset,
                                            stack->layers[i].sequence_index,
                                            stack->layers[i].frame_index,
                                            stack->layers[i].frame_t_q16,
                                            stack->layers[i].rot_interp,
                                            &layer_pose);
        if (r != PSK3D_OK) {
            return r;
        }
        if (started == 0u) {
            if (base_pose != (const psk3d_pose *)0) {
                *out_pose = *base_pose;
                r = psk3d_pose_blend(out_pose, &layer_pose, stack->layers[i].weight_q16, stack->layers[i].rot_interp, &blended);
                if (r != PSK3D_OK) {
                    return r;
                }
                *out_pose = blended;
            } else {
                *out_pose = layer_pose;
                if (stack->layers[i].weight_q16 != PSK3D_Q16_ONE) {
                    r = psk3d_pose_blend(&identity_base, &layer_pose, stack->layers[i].weight_q16, stack->layers[i].rot_interp, &blended);
                    if (r != PSK3D_OK) {
                        return r;
                    }
                    *out_pose = blended;
                }
            }
            started = 1u;
        } else if (stack->layers[i].blend_mode == PSK3D_BLEND_ADDITIVE) {
            r = psk3d_pose_apply_additive(add_base, &layer_pose, stack->layers[i].weight_q16, stack->layers[i].rot_interp, out_pose);
            if (r != PSK3D_OK) {
                return r;
            }
        } else {
            r = psk3d_pose_blend(out_pose, &layer_pose, stack->layers[i].weight_q16, stack->layers[i].rot_interp, &blended);
            if (r != PSK3D_OK) {
                return r;
            }
            *out_pose = blended;
        }
    }
    if (started == 0u) {
        if (base_pose != (const psk3d_pose *)0) {
            *out_pose = *base_pose;
            return PSK3D_OK;
        }
        return PSK3D_ERR_BAD_INDEX;
    }
    return PSK3D_OK;
}


void psk3d_clear_morph_weights(psk3d_morph_weights *weights)
{
    if (weights != (psk3d_morph_weights *)0) {
        psk3d_zero(weights, (psk3d_u32)sizeof(*weights));
    }
}

psk3d_result psk3d_set_morph_weight(psk3d_morph_weights *weights,
                                     const psk3d_model *model,
                                     psk3d_u32 target_index,
                                     psk3d_i32 weight_q16)
{
    if (weights == (psk3d_morph_weights *)0 || model == (const psk3d_model *)0) {
        return PSK3D_ERR_NULL;
    }
    if (target_index >= model->morph_target_count || target_index >= PSK3D_MAX_MORPH_TARGETS) {
        return PSK3D_ERR_BAD_INDEX;
    }
    if (weights->target_count < model->morph_target_count) {
        weights->target_count = model->morph_target_count;
    }
    weights->weights[target_index] = weight_q16;
    return PSK3D_OK;
}

psk3d_i32 psk3d_find_morph_target(const psk3d_model *model,
                                   const char *name)
{
    psk3d_u32 i;
    if (model == (const psk3d_model *)0 || name == (const char *)0) {
        return -1;
    }
    for (i = 0u; i < model->morph_target_count; i++) {
        if (strcmp(model->morph_targets[i].name, name) == 0) {
            return (psk3d_i32)i;
        }
    }
    return -1;
}

void psk3d_clear_morph_stack(psk3d_morph_stack *stack)
{
    if (stack != (psk3d_morph_stack *)0) {
        psk3d_zero(stack, (psk3d_u32)sizeof(*stack));
    }
}

psk3d_result psk3d_morph_stack_push(psk3d_morph_stack *stack,
                                     psk3d_u32 target_index,
                                     psk3d_i32 weight_q16)
{
    if (stack == (psk3d_morph_stack *)0) {
        return PSK3D_ERR_NULL;
    }
    if (stack->layer_count >= PSK3D_MAX_MORPH_LAYERS) {
        return PSK3D_ERR_CAPACITY;
    }
    stack->layers[stack->layer_count].target_index = target_index;
    stack->layers[stack->layer_count].weight = weight_q16;
    stack->layer_count++;
    return PSK3D_OK;
}

static psk3d_result psk3d_apply_one_morph_target(const psk3d_model *model,
                                                  psk3d_u32 target_index,
                                                  psk3d_i32 weight_q16,
                                                  psk3d_skinned_points *out_points)
{
    const psk3d_morph_info *info;
    psk3d_u32 i;
    psk3d_u32 idx;
    psk3d_u32 p;
    psk3d_vec3 delta;

    if (model == (const psk3d_model *)0 || out_points == (psk3d_skinned_points *)0) {
        return PSK3D_ERR_NULL;
    }
    if (target_index >= model->morph_target_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    if (weight_q16 == 0) {
        return PSK3D_OK;
    }
    info = &model->morph_targets[target_index];
    if (info->data_offset > model->morph_delta_count || info->vertex_count > model->morph_delta_count - info->data_offset) {
        return PSK3D_ERR_BAD_INDEX;
    }
    for (i = 0u; i < info->vertex_count; i++) {
        idx = info->data_offset + i;
        p = model->morph_deltas[idx].point_index;
        if (p >= out_points->point_count) {
            return PSK3D_ERR_BAD_INDEX;
        }
        delta.x = psk3d_q16_mul(model->morph_deltas[idx].position_delta.x, weight_q16);
        delta.y = psk3d_q16_mul(model->morph_deltas[idx].position_delta.y, weight_q16);
        delta.z = psk3d_q16_mul(model->morph_deltas[idx].position_delta.z, weight_q16);
        psk3d_vec3_add(&out_points->points[p], &delta, &out_points->points[p]);
    }
    return PSK3D_OK;
}

psk3d_result psk3d_apply_shape_keys_points(const psk3d_model *model,
                                           const psk3d_morph_weights *weights,
                                           psk3d_skinned_points *out_points)
{
    psk3d_u32 i;
    psk3d_result r;
    if (model == (const psk3d_model *)0 || weights == (const psk3d_morph_weights *)0 || out_points == (psk3d_skinned_points *)0) {
        return PSK3D_ERR_NULL;
    }
    if (model->point_count > PSK3D_MAX_POINTS || model->morph_target_count > PSK3D_MAX_MORPH_TARGETS) {
        return PSK3D_ERR_CAPACITY;
    }
    psk3d_zero(out_points, (psk3d_u32)sizeof(*out_points));
    out_points->point_count = model->point_count;
    for (i = 0u; i < model->point_count; i++) {
        out_points->points[i] = model->points[i].p;
    }
    for (i = 0u; i < model->morph_target_count; i++) {
        r = psk3d_apply_one_morph_target(model, i, weights->weights[i], out_points);
        if (r != PSK3D_OK) {
            return r;
        }
    }
    return PSK3D_OK;
}

psk3d_result psk3d_apply_shape_key_stack_points(const psk3d_model *model,
                                                const psk3d_morph_stack *stack,
                                                psk3d_skinned_points *out_points)
{
    psk3d_u32 i;
    psk3d_result r;
    if (model == (const psk3d_model *)0 || stack == (const psk3d_morph_stack *)0 || out_points == (psk3d_skinned_points *)0) {
        return PSK3D_ERR_NULL;
    }
    psk3d_zero(out_points, (psk3d_u32)sizeof(*out_points));
    out_points->point_count = model->point_count;
    for (i = 0u; i < model->point_count; i++) {
        out_points->points[i] = model->points[i].p;
    }
    for (i = 0u; i < stack->layer_count; i++) {
        r = psk3d_apply_one_morph_target(model, stack->layers[i].target_index, stack->layers[i].weight, out_points);
        if (r != PSK3D_OK) {
            return r;
        }
    }
    return PSK3D_OK;
}


psk3d_result psk3d_build_bone_map(const psk3d_model *model,
                                  const psk3d_animset *animset,
                                  psk3d_u32 flags,
                                  psk3d_bone_map *out_map)
{
    psk3d_u32 i;
    psk3d_u32 j;
    int found;
    int insensitive;

    if (model == (const psk3d_model *)0 || animset == (const psk3d_animset *)0 || out_map == (psk3d_bone_map *)0) {
        return PSK3D_ERR_NULL;
    }
    if (model->bone_count > PSK3D_MAX_BONES || animset->bone_count > PSK3D_MAX_BONES) {
        return PSK3D_ERR_CAPACITY;
    }

    psk3d_zero(out_map, (psk3d_u32)sizeof(*out_map));
    out_map->model_bone_count = model->bone_count;
    out_map->anim_bone_count = animset->bone_count;
    insensitive = ((flags & PSK3D_BONEMAP_CASE_INSENSITIVE) != 0u);

    for (i = 0u; i < PSK3D_MAX_BONES; i++) {
        out_map->anim_index_for_model_bone[i] = PSK3D_U32_INVALID;
    }

    for (i = 0u; i < model->bone_count; i++) {
        found = 0;
        for (j = 0u; j < animset->bone_count; j++) {
            if (psk3d_name_eq(model->bones[i].name, animset->bones[j].name, insensitive)) {
                out_map->anim_index_for_model_bone[i] = j;
                found = 1;
                break;
            }
        }
        if (!found && i == 0u && (flags & PSK3D_BONEMAP_ROOT_FALLBACK) != 0u && animset->bone_count > 0u) {
            out_map->anim_index_for_model_bone[i] = 0u;
            found = 1;
        }
        if (!found) {
            out_map->missing_count++;
            if ((flags & PSK3D_BONEMAP_ALLOW_MISSING) == 0u) {
                return PSK3D_ERR_BAD_INDEX;
            }
        }
    }
    return PSK3D_OK;
}

psk3d_result psk3d_sample_sequence_mapped_ex(const psk3d_model *model,
                                             const psk3d_animset *animset,
                                             const psk3d_bone_map *map,
                                             psk3d_u32 sequence_index,
                                             psk3d_u32 frame_index,
                                             psk3d_i32 frame_t_q16,
                                             psk3d_rot_interp rot_interp,
                                             psk3d_pose *out_pose)
{
    psk3d_pose anim_pose;
    psk3d_u32 i;
    psk3d_u32 ai;
    psk3d_result r;

    if (model == (const psk3d_model *)0 || animset == (const psk3d_animset *)0 ||
        map == (const psk3d_bone_map *)0 || out_pose == (psk3d_pose *)0) {
        return PSK3D_ERR_NULL;
    }
    if (map->model_bone_count != model->bone_count || map->anim_bone_count != animset->bone_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    if (model->bone_count > PSK3D_MAX_BONES) {
        return PSK3D_ERR_CAPACITY;
    }

    r = psk3d_sample_sequence_linear_ex(animset, sequence_index, frame_index, frame_t_q16, rot_interp, &anim_pose);
    if (r != PSK3D_OK) {
        return r;
    }

    psk3d_zero(out_pose, (psk3d_u32)sizeof(*out_pose));
    out_pose->bone_count = model->bone_count;
    psk3d_pose_set_identity_scales(out_pose);
    for (i = 0u; i < model->bone_count; i++) {
        ai = map->anim_index_for_model_bone[i];
        if (ai != PSK3D_U32_INVALID && ai < anim_pose.bone_count) {
            out_pose->positions[i] = anim_pose.positions[ai];
            out_pose->orientations[i] = anim_pose.orientations[ai];
            out_pose->scales[i] = anim_pose.scales[ai];
        } else {
            out_pose->positions[i] = model->bones[i].position;
            out_pose->orientations[i] = model->bones[i].orientation;
            out_pose->scales[i].x = PSK3D_Q16_ONE;
            out_pose->scales[i].y = PSK3D_Q16_ONE;
            out_pose->scales[i].z = PSK3D_Q16_ONE;
        }
    }
    return PSK3D_OK;
}

psk3d_result psk3d_sample_sequence_mapped_scaled_ex(const psk3d_model *model,
                                                    const psk3d_animset *animset,
                                                    const psk3d_bone_map *map,
                                                    psk3d_u32 sequence_index,
                                                    psk3d_u32 frame_index,
                                                    psk3d_i32 frame_t_q16,
                                                    psk3d_rot_interp rot_interp,
                                                    psk3d_pose *out_pose)
{
    return psk3d_sample_sequence_mapped_ex(model,
                                           animset,
                                           map,
                                           sequence_index,
                                           frame_index,
                                           frame_t_q16,
                                           rot_interp,
                                           out_pose);
}


void psk3d_cursor_reset(psk3d_anim_cursor *cursor, psk3d_u32 sequence_index)
{
    if (cursor == (psk3d_anim_cursor *)0) {
        return;
    }
    psk3d_zero(cursor, (psk3d_u32)sizeof(*cursor));
    cursor->sequence_index = sequence_index;
}

psk3d_result psk3d_cursor_advance(const psk3d_animset *animset,
                                  psk3d_anim_cursor *cursor,
                                  psk3d_i32 delta_seconds_q16,
                                  psk3d_u32 loop)
{
    const psk3d_anim_info *seq;
    psk3d_i32 rate;
    psk3d_i32 step;
    psk3d_i32 total;
    psk3d_u32 add_frames;
    psk3d_u32 frames;

    if (animset == (const psk3d_animset *)0 || cursor == (psk3d_anim_cursor *)0) {
        return PSK3D_ERR_NULL;
    }
    if (cursor->sequence_index >= animset->sequence_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    seq = &animset->sequences[cursor->sequence_index];
    if (seq->num_raw_frames <= 0) {
        return PSK3D_ERR_BAD_INDEX;
    }
    if (delta_seconds_q16 < 0) {
        return PSK3D_ERR_UNSUPPORTED;
    }

    frames = (psk3d_u32)seq->num_raw_frames;
    rate = seq->anim_rate;
    if (rate <= 0) {
        rate = 30 * PSK3D_Q16_ONE;
    }
    step = psk3d_q16_mul(rate, delta_seconds_q16);
    total = psk3d_q16_add_sat(cursor->frame_t_q16, step);
    if (total < 0) {
        total = 0;
    }
    add_frames = (psk3d_u32)((psk3d_u32)total >> 16);
    cursor->frame_t_q16 = (psk3d_i32)(total & 65535);

    while (add_frames > 0u) {
        if (cursor->frame_index + 1u < frames) {
            cursor->frame_index++;
        } else {
            if (loop != 0u) {
                cursor->frame_index = 0u;
                cursor->looped++;
            } else {
                cursor->frame_index = frames - 1u;
                cursor->frame_t_q16 = 0;
                cursor->done = 1u;
                return PSK3D_OK;
            }
        }
        add_frames--;
    }
    return PSK3D_OK;
}

psk3d_result psk3d_globalize_pose(const psk3d_bone *bones,
                                  psk3d_u32 bone_count,
                                  const psk3d_pose *local_pose,
                                  psk3d_pose *out_global_pose)
{
    psk3d_u32 i;
    psk3d_u32 parent;
    psk3d_vec3 rotated;
    psk3d_vec3 local_scale;
    psk3d_vec3 parent_scale;
    psk3d_vec3 scaled_pos;
    psk3d_quat q;

    if (bones == (const psk3d_bone *)0 || local_pose == (const psk3d_pose *)0 || out_global_pose == (psk3d_pose *)0) {
        return PSK3D_ERR_NULL;
    }
    if (bone_count > PSK3D_MAX_BONES || local_pose->bone_count < bone_count) {
        return PSK3D_ERR_CAPACITY;
    }
    psk3d_zero(out_global_pose, (psk3d_u32)sizeof(*out_global_pose));
    out_global_pose->bone_count = bone_count;
    for (i = 0u; i < bone_count; i++) {
        q = local_pose->orientations[i];
        psk3d_quat_normalize(&q);
        psk3d_vec3_get_scale_or_one(&local_pose->scales[i], &local_scale);
        if (i == 0u || bones[i].parent_index < 0 || (psk3d_u32)bones[i].parent_index == i) {
            out_global_pose->positions[i] = local_pose->positions[i];
            out_global_pose->orientations[i] = q;
            out_global_pose->scales[i] = local_scale;
        } else {
            parent = (psk3d_u32)bones[i].parent_index;
            if (parent >= i || parent >= bone_count) {
                return PSK3D_ERR_BAD_INDEX;
            }
            psk3d_vec3_get_scale_or_one(&out_global_pose->scales[parent], &parent_scale);
            psk3d_vec3_component_mul(&local_pose->positions[i], &parent_scale, &scaled_pos);
            psk3d_quat_rotate_vec3(&out_global_pose->orientations[parent], &scaled_pos, &rotated);
            psk3d_vec3_add(&out_global_pose->positions[parent], &rotated, &out_global_pose->positions[i]);
            psk3d_quat_mul(&out_global_pose->orientations[parent], &q, &out_global_pose->orientations[i]);
            psk3d_quat_normalize(&out_global_pose->orientations[i]);
            psk3d_vec3_component_mul(&parent_scale, &local_scale, &out_global_pose->scales[i]);
        }
    }
    return PSK3D_OK;
}


psk3d_result psk3d_skin_points_lbs_global(const psk3d_model *model,
                                          const psk3d_pose *bind_global_pose,
                                          const psk3d_pose *current_global_pose,
                                          psk3d_skinned_points *out_points)
{
    psk3d_u32 p;
    psk3d_u32 i;
    psk3d_i32 sum;
    psk3d_i32 w;
    psk3d_vec3 transformed;
    psk3d_vec3 acc;
    int used;

    if (model == (const psk3d_model *)0 || bind_global_pose == (const psk3d_pose *)0 ||
        current_global_pose == (const psk3d_pose *)0 || out_points == (psk3d_skinned_points *)0) {
        return PSK3D_ERR_NULL;
    }
    if (bind_global_pose->bone_count < model->bone_count || current_global_pose->bone_count < model->bone_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    psk3d_zero(out_points, (psk3d_u32)sizeof(*out_points));
    out_points->point_count = model->point_count;

    for (p = 0u; p < model->point_count; p++) {
        sum = 0;
        used = 0;
        for (i = 0u; i < model->influence_count; i++) {
            if (model->influences[i].point_index == p && model->influences[i].weight > 0) {
                sum = psk3d_q16_add_sat(sum, model->influences[i].weight);
                used = 1;
            }
        }
        if (!used || sum <= 0) {
            out_points->points[p] = model->points[p].p;
        } else {
            acc.x = 0;
            acc.y = 0;
            acc.z = 0;
            for (i = 0u; i < model->influence_count; i++) {
                if (model->influences[i].point_index == p && model->influences[i].weight > 0) {
                    if (model->influences[i].bone_index >= model->bone_count) {
                        return PSK3D_ERR_BAD_INDEX;
                    }
                    if (sum == PSK3D_Q16_ONE) {
                        w = model->influences[i].weight;
                    } else {
                        w = psk3d_q16_div(model->influences[i].weight, sum);
                    }
                    psk3d_transform_point_delta_scaled(bind_global_pose,
                                                       current_global_pose,
                                                model->influences[i].bone_index,
                                                &model->points[p].p,
                                                &transformed);
                    psk3d_vec3_scale_accum(&acc, &transformed, w);
                }
            }
            out_points->points[p] = acc;
        }
    }
    return PSK3D_OK;
}


psk3d_result psk3d_build_skin_cache(const psk3d_model *model,
                                    psk3d_skin_cache *out_cache)
{
    psk3d_pose bind_local;
    psk3d_u32 i;
    psk3d_u32 p;
    psk3d_result r;

    if (model == (const psk3d_model *)0 || out_cache == (psk3d_skin_cache *)0) {
        return PSK3D_ERR_NULL;
    }
    if (model->point_count > PSK3D_MAX_POINTS ||
        model->bone_count > PSK3D_MAX_BONES ||
        model->influence_count > PSK3D_MAX_INFLUENCES) {
        return PSK3D_ERR_CAPACITY;
    }

    psk3d_zero(out_cache, (psk3d_u32)sizeof(*out_cache));
    out_cache->point_count = model->point_count;
    out_cache->bone_count = model->bone_count;
    out_cache->influence_count = model->influence_count;

    for (p = 0u; p < PSK3D_MAX_POINTS; p++) {
        out_cache->first_influence[p] = PSK3D_U32_INVALID;
        out_cache->weight_sum[p] = 0;
    }
    for (i = 0u; i < PSK3D_MAX_INFLUENCES; i++) {
        out_cache->next_influence[i] = PSK3D_U32_INVALID;
    }

    r = psk3d_make_bind_pose_from_model(model, &bind_local);
    if (r != PSK3D_OK) {
        return r;
    }
    r = psk3d_globalize_pose(model->bones, model->bone_count, &bind_local, &out_cache->bind_global_pose);
    if (r != PSK3D_OK) {
        return r;
    }

    for (i = model->influence_count; i > 0u; i--) {
        psk3d_u32 idx;
        idx = i - 1u;
        if (model->influences[idx].point_index >= model->point_count ||
            model->influences[idx].bone_index >= model->bone_count) {
            return PSK3D_ERR_BAD_INDEX;
        }
        p = model->influences[idx].point_index;
        if (model->influences[idx].weight > 0) {
            out_cache->next_influence[idx] = out_cache->first_influence[p];
            out_cache->first_influence[p] = idx;
            out_cache->weight_sum[p] = psk3d_q16_add_sat(out_cache->weight_sum[p], model->influences[idx].weight);
        }
    }
    return PSK3D_OK;
}

psk3d_result psk3d_skin_points_lbs_cached(const psk3d_model *model,
                                          const psk3d_skin_cache *cache,
                                          const psk3d_pose *current_global_pose,
                                          psk3d_skinned_points *out_points)
{
    psk3d_u32 p;
    psk3d_u32 i;
    psk3d_i32 sum;
    psk3d_i32 w;
    psk3d_vec3 transformed;
    psk3d_vec3 acc;

    if (model == (const psk3d_model *)0 || cache == (const psk3d_skin_cache *)0 ||
        current_global_pose == (const psk3d_pose *)0 || out_points == (psk3d_skinned_points *)0) {
        return PSK3D_ERR_NULL;
    }
    if (cache->point_count != model->point_count ||
        cache->bone_count != model->bone_count ||
        cache->influence_count != model->influence_count ||
        current_global_pose->bone_count < model->bone_count ||
        cache->bind_global_pose.bone_count < model->bone_count) {
        return PSK3D_ERR_BAD_INDEX;
    }

    psk3d_zero(out_points, (psk3d_u32)sizeof(*out_points));
    out_points->point_count = model->point_count;

    for (p = 0u; p < model->point_count; p++) {
        sum = cache->weight_sum[p];
        if (cache->first_influence[p] == PSK3D_U32_INVALID || sum <= 0) {
            out_points->points[p] = model->points[p].p;
        } else {
            acc.x = 0;
            acc.y = 0;
            acc.z = 0;
            i = cache->first_influence[p];
            while (i != PSK3D_U32_INVALID) {
                if (i >= model->influence_count) {
                    return PSK3D_ERR_BAD_INDEX;
                }
                if (model->influences[i].bone_index >= model->bone_count) {
                    return PSK3D_ERR_BAD_INDEX;
                }
                if (sum == PSK3D_Q16_ONE) {
                    w = model->influences[i].weight;
                } else {
                    w = psk3d_q16_div(model->influences[i].weight, sum);
                }
                psk3d_transform_point_delta_scaled(&cache->bind_global_pose,
                                                   current_global_pose,
                                            model->influences[i].bone_index,
                                            &model->points[p].p,
                                            &transformed);
                psk3d_vec3_scale_accum(&acc, &transformed, w);
                i = cache->next_influence[i];
            }
            out_points->points[p] = acc;
        }
    }
    return PSK3D_OK;
}


psk3d_result psk3d_skin_points_lbs_cached_from_points(const psk3d_model *model,
                                                     const psk3d_skin_cache *cache,
                                                     const psk3d_pose *current_global_pose,
                                                     const psk3d_skinned_points *source_points,
                                                     psk3d_skinned_points *out_points)
{
    psk3d_u32 p;
    psk3d_u32 i;
    psk3d_i32 sum;
    psk3d_i32 w;
    psk3d_vec3 transformed;
    psk3d_vec3 acc;

    if (model == (const psk3d_model *)0 || cache == (const psk3d_skin_cache *)0 ||
        current_global_pose == (const psk3d_pose *)0 || source_points == (const psk3d_skinned_points *)0 ||
        out_points == (psk3d_skinned_points *)0) {
        return PSK3D_ERR_NULL;
    }
    if (cache->point_count != model->point_count ||
        source_points->point_count < model->point_count ||
        cache->bone_count != model->bone_count ||
        cache->influence_count != model->influence_count ||
        current_global_pose->bone_count < model->bone_count ||
        cache->bind_global_pose.bone_count < model->bone_count) {
        return PSK3D_ERR_BAD_INDEX;
    }

    psk3d_zero(out_points, (psk3d_u32)sizeof(*out_points));
    out_points->point_count = model->point_count;

    for (p = 0u; p < model->point_count; p++) {
        sum = cache->weight_sum[p];
        if (cache->first_influence[p] == PSK3D_U32_INVALID || sum <= 0) {
            out_points->points[p] = source_points->points[p];
        } else {
            acc.x = 0;
            acc.y = 0;
            acc.z = 0;
            i = cache->first_influence[p];
            while (i != PSK3D_U32_INVALID) {
                if (i >= model->influence_count) {
                    return PSK3D_ERR_BAD_INDEX;
                }
                if (model->influences[i].bone_index >= model->bone_count) {
                    return PSK3D_ERR_BAD_INDEX;
                }
                if (sum == PSK3D_Q16_ONE) {
                    w = model->influences[i].weight;
                } else {
                    w = psk3d_q16_div(model->influences[i].weight, sum);
                }
                psk3d_transform_point_delta_scaled(&cache->bind_global_pose,
                                                   current_global_pose,
                                                   model->influences[i].bone_index,
                                                   &source_points->points[p],
                                                   &transformed);
                psk3d_vec3_scale_accum(&acc, &transformed, w);
                i = cache->next_influence[i];
            }
            out_points->points[p] = acc;
        }
    }
    return PSK3D_OK;
}

psk3d_result psk3d_skin_points_lbs_cached_morphed(const psk3d_model *model,
                                                  const psk3d_skin_cache *cache,
                                                  const psk3d_pose *current_global_pose,
                                                  const psk3d_morph_weights *weights,
                                                  psk3d_skinned_points *out_points)
{
    psk3d_skinned_points morphed;
    psk3d_result r;
    if (model == (const psk3d_model *)0 || cache == (const psk3d_skin_cache *)0 ||
        current_global_pose == (const psk3d_pose *)0 || weights == (const psk3d_morph_weights *)0 ||
        out_points == (psk3d_skinned_points *)0) {
        return PSK3D_ERR_NULL;
    }
    r = psk3d_apply_shape_keys_points(model, weights, &morphed);
    if (r != PSK3D_OK) {
        return r;
    }
    return psk3d_skin_points_lbs_cached_from_points(model, cache, current_global_pose, &morphed, out_points);
}



psk3d_result psk3d_skin_normals_lbs_cached(const psk3d_model *model,
                                           const psk3d_skin_cache *cache,
                                           const psk3d_pose *current_global_pose,
                                           psk3d_skinned_normals *out_normals)
{
    psk3d_u32 n;
    psk3d_u32 p;
    psk3d_u32 i;
    psk3d_i32 sum;
    psk3d_i32 w;
    psk3d_vec3 transformed;
    psk3d_vec3 acc;

    if (model == (const psk3d_model *)0 || cache == (const psk3d_skin_cache *)0 ||
        current_global_pose == (const psk3d_pose *)0 || out_normals == (psk3d_skinned_normals *)0) {
        return PSK3D_ERR_NULL;
    }
    if (cache->point_count != model->point_count ||
        cache->bone_count != model->bone_count ||
        cache->influence_count != model->influence_count ||
        current_global_pose->bone_count < model->bone_count ||
        cache->bind_global_pose.bone_count < model->bone_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    psk3d_zero(out_normals, (psk3d_u32)sizeof(*out_normals));
    out_normals->normal_count = model->normal_count;

    for (n = 0u; n < model->normal_count; n++) {
        if (model->normal_count == model->point_count) {
            p = n;
        } else {
            if (n >= model->wedge_count) {
                return PSK3D_ERR_BAD_INDEX;
            }
            p = model->wedges[n].point_index;
        }
        if (p >= model->point_count) {
            return PSK3D_ERR_BAD_INDEX;
        }
        sum = cache->weight_sum[p];
        if (cache->first_influence[p] == PSK3D_U32_INVALID || sum <= 0) {
            out_normals->normals[n] = model->normals[n];
        } else {
            acc.x = 0;
            acc.y = 0;
            acc.z = 0;
            i = cache->first_influence[p];
            while (i != PSK3D_U32_INVALID) {
                if (i >= model->influence_count || model->influences[i].bone_index >= model->bone_count) {
                    return PSK3D_ERR_BAD_INDEX;
                }
                if (sum == PSK3D_Q16_ONE) {
                    w = model->influences[i].weight;
                } else {
                    w = psk3d_q16_div(model->influences[i].weight, sum);
                }
                psk3d_transform_normal_delta(&cache->bind_global_pose,
                                             current_global_pose,
                                             model->influences[i].bone_index,
                                             &model->normals[n],
                                             &transformed);
                psk3d_vec3_scale_accum(&acc, &transformed, w);
                i = cache->next_influence[i];
            }
            psk3d_vec3_normalize(&acc);
            out_normals->normals[n] = acc;
        }
    }
    return PSK3D_OK;
}

psk3d_result psk3d_skin_points_lbs(const psk3d_model *model,
                                   const psk3d_pose *current_local_pose,
                                   psk3d_skinned_points *out_points)
{
    psk3d_pose bind_local;
    psk3d_pose bind_global;
    psk3d_pose current_global;
    psk3d_result r;

    if (model == (const psk3d_model *)0 || current_local_pose == (const psk3d_pose *)0 || out_points == (psk3d_skinned_points *)0) {
        return PSK3D_ERR_NULL;
    }
    r = psk3d_make_bind_pose_from_model(model, &bind_local);
    if (r != PSK3D_OK) {
        return r;
    }
    r = psk3d_globalize_pose(model->bones, model->bone_count, &bind_local, &bind_global);
    if (r != PSK3D_OK) {
        return r;
    }
    r = psk3d_globalize_pose(model->bones, model->bone_count, current_local_pose, &current_global);
    if (r != PSK3D_OK) {
        return r;
    }
    return psk3d_skin_points_lbs_global(model, &bind_global, &current_global, out_points);
}

psk3d_result psk3d_build_triangle_stream(const psk3d_model *model,
                                         const psk3d_skinned_points *points,
                                         const psk3d_skinned_normals *normals,
                                         psk3d_triangle_stream *out_stream)
{
    psk3d_u32 f;
    psk3d_u32 c;
    psk3d_u32 out_i;
    psk3d_u32 widx;
    psk3d_u32 pidx;
    psk3d_final_vertex *v;
    if (model == (const psk3d_model *)0 || points == (const psk3d_skinned_points *)0 || out_stream == (psk3d_triangle_stream *)0) {
        return PSK3D_ERR_NULL;
    }
    if (model->face_count > PSK3D_MAX_TRI_VERTS / 3u) {
        return PSK3D_ERR_CAPACITY;
    }
    if (points->point_count < model->point_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    if (normals != (const psk3d_skinned_normals *)0 && normals->normal_count < model->wedge_count) {
        return PSK3D_ERR_BAD_INDEX;
    }
    psk3d_zero(out_stream, (psk3d_u32)sizeof(*out_stream));
    out_stream->triangle_count = model->face_count;
    out_stream->vertex_count = model->face_count * 3u;
    out_i = 0u;
    for (f = 0u; f < model->face_count; f++) {
        for (c = 0u; c < 3u; c++) {
            widx = model->faces[f].wedge[c];
            if (widx >= model->wedge_count) {
                return PSK3D_ERR_BAD_INDEX;
            }
            pidx = model->wedges[widx].point_index;
            if (pidx >= model->point_count || pidx >= points->point_count) {
                return PSK3D_ERR_BAD_INDEX;
            }
            v = &out_stream->vertices[out_i];
            v->position = points->points[pidx];
            if (normals != (const psk3d_skinned_normals *)0 && widx < normals->normal_count) {
                v->normal = normals->normals[widx];
            } else if (widx < model->normal_count) {
                v->normal = model->normals[widx];
            } else {
                v->normal.x = 0;
                v->normal.y = 0;
                v->normal.z = PSK3D_Q16_ONE;
            }
            v->uv.u = model->wedges[widx].u;
            v->uv.v = model->wedges[widx].v;
            if (model->extra_uv_set_count > 0u && widx < model->extra_uv_counts[0]) {
                v->extra_uv0 = model->extra_uvs[0][widx];
            } else {
                v->extra_uv0.u = 0;
                v->extra_uv0.v = 0;
            }
            if (widx < model->vertex_color_count) {
                v->color = model->vertex_colors[widx];
            } else {
                v->color.r = 255u;
                v->color.g = 255u;
                v->color.b = 255u;
                v->color.a = 255u;
            }
            v->point_index = pidx;
            v->wedge_index = widx;
            v->material_index = model->faces[f].mat_index;
            out_i++;
        }
    }
    return PSK3D_OK;
}

const char *psk3d_result_name(psk3d_result r)
{
    switch (r) {
    case PSK3D_OK: return "PSK3D_OK";
    case PSK3D_ERR_NULL: return "PSK3D_ERR_NULL";
    case PSK3D_ERR_TYPESIZE: return "PSK3D_ERR_TYPESIZE";
    case PSK3D_ERR_EOF: return "PSK3D_ERR_EOF";
    case PSK3D_ERR_BAD_MAGIC: return "PSK3D_ERR_BAD_MAGIC";
    case PSK3D_ERR_BAD_SIZE: return "PSK3D_ERR_BAD_SIZE";
    case PSK3D_ERR_CAPACITY: return "PSK3D_ERR_CAPACITY";
    case PSK3D_ERR_BAD_INDEX: return "PSK3D_ERR_BAD_INDEX";
    case PSK3D_ERR_UNSUPPORTED: return "PSK3D_ERR_UNSUPPORTED";
    case PSK3D_ERR_OVERFLOW: return "PSK3D_ERR_OVERFLOW";
    default: return "PSK3D_UNKNOWN_RESULT";
    }
}
