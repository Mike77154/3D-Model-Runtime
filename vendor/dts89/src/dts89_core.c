#include "dts89.h"
#include <string.h>

static dts89_u32 dts89_align_up(dts89_u32 v, dts89_u32 a)
{
    dts89_u32 m;
    if (a == 0u) return v;
    m = a - 1u;
    return (v + m) & ~m;
}

void dts89_arena_init(dts89_arena *a, void *buffer, dts89_u32 size)
{
    if (!a) return;
    a->base = (dts89_u8*)buffer;
    a->capacity = size;
    a->used = 0u;
}

void *dts89_arena_alloc(dts89_arena *a, dts89_u32 size, dts89_u32 align)
{
    dts89_u32 pos;
    dts89_u32 mask;
    if (!a || !a->base) return 0;
    if (align == 0u) align = 1u;
    if (align < 8u) align = 8u;
    if ((align & (align - 1u)) != 0u) return 0;
    mask = align - 1u;
    if (a->used > (0xFFFFFFFFu - mask)) return 0;
    pos = dts89_align_up(a->used, align);
    if (pos > a->capacity || size > (a->capacity - pos)) return 0;
    a->used = pos + size;
    return a->base + pos;
}

void dts89_reader_init(dts89_reader *r, const void *data, dts89_u32 size)
{
    if (!r) return;
    r->data = (const dts89_u8*)data;
    r->size = size;
    r->pos = 0u;
}

void dts89_writer_init(dts89_writer *w, void *data, dts89_u32 size)
{
    if (!w) return;
    w->data = (dts89_u8*)data;
    w->size = size;
    w->pos = 0u;
}

int dts89_read_u8(dts89_reader *r, dts89_u8 *v)
{
    if (!r || !v) return DTS89_ERR_BAD_ARG;
    if (r->pos > r->size || 1u > (r->size - r->pos)) return DTS89_ERR_TRUNCATED;
    *v = r->data[r->pos];
    r->pos += 1u;
    return DTS89_OK;
}

int dts89_read_u16le(dts89_reader *r, dts89_u16 *v)
{
    dts89_u16 x;
    if (!r || !v) return DTS89_ERR_BAD_ARG;
    if (r->pos > r->size || 2u > (r->size - r->pos)) return DTS89_ERR_TRUNCATED;
    x = (dts89_u16)r->data[r->pos];
    x |= (dts89_u16)((dts89_u16)r->data[r->pos + 1u] << 8);
    *v = x;
    r->pos += 2u;
    return DTS89_OK;
}

int dts89_read_u32le(dts89_reader *r, dts89_u32 *v)
{
    dts89_u32 x;
    if (!r || !v) return DTS89_ERR_BAD_ARG;
    if (r->pos > r->size || 4u > (r->size - r->pos)) return DTS89_ERR_TRUNCATED;
    x = (dts89_u32)r->data[r->pos];
    x |= ((dts89_u32)r->data[r->pos + 1u] << 8);
    x |= ((dts89_u32)r->data[r->pos + 2u] << 16);
    x |= ((dts89_u32)r->data[r->pos + 3u] << 24);
    *v = x;
    r->pos += 4u;
    return DTS89_OK;
}

static int dts89_writer_reserve(dts89_writer *w, dts89_u32 n)
{
    if (!w) return DTS89_ERR_BAD_ARG;
    if (w->pos > w->size || n > (w->size - w->pos)) return DTS89_ERR_NO_SPACE;
    return DTS89_OK;
}

int dts89_write_u8(dts89_writer *w, dts89_u8 v)
{
    int rc;
    rc = dts89_writer_reserve(w, 1u);
    if (rc) return rc;
    if (w->data) w->data[w->pos] = v;
    w->pos += 1u;
    return DTS89_OK;
}

int dts89_write_u16le(dts89_writer *w, dts89_u16 v)
{
    int rc;
    rc = dts89_writer_reserve(w, 2u);
    if (rc) return rc;
    if (w->data) {
        w->data[w->pos] = (dts89_u8)(v & 255u);
        w->data[w->pos + 1u] = (dts89_u8)((v >> 8) & 255u);
    }
    w->pos += 2u;
    return DTS89_OK;
}

int dts89_write_u32le(dts89_writer *w, dts89_u32 v)
{
    int rc;
    rc = dts89_writer_reserve(w, 4u);
    if (rc) return rc;
    if (w->data) {
        w->data[w->pos] = (dts89_u8)(v & 255u);
        w->data[w->pos + 1u] = (dts89_u8)((v >> 8) & 255u);
        w->data[w->pos + 2u] = (dts89_u8)((v >> 16) & 255u);
        w->data[w->pos + 3u] = (dts89_u8)((v >> 24) & 255u);
    }
    w->pos += 4u;
    return DTS89_OK;
}

int dts89_write_bytes(dts89_writer *w, const void *p, dts89_u32 n)
{
    int rc;
    if (!w || (!p && n && w->data)) return DTS89_ERR_BAD_ARG;
    rc = dts89_writer_reserve(w, n);
    if (rc) return rc;
    if (n && w->data) memcpy(w->data + w->pos, p, (size_t)n);
    w->pos += n;
    return DTS89_OK;
}

dts89_fx16_16 dts89_fx_from_int(dts89_s32 v)
{
    return (dts89_fx16_16)(v << 16);
}

dts89_s32 dts89_fx_to_int(dts89_fx16_16 v)
{
    return (dts89_s32)(v >> 16);
}

static dts89_u32 dts89_u32_abs_s32(dts89_s32 v)
{
    if (v < 0) {
        return (dts89_u32)(0u - (dts89_u32)v);
    }
    return (dts89_u32)v;
}

static dts89_fx16_16 dts89_fx_apply_sign_sat(dts89_u32 mag, int neg)
{
    if (neg) {
        if (mag >= 0x80000000u) return (dts89_fx16_16)0x80000000u;
        return (dts89_fx16_16)(0u - mag);
    }
    if (mag > 0x7FFFFFFFu) return (dts89_fx16_16)0x7FFFFFFFu;
    return (dts89_fx16_16)mag;
}

static dts89_u32 dts89_u32_sat_add(dts89_u32 a, dts89_u32 b)
{
    dts89_u32 c;
    c = a + b;
    if (c < a) return 0xFFFFFFFFu;
    return c;
}

dts89_fx16_16 dts89_fx_mul(dts89_fx16_16 a, dts89_fx16_16 b)
{
    dts89_u32 ua;
    dts89_u32 ub;
    dts89_u32 ai;
    dts89_u32 bi;
    dts89_u32 af;
    dts89_u32 bf;
    dts89_u32 r;
    int neg;

    neg = ((a < 0) != (b < 0));
    ua = dts89_u32_abs_s32(a);
    ub = dts89_u32_abs_s32(b);

    ai = ua >> 16;
    bi = ub >> 16;
    af = ua & 0xFFFFu;
    bf = ub & 0xFFFFu;

    if (ai != 0u && bi > (0x7FFFFFFFu >> 16) / ai) {
        return dts89_fx_apply_sign_sat(0x80000000u, neg);
    }

    r = (ai * bi) << 16;
    r = dts89_u32_sat_add(r, ai * bf);
    r = dts89_u32_sat_add(r, bi * af);
    r = dts89_u32_sat_add(r, (af * bf) >> 16);

    return dts89_fx_apply_sign_sat(r, neg);
}

dts89_fx16_16 dts89_fx_div(dts89_fx16_16 a, dts89_fx16_16 b)
{
    dts89_u32 ua;
    dts89_u32 ub;
    dts89_u32 q;
    dts89_u32 rem;
    dts89_u32 frac;
    dts89_u32 bit;
    dts89_u32 i;
    int neg;

    if (b == 0) return 0;

    neg = ((a < 0) != (b < 0));
    ua = dts89_u32_abs_s32(a);
    ub = dts89_u32_abs_s32(b);

    q = ua / ub;
    rem = ua % ub;
    if (q >= 0x8000u) {
        return dts89_fx_apply_sign_sat(0x80000000u, neg);
    }

    frac = 0u;
    bit = 0x8000u;
    for (i = 0u; i < 16u; ++i) {
        if (rem > (0xFFFFFFFFu >> 1)) {
            rem = 0xFFFFFFFFu;
        } else {
            rem <<= 1;
        }
        if (rem >= ub) {
            rem -= ub;
            frac |= bit;
        }
        bit >>= 1;
    }

    return dts89_fx_apply_sign_sat((q << 16) | frac, neg);
}

dts89_u32 dts89_ieee32_from_raw_parts(dts89_u32 sign, dts89_s32 exp2, dts89_u32 mant24)
{
    dts89_s32 exp_field;

    if (mant24 == 0u) return sign ? 0x80000000u : 0u;

    while (mant24 && mant24 < 0x00800000u) {
        mant24 <<= 1;
        --exp2;
    }
    while (mant24 >= 0x01000000u) {
        mant24 >>= 1;
        ++exp2;
    }

    exp_field = exp2 + 127;
    if (exp_field <= 0) return sign ? 0x80000000u : 0u;
    if (exp_field >= 255) return sign ? 0xFF800000u : 0x7F800000u;

    return (sign ? 0x80000000u : 0u) |
           (((dts89_u32)exp_field) << 23) |
           (mant24 & 0x007FFFFFu);
}

dts89_fx16_16 dts89_f32bits_to_fx(dts89_u32 bits)
{
    dts89_u32 sign;
    dts89_u32 exp_field;
    dts89_u32 frac;
    dts89_u32 mant;
    dts89_s32 shift;
    dts89_u32 mag;

    sign = bits & 0x80000000u;
    exp_field = (bits >> 23) & 0xFFu;
    frac = bits & 0x007FFFFFu;

    if (exp_field == 255u) {
        return sign ? (dts89_fx16_16)0x80000000u : (dts89_fx16_16)0x7FFFFFFFu;
    }

    if (exp_field == 0u) {
        return 0;
    }

    mant = 0x00800000u | frac;
    shift = (dts89_s32)exp_field - 134;

    if (shift >= 8) {
        mag = 0x80000000u;
    } else if (shift >= 0) {
        mag = mant << (dts89_u32)shift;
    } else if (shift <= -32) {
        mag = 0u;
    } else {
        mag = mant >> (dts89_u32)(-shift);
    }

    return dts89_fx_apply_sign_sat(mag, sign != 0u);
}

dts89_u32 dts89_fx_to_f32bits(dts89_fx16_16 fx)
{
    dts89_u32 sign;
    dts89_u32 mag;
    dts89_u32 p;
    dts89_u32 t;
    dts89_u32 sig24;
    dts89_s32 exp2;

    if (fx == 0) return 0u;

    sign = (fx < 0) ? 0x80000000u : 0u;
    mag = dts89_u32_abs_s32(fx);

    p = 0u;
    t = mag;
    while (t >>= 1) ++p;

    exp2 = (dts89_s32)p - 16;
    if (p > 23u) {
        dts89_u32 lost;
        dts89_u32 add;
        lost = mag & ((1u << (p - 23u)) - 1u);
        add = 1u << (p - 24u);
        sig24 = mag >> (p - 23u);
        if (lost > add || (lost == add && (sig24 & 1u))) {
            ++sig24;
            if (sig24 == 0x01000000u) {
                sig24 >>= 1;
                ++exp2;
            }
        }
    } else {
        sig24 = mag << (23u - p);
    }

    return dts89_ieee32_from_raw_parts(sign != 0u, exp2, sig24);
}

int dts89_fx_vec2_from_bits2(const dts89_u32 *bits2, dts89_fx_vec2 *out_v)
{
    if (!bits2 || !out_v) return DTS89_ERR_BAD_ARG;
    out_v->x = dts89_f32bits_to_fx(bits2[0]);
    out_v->y = dts89_f32bits_to_fx(bits2[1]);
    return DTS89_OK;
}

int dts89_fx_vec3_from_bits3(const dts89_u32 *bits3, dts89_fx_vec3 *out_v)
{
    if (!bits3 || !out_v) return DTS89_ERR_BAD_ARG;
    out_v->x = dts89_f32bits_to_fx(bits3[0]);
    out_v->y = dts89_f32bits_to_fx(bits3[1]);
    out_v->z = dts89_f32bits_to_fx(bits3[2]);
    return DTS89_OK;
}

int dts89_shape_get_fx_bounds(const dts89_shape *shape, dts89_fx_vec3 *out_min, dts89_fx_vec3 *out_max)
{
    if (!shape || !out_min || !out_max) return DTS89_ERR_BAD_ARG;
    out_min->x = dts89_f32bits_to_fx(shape->bounds_bits[0]);
    out_min->y = dts89_f32bits_to_fx(shape->bounds_bits[1]);
    out_min->z = dts89_f32bits_to_fx(shape->bounds_bits[2]);
    out_max->x = dts89_f32bits_to_fx(shape->bounds_bits[3]);
    out_max->y = dts89_f32bits_to_fx(shape->bounds_bits[4]);
    out_max->z = dts89_f32bits_to_fx(shape->bounds_bits[5]);
    return DTS89_OK;
}

int dts89_mesh_get_fx_bounds(const dts89_mesh_view *mesh, dts89_fx_vec3 *out_min, dts89_fx_vec3 *out_max)
{
    if (!mesh || !out_min || !out_max) return DTS89_ERR_BAD_ARG;
    out_min->x = dts89_f32bits_to_fx(mesh->bounds_bits[0]);
    out_min->y = dts89_f32bits_to_fx(mesh->bounds_bits[1]);
    out_min->z = dts89_f32bits_to_fx(mesh->bounds_bits[2]);
    out_max->x = dts89_f32bits_to_fx(mesh->bounds_bits[3]);
    out_max->y = dts89_f32bits_to_fx(mesh->bounds_bits[4]);
    out_max->z = dts89_f32bits_to_fx(mesh->bounds_bits[5]);
    return DTS89_OK;
}

int dts89_mesh_get_fx_center(const dts89_mesh_view *mesh, dts89_fx_vec3 *out_center)
{
    if (!mesh || !out_center) return DTS89_ERR_BAD_ARG;
    return dts89_fx_vec3_from_bits3(mesh->center_bits, out_center);
}

int dts89_mesh_get_fx_vert(const dts89_mesh_view *mesh, dts89_s32 index, dts89_fx_vec3 *out_vert)
{
    if (!mesh || !out_vert) return DTS89_ERR_BAD_ARG;
    if (index < 0 || index >= mesh->num_verts_editable) return DTS89_ERR_RANGE;
    if (!mesh->verts_bits) return DTS89_ERR_FORMAT;
    return dts89_fx_vec3_from_bits3(mesh->verts_bits + ((dts89_u32)index * 3u), out_vert);
}

int dts89_mesh_get_fx_tvert(const dts89_mesh_view *mesh, dts89_s32 layer, dts89_s32 index, dts89_fx_vec2 *out_uv)
{
    const dts89_u32 *src;
    if (!mesh || !out_uv) return DTS89_ERR_BAD_ARG;
    if (layer == 0) {
        if (index < 0 || index >= mesh->num_tverts) return DTS89_ERR_RANGE;
        src = mesh->tverts_bits;
    } else if (layer == 1) {
        if (index < 0 || index >= mesh->num_tverts2) return DTS89_ERR_RANGE;
        src = mesh->tverts2_bits;
    } else {
        return DTS89_ERR_RANGE;
    }
    if (!src) return DTS89_ERR_FORMAT;
    return dts89_fx_vec2_from_bits2(src + ((dts89_u32)index * 2u), out_uv);
}

void dts89_bitset_clear(dts89_bitset *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
}

int dts89_bitset_read(dts89_reader *r, dts89_bitset *s)
{
    dts89_u32 dummy;
    dts89_u32 sz;
    dts89_u32 i;
    int rc;
    if (!r || !s) return DTS89_ERR_BAD_ARG;
    dts89_bitset_clear(s);
    rc = dts89_read_u32le(r, &dummy);
    if (rc) return rc;
    rc = dts89_read_u32le(r, &sz);
    if (rc) return rc;
    if (sz > DTS89_MAX_TS_SET_DWORDS) return DTS89_ERR_RANGE;
    s->size_dwords = sz;
    for (i = 0u; i < sz; ++i)
    {
        rc = dts89_read_u32le(r, &s->bits[i]);
        if (rc) return rc;
    }
    return DTS89_OK;
}

int dts89_bitset_write(dts89_writer *w, const dts89_bitset *s)
{
    dts89_u32 i;
    int rc;
    if (!w || !s) return DTS89_ERR_BAD_ARG;
    rc = dts89_write_u32le(w, 0u);
    if (rc) return rc;
    rc = dts89_write_u32le(w, s->size_dwords);
    if (rc) return rc;
    for (i = 0u; i < s->size_dwords; ++i)
    {
        rc = dts89_write_u32le(w, s->bits[i]);
        if (rc) return rc;
    }
    return DTS89_OK;
}

int dts89_bitset_test(const dts89_bitset *s, dts89_s32 bit_index)
{
    dts89_u32 word;
    dts89_u32 bit;
    if (!s || bit_index < 0) return DTS89_FALSE;
    word = (dts89_u32)bit_index >> 5;
    bit = (dts89_u32)bit_index & 31u;
    if (word >= s->size_dwords || word >= DTS89_MAX_TS_SET_DWORDS) return DTS89_FALSE;
    return (s->bits[word] & (1u << bit)) ? DTS89_TRUE : DTS89_FALSE;
}

static dts89_s32 dts89_popcount32(dts89_u32 v)
{
    dts89_s32 n;
    n = 0;
    while (v) {
        v &= v - 1u;
        ++n;
    }
    return n;
}

dts89_s32 dts89_bitset_count(const dts89_bitset *s)
{
    dts89_u32 i;
    dts89_s32 n;
    if (!s) return 0;
    n = 0;
    for (i = 0u; i < s->size_dwords && i < DTS89_MAX_TS_SET_DWORDS; ++i) {
        n += dts89_popcount32(s->bits[i]);
    }
    return n;
}

dts89_s32 dts89_bitset_count_before(const dts89_bitset *s, dts89_s32 bit_index)
{
    dts89_u32 word;
    dts89_u32 bit;
    dts89_u32 i;
    dts89_s32 n;
    if (!s || bit_index <= 0) return 0;
    word = (dts89_u32)bit_index >> 5;
    bit = (dts89_u32)bit_index & 31u;
    n = 0;
    for (i = 0u; i < word && i < s->size_dwords && i < DTS89_MAX_TS_SET_DWORDS; ++i) {
        n += dts89_popcount32(s->bits[i]);
    }
    if (word < s->size_dwords && word < DTS89_MAX_TS_SET_DWORDS && bit != 0u) {
        n += dts89_popcount32(s->bits[word] & ((1u << bit) - 1u));
    }
    return n;
}

int dts89_bitset_set(dts89_bitset *s, dts89_s32 bit_index)
{
    dts89_u32 word;
    dts89_u32 bit;
    if (!s || bit_index < 0) return DTS89_ERR_BAD_ARG;
    word = (dts89_u32)bit_index >> 5;
    bit = (dts89_u32)bit_index & 31u;
    if (word >= DTS89_MAX_TS_SET_DWORDS) return DTS89_ERR_RANGE;
    if (s->size_dwords <= word) s->size_dwords = word + 1u;
    s->bits[word] |= 1u << bit;
    return DTS89_OK;
}

const char *dts89_name_at(const dts89_shape *shape, dts89_s32 idx)
{
    if (!shape) return 0;
    if (idx < 0 || idx >= shape->num_names) return 0;
    return shape->names[idx].ptr;
}

const char *dts89_material_name_at(const dts89_shape *shape, dts89_s32 idx)
{
    if (!shape) return 0;
    if (idx < 0 || idx >= shape->num_materials) return 0;
    return shape->materials[idx].name.ptr;
}

int dts89_is_mesh_supported_semantically(const dts89_mesh_view *mesh)
{
    if (!mesh) return DTS89_FALSE;
    if (mesh->mesh_type == DTS89_MESH_STANDARD) return DTS89_TRUE;
    if (mesh->mesh_type == DTS89_MESH_SKIN) return DTS89_TRUE;
    if (mesh->mesh_type == DTS89_MESH_DECAL) return DTS89_TRUE;
    if (mesh->mesh_type == DTS89_MESH_SORTED) return DTS89_TRUE;
    if (mesh->mesh_type == DTS89_MESH_NULL) return DTS89_TRUE;
    return DTS89_FALSE;
}
