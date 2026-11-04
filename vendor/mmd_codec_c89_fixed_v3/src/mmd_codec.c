#include "mmd_codec.h"
#include <limits.h>
#include <string.h>

#if UINT_MAX != 4294967295U
#error mmd_codec requires 32-bit unsigned int in this C89 configuration.
#endif
#if USHRT_MAX != 65535U
#error mmd_codec requires 16-bit unsigned short in this C89 configuration.
#endif

typedef struct MMD_Rd {
    const mmd_u8 *p;
    mmd_u32 size;
    mmd_u32 pos;
    int err;
} MMD_Rd;

typedef struct MMD_PmxGlobals {
    mmd_u8 text_encoding;
    mmd_u8 add_vec4_count;
    mmd_u8 vertex_index_size;
    mmd_u8 texture_index_size;
    mmd_u8 material_index_size;
    mmd_u8 bone_index_size;
    mmd_u8 morph_index_size;
    mmd_u8 rigid_index_size;
} MMD_PmxGlobals;

static void rd_init(MMD_Rd *r, const mmd_u8 *data, mmd_u32 size)
{
    r->p = data;
    r->size = size;
    r->pos = 0u;
    r->err = MMD_OK;
}

static int rd_need(MMD_Rd *r, mmd_u32 n)
{
    if (r->err != MMD_OK) return 0;
    if (n > r->size || r->pos > (r->size - n)) {
        r->err = MMD_ERR_SHORT;
        return 0;
    }
    return 1;
}

static mmd_u8 rd_u8(MMD_Rd *r)
{
    mmd_u8 v;
    if (!rd_need(r, 1u)) return 0u;
    v = r->p[r->pos];
    r->pos += 1u;
    return v;
}

static mmd_u16 rd_u16(MMD_Rd *r)
{
    mmd_u16 a;
    mmd_u16 b;
    if (!rd_need(r, 2u)) return 0u;
    a = (mmd_u16)r->p[r->pos];
    b = (mmd_u16)r->p[r->pos + 1u];
    r->pos += 2u;
    return (mmd_u16)(a | (mmd_u16)(b << 8));
}

static mmd_u32 rd_u32(MMD_Rd *r)
{
    mmd_u32 a;
    mmd_u32 b;
    mmd_u32 c;
    mmd_u32 d;
    if (!rd_need(r, 4u)) return 0u;
    a = (mmd_u32)r->p[r->pos];
    b = (mmd_u32)r->p[r->pos + 1u];
    c = (mmd_u32)r->p[r->pos + 2u];
    d = (mmd_u32)r->p[r->pos + 3u];
    r->pos += 4u;
    return a | (b << 8) | (c << 16) | (d << 24);
}

static mmd_i16 rd_i16(MMD_Rd *r)
{
    mmd_u16 u;
    u = rd_u16(r);
    if (u >= 0x8000u) return (mmd_i16)((mmd_i32)u - 65536);
    return (mmd_i16)u;
}

static mmd_i32 rd_i32(MMD_Rd *r)
{
    mmd_u32 u;
    u = rd_u32(r);
    if (u >= 0x80000000u) return (mmd_i32)(-((mmd_i32)(0xffffffffu - u) + 1));
    return (mmd_i32)u;
}

static void rd_skip(MMD_Rd *r, mmd_u32 n)
{
    if (rd_need(r, n)) r->pos += n;
}

static int mem_eq(const mmd_u8 *a, const char *b, mmd_u32 n)
{
    mmd_u32 i;
    for (i = 0u; i < n; ++i) {
        if (a[i] != (mmd_u8)b[i]) return 0;
    }
    return 1;
}

static mmd_fp fp_sat_pos(void) { return (mmd_fp)0x7fffffffu; }
static mmd_fp fp_sat_neg(void) { return (mmd_fp)0x80000000u; }

static mmd_fp mmd_fp_from_ieee754_u32(mmd_u32 bits)
{
    mmd_u32 sign;
    mmd_u32 exp_raw;
    mmd_u32 mant;
    int exp;
    int shift;
    mmd_u32 q;

    sign = bits >> 31;
    exp_raw = (bits >> 23) & 0xffu;
    mant = bits & 0x7fffffu;

    if (exp_raw == 0u) return 0;
    if (exp_raw == 255u) return sign ? fp_sat_neg() : fp_sat_pos();

    mant |= 0x800000u;
    exp = (int)exp_raw - 127;
    shift = exp - 7;

    if (shift >= 8) return sign ? fp_sat_neg() : fp_sat_pos();

    if (shift >= 0) {
        q = mant << shift;
        if (q > 0x7fffffffu) return sign ? fp_sat_neg() : fp_sat_pos();
    } else {
        int rshift;
        mmd_u32 round;
        rshift = -shift;
        if (rshift >= 31) {
            q = 0u;
        } else {
            round = (rshift > 0) ? (1u << (rshift - 1)) : 0u;
            q = (mant + round) >> rshift;
        }
    }

    if (sign) {
        if (q >= 0x80000000u) return fp_sat_neg();
        return (mmd_fp)(-(mmd_i32)q);
    }
    return (mmd_fp)q;
}

static mmd_fp rd_fp(MMD_Rd *r)
{
    return mmd_fp_from_ieee754_u32(rd_u32(r));
}

static void rd_vec2(MMD_Rd *r, MMD_Vec2 *v)
{
    v->x = rd_fp(r);
    v->y = rd_fp(r);
}

static void rd_vec3(MMD_Rd *r, MMD_Vec3 *v)
{
    v->x = rd_fp(r);
    v->y = rd_fp(r);
    v->z = rd_fp(r);
}

static void rd_vec4(MMD_Rd *r, MMD_Vec4 *v)
{
    v->x = rd_fp(r);
    v->y = rd_fp(r);
    v->z = rd_fp(r);
    v->w = rd_fp(r);
}

static void vec3_zero(MMD_Vec3 *v)
{
    v->x = 0;
    v->y = 0;
    v->z = 0;
}

static void vec4_zero(MMD_Vec4 *v)
{
    v->x = 0;
    v->y = 0;
    v->z = 0;
    v->w = 0;
}

static void clear_name(char *dst, mmd_u32 cap)
{
    if (cap > 0u) dst[0] = '\0';
}

static void copy_bytes_name(char *dst, mmd_u32 cap, const mmd_u8 *src, mmd_u32 n, mmd_u32 *flags)
{
    mmd_u32 i;
    mmd_u32 lim;
    if (cap == 0u) return;
    lim = n;
    if (lim >= cap) {
        lim = cap - 1u;
        if (flags != (mmd_u32 *)0) *flags |= MMD_FLAG_TRUNCATED_TEXT;
    }
    for (i = 0u; i < lim; ++i) {
        mmd_u8 c;
        c = src[i];
        if (c == 0u) break;
        dst[i] = (char)c;
    }
    dst[i] = '\0';
}

static void copy_fixed_name(MMD_Rd *r, char *dst, mmd_u32 cap, mmd_u32 fixed_len, mmd_u32 *flags)
{
    if (!rd_need(r, fixed_len)) {
        clear_name(dst, cap);
        return;
    }
    copy_bytes_name(dst, cap, r->p + r->pos, fixed_len, flags);
    r->pos += fixed_len;
}

static void skip_pmx_text(MMD_Rd *r)
{
    mmd_i32 len_i;
    len_i = rd_i32(r);
    if (r->err != MMD_OK) return;
    if (len_i < 0) {
        r->err = MMD_ERR_CORRUPT;
        return;
    }
    rd_skip(r, (mmd_u32)len_i);
}

static void utf8_emit(char *dst, mmd_u32 cap, mmd_u32 *out, mmd_u32 cp, mmd_u32 *flags)
{
    mmd_u8 tmp[4];
    mmd_u32 n;
    mmd_u32 i;
    if (cap == 0u || dst == (char *)0) return;
    if (cp == 0u) return;
    if (cp > 0x10ffffu || (cp >= 0xd800u && cp <= 0xdfffu)) {
        cp = (mmd_u32)'?';
        if (flags != (mmd_u32 *)0) *flags |= MMD_FLAG_INVALID_UNICODE;
    }
    if (cp < 0x80u) {
        tmp[0] = (mmd_u8)cp;
        n = 1u;
    } else if (cp < 0x800u) {
        tmp[0] = (mmd_u8)(0xc0u | (cp >> 6));
        tmp[1] = (mmd_u8)(0x80u | (cp & 0x3fu));
        n = 2u;
    } else if (cp < 0x10000u) {
        tmp[0] = (mmd_u8)(0xe0u | (cp >> 12));
        tmp[1] = (mmd_u8)(0x80u | ((cp >> 6) & 0x3fu));
        tmp[2] = (mmd_u8)(0x80u | (cp & 0x3fu));
        n = 3u;
    } else {
        tmp[0] = (mmd_u8)(0xf0u | (cp >> 18));
        tmp[1] = (mmd_u8)(0x80u | ((cp >> 12) & 0x3fu));
        tmp[2] = (mmd_u8)(0x80u | ((cp >> 6) & 0x3fu));
        tmp[3] = (mmd_u8)(0x80u | (cp & 0x3fu));
        n = 4u;
    }
    if ((*out + n) >= cap) {
        if (flags != (mmd_u32 *)0) *flags |= MMD_FLAG_TRUNCATED_TEXT;
        return;
    }
    for (i = 0u; i < n; ++i) dst[*out + i] = (char)tmp[i];
    *out += n;
}

static void read_pmx_text(MMD_Rd *r, const MMD_PmxGlobals *g, char *dst, mmd_u32 cap, mmd_u32 *flags)
{
    mmd_i32 len_i;
    mmd_u32 len;
    mmd_u32 i;
    mmd_u32 out;

    if (cap > 0u && dst != (char *)0) dst[0] = '\0';
    len_i = rd_i32(r);
    if (r->err != MMD_OK) return;
    if (len_i < 0) {
        r->err = MMD_ERR_CORRUPT;
        return;
    }
    len = (mmd_u32)len_i;
    if (!rd_need(r, len)) return;

    if (cap == 0u || dst == (char *)0) {
        r->pos += len;
        return;
    }

    out = 0u;
    if (g->text_encoding == 0u) {
        i = 0u;
        while ((i + 1u) < len) {
            mmd_u32 u;
            mmd_u32 cp;
            u = (mmd_u32)r->p[r->pos + i] | ((mmd_u32)r->p[r->pos + i + 1u] << 8);
            if (u == 0u) break;
            if (u >= 0xd800u && u <= 0xdbffu) {
                if ((i + 3u) < len) {
                    mmd_u32 lo;
                    lo = (mmd_u32)r->p[r->pos + i + 2u] | ((mmd_u32)r->p[r->pos + i + 3u] << 8);
                    if (lo >= 0xdc00u && lo <= 0xdfffu) {
                        cp = 0x10000u + (((u - 0xd800u) << 10) | (lo - 0xdc00u));
                        utf8_emit(dst, cap, &out, cp, flags);
                        i += 4u;
                        continue;
                    }
                }
                if (flags != (mmd_u32 *)0) *flags |= MMD_FLAG_INVALID_UNICODE;
                utf8_emit(dst, cap, &out, (mmd_u32)'?', flags);
            } else if (u >= 0xdc00u && u <= 0xdfffu) {
                if (flags != (mmd_u32 *)0) *flags |= MMD_FLAG_INVALID_UNICODE;
                utf8_emit(dst, cap, &out, (mmd_u32)'?', flags);
            } else {
                utf8_emit(dst, cap, &out, u, flags);
            }
            i += 2u;
        }
        if ((len & 1u) != 0u && flags != (mmd_u32 *)0) *flags |= MMD_FLAG_INVALID_UNICODE;
    } else {
        i = 0u;
        while (i < len) {
            mmd_u8 c;
            c = r->p[r->pos + i];
            if (c == 0u) break;
            if ((out + 1u) >= cap) {
                if (flags != (mmd_u32 *)0) *flags |= MMD_FLAG_TRUNCATED_TEXT;
            } else {
                dst[out++] = (char)c;
            }
            ++i;
        }
    }
    dst[out] = '\0';
    r->pos += len;
}

static void pmx_read_name_pair(MMD_Rd *r, const MMD_PmxGlobals *g, char *name, char *ename, mmd_u32 *flags)
{
    if (name != (char *)0) read_pmx_text(r, g, name, MMD_NAME_MAX, flags);
    else skip_pmx_text(r);
    if (ename != (char *)0) read_pmx_text(r, g, ename, MMD_NAME_MAX, flags);
    else skip_pmx_text(r);
}

static mmd_i32 read_pmx_index(MMD_Rd *r, mmd_u8 size, int is_vertex)
{
    mmd_u8 u8;
    mmd_u16 u16;
    if (size == 1u) {
        u8 = rd_u8(r);
        if (is_vertex) return (mmd_i32)u8;
        if (u8 >= 128u) return (mmd_i32)u8 - 256;
        return (mmd_i32)u8;
    }
    if (size == 2u) {
        u16 = rd_u16(r);
        if (is_vertex) return (mmd_i32)u16;
        if (u16 >= 32768u) return (mmd_i32)u16 - 65536;
        return (mmd_i32)u16;
    }
    if (size == 4u) return rd_i32(r);
    r->err = MMD_ERR_UNSUPPORTED;
    return -1;
}

static void set_default_vertex(MMD_Vertex *v)
{
    int i;
    v->position.x = 0;
    v->position.y = 0;
    v->position.z = 0;
    v->normal.x = 0;
    v->normal.y = MMD_FP_ONE;
    v->normal.z = 0;
    v->uv.x = 0;
    v->uv.y = 0;
    for (i = 0; i < MMD_MAX_EXTRA_UV; ++i) vec4_zero(&v->extra_uv[i]);
    for (i = 0; i < 4; ++i) {
        v->bone[i] = -1;
        v->weight[i] = 0;
    }
    v->deform = 0u;
    v->edge_flag = 0u;
    v->edge_scale = MMD_FP_ONE;
    vec3_zero(&v->sdef_c);
    vec3_zero(&v->sdef_r0);
    vec3_zero(&v->sdef_r1);
}

static void morph_offset_zero(MMD_MorphOffset *o)
{
    o->morph = 0;
    o->type = 0u;
    o->op = 0u;
    o->target = -1;
    vec4_zero(&o->a);
    vec4_zero(&o->b);
    vec4_zero(&o->c);
    vec4_zero(&o->d);
    vec4_zero(&o->e);
    vec4_zero(&o->f);
    vec4_zero(&o->g);
    vec4_zero(&o->h);
}

static int count_bad(mmd_i32 n)
{
    return n < 0;
}

static int char_lower(int c)
{
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static int str_ieq_tail(const char *s, const char *tail)
{
    mmd_u32 slen;
    mmd_u32 tlen;
    mmd_u32 i;
    slen = 0u;
    tlen = 0u;
    while (s[slen] != '\0') ++slen;
    while (tail[tlen] != '\0') ++tlen;
    if (slen < tlen) return 0;
    for (i = 0u; i < tlen; ++i) {
        if (char_lower((int)s[slen - tlen + i]) != char_lower((int)tail[i])) return 0;
    }
    return 1;
}

static int register_texture(MMD_Model *m, const char *path)
{
    mmd_i32 i;
    mmd_u32 j;
    if (path == (const char *)0 || path[0] == '\0') return -1;
    for (i = 0; i < m->texture_count; ++i) {
        if (strncmp(m->textures[i].path, path, MMD_NAME_MAX) == 0) return i;
    }
    if (m->texture_count >= MMD_MAX_TEXTURES) {
        m->flags |= MMD_FLAG_TEXTURE_OVERFLOW;
        return -1;
    }
    for (j = 0u; j < (MMD_NAME_MAX - 1u) && path[j] != '\0'; ++j) m->textures[m->texture_count].path[j] = path[j];
    m->textures[m->texture_count].path[j] = '\0';
    ++m->texture_count;
    if (m->texture_count > m->stats.source_texture_count) m->stats.source_texture_count = m->texture_count;
    return m->texture_count - 1;
}

static void pmd_assign_material_texture(MMD_Model *m, MMD_Material *mat, const char *raw)
{
    char a[MMD_NAME_MAX];
    char b[MMD_NAME_MAX];
    mmd_u32 i;
    mmd_u32 out;
    int second;
    a[0] = '\0';
    b[0] = '\0';
    out = 0u;
    second = 0;
    for (i = 0u; raw[i] != '\0' && i < (MMD_NAME_MAX - 1u); ++i) {
        if (raw[i] == '*') {
            second = 1;
            out = 0u;
            continue;
        }
        if (!second) {
            if (out < MMD_NAME_MAX - 1u) {
                a[out++] = raw[i];
                a[out] = '\0';
            }
        } else {
            if (out < MMD_NAME_MAX - 1u) {
                b[out++] = raw[i];
                b[out] = '\0';
            }
        }
    }
    mat->texture_index = -1;
    mat->sphere_texture_index = -1;
    mat->sphere_mode = 0u;
    if (a[0] != '\0') {
        if (str_ieq_tail(a, ".sph") || str_ieq_tail(a, ".spa")) {
            mat->sphere_texture_index = register_texture(m, a);
            mat->sphere_mode = str_ieq_tail(a, ".spa") ? 2u : 1u;
        } else {
            mat->texture_index = register_texture(m, a);
        }
    }
    if (b[0] != '\0') {
        mat->sphere_texture_index = register_texture(m, b);
        mat->sphere_mode = str_ieq_tail(b, ".spa") ? 2u : 1u;
    }
}

void mmd_model_init(MMD_Model *model)
{
    if (model != (MMD_Model *)0) memset(model, 0, sizeof(MMD_Model));
}

void mmd_motion_init(MMD_Motion *motion)
{
    if (motion != (MMD_Motion *)0) memset(motion, 0, sizeof(MMD_Motion));
}

void mmd_pose_init(MMD_Pose *pose)
{
    if (pose != (MMD_Pose *)0) memset(pose, 0, sizeof(MMD_Pose));
}

int mmd_probe(const unsigned char *data, mmd_u32 size)
{
    if (data == (const unsigned char *)0) return MMD_FMT_UNKNOWN;
    if (size >= 4u && data[0] == 'P' && data[1] == 'M' && data[2] == 'X' && data[3] == ' ') return MMD_FMT_PMX;
    if (size >= 3u && data[0] == 'P' && data[1] == 'm' && data[2] == 'd') return MMD_FMT_PMD;
    if (size >= 20u && mem_eq(data, "Vocaloid Motion Data", 20u)) return MMD_FMT_VMD;
    if (size >= 23u && mem_eq(data, "Vocaloid Pose Data file", 23u)) return MMD_FMT_VPD;
    return MMD_FMT_UNKNOWN;
}

mmd_i32 mmd_fp_to_int(mmd_fp value)
{
    if (value >= 0) return (mmd_i32)((value + (MMD_FP_ONE / 2)) >> MMD_FP_SHIFT);
    return (mmd_i32)(-(((-value) + (MMD_FP_ONE / 2)) >> MMD_FP_SHIFT));
}

mmd_fp mmd_fp_from_int(mmd_i32 value)
{
    if (value > 32767) return fp_sat_pos();
    if (value < -32768) return fp_sat_neg();
    return (mmd_fp)(value << MMD_FP_SHIFT);
}

mmd_fp mmd_fp_mul(mmd_fp a, mmd_fp b)
{
    int sign;
    mmd_u32 ua;
    mmd_u32 ub;
    mmd_u32 hi;
    mmd_u32 lo;
    mmd_u32 res;
    sign = 0;
    if (a < 0) { sign = !sign; ua = (mmd_u32)(-a); } else ua = (mmd_u32)a;
    if (b < 0) { sign = !sign; ub = (mmd_u32)(-b); } else ub = (mmd_u32)b;
    hi = (ua >> 16) * (ub >> 16);
    if (hi > 0x7fffu) return sign ? fp_sat_neg() : fp_sat_pos();
    lo = ((ua & 0xffffu) * (ub & 0xffffu)) >> 16;
    res = (hi << 16) + ((ua >> 16) * (ub & 0xffffu)) + ((ub >> 16) * (ua & 0xffffu)) + lo;
    if (res > 0x7fffffffu) return sign ? fp_sat_neg() : fp_sat_pos();
    return sign ? (mmd_fp)(-(mmd_i32)res) : (mmd_fp)res;
}

mmd_fp mmd_fp_div(mmd_fp a, mmd_fp b)
{
    int sign;
    mmd_u32 ua;
    mmd_u32 ub;
    mmd_u32 q;
    mmd_u32 rem;
    mmd_u32 bit;
    if (b == 0) return a < 0 ? fp_sat_neg() : fp_sat_pos();
    sign = 0;
    if (a < 0) { sign = !sign; ua = (mmd_u32)(-a); } else ua = (mmd_u32)a;
    if (b < 0) { sign = !sign; ub = (mmd_u32)(-b); } else ub = (mmd_u32)b;
    q = 0u;
    rem = ua;
    for (bit = 0u; bit < 16u; ++bit) {
        rem <<= 1;
        q <<= 1;
        if (rem >= ub) {
            rem -= ub;
            q |= 1u;
        }
    }
    q += (ua / ub) << 16;
    if (q > 0x7fffffffu) return sign ? fp_sat_neg() : fp_sat_pos();
    return sign ? (mmd_fp)(-(mmd_i32)q) : (mmd_fp)q;
}

const char *mmd_error_string(int code)
{
    switch (code) {
    case MMD_OK: return "ok";
    case MMD_ERR_NULL: return "null argument";
    case MMD_ERR_SHORT: return "unexpected end of buffer";
    case MMD_ERR_MAGIC: return "not a supported MMD asset";
    case MMD_ERR_VERSION: return "unsupported version";
    case MMD_ERR_RANGE: return "invalid count/range";
    case MMD_ERR_UNSUPPORTED: return "unsupported field value";
    case MMD_ERR_OVERFLOW: return "asset exceeded static limits";
    case MMD_ERR_CORRUPT: return "corrupt asset data";
    default: return "unknown error";
    }
}

static int add_morph_offset(MMD_Model *m, const MMD_MorphOffset *off)
{
    if (m->morph_offset_count < MMD_MAX_MORPH_OFFSETS) {
        m->morph_offsets[m->morph_offset_count] = *off;
        ++m->morph_offset_count;
        ++m->stats.source_morph_offset_count;
        return 1;
    }
    m->flags |= MMD_FLAG_MORPH_OFFSET_OVERFLOW;
    ++m->stats.source_morph_offset_count;
    return 0;
}

static int load_pmd_optional_physics(MMD_Rd *r, MMD_Model *m)
{
    mmd_i32 n;
    mmd_i32 i;
    if (r->pos >= r->size) return MMD_OK;
    if ((r->size - r->pos) < 4u) { m->flags |= MMD_FLAG_OPTIONAL_TAIL_BYTES; return MMD_OK; }
    n = (mmd_i32)rd_u32(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_rigid_body_count = n;
    m->rigid_body_count = (n > MMD_MAX_RIGID_BODIES) ? MMD_MAX_RIGID_BODIES : n;
    if (n > MMD_MAX_RIGID_BODIES) m->flags |= MMD_FLAG_RIGID_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_RigidBody tmp;
        MMD_RigidBody *rb;
        memset(&tmp, 0, sizeof(tmp));
        rb = (i < MMD_MAX_RIGID_BODIES) ? &m->rigid_bodies[i] : &tmp;
        copy_fixed_name(r, rb->name, MMD_NAME_MAX, 20u, &m->flags);
        rb->bone = (mmd_i32)rd_u16(r);
        rb->group = rd_u8(r);
        rb->group_mask = rd_u16(r);
        rb->shape = rd_u8(r);
        rd_vec3(r, &rb->size);
        rd_vec3(r, &rb->position);
        rd_vec3(r, &rb->rotation);
        rb->mass = rd_fp(r);
        rb->linear_damping = rd_fp(r);
        rb->angular_damping = rd_fp(r);
        rb->restitution = rd_fp(r);
        rb->friction = rd_fp(r);
        rb->physics_mode = rd_u8(r);
    }
    if (r->err != MMD_OK) return r->err;
    if (r->pos >= r->size) return MMD_OK;
    if ((r->size - r->pos) < 4u) { m->flags |= MMD_FLAG_OPTIONAL_TAIL_BYTES; return MMD_OK; }
    n = (mmd_i32)rd_u32(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_joint_count = n;
    m->joint_count = (n > MMD_MAX_JOINTS) ? MMD_MAX_JOINTS : n;
    if (n > MMD_MAX_JOINTS) m->flags |= MMD_FLAG_JOINT_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_Joint tmp;
        MMD_Joint *j;
        memset(&tmp, 0, sizeof(tmp));
        j = (i < MMD_MAX_JOINTS) ? &m->joints[i] : &tmp;
        copy_fixed_name(r, j->name, MMD_NAME_MAX, 20u, &m->flags);
        j->rigid_body_a = rd_i32(r);
        j->rigid_body_b = rd_i32(r);
        rd_vec3(r, &j->position);
        rd_vec3(r, &j->rotation);
        rd_vec3(r, &j->move_limit_min);
        rd_vec3(r, &j->move_limit_max);
        rd_vec3(r, &j->rotation_limit_min);
        rd_vec3(r, &j->rotation_limit_max);
        rd_vec3(r, &j->spring_move);
        rd_vec3(r, &j->spring_rotation);
    }
    return r->err;
}

static int load_pmd(MMD_Rd *r, MMD_Model *m)
{
    mmd_i32 n;
    mmd_i32 i;
    mmd_i32 j;
    mmd_i32 face_count;
    mmd_i32 bone_count_for_english;
    mmd_i32 morph_count_for_english;
    mmd_i32 group_count_for_english;
    char raw_tex[MMD_NAME_MAX];

    m->format = MMD_FMT_PMD;
    rd_skip(r, 3u);
    m->version_q16 = rd_fp(r);
    copy_fixed_name(r, m->name, MMD_NAME_MAX, 20u, &m->flags);
    copy_fixed_name(r, m->comment, MMD_TEXT_MAX, 256u, &m->flags);
    if (r->err != MMD_OK) return r->err;

    n = (mmd_i32)rd_u32(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_vertex_count = n;
    m->vertex_count = (n > MMD_MAX_VERTICES) ? MMD_MAX_VERTICES : n;
    if (n > MMD_MAX_VERTICES) m->flags |= MMD_FLAG_VERTEX_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_Vertex tmp;
        MMD_Vertex *v;
        set_default_vertex(&tmp);
        v = (i < MMD_MAX_VERTICES) ? &m->vertices[i] : &tmp;
        rd_vec3(r, &v->position);
        rd_vec3(r, &v->normal);
        rd_vec2(r, &v->uv);
        v->bone[0] = (mmd_i32)rd_u16(r);
        v->bone[1] = (mmd_i32)rd_u16(r);
        v->weight[0] = (mmd_fp)(((mmd_i32)rd_u8(r) * MMD_FP_ONE) / 100);
        v->weight[1] = MMD_FP_ONE - v->weight[0];
        v->deform = MMD_DEFORM_PMD;
        v->edge_flag = rd_u8(r);
    }
    if (r->err != MMD_OK) return r->err;

    n = (mmd_i32)rd_u32(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_index_count = n;
    m->index_count = (n > MMD_MAX_INDICES) ? MMD_MAX_INDICES : n;
    if (n > MMD_MAX_INDICES) m->flags |= MMD_FLAG_INDEX_OVERFLOW;
    for (i = 0; i < n; ++i) {
        mmd_u32 idx;
        idx = (mmd_u32)rd_u16(r);
        if (i < MMD_MAX_INDICES) m->indices[i] = idx;
    }
    if (r->err != MMD_OK) return r->err;

    n = (mmd_i32)rd_u32(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_material_count = n;
    m->material_count = (n > MMD_MAX_MATERIALS) ? MMD_MAX_MATERIALS : n;
    if (n > MMD_MAX_MATERIALS) m->flags |= MMD_FLAG_MATERIAL_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_Material tmp;
        MMD_Material *mat;
        memset(&tmp, 0, sizeof(tmp));
        tmp.texture_index = -1;
        tmp.sphere_texture_index = -1;
        tmp.toon_index = -1;
        mat = (i < MMD_MAX_MATERIALS) ? &m->materials[i] : &tmp;
        rd_vec4(r, &mat->diffuse);
        mat->specular_strength = rd_fp(r);
        rd_vec3(r, &mat->specular);
        rd_vec3(r, &mat->ambient);
        mat->toon_index = (mmd_i32)rd_u8(r);
        mat->flags = rd_u8(r);
        mat->index_count = (mmd_i32)rd_u32(r);
        copy_fixed_name(r, raw_tex, MMD_NAME_MAX, 20u, &m->flags);
        if (i < MMD_MAX_MATERIALS) pmd_assign_material_texture(m, mat, raw_tex);
    }
    if (r->err != MMD_OK) return r->err;

    n = (mmd_i32)rd_u16(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_bone_count = n;
    bone_count_for_english = n;
    m->bone_count = (n > MMD_MAX_BONES) ? MMD_MAX_BONES : n;
    if (n > MMD_MAX_BONES) m->flags |= MMD_FLAG_BONE_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_Bone tmp;
        MMD_Bone *b;
        memset(&tmp, 0, sizeof(tmp));
        tmp.parent = -1;
        tmp.tail = -1;
        tmp.ik_target = -1;
        tmp.ik_first_link = -1;
        b = (i < MMD_MAX_BONES) ? &m->bones[i] : &tmp;
        copy_fixed_name(r, b->name, MMD_NAME_MAX, 20u, &m->flags);
        b->parent = (mmd_i32)rd_i16(r);
        b->tail = (mmd_i32)rd_i16(r);
        b->flags = (mmd_u16)rd_u8(r);
        b->ik_target = (mmd_i32)rd_i16(r);
        rd_vec3(r, &b->position);
    }
    if (r->err != MMD_OK) return r->err;

    n = (mmd_i32)rd_u16(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    for (i = 0; i < n; ++i) {
        mmd_i32 link_count;
        mmd_i32 first_link;
        mmd_i32 target;
        mmd_i32 effector;
        target = (mmd_i32)rd_i16(r);
        effector = (mmd_i32)rd_i16(r);
        link_count = (mmd_i32)rd_u8(r);
        first_link = m->ik_link_count;
        if (target >= 0 && target < m->bone_count) {
            m->bones[target].ik_target = effector;
            m->bones[target].ik_first_link = first_link;
            m->bones[target].ik_link_count = link_count;
            m->bones[target].ik_loop_count = (mmd_i32)rd_u16(r);
            m->bones[target].ik_limit_radian = rd_fp(r);
        } else {
            rd_skip(r, 2u + 4u);
        }
        for (j = 0; j < link_count; ++j) {
            MMD_IKLink link;
            link.bone = (mmd_i32)rd_i16(r);
            link.has_limit = 0u;
            vec3_zero(&link.limit_min);
            vec3_zero(&link.limit_max);
            if (m->ik_link_count < MMD_MAX_IK_LINKS) {
                m->ik_links[m->ik_link_count] = link;
                ++m->ik_link_count;
            } else {
                m->flags |= MMD_FLAG_IK_OVERFLOW;
            }
            ++m->stats.source_ik_link_count;
        }
    }
    if (r->err != MMD_OK) return r->err;

    face_count = (mmd_i32)rd_u16(r);
    if (count_bad(face_count)) return MMD_ERR_RANGE;
    m->stats.source_morph_count = face_count;
    morph_count_for_english = face_count;
    m->morph_count = (face_count > MMD_MAX_MORPHS) ? MMD_MAX_MORPHS : face_count;
    if (face_count > MMD_MAX_MORPHS) m->flags |= MMD_FLAG_MORPH_OVERFLOW;
    for (i = 0; i < face_count; ++i) {
        MMD_Morph tmp_m;
        MMD_Morph *mo;
        mmd_i32 vcount;
        memset(&tmp_m, 0, sizeof(tmp_m));
        mo = (i < MMD_MAX_MORPHS) ? &m->morphs[i] : &tmp_m;
        copy_fixed_name(r, mo->name, MMD_NAME_MAX, 20u, &m->flags);
        vcount = (mmd_i32)rd_u32(r);
        mo->type = rd_u8(r);
        mo->panel = 1u;
        mo->first_offset = m->morph_offset_count;
        mo->offset_count = vcount;
        if (vcount < 0) return MMD_ERR_CORRUPT;
        for (j = 0; j < vcount; ++j) {
            MMD_MorphOffset off;
            morph_offset_zero(&off);
            off.morph = i;
            off.type = MMD_MORPH_VERTEX;
            off.target = (mmd_i32)rd_u32(r);
            rd_vec3(r, (MMD_Vec3 *)&off.a);
            add_morph_offset(m, &off);
        }
    }
    if (r->err != MMD_OK) return r->err;

    n = (mmd_i32)rd_u8(r);
    for (i = 0; i < n; ++i) (void)rd_u16(r);
    n = (mmd_i32)rd_u8(r);
    group_count_for_english = n;
    for (i = 0; i < n; ++i) copy_fixed_name(r, raw_tex, MMD_NAME_MAX, 50u, &m->flags);
    n = (mmd_i32)rd_u32(r);
    for (i = 0; i < n; ++i) rd_skip(r, 3u);
    if (r->err != MMD_OK) return r->err;

    if (r->pos < r->size) {
        mmd_u8 english;
        english = rd_u8(r);
        if (r->err != MMD_OK) return r->err;
        if (english != 0u) {
            copy_fixed_name(r, m->english_name, MMD_NAME_MAX, 20u, &m->flags);
            copy_fixed_name(r, m->english_comment, MMD_TEXT_MAX, 256u, &m->flags);
            for (i = 0; i < bone_count_for_english; ++i) {
                if (i < MMD_MAX_BONES) copy_fixed_name(r, m->bones[i].english_name, MMD_NAME_MAX, 20u, &m->flags);
                else copy_fixed_name(r, raw_tex, MMD_NAME_MAX, 20u, &m->flags);
            }
            for (i = 0; i < morph_count_for_english; ++i) {
                if (i < MMD_MAX_MORPHS) copy_fixed_name(r, m->morphs[i].english_name, MMD_NAME_MAX, 20u, &m->flags);
                else copy_fixed_name(r, raw_tex, MMD_NAME_MAX, 20u, &m->flags);
            }
            for (i = 0; i < group_count_for_english; ++i) copy_fixed_name(r, raw_tex, MMD_NAME_MAX, 50u, &m->flags);
        }
    }
    if (r->err != MMD_OK) return r->err;

    if (r->pos < r->size) {
        for (i = 0; i < 10 && r->pos < r->size; ++i) copy_fixed_name(r, raw_tex, MMD_NAME_MAX, 100u, &m->flags);
    }
    if (r->err != MMD_OK) return r->err;
    return load_pmd_optional_physics(r, m);
}

static int read_pmx_header(MMD_Rd *r, MMD_Model *m, MMD_PmxGlobals *g)
{
    mmd_u8 count;
    m->format = MMD_FMT_PMX;
    rd_skip(r, 4u);
    m->version_q16 = rd_fp(r);
    if (m->version_q16 < (2 * MMD_FP_ONE) || m->version_q16 > (mmd_fp)(2 * MMD_FP_ONE + (MMD_FP_ONE / 4))) return MMD_ERR_VERSION;
    count = rd_u8(r);
    if (count < 8u) return MMD_ERR_VERSION;
    g->text_encoding = rd_u8(r);
    g->add_vec4_count = rd_u8(r);
    g->vertex_index_size = rd_u8(r);
    g->texture_index_size = rd_u8(r);
    g->material_index_size = rd_u8(r);
    g->bone_index_size = rd_u8(r);
    g->morph_index_size = rd_u8(r);
    g->rigid_index_size = rd_u8(r);
    if (count > 8u) rd_skip(r, (mmd_u32)(count - 8u));
    if (g->add_vec4_count > MMD_MAX_EXTRA_UV) return MMD_ERR_UNSUPPORTED;
    if (g->text_encoding > 1u) return MMD_ERR_UNSUPPORTED;
    read_pmx_text(r, g, m->name, MMD_NAME_MAX, &m->flags);
    read_pmx_text(r, g, m->english_name, MMD_NAME_MAX, &m->flags);
    read_pmx_text(r, g, m->comment, MMD_TEXT_MAX, &m->flags);
    read_pmx_text(r, g, m->english_comment, MMD_TEXT_MAX, &m->flags);
    return r->err;
}


static void soft_body_zero(MMD_SoftBody *sb)
{
    if (sb != (MMD_SoftBody *)0) {
        memset(sb, 0, sizeof(MMD_SoftBody));
        sb->material = -1;
        sb->aero_model = -1;
        sb->first_anchor = -1;
        sb->first_pin = -1;
    }
}

static void add_soft_anchor(MMD_Model *m, int keep, const MMD_SoftBodyAnchor *a, mmd_i32 *stored)
{
    ++m->stats.source_soft_body_anchor_count;
    if (!keep) return;
    if (m->soft_body_anchor_count < MMD_MAX_SOFT_BODY_ANCHORS) {
        m->soft_body_anchors[m->soft_body_anchor_count] = *a;
        ++m->soft_body_anchor_count;
        ++(*stored);
    } else {
        m->flags |= MMD_FLAG_SOFT_ANCHOR_OVERFLOW;
    }
}

static void add_soft_pin(MMD_Model *m, int keep, const MMD_SoftBodyPin *pin, mmd_i32 *stored)
{
    ++m->stats.source_soft_body_pin_count;
    if (!keep) return;
    if (m->soft_body_pin_count < MMD_MAX_SOFT_BODY_PINS) {
        m->soft_body_pins[m->soft_body_pin_count] = *pin;
        ++m->soft_body_pin_count;
        ++(*stored);
    } else {
        m->flags |= MMD_FLAG_SOFT_PIN_OVERFLOW;
    }
}

static int load_pmx(MMD_Rd *r, MMD_Model *m)
{
    MMD_PmxGlobals g;
    mmd_i32 n;
    mmd_i32 i;
    mmd_i32 j;
    int rc;
    memset(&g, 0, sizeof(g));
    rc = read_pmx_header(r, m, &g);
    if (rc != MMD_OK) return rc;
    if (r->err != MMD_OK) return r->err;

    n = rd_i32(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_vertex_count = n;
    m->vertex_count = (n > MMD_MAX_VERTICES) ? MMD_MAX_VERTICES : n;
    if (n > MMD_MAX_VERTICES) m->flags |= MMD_FLAG_VERTEX_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_Vertex tmp;
        MMD_Vertex *v;
        mmd_u8 deform;
        set_default_vertex(&tmp);
        v = (i < MMD_MAX_VERTICES) ? &m->vertices[i] : &tmp;
        rd_vec3(r, &v->position);
        rd_vec3(r, &v->normal);
        rd_vec2(r, &v->uv);
        for (j = 0; j < (mmd_i32)g.add_vec4_count; ++j) rd_vec4(r, &v->extra_uv[j]);
        deform = rd_u8(r);
        v->deform = deform;
        if (deform == MMD_DEFORM_BDEF1) {
            v->bone[0] = read_pmx_index(r, g.bone_index_size, 0);
            v->weight[0] = MMD_FP_ONE;
        } else if (deform == MMD_DEFORM_BDEF2) {
            v->bone[0] = read_pmx_index(r, g.bone_index_size, 0);
            v->bone[1] = read_pmx_index(r, g.bone_index_size, 0);
            v->weight[0] = rd_fp(r);
            v->weight[1] = MMD_FP_ONE - v->weight[0];
        } else if (deform == MMD_DEFORM_BDEF4 || deform == MMD_DEFORM_QDEF) {
            for (j = 0; j < 4; ++j) v->bone[j] = read_pmx_index(r, g.bone_index_size, 0);
            for (j = 0; j < 4; ++j) v->weight[j] = rd_fp(r);
        } else if (deform == MMD_DEFORM_SDEF) {
            v->bone[0] = read_pmx_index(r, g.bone_index_size, 0);
            v->bone[1] = read_pmx_index(r, g.bone_index_size, 0);
            v->weight[0] = rd_fp(r);
            v->weight[1] = MMD_FP_ONE - v->weight[0];
            rd_vec3(r, &v->sdef_c);
            rd_vec3(r, &v->sdef_r0);
            rd_vec3(r, &v->sdef_r1);
        } else {
            m->flags |= MMD_FLAG_UNKNOWN_DEFORM;
            return MMD_ERR_UNSUPPORTED;
        }
        v->edge_scale = rd_fp(r);
    }
    if (r->err != MMD_OK) return r->err;

    n = rd_i32(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_index_count = n;
    m->index_count = (n > MMD_MAX_INDICES) ? MMD_MAX_INDICES : n;
    if (n > MMD_MAX_INDICES) m->flags |= MMD_FLAG_INDEX_OVERFLOW;
    for (i = 0; i < n; ++i) {
        mmd_i32 idx;
        idx = read_pmx_index(r, g.vertex_index_size, 1);
        if (i < MMD_MAX_INDICES) m->indices[i] = (mmd_u32)idx;
    }
    if (r->err != MMD_OK) return r->err;

    n = rd_i32(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_texture_count = n;
    m->texture_count = (n > MMD_MAX_TEXTURES) ? MMD_MAX_TEXTURES : n;
    if (n > MMD_MAX_TEXTURES) m->flags |= MMD_FLAG_TEXTURE_OVERFLOW;
    for (i = 0; i < n; ++i) {
        if (i < MMD_MAX_TEXTURES) read_pmx_text(r, &g, m->textures[i].path, MMD_NAME_MAX, &m->flags);
        else skip_pmx_text(r);
    }
    if (r->err != MMD_OK) return r->err;

    n = rd_i32(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_material_count = n;
    m->material_count = (n > MMD_MAX_MATERIALS) ? MMD_MAX_MATERIALS : n;
    if (n > MMD_MAX_MATERIALS) m->flags |= MMD_FLAG_MATERIAL_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_Material tmp;
        MMD_Material *mat;
        mmd_u8 toon_shared;
        memset(&tmp, 0, sizeof(tmp));
        tmp.texture_index = -1;
        tmp.sphere_texture_index = -1;
        tmp.toon_index = -1;
        mat = (i < MMD_MAX_MATERIALS) ? &m->materials[i] : &tmp;
        pmx_read_name_pair(r, &g, mat->name, mat->english_name, &m->flags);
        rd_vec4(r, &mat->diffuse);
        rd_vec3(r, &mat->specular);
        mat->specular_strength = rd_fp(r);
        rd_vec3(r, &mat->ambient);
        mat->flags = rd_u8(r);
        rd_vec4(r, &mat->edge_color);
        mat->edge_scale = rd_fp(r);
        mat->texture_index = read_pmx_index(r, g.texture_index_size, 0);
        mat->sphere_texture_index = read_pmx_index(r, g.texture_index_size, 0);
        mat->sphere_mode = rd_u8(r);
        toon_shared = rd_u8(r);
        if (toon_shared == 0u) mat->toon_index = read_pmx_index(r, g.texture_index_size, 0);
        else mat->toon_index = (mmd_i32)rd_u8(r);
        skip_pmx_text(r);
        mat->index_count = rd_i32(r);
    }
    if (r->err != MMD_OK) return r->err;

    n = rd_i32(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_bone_count = n;
    m->bone_count = (n > MMD_MAX_BONES) ? MMD_MAX_BONES : n;
    if (n > MMD_MAX_BONES) m->flags |= MMD_FLAG_BONE_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_Bone tmp;
        MMD_Bone *b;
        mmd_u16 flags;
        memset(&tmp, 0, sizeof(tmp));
        tmp.parent = -1;
        tmp.tail = -1;
        tmp.inherit_parent = -1;
        tmp.external_parent_key = -1;
        tmp.ik_target = -1;
        tmp.ik_first_link = -1;
        b = (i < MMD_MAX_BONES) ? &m->bones[i] : &tmp;
        pmx_read_name_pair(r, &g, b->name, b->english_name, &m->flags);
        rd_vec3(r, &b->position);
        b->parent = read_pmx_index(r, g.bone_index_size, 0);
        b->layer = rd_i32(r);
        flags = rd_u16(r);
        b->flags = flags;
        if ((flags & 0x0001u) != 0u) b->tail = read_pmx_index(r, g.bone_index_size, 0);
        else { rd_vec3(r, &b->tail_offset); b->tail = -1; }
        if ((flags & 0x0100u) != 0u || (flags & 0x0200u) != 0u) {
            b->inherit_parent = read_pmx_index(r, g.bone_index_size, 0);
            b->inherit_weight = rd_fp(r);
        }
        if ((flags & 0x0400u) != 0u) rd_vec3(r, &b->fixed_axis);
        if ((flags & 0x0800u) != 0u) { rd_vec3(r, &b->local_axis_x); rd_vec3(r, &b->local_axis_z); }
        if ((flags & 0x2000u) != 0u) b->external_parent_key = rd_i32(r);
        if ((flags & 0x0020u) != 0u) {
            mmd_i32 link_count;
            b->ik_target = read_pmx_index(r, g.bone_index_size, 0);
            b->ik_loop_count = rd_i32(r);
            b->ik_limit_radian = rd_fp(r);
            link_count = rd_i32(r);
            if (link_count < 0) return MMD_ERR_CORRUPT;
            b->ik_first_link = m->ik_link_count;
            b->ik_link_count = link_count;
            for (j = 0; j < link_count; ++j) {
                MMD_IKLink link;
                link.bone = read_pmx_index(r, g.bone_index_size, 0);
                link.has_limit = rd_u8(r);
                vec3_zero(&link.limit_min);
                vec3_zero(&link.limit_max);
                if (link.has_limit != 0u) {
                    rd_vec3(r, &link.limit_min);
                    rd_vec3(r, &link.limit_max);
                }
                if (m->ik_link_count < MMD_MAX_IK_LINKS) {
                    m->ik_links[m->ik_link_count] = link;
                    ++m->ik_link_count;
                } else {
                    m->flags |= MMD_FLAG_IK_OVERFLOW;
                }
                ++m->stats.source_ik_link_count;
            }
        }
    }
    if (r->err != MMD_OK) return r->err;

    n = rd_i32(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_morph_count = n;
    m->morph_count = (n > MMD_MAX_MORPHS) ? MMD_MAX_MORPHS : n;
    if (n > MMD_MAX_MORPHS) m->flags |= MMD_FLAG_MORPH_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_Morph tmp_morph;
        MMD_Morph *mo;
        mmd_u8 type;
        mmd_i32 offset_count;
        memset(&tmp_morph, 0, sizeof(tmp_morph));
        mo = (i < MMD_MAX_MORPHS) ? &m->morphs[i] : &tmp_morph;
        pmx_read_name_pair(r, &g, mo->name, mo->english_name, &m->flags);
        mo->panel = rd_u8(r);
        type = rd_u8(r);
        mo->type = type;
        offset_count = rd_i32(r);
        if (offset_count < 0) return MMD_ERR_CORRUPT;
        mo->first_offset = m->morph_offset_count;
        mo->offset_count = offset_count;
        for (j = 0; j < offset_count; ++j) {
            MMD_MorphOffset off;
            morph_offset_zero(&off);
            off.morph = i;
            off.type = type;
            if (type == MMD_MORPH_GROUP) {
                off.target = read_pmx_index(r, g.morph_index_size, 0);
                off.a.x = rd_fp(r);
            } else if (type == MMD_MORPH_VERTEX) {
                off.target = read_pmx_index(r, g.vertex_index_size, 1);
                rd_vec3(r, (MMD_Vec3 *)&off.a);
            } else if (type == MMD_MORPH_BONE) {
                off.target = read_pmx_index(r, g.bone_index_size, 0);
                rd_vec3(r, (MMD_Vec3 *)&off.a);
                rd_vec4(r, &off.b);
            } else if (type >= MMD_MORPH_UV && type <= MMD_MORPH_UV_EXT4) {
                off.target = read_pmx_index(r, g.vertex_index_size, 1);
                rd_vec4(r, &off.a);
            } else if (type == MMD_MORPH_MATERIAL) {
                off.target = read_pmx_index(r, g.material_index_size, 0);
                off.op = rd_u8(r);
                rd_vec4(r, &off.a);
                rd_vec3(r, (MMD_Vec3 *)&off.b);
                off.b.w = rd_fp(r);
                rd_vec3(r, (MMD_Vec3 *)&off.c);
                rd_vec4(r, &off.d);
                off.e.x = rd_fp(r);
                rd_vec4(r, &off.f);
                rd_vec4(r, &off.g);
                rd_vec4(r, &off.h);
            } else if (type == MMD_MORPH_FLIP) {
                off.target = read_pmx_index(r, g.morph_index_size, 0);
                off.a.x = rd_fp(r);
            } else if (type == MMD_MORPH_IMPULSE) {
                off.target = read_pmx_index(r, g.rigid_index_size, 0);
                off.op = rd_u8(r);
                rd_vec3(r, (MMD_Vec3 *)&off.a);
                rd_vec3(r, (MMD_Vec3 *)&off.b);
            } else {
                return MMD_ERR_UNSUPPORTED;
            }
            add_morph_offset(m, &off);
        }
    }
    if (r->err != MMD_OK) return r->err;

    n = rd_i32(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_display_frame_count = n;
    for (i = 0; i < n; ++i) {
        mmd_i32 frame_count;
        pmx_read_name_pair(r, &g, (char *)0, (char *)0, &m->flags);
        rd_skip(r, 1u);
        frame_count = rd_i32(r);
        if (frame_count < 0) return MMD_ERR_CORRUPT;
        for (j = 0; j < frame_count; ++j) {
            mmd_u8 frame_type;
            frame_type = rd_u8(r);
            if (frame_type == 0u) (void)read_pmx_index(r, g.bone_index_size, 0);
            else if (frame_type == 1u) (void)read_pmx_index(r, g.morph_index_size, 0);
            else return MMD_ERR_CORRUPT;
        }
    }
    if (r->err != MMD_OK) return r->err;

    n = rd_i32(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_rigid_body_count = n;
    m->rigid_body_count = (n > MMD_MAX_RIGID_BODIES) ? MMD_MAX_RIGID_BODIES : n;
    if (n > MMD_MAX_RIGID_BODIES) m->flags |= MMD_FLAG_RIGID_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_RigidBody tmp;
        MMD_RigidBody *rb;
        memset(&tmp, 0, sizeof(tmp));
        rb = (i < MMD_MAX_RIGID_BODIES) ? &m->rigid_bodies[i] : &tmp;
        pmx_read_name_pair(r, &g, rb->name, rb->english_name, &m->flags);
        rb->bone = read_pmx_index(r, g.bone_index_size, 0);
        rb->group = rd_u8(r);
        rb->group_mask = rd_u16(r);
        rb->shape = rd_u8(r);
        rd_vec3(r, &rb->size);
        rd_vec3(r, &rb->position);
        rd_vec3(r, &rb->rotation);
        rb->mass = rd_fp(r);
        rb->linear_damping = rd_fp(r);
        rb->angular_damping = rd_fp(r);
        rb->restitution = rd_fp(r);
        rb->friction = rd_fp(r);
        rb->physics_mode = rd_u8(r);
    }
    if (r->err != MMD_OK) return r->err;

    n = rd_i32(r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    m->stats.source_joint_count = n;
    m->joint_count = (n > MMD_MAX_JOINTS) ? MMD_MAX_JOINTS : n;
    if (n > MMD_MAX_JOINTS) m->flags |= MMD_FLAG_JOINT_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_Joint tmp;
        MMD_Joint *joint;
        memset(&tmp, 0, sizeof(tmp));
        joint = (i < MMD_MAX_JOINTS) ? &m->joints[i] : &tmp;
        pmx_read_name_pair(r, &g, joint->name, joint->english_name, &m->flags);
        joint->type = rd_u8(r);
        joint->rigid_body_a = read_pmx_index(r, g.rigid_index_size, 0);
        joint->rigid_body_b = read_pmx_index(r, g.rigid_index_size, 0);
        rd_vec3(r, &joint->position);
        rd_vec3(r, &joint->rotation);
        rd_vec3(r, &joint->move_limit_min);
        rd_vec3(r, &joint->move_limit_max);
        rd_vec3(r, &joint->rotation_limit_min);
        rd_vec3(r, &joint->rotation_limit_max);
        rd_vec3(r, &joint->spring_move);
        rd_vec3(r, &joint->spring_rotation);
    }
    if (r->err != MMD_OK) return r->err;

    if (r->pos < r->size) {
        n = rd_i32(r);
        if (r->err == MMD_OK) {
            if (n < 0) return MMD_ERR_RANGE;
            m->stats.source_soft_body_count = n;
            m->soft_body_count = (n > MMD_MAX_SOFT_BODIES) ? MMD_MAX_SOFT_BODIES : n;
            if (n > MMD_MAX_SOFT_BODIES) m->flags |= MMD_FLAG_SOFT_BODY_OVERFLOW;
            for (i = 0; i < n; ++i) {
                MMD_SoftBody tmp_sb;
                MMD_SoftBody *sb;
                mmd_i32 anchor_count;
                mmd_i32 pin_count;
                mmd_i32 stored_anchors;
                mmd_i32 stored_pins;
                int keep;
                soft_body_zero(&tmp_sb);
                keep = (i < MMD_MAX_SOFT_BODIES);
                sb = keep ? &m->soft_bodies[i] : &tmp_sb;
                soft_body_zero(sb);
                pmx_read_name_pair(r, &g, sb->name, sb->english_name, &m->flags);
                sb->shape = rd_u8(r);
                sb->material = read_pmx_index(r, g.material_index_size, 0);
                sb->group = rd_u8(r);
                sb->group_mask = rd_u16(r);
                sb->flags = rd_u8(r);
                sb->blink_distance = rd_i32(r);
                sb->cluster_count = rd_i32(r);
                sb->total_mass = rd_fp(r);
                sb->collision_margin = rd_fp(r);
                sb->aero_model = rd_i32(r);
                sb->vcf = rd_fp(r);
                sb->dp = rd_fp(r);
                sb->dg = rd_fp(r);
                sb->lf = rd_fp(r);
                sb->pr = rd_fp(r);
                sb->vc = rd_fp(r);
                sb->df = rd_fp(r);
                sb->mt = rd_fp(r);
                sb->chr = rd_fp(r);
                sb->khr = rd_fp(r);
                sb->shr = rd_fp(r);
                sb->ahr = rd_fp(r);
                sb->srhr_cl = rd_fp(r);
                sb->skhr_cl = rd_fp(r);
                sb->sshr_cl = rd_fp(r);
                sb->sr_splt_cl = rd_fp(r);
                sb->sk_splt_cl = rd_fp(r);
                sb->ss_splt_cl = rd_fp(r);
                sb->v_it = rd_i32(r);
                sb->p_it = rd_i32(r);
                sb->d_it = rd_i32(r);
                sb->c_it = rd_i32(r);
                sb->lst = rd_i32(r);
                sb->ast = rd_i32(r);
                sb->vst = rd_i32(r);
                anchor_count = rd_i32(r);
                if (anchor_count < 0) return MMD_ERR_CORRUPT;
                sb->first_anchor = keep ? m->soft_body_anchor_count : -1;
                stored_anchors = 0;
                for (j = 0; j < anchor_count; ++j) {
                    MMD_SoftBodyAnchor a;
                    a.rigid_body = read_pmx_index(r, g.rigid_index_size, 0);
                    a.vertex = read_pmx_index(r, g.vertex_index_size, 1);
                    a.near_mode = rd_u8(r);
                    add_soft_anchor(m, keep, &a, &stored_anchors);
                }
                sb->anchor_count = stored_anchors;
                pin_count = rd_i32(r);
                if (pin_count < 0) return MMD_ERR_CORRUPT;
                sb->first_pin = keep ? m->soft_body_pin_count : -1;
                stored_pins = 0;
                for (j = 0; j < pin_count; ++j) {
                    MMD_SoftBodyPin pin;
                    pin.vertex = read_pmx_index(r, g.vertex_index_size, 1);
                    add_soft_pin(m, keep, &pin, &stored_pins);
                }
                sb->pin_count = stored_pins;
            }
        }
    }
    return r->err;
}

int mmd_load_model(const unsigned char *data, mmd_u32 size, MMD_Model *model)
{
    MMD_Rd r;
    int fmt;
    int rc;
    if (data == (const unsigned char *)0 || model == (MMD_Model *)0) return MMD_ERR_NULL;
    mmd_model_init(model);
    fmt = mmd_probe(data, size);
    if (fmt != MMD_FMT_PMD && fmt != MMD_FMT_PMX) return MMD_ERR_MAGIC;
    rd_init(&r, data, size);
    rc = (fmt == MMD_FMT_PMD) ? load_pmd(&r, model) : load_pmx(&r, model);
    if (rc != MMD_OK) return rc;
    if (r.err != MMD_OK) return r.err;
    if (model->stats.source_index_count > 0 && model->stats.source_material_count > 0) {
        mmd_i32 sum;
        sum = 0;
        for (rc = 0; rc < model->material_count; ++rc) sum += model->materials[rc].index_count;
        if (model->material_count == model->stats.source_material_count && sum != model->stats.source_index_count) model->flags |= MMD_FLAG_COUNTER_MISMATCH;
    }
    return MMD_OK;
}

int mmd_load(const unsigned char *data, mmd_u32 size, MMD_Model *model)
{
    return mmd_load_model(data, size, model);
}

int mmd_load_vmd(const unsigned char *data, mmd_u32 size, MMD_Motion *motion)
{
    MMD_Rd r;
    mmd_i32 n;
    mmd_i32 i;
    if (data == (const unsigned char *)0 || motion == (MMD_Motion *)0) return MMD_ERR_NULL;
    mmd_motion_init(motion);
    if (mmd_probe(data, size) != MMD_FMT_VMD) return MMD_ERR_MAGIC;
    rd_init(&r, data, size);
    if (!rd_need(&r, 30u)) return r.err;
    if (mem_eq(r.p, "Vocaloid Motion Data 0002", 25u)) {
        r.pos += 30u;
        copy_fixed_name(&r, motion->model_name, MMD_NAME_MAX, 20u, &motion->flags);
    } else {
        r.pos += 30u;
        copy_fixed_name(&r, motion->model_name, MMD_NAME_MAX, 10u, &motion->flags);
    }
    if (r.err != MMD_OK) return r.err;

    n = (mmd_i32)rd_u32(&r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    motion->source_bone_key_count = n;
    motion->bone_key_count = (n > MMD_MAX_VMD_BONE_KEYS) ? MMD_MAX_VMD_BONE_KEYS : n;
    if (n > MMD_MAX_VMD_BONE_KEYS) motion->flags |= MMD_FLAG_VMD_BONE_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_VMDBoneKey tmp;
        MMD_VMDBoneKey *k;
        memset(&tmp, 0, sizeof(tmp));
        k = (i < MMD_MAX_VMD_BONE_KEYS) ? &motion->bone_keys[i] : &tmp;
        copy_fixed_name(&r, k->name, MMD_NAME_MAX, 15u, &motion->flags);
        k->frame = rd_u32(&r);
        rd_vec3(&r, &k->translation);
        rd_vec4(&r, &k->rotation);
        if (rd_need(&r, 64u)) { memcpy(k->interpolation, r.p + r.pos, 64u); r.pos += 64u; }
    }
    if (r.err != MMD_OK) return r.err;

    if (r.pos >= r.size) return MMD_OK;
    n = (mmd_i32)rd_u32(&r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    motion->source_morph_key_count = n;
    motion->morph_key_count = (n > MMD_MAX_VMD_MORPH_KEYS) ? MMD_MAX_VMD_MORPH_KEYS : n;
    if (n > MMD_MAX_VMD_MORPH_KEYS) motion->flags |= MMD_FLAG_VMD_MORPH_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_VMDMorphKey tmp;
        MMD_VMDMorphKey *k;
        memset(&tmp, 0, sizeof(tmp));
        k = (i < MMD_MAX_VMD_MORPH_KEYS) ? &motion->morph_keys[i] : &tmp;
        copy_fixed_name(&r, k->name, MMD_NAME_MAX, 15u, &motion->flags);
        k->frame = rd_u32(&r);
        k->weight = rd_fp(&r);
    }
    if (r.err != MMD_OK) return r.err;

    if (r.pos >= r.size) return MMD_OK;
    n = (mmd_i32)rd_u32(&r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    motion->source_camera_key_count = n;
    motion->camera_key_count = (n > MMD_MAX_VMD_CAMERA_KEYS) ? MMD_MAX_VMD_CAMERA_KEYS : n;
    if (n > MMD_MAX_VMD_CAMERA_KEYS) motion->flags |= MMD_FLAG_VMD_CAMERA_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_VMDCameraKey tmp;
        MMD_VMDCameraKey *k;
        memset(&tmp, 0, sizeof(tmp));
        k = (i < MMD_MAX_VMD_CAMERA_KEYS) ? &motion->camera_keys[i] : &tmp;
        k->frame = rd_u32(&r);
        k->distance = rd_fp(&r);
        rd_vec3(&r, &k->position);
        rd_vec3(&r, &k->rotation);
        if (rd_need(&r, 24u)) { memcpy(k->interpolation, r.p + r.pos, 24u); r.pos += 24u; }
        k->fov = rd_i32(&r);
        k->perspective = rd_u8(&r);
    }
    if (r.err != MMD_OK) return r.err;

    if (r.pos >= r.size) return MMD_OK;
    n = (mmd_i32)rd_u32(&r);
    if (count_bad(n)) return MMD_ERR_RANGE;
    motion->source_light_key_count = n;
    motion->light_key_count = (n > MMD_MAX_VMD_LIGHT_KEYS) ? MMD_MAX_VMD_LIGHT_KEYS : n;
    if (n > MMD_MAX_VMD_LIGHT_KEYS) motion->flags |= MMD_FLAG_VMD_LIGHT_OVERFLOW;
    for (i = 0; i < n; ++i) {
        MMD_VMDLightKey tmp;
        MMD_VMDLightKey *k;
        memset(&tmp, 0, sizeof(tmp));
        k = (i < MMD_MAX_VMD_LIGHT_KEYS) ? &motion->light_keys[i] : &tmp;
        k->frame = rd_u32(&r);
        rd_vec3(&r, &k->color);
        rd_vec3(&r, &k->direction);
    }
    if (r.err != MMD_OK) return r.err;

    if (r.pos < r.size) {
        n = (mmd_i32)rd_u32(&r);
        if (n >= 0) {
            motion->source_shadow_key_count = n;
            for (i = 0; i < n; ++i) rd_skip(&r, 4u + 1u + 4u);
        }
    }
    return r.err;
}

static int is_space_or_comma(mmd_u8 c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == ';';
}

static void line_trim_copy(char *dst, mmd_u32 cap, const mmd_u8 *src, mmd_u32 n, mmd_u32 *flags)
{
    mmd_u32 start;
    mmd_u32 end;
    start = 0u;
    while (start < n && is_space_or_comma(src[start])) ++start;
    end = n;
    while (end > start && is_space_or_comma(src[end - 1u])) --end;
    copy_bytes_name(dst, cap, src + start, end - start, flags);
}

static mmd_u32 next_line(const mmd_u8 *data, mmd_u32 size, mmd_u32 *pos, const mmd_u8 **line)
{
    mmd_u32 start;
    mmd_u32 end;
    if (*pos >= size) {
        *line = data + size;
        return 0u;
    }
    start = *pos;
    end = start;
    while (end < size && data[end] != '\n') ++end;
    *line = data + start;
    *pos = (end < size) ? end + 1u : end;
    return end - start;
}

static int line_has_char(const mmd_u8 *line, mmd_u32 n, char c)
{
    mmd_u32 i;
    for (i = 0u; i < n; ++i) if (line[i] == (mmd_u8)c) return 1;
    return 0;
}

static int parse_fixed_token(const mmd_u8 *s, mmd_u32 n, mmd_u32 *pos, mmd_fp *out)
{
    int neg;
    mmd_i32 whole;
    mmd_i32 frac;
    mmd_i32 scale;
    mmd_u32 p;
    int saw;
    p = *pos;
    while (p < n && is_space_or_comma(s[p])) ++p;
    neg = 0;
    if (p < n && (s[p] == '-' || s[p] == '+')) { neg = (s[p] == '-'); ++p; }
    whole = 0;
    saw = 0;
    while (p < n && s[p] >= '0' && s[p] <= '9') {
        saw = 1;
        if (whole < 30000) whole = (mmd_i32)(whole * 10 + (s[p] - '0'));
        ++p;
    }
    frac = 0;
    scale = 10;
    if (p < n && s[p] == '.') {
        ++p;
        while (p < n && s[p] >= '0' && s[p] <= '9' && scale <= 1000000) {
            saw = 1;
            frac += (mmd_i32)(((mmd_i32)(s[p] - '0') * MMD_FP_ONE) / scale);
            scale *= 10;
            ++p;
        }
        while (p < n && s[p] >= '0' && s[p] <= '9') ++p;
    }
    if (!saw) return 0;
    *out = (mmd_fp)((whole << MMD_FP_SHIFT) + frac);
    if (neg) *out = (mmd_fp)(-*out);
    *pos = p;
    return 1;
}

static int parse_line_vec3(const mmd_u8 *line, mmd_u32 n, MMD_Vec3 *v)
{
    mmd_u32 p;
    p = 0u;
    return parse_fixed_token(line, n, &p, &v->x) && parse_fixed_token(line, n, &p, &v->y) && parse_fixed_token(line, n, &p, &v->z);
}

static int parse_line_vec4(const mmd_u8 *line, mmd_u32 n, MMD_Vec4 *v)
{
    mmd_u32 p;
    p = 0u;
    return parse_fixed_token(line, n, &p, &v->x) && parse_fixed_token(line, n, &p, &v->y) && parse_fixed_token(line, n, &p, &v->z) && parse_fixed_token(line, n, &p, &v->w);
}

static int ascii_starts_with(const mmd_u8 *line, mmd_u32 n, const char *prefix)
{
    mmd_u32 i;
    for (i = 0u; prefix[i] != '\0'; ++i) {
        if (i >= n) return 0;
        if (line[i] != (mmd_u8)prefix[i]) return 0;
    }
    return 1;
}

static mmd_u32 strip_comment_len(const mmd_u8 *line, mmd_u32 n)
{
    mmd_u32 i;
    mmd_u32 end;
    end = n;
    for (i = 0u; i + 1u < n; ++i) {
        if (line[i] == '/' && line[i + 1u] == '/') {
            end = i;
            break;
        }
    }
    while (end > 0u && (line[end - 1u] == '\r' || line[end - 1u] == '\n' || is_space_or_comma(line[end - 1u]))) --end;
    return end;
}

static void trim_bounds(const mmd_u8 *line, mmd_u32 n, mmd_u32 *start, mmd_u32 *end)
{
    *start = 0u;
    *end = n;
    while (*start < *end && is_space_or_comma(line[*start])) ++(*start);
    while (*end > *start && is_space_or_comma(line[*end - 1u])) --(*end);
}

static int parse_uint_line(const mmd_u8 *line, mmd_u32 n, mmd_i32 *out)
{
    mmd_u32 start;
    mmd_u32 end;
    mmd_u32 p;
    mmd_i32 v;
    int saw;
    trim_bounds(line, n, &start, &end);
    p = start;
    v = 0;
    saw = 0;
    while (p < end && line[p] >= '0' && line[p] <= '9') {
        saw = 1;
        if (v < 200000000) v = (mmd_i32)(v * 10 + (line[p] - '0'));
        ++p;
    }
    while (p < end && is_space_or_comma(line[p])) ++p;
    if (!saw || p != end) return 0;
    *out = v;
    return 1;
}

static int vpd_parse_osm(const mmd_u8 *line, mmd_u32 n, char *dst, mmd_u32 cap, mmd_u32 *flags)
{
    mmd_u32 start;
    mmd_u32 end;
    mmd_u32 i;
    trim_bounds(line, n, &start, &end);
    if (end <= start + 5u) return 0;
    for (i = start; i + 4u <= end; ++i) {
        if ((line[i] == '.' || line[i] == '.')) {
            if (i + 4u <= end && char_lower(line[i + 1u]) == 'o' && char_lower(line[i + 2u]) == 's' && char_lower(line[i + 3u]) == 'm') {
                line_trim_copy(dst, cap, line + start, i + 4u - start, flags);
                return 1;
            }
        }
    }
    return 0;
}

static int vpd_open_bone(const mmd_u8 *line, mmd_u32 n, MMD_VPDBonePose *cur, mmd_u32 *flags)
{
    mmd_u32 start;
    mmd_u32 end;
    mmd_u32 p;
    mmd_u32 brace;
    trim_bounds(line, n, &start, &end);
    if (end <= start) return 0;
    p = start;
    if (!ascii_starts_with(line + p, end - p, "Bone")) {
        if (!line_has_char(line + start, end - start, '{')) return 0;
    } else {
        p += 4u;
        while (p < end && line[p] >= '0' && line[p] <= '9') ++p;
        while (p < end && is_space_or_comma(line[p])) ++p;
        if (p >= end || line[p] != '{') return 0;
    }
    brace = p;
    while (brace < end && line[brace] != '{') ++brace;
    if (brace >= end) return 0;
    memset(cur, 0, sizeof(*cur));
    if ((brace + 1u) < end) {
        line_trim_copy(cur->name, MMD_NAME_MAX, line + brace + 1u, end - brace - 1u, flags);
        if (cur->name[0] != '\0') return 1;
    }
    return 2;
}

static void vpd_commit_pose(MMD_Pose *pose, const MMD_VPDBonePose *cur)
{
    ++pose->source_bone_count;
    if (pose->bone_count < MMD_MAX_VPD_BONES) {
        pose->bones[pose->bone_count] = *cur;
        ++pose->bone_count;
    } else {
        pose->flags |= MMD_FLAG_VPD_BONE_OVERFLOW;
    }
}

int mmd_load_vpd(const unsigned char *data, mmd_u32 size, MMD_Pose *pose)
{
    mmd_u32 pos;
    const mmd_u8 *line;
    mmd_u32 len;
    mmd_u32 clean_len;
    int state;
    mmd_i32 expected_count;
    MMD_VPDBonePose cur;
    if (data == (const unsigned char *)0 || pose == (MMD_Pose *)0) return MMD_ERR_NULL;
    mmd_pose_init(pose);
    if (mmd_probe(data, size) != MMD_FMT_VPD) return MMD_ERR_MAGIC;
    pose->format = MMD_FMT_VPD;
    pos = 0u;
    state = 0;
    expected_count = -1;
    memset(&cur, 0, sizeof(cur));
    while (pos < size) {
        int opened;
        len = next_line(data, size, &pos, &line);
        clean_len = strip_comment_len(line, len);
        if (clean_len == 0u) continue;
        if (state == 0) {
            mmd_i32 parsed_count;
            if (ascii_starts_with(line, clean_len, "Vocaloid Pose Data file")) continue;
            if (vpd_parse_osm(line, clean_len, pose->model_name, MMD_NAME_MAX, &pose->flags)) continue;
            if (parse_uint_line(line, clean_len, &parsed_count)) {
                expected_count = parsed_count;
                continue;
            }
            opened = vpd_open_bone(line, clean_len, &cur, &pose->flags);
            if (opened == 1) {
                state = 2;
                continue;
            }
            if (opened == 2) {
                state = 1;
                continue;
            }
        } else if (state == 1) {
            line_trim_copy(cur.name, MMD_NAME_MAX, line, clean_len, &pose->flags);
            state = 2;
        } else if (state == 2) {
            if (parse_line_vec3(line, clean_len, &cur.translation)) {
                state = 3;
            } else {
                opened = vpd_open_bone(line, clean_len, &cur, &pose->flags);
                if (opened != 0) {
                    pose->flags |= MMD_FLAG_VPD_RECOVERED_PARSE;
                    state = (opened == 1) ? 2 : 1;
                }
            }
        } else if (state == 3) {
            if (parse_line_vec4(line, clean_len, &cur.rotation)) {
                if (line_has_char(line, clean_len, '}')) {
                    vpd_commit_pose(pose, &cur);
                    state = 0;
                } else {
                    state = 4;
                }
            }
        } else {
            if (line_has_char(line, clean_len, '}')) {
                vpd_commit_pose(pose, &cur);
                state = 0;
            }
        }
    }
    if (expected_count >= 0 && expected_count != pose->source_bone_count) pose->flags |= MMD_FLAG_VPD_COUNT_MISMATCH;
    return MMD_OK;
}
