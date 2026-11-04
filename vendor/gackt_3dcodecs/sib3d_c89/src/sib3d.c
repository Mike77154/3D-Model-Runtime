#include "sib3d.h"

#include <string.h>
#include <stdio.h>
#include <limits.h>

#define SIB_TAG(a,b,c,d) ((((SIB_U32)(SIB_U8)(a)) << 24) | (((SIB_U32)(SIB_U8)(b)) << 16) | (((SIB_U32)(SIB_U8)(c)) << 8) | ((SIB_U32)(SIB_U8)(d)))
#define SIB_MIN(a,b) ((a) < (b) ? (a) : (b))
#define SIB_MAX(a,b) ((a) > (b) ? (a) : (b))

#ifndef SIB_CFG_ASSUME_I32_MAX
#define SIB_CFG_ASSUME_I32_MAX 2147483647L
#endif
#ifndef SIB_CFG_ASSUME_I32_MIN
#define SIB_CFG_ASSUME_I32_MIN (-2147483647L - 1L)
#endif

typedef struct SIB_Reader {
    const SIB_U8 *data;
    SIB_U32 size;
    SIB_U32 pos;
} SIB_Reader;

typedef struct SIB_Chunk {
    SIB_U32 tag;
    SIB_U32 size;
    SIB_U32 start;
    SIB_U32 end;
} SIB_Chunk;

static void sib_zero(void *p, SIB_U32 n) {
    memset(p, 0, (size_t)n);
}

static void sib_copy_text(char *dst, SIB_U32 dst_size, const char *src) {
    SIB_U32 i;
    if (dst_size == 0u) {
        return;
    }
    i = 0u;
    while (i + 1u < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void sib_set_error(SIB_Diagnostics *diag, SIB_Result r, const char *msg) {
    if (diag != 0) {
        diag->result = r;
        sib_copy_text(diag->error_text, (SIB_U32)sizeof(diag->error_text), msg);
    }
}

static void sib_warn_tag(SIB_Diagnostics *diag, const char *prefix, SIB_U32 tag) {
    char buf[96];
    SIB_U32 n;
    if (diag == 0) {
        return;
    }
    if (diag->warnings >= SIB_CFG_MAX_WARNINGS) {
        return;
    }
    n = diag->warnings;
    sprintf(buf, "%s '%c%c%c%c'", prefix,
            (char)((tag >> 24) & 255u),
            (char)((tag >> 16) & 255u),
            (char)((tag >> 8) & 255u),
            (char)(tag & 255u));
    sib_copy_text(diag->warning_text[n], (SIB_U32)sizeof(diag->warning_text[n]), buf);
    diag->warnings++;
}

static void sib_warn_text(SIB_Diagnostics *diag, const char *msg) {
    SIB_U32 n;
    if (diag == 0) {
        return;
    }
    if (diag->warnings >= SIB_CFG_MAX_WARNINGS) {
        return;
    }
    n = diag->warnings;
    sib_copy_text(diag->warning_text[n], (SIB_U32)sizeof(diag->warning_text[n]), msg);
    diag->warnings++;
}

void sib_diagnostics_init(SIB_Diagnostics *diag) {
    if (diag != 0) {
        sib_zero(diag, (SIB_U32)sizeof(*diag));
        diag->result = SIB_OK;
    }
}

static void sib_mat4_identity(SIB_Mat4 *m) {
    SIB_U32 i;
    if (m == 0) {
        return;
    }
    for (i = 0u; i < 16u; ++i) {
        m->m[i] = 0;
    }
    m->m[0] = SIB_FIXED_ONE;
    m->m[5] = SIB_FIXED_ONE;
    m->m[10] = SIB_FIXED_ONE;
    m->m[15] = SIB_FIXED_ONE;
}

void sib_scene_init(SIB_Scene *scene) {
    if (scene != 0) {
        sib_zero(scene, (SIB_U32)sizeof(*scene));
        scene->material_count = 1u;
        sib_copy_text(scene->materials[0].name, SIB_CFG_MAX_NAME, "DefaultMaterial");
        scene->materials[0].diffuse.x = SIB_FIXED_ONE;
        scene->materials[0].diffuse.y = SIB_FIXED_ONE;
        scene->materials[0].diffuse.z = SIB_FIXED_ONE;
        scene->materials[0].ambient.x = SIB_FIXED_ONE / 5L;
        scene->materials[0].ambient.y = SIB_FIXED_ONE / 5L;
        scene->materials[0].ambient.z = SIB_FIXED_ONE / 5L;
    }
}

const char *sib_result_name(SIB_Result r) {
    switch (r) {
        case SIB_OK: return "SIB_OK";
        case SIB_ERR_NULL: return "SIB_ERR_NULL";
        case SIB_ERR_TRUNCATED: return "SIB_ERR_TRUNCATED";
        case SIB_ERR_BAD_CHUNK: return "SIB_ERR_BAD_CHUNK";
        case SIB_ERR_UNSUPPORTED_VERSION: return "SIB_ERR_UNSUPPORTED_VERSION";
        case SIB_ERR_CAPACITY: return "SIB_ERR_CAPACITY";
        case SIB_ERR_BAD_INDEX: return "SIB_ERR_BAD_INDEX";
        case SIB_ERR_BAD_DATA: return "SIB_ERR_BAD_DATA";
        default: return "SIB_ERR_UNKNOWN";
    }
}

SIB_I32 sib_fixed_from_i32(SIB_I32 v) {
    if (v > (SIB_CFG_ASSUME_I32_MAX / SIB_FIXED_ONE)) {
        return SIB_CFG_ASSUME_I32_MAX;
    }
    if (v < (SIB_CFG_ASSUME_I32_MIN / SIB_FIXED_ONE)) {
        return SIB_CFG_ASSUME_I32_MIN;
    }
    return v * SIB_FIXED_ONE;
}

SIB_I32 sib_fixed_to_i32_floor(SIB_I32 v) {
    return v / SIB_FIXED_ONE;
}

void sib_fixed_to_decimal(SIB_I32 v, char *out, SIB_U32 out_size, SIB_U32 digits) {
    SIB_I32 whole;
    SIB_I32 frac;
    SIB_U32 i;
    SIB_U32 mul;
    char sign[2];
    if (out == 0 || out_size == 0u) {
        return;
    }
    if (digits > 5u) {
        digits = 5u;
    }
    sign[0] = '\0';
    sign[1] = '\0';
    if (v < 0) {
        sign[0] = '-';
        v = -v;
    }
    whole = v / SIB_FIXED_ONE;
    frac = v - whole * SIB_FIXED_ONE;
    mul = 1u;
    for (i = 0u; i < digits; ++i) {
        mul *= 10u;
    }
    frac = (SIB_I32)(((SIB_U32)frac * mul + (SIB_FIXED_ONE / 2u)) / SIB_FIXED_ONE);
    if (digits == 0u) {
        sprintf(out, "%s%ld", sign, (long)whole);
    } else {
        char fmt[16];
        sprintf(fmt, "%%s%%ld.%%0%luld", (unsigned long)digits);
        sprintf(out, fmt, sign, (long)whole, (long)frac);
    }
    out[out_size - 1u] = '\0';
}

static SIB_U32 sib_reader_left(const SIB_Reader *r, SIB_U32 end) {
    if (r == 0 || r->pos > end) {
        return 0u;
    }
    return end - r->pos;
}

static SIB_Result sib_read_u16(SIB_Reader *r, SIB_U32 end, SIB_U16 *out) {
    SIB_U32 p;
    if (r->pos + 2u > end || r->pos + 2u > r->size) {
        return SIB_ERR_TRUNCATED;
    }
    p = r->pos;
    *out = (SIB_U16)((SIB_U16)r->data[p] | ((SIB_U16)r->data[p + 1u] << 8));
    r->pos += 2u;
    return SIB_OK;
}

static SIB_Result sib_read_u32(SIB_Reader *r, SIB_U32 end, SIB_U32 *out) {
    SIB_U32 p;
    if (r->pos + 4u > end || r->pos + 4u > r->size) {
        return SIB_ERR_TRUNCATED;
    }
    p = r->pos;
    *out = ((SIB_U32)r->data[p]) |
           (((SIB_U32)r->data[p + 1u]) << 8) |
           (((SIB_U32)r->data[p + 2u]) << 16) |
           (((SIB_U32)r->data[p + 3u]) << 24);
    *out &= 0xffffffffUL;
    r->pos += 4u;
    return SIB_OK;
}

static SIB_I32 sib_sat_i32_from_u32_shift(SIB_U32 v, SIB_U32 left_shift, SIB_U8 neg) {
    SIB_U32 lim;
    SIB_U32 i;
    if (neg != 0u) {
        lim = 0x80000000UL;
    } else {
        lim = 0x7fffffffUL;
    }
    for (i = 0u; i < left_shift; ++i) {
        if (v > (lim >> 1)) {
            return neg != 0u ? SIB_CFG_ASSUME_I32_MIN : SIB_CFG_ASSUME_I32_MAX;
        }
        v <<= 1;
    }
    if (v > lim) {
        return neg != 0u ? SIB_CFG_ASSUME_I32_MIN : SIB_CFG_ASSUME_I32_MAX;
    }
    if (neg != 0u) {
        if (v == 0x80000000UL) {
            return SIB_CFG_ASSUME_I32_MIN;
        }
        return -(SIB_I32)v;
    }
    return (SIB_I32)v;
}

static SIB_I32 sib_float_bits_to_fixed(SIB_U32 bits) {
    SIB_U32 sign;
    SIB_U32 exp;
    SIB_U32 mant;
    SIB_I32 shift;
    SIB_U32 value;
    SIB_U32 lost;

    sign = (bits >> 31) & 1u;
    exp = (bits >> 23) & 255u;
    mant = bits & 0x7fffffUL;

    if (exp == 255u) {
        return sign != 0u ? SIB_CFG_ASSUME_I32_MIN : SIB_CFG_ASSUME_I32_MAX;
    }
    if (exp == 0u) {
        return 0;
    }

    value = mant | 0x800000UL;
    shift = (SIB_I32)exp - 134L;
    if (shift >= 0) {
        if (shift > 31L) {
            return sign != 0u ? SIB_CFG_ASSUME_I32_MIN : SIB_CFG_ASSUME_I32_MAX;
        }
        return sib_sat_i32_from_u32_shift(value, (SIB_U32)shift, (SIB_U8)sign);
    }

    shift = -shift;
    if (shift >= 32L) {
        return 0;
    }
    lost = value & ((1UL << (SIB_U32)shift) - 1UL);
    value >>= (SIB_U32)shift;
    if (shift > 0L) {
        if (lost >= (1UL << ((SIB_U32)shift - 1u))) {
            value++;
        }
    }
    if (sign != 0u) {
        return -(SIB_I32)value;
    }
    return (SIB_I32)value;
}

static SIB_Result sib_read_f32_fixed(SIB_Reader *r, SIB_U32 end, SIB_I32 *out) {
    SIB_Result res;
    SIB_U32 bits;
    res = sib_read_u32(r, end, &bits);
    if (res != SIB_OK) {
        return res;
    }
    *out = sib_float_bits_to_fixed(bits);
    return SIB_OK;
}

static SIB_Result sib_read_chunk(SIB_Reader *r, SIB_U32 parent_end, SIB_Chunk *chunk) {
    SIB_U32 p;
    SIB_Result res;
    if (sib_reader_left(r, parent_end) < 8u) {
        return SIB_ERR_TRUNCATED;
    }
    p = r->pos;
    chunk->tag = (((SIB_U32)r->data[p]) << 24) |
                 (((SIB_U32)r->data[p + 1u]) << 16) |
                 (((SIB_U32)r->data[p + 2u]) << 8) |
                 ((SIB_U32)r->data[p + 3u]);
    r->pos += 4u;
    res = sib_read_u32(r, parent_end, &chunk->size);
    if (res != SIB_OK) {
        return res;
    }
    chunk->start = r->pos;
    if (chunk->size > parent_end || chunk->start > parent_end - chunk->size) {
        return SIB_ERR_BAD_CHUNK;
    }
    chunk->end = chunk->start + chunk->size;
    return SIB_OK;
}

static SIB_Result sib_skip_to(SIB_Reader *r, SIB_U32 end) {
    if (end > r->size) {
        return SIB_ERR_TRUNCATED;
    }
    r->pos = end;
    return SIB_OK;
}

static void sib_utf16le_read_ascii(SIB_Reader *r, SIB_U32 end, SIB_U32 byte_count, char *out, SIB_U32 out_size) {
    SIB_U32 n;
    SIB_U32 i;
    SIB_U16 wc;
    SIB_Result res;
    if (out_size == 0u) {
        r->pos = SIB_MIN(end, r->pos + byte_count);
        return;
    }
    out[0] = '\0';
    n = byte_count / 2u;
    i = 0u;
    while (n > 0u && r->pos + 2u <= end) {
        res = sib_read_u16(r, end, &wc);
        if (res != SIB_OK) {
            break;
        }
        if (wc == 0u) {
            n--;
            break;
        }
        if (i + 1u < out_size) {
            if (wc < 128u) {
                out[i] = (char)wc;
            } else {
                out[i] = '?';
            }
            i++;
        }
        n--;
    }
    out[i] = '\0';
    if (r->pos < end) {
        r->pos = end;
    }
}

static SIB_I32 sib_fixed_mul_approx(SIB_I32 a, SIB_I32 b) {
    SIB_I32 ar;
    SIB_I32 br;
    SIB_I32 sign;
    SIB_U32 ua;
    SIB_U32 ub;
    SIB_U32 r;
    sign = 1;
    if (a < 0) {
        sign = -sign;
        a = -a;
    }
    if (b < 0) {
        sign = -sign;
        b = -b;
    }
    ar = a >> 8;
    br = b >> 8;
    ua = (SIB_U32)ar;
    ub = (SIB_U32)br;
    if (ua != 0u && ub > 0x7fffffffUL / ua) {
        return sign < 0 ? SIB_CFG_ASSUME_I32_MIN : SIB_CFG_ASSUME_I32_MAX;
    }
    r = ua * ub;
    if (r > 0x7fffffffUL) {
        return sign < 0 ? SIB_CFG_ASSUME_I32_MIN : SIB_CFG_ASSUME_I32_MAX;
    }
    if (sign < 0) {
        return -(SIB_I32)r;
    }
    return (SIB_I32)r;
}

static SIB_I32 sib_fixed_mul(SIB_I32 a, SIB_I32 b) {
#if defined(SIB_CFG_USE_COMPILER_I64) && SIB_CFG_USE_COMPILER_I64
    typedef signed long long SIB_I64_LOCAL;
    SIB_I64_LOCAL v;
    v = ((SIB_I64_LOCAL)a * (SIB_I64_LOCAL)b) >> SIB_FIXED_SHIFT;
    if (v > (SIB_I64_LOCAL)SIB_CFG_ASSUME_I32_MAX) {
        return SIB_CFG_ASSUME_I32_MAX;
    }
    if (v < (SIB_I64_LOCAL)SIB_CFG_ASSUME_I32_MIN) {
        return SIB_CFG_ASSUME_I32_MIN;
    }
    return (SIB_I32)v;
#else
    return sib_fixed_mul_approx(a, b);
#endif
}

static SIB_I32 sib_abs_i32(SIB_I32 v) {
    if (v < 0) {
        return -v;
    }
    return v;
}

static SIB_U32 sib_isqrt_u32(SIB_U32 x) {
    SIB_U32 op;
    SIB_U32 res;
    SIB_U32 one;
    op = x;
    res = 0u;
    one = 1UL << 30;
    while (one > op) {
        one >>= 2;
    }
    while (one != 0u) {
        if (op >= res + one) {
            op -= res + one;
            res += 2u * one;
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

static SIB_Vec3 sib_vec3_sub(SIB_Vec3 a, SIB_Vec3 b) {
    SIB_Vec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}

static SIB_Vec3 sib_vec3_add(SIB_Vec3 a, SIB_Vec3 b) {
    SIB_Vec3 r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    r.z = a.z + b.z;
    return r;
}

static SIB_Vec3 sib_vec3_cross(SIB_Vec3 a, SIB_Vec3 b) {
    SIB_Vec3 r;
    r.x = sib_fixed_mul(a.y, b.z) - sib_fixed_mul(a.z, b.y);
    r.y = sib_fixed_mul(a.z, b.x) - sib_fixed_mul(a.x, b.z);
    r.z = sib_fixed_mul(a.x, b.y) - sib_fixed_mul(a.y, b.x);
    return r;
}

static SIB_Vec3 sib_vec3_normalize(SIB_Vec3 v) {
    SIB_U32 ax;
    SIB_U32 ay;
    SIB_U32 az;
    SIB_U32 sx;
    SIB_U32 sy;
    SIB_U32 sz;
    SIB_U32 len;
    SIB_Vec3 r;
    ax = (SIB_U32)sib_abs_i32(v.x);
    ay = (SIB_U32)sib_abs_i32(v.y);
    az = (SIB_U32)sib_abs_i32(v.z);
    if (ax == 0u && ay == 0u && az == 0u) {
        r.x = 0;
        r.y = 0;
        r.z = SIB_FIXED_ONE;
        return r;
    }
    sx = ax >> 8;
    sy = ay >> 8;
    sz = az >> 8;
    if (sx == 0u && ax != 0u) { sx = 1u; }
    if (sy == 0u && ay != 0u) { sy = 1u; }
    if (sz == 0u && az != 0u) { sz = 1u; }
    if (sx > 46340u) { sx = 46340u; }
    if (sy > 46340u) { sy = 46340u; }
    if (sz > 46340u) { sz = 46340u; }
    len = sib_isqrt_u32(sx * sx + sy * sy + sz * sz);
    if (len == 0u) {
        len = 1u;
    }
    r.x = (SIB_I32)((v.x / (SIB_I32)len) << 8);
    r.y = (SIB_I32)((v.y / (SIB_I32)len) << 8);
    r.z = (SIB_I32)((v.z / (SIB_I32)len) << 8);
    return r;
}

static void sib_mat4_mul(SIB_Mat4 *out, const SIB_Mat4 *a, const SIB_Mat4 *b) {
    SIB_Mat4 r;
    SIB_U32 row;
    SIB_U32 col;
    SIB_U32 k;
    SIB_I32 sum;
    for (row = 0u; row < 4u; ++row) {
        for (col = 0u; col < 4u; ++col) {
            sum = 0;
            for (k = 0u; k < 4u; ++k) {
                sum += sib_fixed_mul(a->m[row * 4u + k], b->m[k * 4u + col]);
            }
            r.m[row * 4u + col] = sum;
        }
    }
    *out = r;
}

static SIB_Result sib_read_color(SIB_Reader *r, SIB_U32 end, SIB_Vec3 *out) {
    SIB_Result res;
    SIB_I32 dummy;
    res = sib_read_f32_fixed(r, end, &out->x);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &out->y);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &out->z);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &dummy);
    return res;
}

static SIB_Result sib_read_axis(SIB_Reader *r, SIB_U32 end, SIB_Mat4 *axis) {
    SIB_Result res;
    sib_mat4_identity(axis);
    res = sib_read_f32_fixed(r, end, &axis->m[3]);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &axis->m[7]);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &axis->m[11]);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &axis->m[0]);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &axis->m[4]);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &axis->m[8]);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &axis->m[1]);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &axis->m[5]);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &axis->m[9]);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &axis->m[2]);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &axis->m[6]);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &axis->m[10]);
    return res;
}

static SIB_Result sib_read_scale_matrix(SIB_Reader *r, SIB_U32 end, SIB_Mat4 *axis) {
    SIB_Result res;
    SIB_Mat4 s;
    SIB_U32 i;
    for (i = 0u; i < 16u; ++i) {
        res = sib_read_f32_fixed(r, end, &s.m[i]);
        if (res != SIB_OK) {
            return res;
        }
    }
    sib_mat4_mul(axis, axis, &s);
    return SIB_OK;
}

static SIB_Result sib_add_edge(SIB_Scene *scene, SIB_Mesh *mesh, SIB_U32 a, SIB_U32 b, SIB_U32 *edge_index) {
    SIB_U32 i;
    SIB_U32 lo;
    SIB_U32 hi;
    SIB_Edge *e;
    lo = a < b ? a : b;
    hi = a < b ? b : a;
    for (i = 0u; i < mesh->edge_count; ++i) {
        e = &scene->edges[mesh->first_edge + i];
        if (e->a == lo && e->b == hi) {
            if (edge_index != 0) {
                *edge_index = mesh->first_edge + i;
            }
            return SIB_OK;
        }
    }
    if (scene->edge_count >= SIB_CFG_MAX_EDGES) {
        return SIB_ERR_CAPACITY;
    }
    e = &scene->edges[scene->edge_count];
    e->a = lo;
    e->b = hi;
    e->face_a = SIB_INVALID_INDEX;
    e->face_b = SIB_INVALID_INDEX;
    e->creased = 0u;
    if (edge_index != 0) {
        *edge_index = scene->edge_count;
    }
    scene->edge_count++;
    mesh->edge_count++;
    return SIB_OK;
}

static SIB_Edge *sib_find_edge(SIB_Scene *scene, SIB_Mesh *mesh, SIB_U32 a, SIB_U32 b) {
    SIB_U32 i;
    SIB_U32 lo;
    SIB_U32 hi;
    SIB_Edge *e;
    lo = a < b ? a : b;
    hi = a < b ? b : a;
    for (i = 0u; i < mesh->edge_count; ++i) {
        e = &scene->edges[mesh->first_edge + i];
        if (e->a == lo && e->b == hi) {
            return e;
        }
    }
    return 0;
}

static SIB_Result sib_read_vertices(SIB_Scene *scene, SIB_Mesh *mesh, SIB_Reader *r, SIB_Chunk *chunk) {
    SIB_U32 count;
    SIB_U32 i;
    SIB_Result res;
    count = chunk->size / 12u;
    if (chunk->size % 12u != 0u) {
        return SIB_ERR_BAD_DATA;
    }
    if (scene->vertex_count > SIB_CFG_MAX_VERTICES - count) {
        return SIB_ERR_CAPACITY;
    }
    if (mesh->vertex_count == 0u) {
        mesh->first_vertex = scene->vertex_count;
    }
    for (i = 0u; i < count; ++i) {
        res = sib_read_f32_fixed(r, chunk->end, &scene->vertices[scene->vertex_count].x);
        if (res != SIB_OK) { return res; }
        res = sib_read_f32_fixed(r, chunk->end, &scene->vertices[scene->vertex_count].y);
        if (res != SIB_OK) { return res; }
        res = sib_read_f32_fixed(r, chunk->end, &scene->vertices[scene->vertex_count].z);
        if (res != SIB_OK) { return res; }
        scene->vertex_count++;
        mesh->vertex_count++;
    }
    return SIB_OK;
}

static SIB_Result sib_read_faces(SIB_Scene *scene, SIB_Mesh *mesh, SIB_Reader *r, SIB_Chunk *chunk) {
    SIB_Result res;
    SIB_U32 num_points;
    SIB_U32 p;
    SIB_U32 n;
    SIB_Face *face;
    while (r->pos < chunk->end) {
        res = sib_read_u32(r, chunk->end, &num_points);
        if (res != SIB_OK) { return res; }
        if (num_points < 3u) {
            return SIB_ERR_BAD_DATA;
        }
        if (scene->face_count >= SIB_CFG_MAX_FACES) {
            return SIB_ERR_CAPACITY;
        }
        if (scene->corner_count > SIB_CFG_MAX_CORNERS - num_points) {
            return SIB_ERR_CAPACITY;
        }
        if (mesh->face_count == 0u) {
            mesh->first_face = scene->face_count;
            mesh->first_corner = scene->corner_count;
        }
        face = &scene->faces[scene->face_count];
        face->first_corner = scene->corner_count;
        face->corner_count = num_points;
        face->material = 0u;
        scene->face_count++;
        mesh->face_count++;
        mesh->corner_count += num_points;
        for (n = 0u; n < num_points; ++n) {
            res = sib_read_u32(r, chunk->end, &p);
            if (res != SIB_OK) { return res; }
            if (p >= mesh->vertex_count) {
                return SIB_ERR_BAD_INDEX;
            }
            scene->corners[scene->corner_count].vertex = mesh->first_vertex + p;
            scene->corners[scene->corner_count].uv.x = 0;
            scene->corners[scene->corner_count].uv.y = 0;
            scene->corners[scene->corner_count].normal.x = 0;
            scene->corners[scene->corner_count].normal.y = 0;
            scene->corners[scene->corner_count].normal.z = SIB_FIXED_ONE;
            scene->corner_count++;
        }
    }
    return SIB_OK;
}

static SIB_Result sib_read_uvs(SIB_Scene *scene, SIB_Mesh *mesh, SIB_Reader *r, SIB_Chunk *chunk) {
    SIB_U32 face_idx;
    SIB_U32 num_points;
    SIB_U32 n;
    SIB_U32 fglobal;
    SIB_Result res;
    SIB_Face *face;
    while (r->pos < chunk->end) {
        res = sib_read_u32(r, chunk->end, &face_idx);
        if (res != SIB_OK) { return res; }
        res = sib_read_u32(r, chunk->end, &num_points);
        if (res != SIB_OK) { return res; }
        if (face_idx >= mesh->face_count) {
            return SIB_ERR_BAD_INDEX;
        }
        fglobal = mesh->first_face + face_idx;
        face = &scene->faces[fglobal];
        for (n = 0u; n < num_points; ++n) {
            SIB_I32 u;
            SIB_I32 v;
            res = sib_read_f32_fixed(r, chunk->end, &u);
            if (res != SIB_OK) { return res; }
            res = sib_read_f32_fixed(r, chunk->end, &v);
            if (res != SIB_OK) { return res; }
            if (n < face->corner_count) {
                scene->corners[face->first_corner + n].uv.x = u;
                scene->corners[face->first_corner + n].uv.y = v;
            }
        }
    }
    return SIB_OK;
}

static SIB_Result sib_read_material_assignments(SIB_Scene *scene, SIB_Mesh *mesh, SIB_Reader *r, SIB_Chunk *chunk) {
    SIB_Result res;
    SIB_U32 prev_face;
    SIB_U32 prev_mtl;
    SIB_U32 face;
    SIB_U32 mtl;
    SIB_U32 i;
    if (chunk->size < 8u) {
        return SIB_OK;
    }
    res = sib_read_u32(r, chunk->end, &prev_face);
    if (res != SIB_OK) { return res; }
    res = sib_read_u32(r, chunk->end, &prev_mtl);
    if (res != SIB_OK) { return res; }
    prev_mtl++;
    while (r->pos < chunk->end) {
        res = sib_read_u32(r, chunk->end, &face);
        if (res != SIB_OK) { return res; }
        res = sib_read_u32(r, chunk->end, &mtl);
        if (res != SIB_OK) { return res; }
        mtl++;
        while (prev_face < face && prev_face < mesh->face_count) {
            scene->faces[mesh->first_face + prev_face].material = prev_mtl;
            prev_face++;
        }
        prev_face = face;
        prev_mtl = mtl;
    }
    for (i = prev_face; i < mesh->face_count; ++i) {
        scene->faces[mesh->first_face + i].material = prev_mtl;
    }
    return SIB_OK;
}

static SIB_Result sib_read_edges(SIB_Scene *scene, SIB_Mesh *mesh, SIB_Reader *r, SIB_Chunk *chunk) {
    SIB_U32 a;
    SIB_U32 b;
    SIB_Result res;
    if (mesh->edge_count == 0u) {
        mesh->first_edge = scene->edge_count;
    }
    while (r->pos < chunk->end) {
        res = sib_read_u32(r, chunk->end, &a);
        if (res != SIB_OK) { return res; }
        res = sib_read_u32(r, chunk->end, &b);
        if (res != SIB_OK) { return res; }
        if (a >= mesh->vertex_count || b >= mesh->vertex_count) {
            return SIB_ERR_BAD_INDEX;
        }
        res = sib_add_edge(scene, mesh, mesh->first_vertex + a, mesh->first_vertex + b, 0);
        if (res != SIB_OK) { return res; }
    }
    return SIB_OK;
}

static SIB_Result sib_read_creases(SIB_Scene *scene, SIB_Mesh *mesh, SIB_Reader *r, SIB_Chunk *chunk) {
    SIB_U32 edge;
    SIB_Result res;
    while (r->pos < chunk->end) {
        res = sib_read_u32(r, chunk->end, &edge);
        if (res != SIB_OK) { return res; }
        if (edge >= mesh->edge_count) {
            return SIB_ERR_BAD_INDEX;
        }
        scene->edges[mesh->first_edge + edge].creased = 1u;
    }
    return SIB_OK;
}

static SIB_Result sib_connect_faces(SIB_Scene *scene, SIB_Mesh *mesh) {
    SIB_U32 fi;
    SIB_U32 ci;
    SIB_U32 curr;
    SIB_U32 prev;
    SIB_U32 edge_index;
    SIB_Result res;
    SIB_Face *face;
    SIB_Edge *edge;
    for (ci = 0u; ci < mesh->edge_count; ++ci) {
        edge = &scene->edges[mesh->first_edge + ci];
        edge->face_a = SIB_INVALID_INDEX;
        edge->face_b = SIB_INVALID_INDEX;
    }
    for (fi = 0u; fi < mesh->face_count; ++fi) {
        face = &scene->faces[mesh->first_face + fi];
        prev = scene->corners[face->first_corner + face->corner_count - 1u].vertex;
        for (ci = 0u; ci < face->corner_count; ++ci) {
            curr = scene->corners[face->first_corner + ci].vertex;
            if (mesh->edge_count == 0u) {
                mesh->first_edge = scene->edge_count;
            }
            res = sib_add_edge(scene, mesh, prev, curr, &edge_index);
            if (res != SIB_OK) {
                return res;
            }
            edge = &scene->edges[edge_index];
            if (edge->face_a == SIB_INVALID_INDEX) {
                edge->face_a = mesh->first_face + fi;
            } else if (edge->face_b == SIB_INVALID_INDEX) {
                edge->face_b = mesh->first_face + fi;
            }
            prev = curr;
        }
    }
    return SIB_OK;
}

static void sib_calculate_face_normals(SIB_Scene *scene, SIB_Mesh *mesh) {
    SIB_U32 fi;
    SIB_U32 ci;
    SIB_Face *face;
    SIB_Vec3 a;
    SIB_Vec3 b;
    SIB_Vec3 c;
    SIB_Vec3 ab;
    SIB_Vec3 ac;
    SIB_Vec3 n;
    for (fi = 0u; fi < mesh->face_count; ++fi) {
        face = &scene->faces[mesh->first_face + fi];
        n.x = 0;
        n.y = 0;
        n.z = 0;
        if (face->corner_count >= 3u) {
            a = scene->vertices[scene->corners[face->first_corner].vertex];
            for (ci = 1u; ci + 1u < face->corner_count; ++ci) {
                b = scene->vertices[scene->corners[face->first_corner + ci].vertex];
                c = scene->vertices[scene->corners[face->first_corner + ci + 1u].vertex];
                ab = sib_vec3_sub(b, a);
                ac = sib_vec3_sub(c, a);
                n = sib_vec3_add(n, sib_vec3_cross(ab, ac));
            }
        }
        scene->_face_normals[mesh->first_face + fi] = sib_vec3_normalize(n);
    }
}

static SIB_U32 sib_other_face(const SIB_Edge *edge, SIB_U32 face) {
    if (edge == 0) {
        return SIB_INVALID_INDEX;
    }
    if (edge->face_a == face) {
        return edge->face_b;
    }
    if (edge->face_b == face) {
        return edge->face_a;
    }
    return SIB_INVALID_INDEX;
}

static void sib_calculate_corner_normals(SIB_Scene *scene, SIB_Mesh *mesh) {
    SIB_U32 fi;
    SIB_U32 ci;
    SIB_U32 vertex;
    SIB_U32 prev;
    SIB_U32 curr;
    SIB_U32 next;
    SIB_U32 other;
    SIB_Face *face;
    SIB_Edge *edge_a;
    SIB_Edge *edge_b;
    SIB_Vec3 n;
    for (fi = 0u; fi < mesh->face_count; ++fi) {
        face = &scene->faces[mesh->first_face + fi];
        for (ci = 0u; ci < face->corner_count; ++ci) {
            vertex = scene->corners[face->first_corner + ci].vertex;
            prev = scene->corners[face->first_corner + ((ci + face->corner_count - 1u) % face->corner_count)].vertex;
            curr = vertex;
            next = scene->corners[face->first_corner + ((ci + 1u) % face->corner_count)].vertex;
            n = scene->_face_normals[mesh->first_face + fi];
            edge_a = sib_find_edge(scene, mesh, prev, curr);
            edge_b = sib_find_edge(scene, mesh, curr, next);
            if (edge_a != 0 && edge_a->creased == 0u) {
                other = sib_other_face(edge_a, mesh->first_face + fi);
                if (other != SIB_INVALID_INDEX) {
                    n = sib_vec3_add(n, scene->_face_normals[other]);
                }
            }
            if (edge_b != 0 && edge_b->creased == 0u) {
                other = sib_other_face(edge_b, mesh->first_face + fi);
                if (other != SIB_INVALID_INDEX) {
                    n = sib_vec3_add(n, scene->_face_normals[other]);
                }
            }
            scene->corners[face->first_corner + ci].normal = sib_vec3_normalize(n);
        }
    }
}

static SIB_Result sib_finalize_shape(SIB_Scene *scene, SIB_Mesh *mesh) {
    SIB_Result res;
    res = sib_connect_faces(scene, mesh);
    if (res != SIB_OK) {
        return res;
    }
    sib_calculate_face_normals(scene, mesh);
    sib_calculate_corner_normals(scene, mesh);
    return SIB_OK;
}

static SIB_Result sib_read_shape(SIB_Scene *scene, SIB_Reader *r, SIB_U32 end, SIB_Diagnostics *diag) {
    SIB_Mesh *mesh;
    SIB_Object *obj;
    SIB_Chunk chunk;
    SIB_Result res;
    SIB_U32 mesh_index;
    if (scene->mesh_count >= SIB_CFG_MAX_MESHES) {
        return SIB_ERR_CAPACITY;
    }
    if (scene->object_count >= SIB_CFG_MAX_OBJECTS) {
        return SIB_ERR_CAPACITY;
    }
    mesh_index = scene->mesh_count;
    mesh = &scene->meshes[mesh_index];
    sib_zero(mesh, (SIB_U32)sizeof(*mesh));
    sib_mat4_identity(&mesh->axis);
    mesh->first_vertex = scene->vertex_count;
    mesh->first_face = scene->face_count;
    mesh->first_corner = scene->corner_count;
    mesh->first_edge = scene->edge_count;
    sib_copy_text(mesh->name, SIB_CFG_MAX_NAME, "SIBShape");
    scene->mesh_count++;

    while (r->pos < end) {
        res = sib_read_chunk(r, end, &chunk);
        if (res != SIB_OK) { return res; }
        switch (chunk.tag) {
            case SIB_TAG('M','I','R','P'):
            case SIB_TAG('I','M','R','P'):
            case SIB_TAG('D','I','N','F'):
            case SIB_TAG('P','I','N','F'):
            case SIB_TAG('V','M','I','R'):
            case SIB_TAG('F','M','I','R'):
            case SIB_TAG('T','X','S','M'):
            case SIB_TAG('F','A','H','S'):
                break;
            case SIB_TAG('V','R','T','S'):
                res = sib_read_vertices(scene, mesh, r, &chunk);
                if (res != SIB_OK) { return res; }
                break;
            case SIB_TAG('F','A','C','S'):
                res = sib_read_faces(scene, mesh, r, &chunk);
                if (res != SIB_OK) { return res; }
                break;
            case SIB_TAG('F','T','V','S'):
                res = sib_read_uvs(scene, mesh, r, &chunk);
                if (res != SIB_OK) { return res; }
                break;
            case SIB_TAG('S','N','A','M'):
                sib_utf16le_read_ascii(r, chunk.end, chunk.size, mesh->name, SIB_CFG_MAX_NAME);
                break;
            case SIB_TAG('F','A','M','A'):
                res = sib_read_material_assignments(scene, mesh, r, &chunk);
                if (res != SIB_OK) { return res; }
                break;
            case SIB_TAG('A','X','I','S'):
                res = sib_read_axis(r, chunk.end, &mesh->axis);
                if (res != SIB_OK) { return res; }
                break;
            case SIB_TAG('E','D','G','S'):
                res = sib_read_edges(scene, mesh, r, &chunk);
                if (res != SIB_OK) { return res; }
                break;
            case SIB_TAG('E','C','R','S'):
                res = sib_read_creases(scene, mesh, r, &chunk);
                if (res != SIB_OK) { return res; }
                break;
            default:
                sib_warn_tag(diag, "shape unknown chunk", chunk.tag);
                break;
        }
        res = sib_skip_to(r, chunk.end);
        if (res != SIB_OK) { return res; }
    }
    res = sib_finalize_shape(scene, mesh);
    if (res != SIB_OK) {
        return res;
    }
    obj = &scene->objects[scene->object_count];
    sib_zero(obj, (SIB_U32)sizeof(*obj));
    sib_copy_text(obj->name, SIB_CFG_MAX_NAME, mesh->name);
    obj->axis = mesh->axis;
    obj->mesh = mesh_index;
    obj->is_instance = 0u;
    scene->object_count++;
    return SIB_OK;
}

static SIB_Result sib_read_material(SIB_Scene *scene, SIB_Reader *r, SIB_U32 end) {
    SIB_Material *mtl;
    SIB_Result res;
    SIB_U32 name_len;
    SIB_U32 tex_len;
    if (scene->material_count >= SIB_CFG_MAX_MATERIALS) {
        return SIB_ERR_CAPACITY;
    }
    mtl = &scene->materials[scene->material_count];
    sib_zero(mtl, (SIB_U32)sizeof(*mtl));
    res = sib_read_color(r, end, &mtl->diffuse);
    if (res != SIB_OK) { return res; }
    res = sib_read_color(r, end, &mtl->ambient);
    if (res != SIB_OK) { return res; }
    res = sib_read_color(r, end, &mtl->specular);
    if (res != SIB_OK) { return res; }
    res = sib_read_color(r, end, &mtl->emissive);
    if (res != SIB_OK) { return res; }
    res = sib_read_u32(r, end, &mtl->shininess_raw);
    if (res != SIB_OK) { return res; }
    res = sib_read_u32(r, end, &name_len);
    if (res != SIB_OK) { return res; }
    if (name_len > end - r->pos) {
        return SIB_ERR_TRUNCATED;
    }
    sib_utf16le_read_ascii(r, r->pos + name_len, name_len, mtl->name, SIB_CFG_MAX_NAME);
    res = sib_read_u32(r, end, &tex_len);
    if (res != SIB_OK) { return res; }
    if (tex_len > end - r->pos) {
        return SIB_ERR_TRUNCATED;
    }
    sib_utf16le_read_ascii(r, r->pos + tex_len, tex_len, mtl->texture, SIB_CFG_MAX_TEXTURE);
    scene->material_count++;
    return SIB_OK;
}

static SIB_Result sib_read_light_info(SIB_Light *light, SIB_Reader *r, SIB_U32 end) {
    SIB_U32 type;
    SIB_Result res;
    res = sib_read_u32(r, end, &type);
    if (res != SIB_OK) { return res; }
    if (type == 0u) {
        light->type = SIB_LIGHT_POINT;
    } else if (type == 1u) {
        light->type = SIB_LIGHT_SPOT;
    } else if (type == 2u) {
        light->type = SIB_LIGHT_DIRECTIONAL;
    } else {
        light->type = SIB_LIGHT_UNDEFINED;
    }
    res = sib_read_f32_fixed(r, end, &light->position.x);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &light->position.y);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &light->position.z);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &light->direction.x);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &light->direction.y);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &light->direction.z);
    if (res != SIB_OK) { return res; }
    res = sib_read_color(r, end, &light->diffuse);
    if (res != SIB_OK) { return res; }
    res = sib_read_color(r, end, &light->ambient);
    if (res != SIB_OK) { return res; }
    res = sib_read_color(r, end, &light->specular);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &light->spot_exponent);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &light->spot_cutoff);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &light->attenuation_constant);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &light->attenuation_linear);
    if (res != SIB_OK) { return res; }
    res = sib_read_f32_fixed(r, end, &light->attenuation_quadratic);
    return res;
}

static SIB_Result sib_read_light(SIB_Scene *scene, SIB_Reader *r, SIB_U32 end, SIB_Diagnostics *diag) {
    SIB_Light *light;
    SIB_Chunk chunk;
    SIB_Result res;
    if (scene->light_count >= SIB_CFG_MAX_LIGHTS) {
        return SIB_ERR_CAPACITY;
    }
    light = &scene->lights[scene->light_count];
    sib_zero(light, (SIB_U32)sizeof(*light));
    light->type = SIB_LIGHT_UNDEFINED;
    sib_copy_text(light->name, SIB_CFG_MAX_NAME, "SIBLight");
    while (r->pos < end) {
        res = sib_read_chunk(r, end, &chunk);
        if (res != SIB_OK) { return res; }
        switch (chunk.tag) {
            case SIB_TAG('L','N','F','O'):
                res = sib_read_light_info(light, r, chunk.end);
                if (res != SIB_OK) { return res; }
                break;
            case SIB_TAG('S','N','A','M'):
                sib_utf16le_read_ascii(r, chunk.end, chunk.size, light->name, SIB_CFG_MAX_NAME);
                break;
            default:
                sib_warn_tag(diag, "light unknown chunk", chunk.tag);
                break;
        }
        res = sib_skip_to(r, chunk.end);
        if (res != SIB_OK) { return res; }
    }
    scene->light_count++;
    return SIB_OK;
}

static SIB_Result sib_read_instance(SIB_Scene *scene, SIB_Reader *r, SIB_U32 end, SIB_Diagnostics *diag) {
    SIB_Object inst;
    SIB_Chunk chunk;
    SIB_Result res;
    SIB_U32 shape_index;
    if (scene->object_count >= SIB_CFG_MAX_OBJECTS) {
        return SIB_ERR_CAPACITY;
    }
    sib_zero(&inst, (SIB_U32)sizeof(inst));
    sib_mat4_identity(&inst.axis);
    sib_copy_text(inst.name, SIB_CFG_MAX_NAME, "SIBInstance");
    shape_index = 0u;
    while (r->pos < end) {
        res = sib_read_chunk(r, end, &chunk);
        if (res != SIB_OK) { return res; }
        switch (chunk.tag) {
            case SIB_TAG('D','I','N','F'):
            case SIB_TAG('P','I','N','F'):
                break;
            case SIB_TAG('A','X','I','S'):
                res = sib_read_axis(r, chunk.end, &inst.axis);
                if (res != SIB_OK) { return res; }
                break;
            case SIB_TAG('I','N','S','I'):
                res = sib_read_u32(r, chunk.end, &shape_index);
                if (res != SIB_OK) { return res; }
                break;
            case SIB_TAG('S','M','T','X'):
                res = sib_read_scale_matrix(r, chunk.end, &inst.axis);
                if (res != SIB_OK) { return res; }
                break;
            case SIB_TAG('S','N','A','M'):
                sib_utf16le_read_ascii(r, chunk.end, chunk.size, inst.name, SIB_CFG_MAX_NAME);
                break;
            default:
                sib_warn_tag(diag, "instance unknown chunk", chunk.tag);
                break;
        }
        res = sib_skip_to(r, chunk.end);
        if (res != SIB_OK) { return res; }
    }
    if (shape_index >= scene->mesh_count) {
        return SIB_ERR_BAD_INDEX;
    }
    inst.mesh = shape_index;
    inst.is_instance = 1u;
    scene->objects[scene->object_count] = inst;
    scene->object_count++;
    return SIB_OK;
}

static SIB_Result sib_read_head(SIB_Reader *r, SIB_U32 end, SIB_Diagnostics *diag) {
    SIB_U32 version;
    SIB_Result res;
    res = sib_read_u32(r, end, &version);
    if (res != SIB_OK) {
        return res;
    }
    if (diag != 0) {
        diag->file_version = version;
    }
    if (version < 1u || version > 2u) {
        return SIB_ERR_UNSUPPORTED_VERSION;
    }
    return SIB_OK;
}

SIB_Result sib_decode_memory(const SIB_U8 *data, SIB_U32 size, SIB_Scene *scene, SIB_Diagnostics *diag) {
    SIB_Reader r;
    SIB_Chunk chunk;
    SIB_Result res;
    SIB_U32 i;

    if (diag != 0) {
        sib_diagnostics_init(diag);
    }
    if (data == 0 || scene == 0) {
        sib_set_error(diag, SIB_ERR_NULL, "null input");
        return SIB_ERR_NULL;
    }
    if (size < 8u) {
        sib_set_error(diag, SIB_ERR_TRUNCATED, "file too small");
        return SIB_ERR_TRUNCATED;
    }
    sib_scene_init(scene);
    r.data = data;
    r.size = size;
    r.pos = 0u;

    while (r.pos < size) {
        res = sib_read_chunk(&r, size, &chunk);
        if (res != SIB_OK) {
            sib_set_error(diag, res, "bad or truncated chunk header");
            return res;
        }
        switch (chunk.tag) {
            case SIB_TAG('H','E','A','D'):
                res = sib_read_head(&r, chunk.end, diag);
                if (res != SIB_OK) {
                    sib_set_error(diag, res, "unsupported or truncated SIB version");
                    return res;
                }
                break;
            case SIB_TAG('S','H','A','P'):
                res = sib_read_shape(scene, &r, chunk.end, diag);
                if (res != SIB_OK) {
                    sib_set_error(diag, res, "shape decode failed");
                    return res;
                }
                break;
            case SIB_TAG('G','R','P','S'):
            case SIB_TAG('T','E','X','P'):
                break;
            case SIB_TAG('I','N','S','T'):
                res = sib_read_instance(scene, &r, chunk.end, diag);
                if (res != SIB_OK) {
                    sib_set_error(diag, res, "instance decode failed");
                    return res;
                }
                break;
            case SIB_TAG('M','A','T','R'):
                res = sib_read_material(scene, &r, chunk.end);
                if (res != SIB_OK) {
                    sib_set_error(diag, res, "material decode failed");
                    return res;
                }
                break;
            case SIB_TAG('L','G','H','T'):
                res = sib_read_light(scene, &r, chunk.end, diag);
                if (res != SIB_OK) {
                    sib_set_error(diag, res, "light decode failed");
                    return res;
                }
                break;
            default:
                sib_warn_tag(diag, "scene unknown chunk", chunk.tag);
                break;
        }
        res = sib_skip_to(&r, chunk.end);
        if (res != SIB_OK) {
            sib_set_error(diag, res, "chunk skip failed");
            return res;
        }
    }

    for (i = 0u; i < scene->face_count; ++i) {
        if (scene->faces[i].material >= scene->material_count) {
            sib_warn_text(diag, "face material index exceeded material table; reset to default");
            scene->faces[i].material = 0u;
        }
    }
    if (diag != 0) {
        diag->result = SIB_OK;
    }
    return SIB_OK;
}
