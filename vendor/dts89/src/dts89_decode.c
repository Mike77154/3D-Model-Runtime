
#include "dts89.h"
#include <string.h>

#define DTS89_DECODE_MAX_COUNT 1048576u

typedef struct dts89_pool_writer {
    dts89_u32 *p32;
    dts89_u16 *p16;
    dts89_u8 *p8;
    dts89_u32 count32;
    dts89_u32 count16;
    dts89_u32 count8;
    dts89_u32 left32;
    dts89_u32 left16;
    dts89_u32 left8;
} dts89_pool_writer;

static int pool_get32(dts89_pool_cursor *c, dts89_u32 *out)
{
    if (!c || !out) return DTS89_ERR_BAD_ARG;
    if (c->left32 == 0u) return DTS89_ERR_TRUNCATED;
    *out = *c->p32++;
    c->left32--;
    return DTS89_OK;
}

static int pool_get32s(dts89_pool_cursor *c, dts89_u32 *out, dts89_u32 n)
{
    if (!c || (!out && n)) return DTS89_ERR_BAD_ARG;
    if (c->left32 < n) return DTS89_ERR_TRUNCATED;
    if (n) memcpy(out, c->p32, (size_t)(n * 4u));
    c->p32 += n;
    c->left32 -= n;
    return DTS89_OK;
}

static int pool_get_s32(dts89_pool_cursor *c, dts89_s32 *out)
{
    dts89_u32 t;
    int rc;
    if (!out) return DTS89_ERR_BAD_ARG;
    rc = pool_get32(c, &t);
    if (rc) return rc;
    *out = (dts89_s32)t;
    return DTS89_OK;
}

static int pool_get_s32s(dts89_pool_cursor *c, dts89_s32 *out, dts89_u32 n)
{
    dts89_u32 i;
    int rc;
    if (!out && n) return DTS89_ERR_BAD_ARG;
    for (i = 0u; i < n; ++i) {
        rc = pool_get_s32(c, &out[i]);
        if (rc) return rc;
    }
    return DTS89_OK;
}

static int pool_get_quat16_to_u32s(dts89_pool_cursor *c, dts89_u32 *out, dts89_u32 count)
{
    dts89_u32 n;
    dts89_u32 i;
    if (!c || (!out && count)) return DTS89_ERR_BAD_ARG;
    if (count > 0xFFFFFFFFu / 4u) return DTS89_ERR_RANGE;
    n = count * 4u;
    if (c->left16 < n) return DTS89_ERR_TRUNCATED;
    for (i = 0u; i < n; ++i) {
        out[i] = (dts89_u32)c->p16[i];
    }
    c->p16 += n;
    c->left16 -= n;
    return DTS89_OK;
}

static int dts89_mul_u32(dts89_u32 a, dts89_u32 b, dts89_u32 *out)
{
    if (!out) return DTS89_ERR_BAD_ARG;
    if (a != 0u && b > 0xFFFFFFFFu / a) return DTS89_ERR_RANGE;
    *out = a * b;
    return DTS89_OK;
}

static dts89_u32 dts89_u16_from_le_mem(const dts89_u8 *p)
{
    return (dts89_u32)p[0] | ((dts89_u32)p[1] << 8);
}

static dts89_u32 dts89_u32_from_le_mem(const dts89_u8 *p)
{
    return (dts89_u32)p[0] |
           ((dts89_u32)p[1] << 8) |
           ((dts89_u32)p[2] << 16) |
           ((dts89_u32)p[3] << 24);
}

static int pool_get8s(dts89_pool_cursor *c, dts89_u8 **out, dts89_u32 n)
{
    if (!c || !out) return DTS89_ERR_BAD_ARG;
    if (c->left8 < n) return DTS89_ERR_TRUNCATED;
    *out = (dts89_u8*)c->p8;
    c->p8 += n;
    c->left8 -= n;
    return DTS89_OK;
}

static char *arena_strdup_z(dts89_arena *arena, const dts89_u8 *src, dts89_u32 max_n, dts89_u32 *out_len)
{
    dts89_u32 i;
    char *dst;
    if (!arena || !src) return 0;
    i = 0u;
    while (i < max_n && src[i] != 0u) i++;
    dst = (char*)dts89_arena_alloc(arena, i + 1u, 1u);
    if (!dst) return 0;
    if (i) memcpy(dst, src, (size_t)i);
    dst[i] = '\0';
    if (out_len) *out_len = i;
    return dst;
}

static int read_names(dts89_shape *shape, dts89_pool_cursor *pc, dts89_arena *arena)
{
    dts89_s32 i;
    for (i = 0; i < shape->num_names; ++i)
    {
        dts89_u32 left;
        dts89_u32 len;
        char *s;
        left = pc->left8;
        if (left == 0u) return DTS89_ERR_TRUNCATED;
        s = arena_strdup_z(arena, pc->p8, left, &len);
        if (!s) return DTS89_ERR_NO_SPACE;
        if (len >= left) return DTS89_ERR_TRUNCATED;
        shape->names[i].ptr = s;
        shape->names[i].len = len;
        pc->p8 += len + 1u;
        pc->left8 -= len + 1u;
    }
    return DTS89_OK;
}

static char *arena_strdup_n(dts89_arena *arena, const char *src, dts89_u32 n)
{
    char *dst;
    if (!arena || (!src && n)) return 0;
    dst = (char*)dts89_arena_alloc(arena, n + 1u, 1u);
    if (!dst) return 0;
    if (n) memcpy(dst, src, (size_t)n);
    dst[n] = '\0';
    return dst;
}

static char *arena_strdup_leaf(dts89_arena *arena, const char *src, dts89_u32 n, dts89_u32 *out_len)
{
    const char *start;
    dts89_u32 i;
    if (!arena || (!src && n)) return 0;
    start = src;
    for (i = 0u; i < n; ++i) {
        if (src[i] == '/' || src[i] == '\\') {
            start = src + i + 1u;
        }
    }
    n = (dts89_u32)((src + n) - start);
    if (out_len) *out_len = n;
    return arena_strdup_n(arena, start, n);
}

static int read_torque_string(dts89_reader *r, dts89_arena *arena, dts89_string *out_s)
{
    dts89_u8 len8;
    dts89_u16 len16;
    dts89_u32 len;
    char *dst;
    int rc;
    if (!r || !arena || !out_s) return DTS89_ERR_BAD_ARG;
    out_s->ptr = 0;
    out_s->len = 0u;
    rc = dts89_read_u8(r, &len8);
    if (rc) return rc;
    len = (dts89_u32)len8;
    if (len8 == 255u) {
        rc = dts89_read_u16le(r, &len16);
        if (rc) return rc;
        len = (dts89_u32)len16;
    }
    if (r->pos > r->size || len > (r->size - r->pos)) return DTS89_ERR_TRUNCATED;
    dst = arena_strdup_leaf(arena, (const char*)(r->data + r->pos), len, &out_s->len);
    if (!dst) return DTS89_ERR_NO_SPACE;
    out_s->ptr = dst;
    r->pos += len;
    return DTS89_OK;
}

static int decode_material_list_binary(dts89_shape *s, dts89_reader *r, dts89_arena *arena)
{
    dts89_u32 count;
    dts89_u32 i;
    int rc;
    if (!s || !r || !arena) return DTS89_ERR_BAD_ARG;
    rc = dts89_read_u32le(r, &count);
    if (rc) return rc;
    s->num_materials = (dts89_s32)count;
    s->materials = (dts89_material*)dts89_arena_alloc(arena, count * (dts89_u32)sizeof(dts89_material), 4u);
    if (!s->materials && count) return DTS89_ERR_NO_SPACE;
    if (count) memset(s->materials, 0, count * (dts89_u32)sizeof(dts89_material));
    for (i = 0u; i < count; ++i) {
        rc = read_torque_string(r, arena, &s->materials[i].name);
        if (rc) return rc;
    }
    if (s->header.read_version < 2u) {
        for (i = 0u; i < count; ++i) s->materials[i].flags = DTS89_MATF_S_WRAP | DTS89_MATF_T_WRAP;
    } else {
        for (i = 0u; i < count; ++i) { rc = dts89_read_u32le(r, &s->materials[i].flags); if (rc) return rc; }
    }
    if (s->header.read_version < 5u) {
        for (i = 0u; i < count; ++i) {
            s->materials[i].reflectance_map = i;
            s->materials[i].bump_map = 0xFFFFFFFFu;
            s->materials[i].detail_map = 0xFFFFFFFFu;
        }
    } else {
        for (i = 0u; i < count; ++i) { rc = dts89_read_u32le(r, &s->materials[i].reflectance_map); if (rc) return rc; }
        for (i = 0u; i < count; ++i) { rc = dts89_read_u32le(r, &s->materials[i].bump_map); if (rc) return rc; }
        for (i = 0u; i < count; ++i) { rc = dts89_read_u32le(r, &s->materials[i].detail_map); if (rc) return rc; }
        if (s->header.read_version == 25u) {
            dts89_u32 dummy;
            for (i = 0u; i < count; ++i) { rc = dts89_read_u32le(r, &dummy); if (rc) return rc; }
        }
    }
    if (s->header.read_version > 11u) {
        for (i = 0u; i < count; ++i) { rc = dts89_read_u32le(r, &s->materials[i].detail_scale_bits); if (rc) return rc; }
    } else {
        for (i = 0u; i < count; ++i) s->materials[i].detail_scale_bits = DTS89_F32_BITS_ONE;
    }
    if (s->header.read_version > 20u) {
        for (i = 0u; i < count; ++i) { rc = dts89_read_u32le(r, &s->materials[i].reflection_amount_bits); if (rc) return rc; }
    } else {
        for (i = 0u; i < count; ++i) s->materials[i].reflection_amount_bits = DTS89_F32_BITS_ONE;
    }
    return DTS89_OK;
}

static int decode_material_list_text(dts89_shape *s, const dts89_u8 *src, dts89_u32 size, dts89_arena *arena)
{
    dts89_u32 count;
    dts89_u32 i;
    dts89_u32 pos;
    if (!s || !arena) return DTS89_ERR_BAD_ARG;
    count = 0u;
    pos = 0u;
    while (pos < size) {
        dts89_u32 start = pos;
        while (pos < size && src[pos] != '\n' && src[pos] != '\r' && src[pos] != 0u) pos++;
        if (pos > start) count++;
        while (pos < size && (src[pos] == '\n' || src[pos] == '\r' || src[pos] == 0u)) pos++;
    }
    s->num_materials = (dts89_s32)count;
    s->materials = (dts89_material*)dts89_arena_alloc(arena, count * (dts89_u32)sizeof(dts89_material), 4u);
    if (!s->materials && count) return DTS89_ERR_NO_SPACE;
    if (count) memset(s->materials, 0, count * (dts89_u32)sizeof(dts89_material));
    pos = 0u;
    i = 0u;
    while (pos < size && i < count) {
        dts89_u32 start = pos;
        char *dst;
        while (pos < size && src[pos] != '\n' && src[pos] != '\r' && src[pos] != 0u) pos++;
        if (pos > start) {
            dst = arena_strdup_leaf(arena, (const char*)(src + start), pos - start, &s->materials[i].name.len);
            if (!dst) return DTS89_ERR_NO_SPACE;
            s->materials[i].name.ptr = dst;
            s->materials[i].flags = DTS89_MATF_S_WRAP | DTS89_MATF_T_WRAP;
            s->materials[i].reflectance_map = i;
            s->materials[i].bump_map = 0xFFFFFFFFu;
            s->materials[i].detail_map = 0xFFFFFFFFu;
            s->materials[i].detail_scale_bits = DTS89_F32_BITS_ONE;
            s->materials[i].reflection_amount_bits = DTS89_F32_BITS_ONE;
            i++;
        }
        while (pos < size && (src[pos] == '\n' || src[pos] == '\r' || src[pos] == 0u)) pos++;
    }
    return DTS89_OK;
}

static int decode_material_list(dts89_shape *s, const dts89_u8 *tail, dts89_u32 tail_bytes, dts89_arena *arena)
{
    dts89_reader r;
    dts89_u8 version;
    int rc;
    if (!s || !arena) return DTS89_ERR_BAD_ARG;
    s->num_materials = 0;
    s->materials = 0;
    s->sequence_tail_start = tail;
    s->sequence_tail_bytes = tail_bytes;
    if (!tail || tail_bytes == 0u) return DTS89_OK;
    dts89_reader_init(&r, tail, tail_bytes);
    rc = dts89_read_u8(&r, &version);
    if (rc) return rc;
    if (version == 1u) {
        rc = decode_material_list_binary(s, &r, arena);
        if (rc) return rc;
    } else {
        rc = decode_material_list_text(s, tail, tail_bytes, arena);
        if (rc) return rc;
        r.pos = tail_bytes;
    }
    s->sequence_tail_start = tail + r.pos;
    s->sequence_tail_bytes = tail_bytes - r.pos;
    return DTS89_OK;
}

int dts89_decode_header(dts89_reader *r, dts89_header *h)
{
    int rc;
    dts89_u32 word;
    if (!r || !h) return DTS89_ERR_BAD_ARG;
    memset(h, 0, sizeof(*h));
    rc = dts89_read_u32le(r, &word);
    if (rc) return rc;
    h->version_word = word;
    h->exporter_version = word >> 16;
    h->read_version = word & 255u;
    rc = dts89_read_u32le(r, &h->mem_buffer_dwords);
    if (rc) return rc;
    rc = dts89_read_u32le(r, &h->start16_dwords);
    if (rc) return rc;
    rc = dts89_read_u32le(r, &h->start8_dwords);
    return rc;
}

int dts89_decode_pools(const dts89_header *h, const dts89_u8 *mem, dts89_u32 mem_bytes, dts89_pools *p)
{
    dts89_u32 off16;
    dts89_u32 off8;
    dts89_u32 total32;
    dts89_u32 need;
    if (!h || !mem || !p) return DTS89_ERR_BAD_ARG;
    memset(p, 0, sizeof(*p));
    total32 = h->mem_buffer_dwords;
    if (dts89_mul_u32(total32, 4u, &need)) return DTS89_ERR_RANGE;
    if (mem_bytes < need) return DTS89_ERR_TRUNCATED;
    if (dts89_mul_u32(h->start16_dwords, 4u, &off16)) return DTS89_ERR_RANGE;
    if (dts89_mul_u32(h->start8_dwords, 4u, &off8)) return DTS89_ERR_RANGE;
    if (off16 > mem_bytes || off8 > mem_bytes || off16 > off8) return DTS89_ERR_FORMAT;
    p->base = mem;
    p->total_bytes = mem_bytes;
    p->pool32 = (const dts89_u32*)mem;
    p->pool16 = (const dts89_u16*)(mem + off16);
    p->pool8 = mem + off8;
    p->count32 = h->start16_dwords;
    p->count16 = (off8 - off16) / 2u;
    p->count8 = mem_bytes - off8;
    return DTS89_OK;
}

static int decode_pools_to_arena(const dts89_header *h, const dts89_u8 *mem, dts89_u32 mem_bytes, dts89_arena *arena, dts89_pools *p)
{
    dts89_u32 off16;
    dts89_u32 off8;
    dts89_u32 need;
    dts89_u32 i;
    dts89_u32 count32;
    dts89_u32 count16;
    dts89_u32 count8;
    dts89_u32 bytes32;
    dts89_u32 bytes16;
    dts89_u32 *v32;
    dts89_u16 *v16;
    dts89_u8 *v8;

    if (!h || !mem || !arena || !p) return DTS89_ERR_BAD_ARG;
    memset(p, 0, sizeof(*p));
    if (dts89_mul_u32(h->mem_buffer_dwords, 4u, &need)) return DTS89_ERR_RANGE;
    if (mem_bytes < need) return DTS89_ERR_TRUNCATED;
    if (dts89_mul_u32(h->start16_dwords, 4u, &off16)) return DTS89_ERR_RANGE;
    if (dts89_mul_u32(h->start8_dwords, 4u, &off8)) return DTS89_ERR_RANGE;
    if (off16 > need || off8 > need || off16 > off8) return DTS89_ERR_FORMAT;

    count32 = h->start16_dwords;
    count16 = (off8 - off16) / 2u;
    count8 = need - off8;
    if (dts89_mul_u32(count32, (dts89_u32)sizeof(dts89_u32), &bytes32)) return DTS89_ERR_RANGE;
    if (dts89_mul_u32(count16, (dts89_u32)sizeof(dts89_u16), &bytes16)) return DTS89_ERR_RANGE;

    v32 = (dts89_u32*)dts89_arena_alloc(arena, bytes32, 4u);
    v16 = (dts89_u16*)dts89_arena_alloc(arena, bytes16, 2u);
    v8 = (dts89_u8*)dts89_arena_alloc(arena, count8, 1u);
    if ((count32 && !v32) || (count16 && !v16) || (count8 && !v8)) return DTS89_ERR_NO_SPACE;

    for (i = 0u; i < count32; ++i) v32[i] = dts89_u32_from_le_mem(mem + i * 4u);
    for (i = 0u; i < count16; ++i) v16[i] = (dts89_u16)dts89_u16_from_le_mem(mem + off16 + i * 2u);
    if (count8) memcpy(v8, mem + off8, (size_t)count8);

    p->base = mem;
    p->total_bytes = need;
    p->pool32 = v32;
    p->pool16 = v16;
    p->pool8 = v8;
    p->count32 = count32;
    p->count16 = count16;
    p->count8 = count8;
    return DTS89_OK;
}


static int pool_get_u16(dts89_pool_cursor *c, dts89_u16 *out)
{
    if (!c || !out) return DTS89_ERR_BAD_ARG;
    if (c->left16 == 0u) return DTS89_ERR_TRUNCATED;
    *out = *c->p16++;
    c->left16--;
    return DTS89_OK;
}

static int pool_skip32(dts89_pool_cursor *c, dts89_u32 n)
{
    if (!c) return DTS89_ERR_BAD_ARG;
    if (c->left32 < n) return DTS89_ERR_TRUNCATED;
    c->p32 += n;
    c->left32 -= n;
    return DTS89_OK;
}

static int pool_check_guard(dts89_pool_cursor *pc, dts89_u32 *g32, dts89_u32 *g16, dts89_u32 *g8)
{
    dts89_u32 v32;
    dts89_u16 v16;
    dts89_u8 v8;
    int rc;
    if (!pc || !g32 || !g16 || !g8) return DTS89_ERR_BAD_ARG;
    rc = pool_get32(pc, &v32);
    if (rc) return rc;
    if (v32 != *g32) return DTS89_ERR_FORMAT;
    *g32 = *g32 + 1u;
    rc = pool_get_u16(pc, &v16);
    if (rc) return rc;
    if (v16 != (dts89_u16)(*g16)) return DTS89_ERR_FORMAT;
    *g16 = *g16 + 1u;
    if (pc->left8 == 0u) return DTS89_ERR_TRUNCATED;
    v8 = *pc->p8++;
    pc->left8--;
    if (v8 != (dts89_u8)(*g8)) return DTS89_ERR_FORMAT;
    *g8 = *g8 + 1u;
    return DTS89_OK;
}

static int pool_take32_ptr(dts89_pool_cursor *c, const dts89_u32 **out, dts89_u32 n)
{
    if (!c || !out) return DTS89_ERR_BAD_ARG;
    if (c->left32 < n) return DTS89_ERR_TRUNCATED;
    *out = c->p32;
    c->p32 += n;
    c->left32 -= n;
    return DTS89_OK;
}

static int pool_take_s32_copy(dts89_pool_cursor *c, dts89_arena *arena, const dts89_s32 **out, dts89_u32 n)
{
    const dts89_u32 *src;
    dts89_s32 *dst;
    dts89_u32 i;
    int rc;
    if (!c || !arena || !out) return DTS89_ERR_BAD_ARG;
    *out = 0;
    rc = pool_take32_ptr(c, &src, n);
    if (rc) return rc;
    if (n == 0u) return DTS89_OK;
    if (n > 0xFFFFFFFFu / (dts89_u32)sizeof(dts89_s32)) return DTS89_ERR_RANGE;
    dst = (dts89_s32*)dts89_arena_alloc(arena, n * (dts89_u32)sizeof(dts89_s32), 4u);
    if (!dst) return DTS89_ERR_NO_SPACE;
    for (i = 0u; i < n; ++i) dst[i] = (dts89_s32)src[i];
    *out = dst;
    return DTS89_OK;
}

static int pool_take16_ptr(dts89_pool_cursor *c, const dts89_u16 **out, dts89_u32 n)
{
    if (!c || !out) return DTS89_ERR_BAD_ARG;
    if (c->left16 < n) return DTS89_ERR_TRUNCATED;
    *out = c->p16;
    c->p16 += n;
    c->left16 -= n;
    return DTS89_OK;
}

static int pool_take8_ptr(dts89_pool_cursor *c, const dts89_u8 **out, dts89_u32 n)
{
    if (!c || !out) return DTS89_ERR_BAD_ARG;
    if (c->left8 < n) return DTS89_ERR_TRUNCATED;
    *out = c->p8;
    c->p8 += n;
    c->left8 -= n;
    return DTS89_OK;
}

static int decode_counts(dts89_shape *s, dts89_pool_cursor *pc)
{
    int rc;
    dts89_s32 arb;
    dts89_s32 ground;
#define GETI(field) do { dts89_u32 t_; rc = pool_get32(pc, &t_); if (rc) return rc; s->field = (dts89_s32)t_; } while (0)
    GETI(num_nodes);
    GETI(num_objects);
    GETI(num_decals_deprecated);
    GETI(num_subshapes);
    GETI(num_ifl_deprecated);
    if (s->header.read_version < 22u) return DTS89_ERR_UNSUPPORTED;
    GETI(num_node_rotations);
    GETI(num_node_translations);
    GETI(num_node_uniform_scales);
    GETI(num_node_aligned_scales);
    rc = pool_get_s32(pc, &arb); if (rc) return rc;
    s->num_node_arbitrary_scale_rots = arb;
    s->num_node_arbitrary_scale_factors = arb;
    ground = 0;
    if (s->header.read_version > 23u) {
        rc = pool_get_s32(pc, &ground); if (rc) return rc;
    }
    s->num_ground_rotations = ground;
    s->num_ground_translations = ground;
    GETI(num_object_states);
    GETI(num_decal_states_deprecated);
    GETI(num_triggers);
    GETI(num_details);
    GETI(num_meshes);
    if (s->header.read_version < 23u) {
        dts89_s32 ignored_skins;
        rc = pool_get_s32(pc, &ignored_skins); if (rc) return rc;
        if (ignored_skins != 0) return DTS89_ERR_UNSUPPORTED;
    }
    GETI(num_names);
    GETI(smallest_visible_size);
    GETI(smallest_visible_dl);
#undef GETI
    return DTS89_OK;
}

static int decode_count_ok_s32(dts89_s32 v)
{
    if (v < 0) return 0;
    if ((dts89_u32)v > DTS89_DECODE_MAX_COUNT) return 0;
    return 1;
}

static int decode_validate_counts(const dts89_shape *s)
{
    if (!s) return DTS89_ERR_BAD_ARG;
    if (!decode_count_ok_s32(s->num_nodes)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_objects)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_decals_deprecated)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_subshapes)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_ifl_deprecated)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_node_rotations)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_node_translations)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_node_uniform_scales)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_node_aligned_scales)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_node_arbitrary_scale_rots)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_node_arbitrary_scale_factors)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_ground_rotations)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_ground_translations)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_object_states)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_decal_states_deprecated)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_triggers)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_details)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_meshes)) return DTS89_ERR_RANGE;
    if (!decode_count_ok_s32(s->num_names)) return DTS89_ERR_RANGE;
    if (s->num_node_arbitrary_scale_rots != s->num_node_arbitrary_scale_factors) return DTS89_ERR_FORMAT;
    if (s->num_ground_rotations != s->num_ground_translations) return DTS89_ERR_FORMAT;
    return DTS89_OK;
}

static int decode_alloc_arrays(dts89_shape *s, dts89_arena *arena)
{
    dts89_u32 bytes;
    int rc;
    if (!s || !arena) return DTS89_ERR_BAD_ARG;
#define ALLOC_COUNT(ptr, count, type, align) do { \
        rc = dts89_mul_u32((dts89_u32)(count), (dts89_u32)sizeof(type), &bytes); \
        if (rc) return rc; \
        ptr = (type*)dts89_arena_alloc(arena, bytes, align); \
        if (bytes && !(ptr)) return DTS89_ERR_NO_SPACE; \
        if (bytes && ptr) memset(ptr, 0, (size_t)bytes); \
    } while (0)
    ALLOC_COUNT(s->nodes, s->num_nodes, dts89_node, 4u);
    ALLOC_COUNT(s->objects, s->num_objects, dts89_object, 4u);
    ALLOC_COUNT(s->subshape_first_node, s->num_subshapes, dts89_s32, 4u);
    ALLOC_COUNT(s->subshape_first_object, s->num_subshapes, dts89_s32, 4u);
    ALLOC_COUNT(s->subshape_first_decal, s->num_subshapes, dts89_s32, 4u);
    ALLOC_COUNT(s->subshape_num_nodes, s->num_subshapes, dts89_s32, 4u);
    ALLOC_COUNT(s->subshape_num_objects, s->num_subshapes, dts89_s32, 4u);
    ALLOC_COUNT(s->subshape_num_decals, s->num_subshapes, dts89_s32, 4u);
    ALLOC_COUNT(s->default_rotations_bits, s->num_nodes * 4, dts89_u32, 4u);
    ALLOC_COUNT(s->default_translations_bits, s->num_nodes * 3, dts89_u32, 4u);
    ALLOC_COUNT(s->node_rotations_bits, s->num_node_rotations * 4, dts89_u32, 4u);
    ALLOC_COUNT(s->node_translations_bits, s->num_node_translations * 3, dts89_u32, 4u);
    ALLOC_COUNT(s->node_uniform_scales_bits, s->num_node_uniform_scales, dts89_u32, 4u);
    ALLOC_COUNT(s->node_aligned_scales_bits, s->num_node_aligned_scales * 3, dts89_u32, 4u);
    ALLOC_COUNT(s->node_arbitrary_scale_rots_bits, s->num_node_arbitrary_scale_rots * 4, dts89_u32, 4u);
    ALLOC_COUNT(s->node_arbitrary_scale_factors_bits, s->num_node_arbitrary_scale_factors * 3, dts89_u32, 4u);
    ALLOC_COUNT(s->ground_rotations_bits, s->num_ground_rotations * 4, dts89_u32, 4u);
    ALLOC_COUNT(s->ground_translations_bits, s->num_ground_translations * 3, dts89_u32, 4u);
    ALLOC_COUNT(s->object_states, s->num_object_states, dts89_object_state, 4u);
    ALLOC_COUNT(s->triggers, s->num_triggers, dts89_trigger, 4u);
    ALLOC_COUNT(s->details, s->num_details, dts89_detail, 4u);
    ALLOC_COUNT(s->meshes, s->num_meshes, dts89_mesh_view, 4u);
    ALLOC_COUNT(s->names, s->num_names, dts89_string, 4u);
#undef ALLOC_COUNT
    return DTS89_OK;
}

static int decode_arrays(dts89_shape *s, dts89_pool_cursor *pc, dts89_arena *arena, dts89_u32 *g32, dts89_u32 *g16, dts89_u32 *g8)
{
    dts89_s32 i;
    int rc;
    rc = decode_alloc_arrays(s, arena);
    if (rc) return rc;

    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;
    rc = pool_get32(pc, &s->radius_bits); if (rc) return rc;
    rc = pool_get32(pc, &s->tube_radius_bits); if (rc) return rc;
    rc = pool_get32s(pc, s->center_bits, 3u); if (rc) return rc;
    rc = pool_get32s(pc, s->bounds_bits, 6u); if (rc) return rc;
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;

    for (i = 0; i < s->num_nodes; ++i) {
        rc = pool_get_s32(pc, &s->nodes[i].name_index); if (rc) return rc;
        rc = pool_get_s32(pc, &s->nodes[i].parent_index); if (rc) return rc;
        rc = pool_get_s32(pc, &s->nodes[i].first_object); if (rc) return rc;
        rc = pool_get_s32(pc, &s->nodes[i].first_child); if (rc) return rc;
        rc = pool_get_s32(pc, &s->nodes[i].next_sibling); if (rc) return rc;
    }
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;

    for (i = 0; i < s->num_objects; ++i) {
        rc = pool_get_s32(pc, &s->objects[i].name_index); if (rc) return rc;
        rc = pool_get_s32(pc, &s->objects[i].num_meshes); if (rc) return rc;
        rc = pool_get_s32(pc, &s->objects[i].start_mesh_index); if (rc) return rc;
        rc = pool_get_s32(pc, &s->objects[i].node_index); if (rc) return rc;
        rc = pool_get_s32(pc, &s->objects[i].next_sibling); if (rc) return rc;
        rc = pool_get_s32(pc, &s->objects[i].first_decal); if (rc) return rc;
    }
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;

    rc = pool_skip32(pc, (dts89_u32)s->num_decals_deprecated * 5u); if (rc) return rc;
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;
    rc = pool_skip32(pc, (dts89_u32)s->num_ifl_deprecated * 5u); if (rc) return rc;
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;

    rc = pool_get_s32s(pc, s->subshape_first_node, (dts89_u32)s->num_subshapes); if (rc) return rc;
    rc = pool_get_s32s(pc, s->subshape_first_object, (dts89_u32)s->num_subshapes); if (rc) return rc;
    rc = pool_get_s32s(pc, s->subshape_first_decal, (dts89_u32)s->num_subshapes); if (rc) return rc;
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;

    rc = pool_get_s32s(pc, s->subshape_num_nodes, (dts89_u32)s->num_subshapes); if (rc) return rc;
    rc = pool_get_s32s(pc, s->subshape_num_objects, (dts89_u32)s->num_subshapes); if (rc) return rc;
    rc = pool_get_s32s(pc, s->subshape_num_decals, (dts89_u32)s->num_subshapes); if (rc) return rc;
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;

    rc = pool_get_quat16_to_u32s(pc, s->default_rotations_bits, (dts89_u32)s->num_nodes); if (rc) return rc;
    rc = pool_get32s(pc, s->default_translations_bits, (dts89_u32)s->num_nodes * 3u); if (rc) return rc;
    rc = pool_get32s(pc, s->node_translations_bits, (dts89_u32)s->num_node_translations * 3u); if (rc) return rc;
    rc = pool_get_quat16_to_u32s(pc, s->node_rotations_bits, (dts89_u32)s->num_node_rotations); if (rc) return rc;
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;

    rc = pool_get32s(pc, s->node_uniform_scales_bits, (dts89_u32)s->num_node_uniform_scales); if (rc) return rc;
    rc = pool_get32s(pc, s->node_aligned_scales_bits, (dts89_u32)s->num_node_aligned_scales * 3u); if (rc) return rc;
    rc = pool_get32s(pc, s->node_arbitrary_scale_factors_bits, (dts89_u32)s->num_node_arbitrary_scale_factors * 3u); if (rc) return rc;
    rc = pool_get_quat16_to_u32s(pc, s->node_arbitrary_scale_rots_bits, (dts89_u32)s->num_node_arbitrary_scale_rots); if (rc) return rc;
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;

    rc = pool_get32s(pc, s->ground_translations_bits, (dts89_u32)s->num_ground_translations * 3u); if (rc) return rc;
    rc = pool_get_quat16_to_u32s(pc, s->ground_rotations_bits, (dts89_u32)s->num_ground_rotations); if (rc) return rc;
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;

    for (i = 0; i < s->num_object_states; ++i) {
        rc = pool_get32(pc, &s->object_states[i].vis_bits); if (rc) return rc;
        rc = pool_get_s32(pc, &s->object_states[i].frame_index); if (rc) return rc;
        rc = pool_get_s32(pc, &s->object_states[i].mat_frame_index); if (rc) return rc;
    }
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;

    rc = pool_skip32(pc, (dts89_u32)s->num_decal_states_deprecated); if (rc) return rc;
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;

    for (i = 0; i < s->num_triggers; ++i) {
        rc = pool_get32(pc, &s->triggers[i].state); if (rc) return rc;
        rc = pool_get32(pc, &s->triggers[i].pos_bits); if (rc) return rc;
    }
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;

    if (s->header.read_version >= 26u) {
        for (i = 0; i < s->num_details; ++i) {
            rc = pool_get_s32(pc, &s->details[i].name_index); if (rc) return rc;
            rc = pool_get_s32(pc, &s->details[i].subshape_num); if (rc) return rc;
            rc = pool_get_s32(pc, &s->details[i].object_detail_num); if (rc) return rc;
            rc = pool_get32(pc, &s->details[i].size_bits); if (rc) return rc;
            rc = pool_get32(pc, &s->details[i].average_error_bits); if (rc) return rc;
            rc = pool_get32(pc, &s->details[i].max_error_bits); if (rc) return rc;
            rc = pool_get_s32(pc, &s->details[i].poly_count); if (rc) return rc;
            rc = pool_get_s32(pc, &s->details[i].bb_dimension); if (rc) return rc;
            rc = pool_get_s32(pc, &s->details[i].bb_detail_level); if (rc) return rc;
            rc = pool_get32(pc, &s->details[i].bb_equator_steps); if (rc) return rc;
            rc = pool_get32(pc, &s->details[i].bb_polar_steps); if (rc) return rc;
            rc = pool_get32(pc, &s->details[i].bb_polar_angle_bits); if (rc) return rc;
            rc = pool_get32(pc, &s->details[i].bb_include_poles); if (rc) return rc;
        }
    } else {
        for (i = 0; i < s->num_details; ++i) {
            dts89_s32 opts;
            rc = pool_get_s32(pc, &s->details[i].name_index); if (rc) return rc;
            rc = pool_get_s32(pc, &s->details[i].subshape_num); if (rc) return rc;
            rc = pool_get_s32(pc, &opts); if (rc) return rc;
            s->details[i].object_detail_num = opts;
            rc = pool_get32(pc, &s->details[i].size_bits); if (rc) return rc;
            rc = pool_get32(pc, &s->details[i].average_error_bits); if (rc) return rc;
            rc = pool_get32(pc, &s->details[i].max_error_bits); if (rc) return rc;
            rc = pool_get_s32(pc, &s->details[i].poly_count); if (rc) return rc;
            s->details[i].bb_equator_steps = (dts89_u32)(opts & 0x7F);
            s->details[i].bb_polar_steps = (dts89_u32)((opts >> 7) & 0x3F);
            s->details[i].bb_polar_angle_bits = dts89_fx_to_f32bits((dts89_fx16_16)((DTS89_FX_PI_OVER_2 * (dts89_s32)((opts >> 13) & 0x3F)) >> 6));
            s->details[i].bb_detail_level = (opts >> 19) & 0x0F;
            s->details[i].bb_dimension = (opts >> 23) & 0xFF;
            s->details[i].bb_include_poles = (opts & (dts89_s32)0x80000000u) ? 1u : 0u;
        }
    }
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;

    if (s->header.read_version >= 27u) {
        dts89_u32 k;
        dts89_u16 vf;
        for (k = 0u; k < 5u; ++k) {
            rc = pool_get_u16(pc, &vf); if (rc) return rc;
            s->vertex_format_words[k] = (dts89_u32)vf;
        }
        rc = pool_get32(pc, &s->packed_vertex_size); if (rc) return rc;
        if (s->packed_vertex_size > 0u) {
            dts89_u8 *p8;
            rc = pool_get8s(pc, &p8, s->packed_vertex_size); if (rc) return rc;
            s->packed_vertex_data = p8;
        }
    }
    return DTS89_OK;
}

static int shared32_from_parent(const dts89_shape *s, dts89_s32 mesh_index, dts89_s32 parent, const dts89_u32 *parent_ptr, const dts89_u32 **out)
{
    if (!out) return DTS89_ERR_BAD_ARG;
    if (parent < 0) return DTS89_ERR_BAD_ARG;
    if (!s || parent >= mesh_index || parent >= s->num_meshes) return DTS89_ERR_FORMAT;
    *out = parent_ptr;
    return DTS89_OK;
}

static int shared_s32_from_parent(const dts89_shape *s, dts89_s32 mesh_index, dts89_s32 parent, const dts89_s32 *parent_ptr, const dts89_s32 **out)
{
    if (!out) return DTS89_ERR_BAD_ARG;
    if (parent < 0) return DTS89_ERR_BAD_ARG;
    if (!s || parent >= mesh_index || parent >= s->num_meshes) return DTS89_ERR_FORMAT;
    *out = parent_ptr;
    return DTS89_OK;
}

static int shared8_from_parent(const dts89_shape *s, dts89_s32 mesh_index, dts89_s32 parent, const dts89_u8 *parent_ptr, const dts89_u8 **out)
{
    if (!out) return DTS89_ERR_BAD_ARG;
    if (parent < 0) return DTS89_ERR_BAD_ARG;
    if (!s || parent >= mesh_index || parent >= s->num_meshes) return DTS89_ERR_FORMAT;
    *out = parent_ptr;
    return DTS89_OK;
}

static int take_shared32(dts89_shape *s, dts89_pool_cursor *pc, dts89_s32 mesh_index, dts89_s32 parent, dts89_u32 words, const dts89_u32 *parent_ptr, const dts89_u32 **out)
{
    if (parent < 0) return pool_take32_ptr(pc, out, words);
    return shared32_from_parent(s, mesh_index, parent, parent_ptr, out);
}

static int take_shared8(dts89_shape *s, dts89_pool_cursor *pc, dts89_s32 mesh_index, dts89_s32 parent, dts89_u32 bytes, const dts89_u8 *parent_ptr, const dts89_u8 **out)
{
    if (parent < 0) return pool_take8_ptr(pc, out, bytes);
    return shared8_from_parent(s, mesh_index, parent, parent_ptr, out);
}

static int copy_primitives32(dts89_pool_cursor *pc, dts89_arena *arena, dts89_mesh_view *m)
{
    dts89_draw_primitive *pr;
    dts89_u32 i;
    dts89_u32 words;
    int rc;
    if (m->num_primitives <= 0) return DTS89_OK;
    if (dts89_mul_u32((dts89_u32)m->num_primitives, 3u, &words)) return DTS89_ERR_RANGE;
    pr = (dts89_draw_primitive*)dts89_arena_alloc(arena, (dts89_u32)m->num_primitives * (dts89_u32)sizeof(dts89_draw_primitive), 4u);
    if (!pr) return DTS89_ERR_NO_SPACE;
    if (pc->left32 < words) return DTS89_ERR_TRUNCATED;
    for (i = 0u; i < (dts89_u32)m->num_primitives; ++i) {
        pr[i].start = (dts89_s32)pc->p32[i * 3u];
        pr[i].num_elements = (dts89_s32)pc->p32[i * 3u + 1u];
        pr[i].mat_index = (dts89_s32)pc->p32[i * 3u + 2u];
    }
    pc->p32 += words;
    pc->left32 -= words;
    m->primitives = pr;
    rc = DTS89_OK;
    return rc;
}

static int copy_indices32(dts89_pool_cursor *pc, dts89_arena *arena, dts89_mesh_view *m)
{
    dts89_s32 *idx;
    dts89_u32 i;
    if (m->num_indices <= 0) return DTS89_OK;
    if (pc->left32 < (dts89_u32)m->num_indices) return DTS89_ERR_TRUNCATED;
    idx = (dts89_s32*)dts89_arena_alloc(arena, (dts89_u32)m->num_indices * (dts89_u32)sizeof(dts89_s32), 4u);
    if (!idx) return DTS89_ERR_NO_SPACE;
    for (i = 0u; i < (dts89_u32)m->num_indices; ++i) idx[i] = (dts89_s32)pc->p32[i];
    pc->p32 += (dts89_u32)m->num_indices;
    pc->left32 -= (dts89_u32)m->num_indices;
    m->indices = idx;
    return DTS89_OK;
}

static int decode_base_mesh(dts89_shape *s, dts89_pool_cursor *pc, dts89_arena *arena, dts89_mesh_view *m, dts89_s32 mesh_index, dts89_u32 *g32, dts89_u32 *g16, dts89_u32 *g8)
{
    int rc;
    dts89_u32 n;
    dts89_s32 parent;
    const dts89_mesh_view *pm;
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;
    rc = pool_get_s32(pc, &m->num_frames); if (rc) return rc;
    rc = pool_get_s32(pc, &m->num_mat_frames); if (rc) return rc;
    rc = pool_get_s32(pc, &m->parent_mesh); if (rc) return rc;
    parent = m->parent_mesh;
    pm = 0;
    if (parent >= 0) {
        if (parent >= mesh_index || parent >= s->num_meshes) return DTS89_ERR_FORMAT;
        pm = &s->meshes[parent];
    }
    rc = pool_get32s(pc, m->bounds_bits, 6u); if (rc) return rc;
    rc = pool_get32s(pc, m->center_bits, 3u); if (rc) return rc;
    rc = pool_get32(pc, &m->radius_bits); if (rc) return rc;
    if (s->header.read_version >= 27u) {
        rc = pool_get_s32(pc, &m->vert_offset); if (rc) return rc;
        rc = pool_get_s32(pc, &m->num_verts); if (rc) return rc;
        rc = pool_get_s32(pc, &m->vert_size); if (rc) return rc;
    }

    rc = pool_get_s32(pc, &m->num_verts_editable); if (rc) return rc;
    if (m->num_verts_editable < 0) return DTS89_ERR_RANGE;
    rc = take_shared32(s, pc, mesh_index, parent, (dts89_u32)m->num_verts_editable * 3u, pm ? pm->verts_bits : 0, &m->verts_bits); if (rc) return rc;

    rc = pool_get_s32(pc, &m->num_tverts); if (rc) return rc;
    if (m->num_tverts < 0) return DTS89_ERR_RANGE;
    rc = take_shared32(s, pc, mesh_index, parent, (dts89_u32)m->num_tverts * 2u, pm ? pm->tverts_bits : 0, &m->tverts_bits); if (rc) return rc;

    if (s->header.read_version > 25u) {
        rc = pool_get_s32(pc, &m->num_tverts2); if (rc) return rc;
        if (m->num_tverts2 < 0) return DTS89_ERR_RANGE;
        rc = take_shared32(s, pc, mesh_index, parent, (dts89_u32)m->num_tverts2 * 2u, pm ? pm->tverts2_bits : 0, &m->tverts2_bits); if (rc) return rc;
        rc = pool_get_s32(pc, &m->num_colors); if (rc) return rc;
        if (m->num_colors < 0) return DTS89_ERR_RANGE;
        rc = take_shared32(s, pc, mesh_index, parent, (dts89_u32)m->num_colors, pm ? pm->colors_bits : 0, &m->colors_bits); if (rc) return rc;
    }

    rc = take_shared32(s, pc, mesh_index, parent, (dts89_u32)m->num_verts_editable * 3u, pm ? pm->norms_bits : 0, &m->norms_bits); if (rc) return rc;
    if (parent < 0) {
        if (m->num_verts_editable > 0) {
            rc = pool_take8_ptr(pc, &m->encoded_norms, (dts89_u32)m->num_verts_editable); if (rc) return rc;
        }
    } else {
        rc = shared8_from_parent(s, mesh_index, parent, pm ? pm->encoded_norms : 0, &m->encoded_norms); if (rc) return rc;
    }

    rc = pool_get_s32(pc, &m->num_primitives); if (rc) return rc;
    if (m->num_primitives < 0) return DTS89_ERR_RANGE;
    if (s->header.read_version > 25u) {
        rc = copy_primitives32(pc, arena, m); if (rc) return rc;
        rc = pool_get_s32(pc, &m->num_indices); if (rc) return rc;
        if (m->num_indices < 0) return DTS89_ERR_RANGE;
        rc = copy_indices32(pc, arena, m); if (rc) return rc;
    } else {
        const dts89_u16 *p16;
        dts89_draw_primitive *pr;
        dts89_s32 *idx;
        rc = pool_take16_ptr(pc, &p16, (dts89_u32)m->num_primitives * 2u); if (rc) return rc;
        pr = (dts89_draw_primitive*)dts89_arena_alloc(arena, (dts89_u32)m->num_primitives * (dts89_u32)sizeof(dts89_draw_primitive), 4u);
        if (!pr && m->num_primitives) return DTS89_ERR_NO_SPACE;
        for (n = 0u; n < (dts89_u32)m->num_primitives; ++n) {
            pr[n].start = (dts89_s16)p16[n * 2u];
            pr[n].num_elements = (dts89_s16)p16[n * 2u + 1u];
        }
        if (pc->left32 < (dts89_u32)m->num_primitives) return DTS89_ERR_TRUNCATED;
        for (n = 0u; n < (dts89_u32)m->num_primitives; ++n) pr[n].mat_index = (dts89_s32)pc->p32[n];
        pc->p32 += (dts89_u32)m->num_primitives;
        pc->left32 -= (dts89_u32)m->num_primitives;
        m->primitives = pr;
        rc = pool_get_s32(pc, &m->num_indices); if (rc) return rc;
        if (m->num_indices < 0) return DTS89_ERR_RANGE;
        rc = pool_take16_ptr(pc, &p16, (dts89_u32)m->num_indices); if (rc) return rc;
        idx = (dts89_s32*)dts89_arena_alloc(arena, (dts89_u32)m->num_indices * (dts89_u32)sizeof(dts89_s32), 4u);
        if (!idx && m->num_indices) return DTS89_ERR_NO_SPACE;
        for (n = 0u; n < (dts89_u32)m->num_indices; ++n) idx[n] = p16[n];
        m->indices = idx;
    }

    rc = pool_get_s32(pc, &m->merge_indices_count); if (rc) return rc;
    if (m->merge_indices_count < 0) return DTS89_ERR_RANGE;
    if (m->merge_indices_count > 0) {
        rc = pool_take16_ptr(pc, &m->merge_indices16, (dts89_u32)m->merge_indices_count); if (rc) return rc;
    }
    rc = pool_get_s32(pc, &m->verts_per_frame); if (rc) return rc;
    rc = pool_get32(pc, &m->flags); if (rc) return rc;
    if (s->header.read_version < 27u) {
        if (m->num_colors > 0) m->flags |= DTS89_MESHF_HAS_COLOR;
        if (m->num_tverts2 > 0) m->flags |= DTS89_MESHF_HAS_TVERT2;
        m->num_verts = m->num_verts_editable;
    }
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;
    return DTS89_OK;
}

static int decode_skin_extra(dts89_shape *s, dts89_pool_cursor *pc, dts89_arena *arena, dts89_mesh_view *m, dts89_s32 mesh_index, dts89_u32 *g32, dts89_u32 *g16, dts89_u32 *g8)
{
    int rc;
    dts89_s32 parent;
    const dts89_mesh_view *pm;
    parent = m->parent_mesh;
    pm = 0;
    if (parent >= 0) {
        if (parent >= mesh_index || parent >= s->num_meshes) return DTS89_ERR_FORMAT;
        pm = &s->meshes[parent];
    }
    if (s->header.read_version >= 27u) {
        rc = pool_get_s32(pc, &m->skin_max_bones); if (rc) return rc;
        m->skin_initial_vert_count = m->num_verts_editable;
        m->skin_initial_verts_bits = m->verts_bits;
        m->skin_initial_norms_bits = m->norms_bits;
        m->skin_initial_encoded_norms = m->encoded_norms;
    } else {
        rc = pool_get_s32(pc, &m->skin_initial_vert_count); if (rc) return rc;
        if (m->skin_initial_vert_count < 0) return DTS89_ERR_RANGE;
        rc = take_shared32(s, pc, mesh_index, parent, (dts89_u32)m->skin_initial_vert_count * 3u, pm ? pm->skin_initial_verts_bits : 0, &m->skin_initial_verts_bits); if (rc) return rc;
        rc = take_shared32(s, pc, mesh_index, parent, (dts89_u32)m->skin_initial_vert_count * 3u, pm ? pm->skin_initial_norms_bits : 0, &m->skin_initial_norms_bits); if (rc) return rc;
        rc = take_shared8(s, pc, mesh_index, parent, (dts89_u32)m->skin_initial_vert_count, pm ? pm->skin_initial_encoded_norms : 0, &m->skin_initial_encoded_norms); if (rc) return rc;
    }
    rc = pool_get_s32(pc, &m->skin_initial_transform_count); if (rc) return rc;
    if (m->skin_initial_transform_count < 0) return DTS89_ERR_RANGE;
    rc = take_shared32(s, pc, mesh_index, parent, (dts89_u32)m->skin_initial_transform_count * 16u, pm ? pm->skin_initial_transforms_bits : 0, &m->skin_initial_transforms_bits); if (rc) return rc;
    rc = pool_get_s32(pc, &m->skin_vertex_index_count); if (rc) return rc;
    if (m->skin_vertex_index_count < 0) return DTS89_ERR_RANGE;
    if (parent < 0) {
        rc = pool_take_s32_copy(pc, arena, &m->skin_vertex_index, (dts89_u32)m->skin_vertex_index_count); if (rc) return rc;
        rc = pool_take_s32_copy(pc, arena, &m->skin_bone_index, (dts89_u32)m->skin_vertex_index_count); if (rc) return rc;
        rc = pool_take32_ptr(pc, &m->skin_weight_bits, (dts89_u32)m->skin_vertex_index_count); if (rc) return rc;
    } else {
        rc = shared_s32_from_parent(s, mesh_index, parent, pm ? pm->skin_vertex_index : 0, &m->skin_vertex_index); if (rc) return rc;
        rc = shared_s32_from_parent(s, mesh_index, parent, pm ? pm->skin_bone_index : 0, &m->skin_bone_index); if (rc) return rc;
        rc = shared32_from_parent(s, mesh_index, parent, pm ? pm->skin_weight_bits : 0, &m->skin_weight_bits); if (rc) return rc;
    }
    rc = pool_get_s32(pc, &m->skin_node_index_count); if (rc) return rc;
    if (m->skin_node_index_count < 0) return DTS89_ERR_RANGE;
    if (parent < 0) {
        rc = pool_take_s32_copy(pc, arena, &m->skin_node_index, (dts89_u32)m->skin_node_index_count); if (rc) return rc;
    } else {
        rc = shared_s32_from_parent(s, mesh_index, parent, pm ? pm->skin_node_index : 0, &m->skin_node_index); if (rc) return rc;
    }
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;
    return DTS89_OK;
}

static int decode_sorted_mesh(dts89_shape *s, dts89_pool_cursor *pc, dts89_arena *arena, dts89_mesh_view *m, dts89_s32 mesh_index, dts89_u32 *g32, dts89_u32 *g16, dts89_u32 *g8)
{
    int rc;
    rc = decode_base_mesh(s, pc, arena, m, mesh_index, g32, g16, g8); if (rc) return rc;
    rc = pool_get_s32(pc, &m->sorted_num_clusters); if (rc) return rc;
    if (m->sorted_num_clusters < 0) return DTS89_ERR_RANGE;
    if (m->sorted_num_clusters > 0) {
        rc = pool_take32_ptr(pc, &m->sorted_clusters_bits, (dts89_u32)m->sorted_num_clusters * 8u); if (rc) return rc;
    }
    rc = pool_get_s32(pc, &m->sorted_start_cluster_count); if (rc) return rc;
    if (m->sorted_start_cluster_count < 0) return DTS89_ERR_RANGE;
    if (m->sorted_start_cluster_count > 0) {
        rc = pool_take_s32_copy(pc, arena, &m->sorted_start_cluster, (dts89_u32)m->sorted_start_cluster_count); if (rc) return rc;
    }
    rc = pool_get_s32(pc, &m->sorted_first_verts_count); if (rc) return rc;
    if (m->sorted_first_verts_count < 0) return DTS89_ERR_RANGE;
    if (m->sorted_first_verts_count > 0) {
        rc = pool_take_s32_copy(pc, arena, &m->sorted_first_verts, (dts89_u32)m->sorted_first_verts_count); if (rc) return rc;
    }
    rc = pool_get_s32(pc, &m->sorted_num_verts_count); if (rc) return rc;
    if (m->sorted_num_verts_count < 0) return DTS89_ERR_RANGE;
    if (m->sorted_num_verts_count > 0) {
        rc = pool_take_s32_copy(pc, arena, &m->sorted_num_verts, (dts89_u32)m->sorted_num_verts_count); if (rc) return rc;
    }
    rc = pool_get_s32(pc, &m->sorted_first_tverts_count); if (rc) return rc;
    if (m->sorted_first_tverts_count < 0) return DTS89_ERR_RANGE;
    if (m->sorted_first_tverts_count > 0) {
        rc = pool_take_s32_copy(pc, arena, &m->sorted_first_tverts, (dts89_u32)m->sorted_first_tverts_count); if (rc) return rc;
    }
    rc = pool_get_s32(pc, &m->sorted_always_write_depth); if (rc) return rc;
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;
    return DTS89_OK;
}

static int decode_decal_mesh(dts89_shape *s, dts89_pool_cursor *pc, dts89_arena *arena, dts89_mesh_view *m, dts89_u32 *g32, dts89_u32 *g16, dts89_u32 *g8)
{
    int rc;
    dts89_u32 n;
    m->mesh_type = DTS89_MESH_DECAL;
    if (s->header.read_version < 20u) {
        rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;
        rc = pool_skip32(pc, 15u); if (rc) return rc;
    }
    rc = pool_get_s32(pc, &m->num_primitives); if (rc) return rc;
    if (m->num_primitives < 0) return DTS89_ERR_RANGE;
    if (m->num_primitives > 0) {
        const dts89_u16 *p16;
        dts89_draw_primitive *pr;
        pr = (dts89_draw_primitive*)dts89_arena_alloc(arena, (dts89_u32)m->num_primitives * (dts89_u32)sizeof(dts89_draw_primitive), 4u);
        if (!pr) return DTS89_ERR_NO_SPACE;
        rc = pool_take16_ptr(pc, &p16, (dts89_u32)m->num_primitives * 2u); if (rc) return rc;
        for (n = 0u; n < (dts89_u32)m->num_primitives; ++n) {
            pr[n].start = (dts89_s16)p16[n * 2u];
            pr[n].num_elements = (dts89_s16)p16[n * 2u + 1u];
        }
        if (pc->left32 < (dts89_u32)m->num_primitives) return DTS89_ERR_TRUNCATED;
        for (n = 0u; n < (dts89_u32)m->num_primitives; ++n) pr[n].mat_index = (dts89_s32)pc->p32[n];
        pc->p32 += (dts89_u32)m->num_primitives;
        pc->left32 -= (dts89_u32)m->num_primitives;
        m->primitives = pr;
    }
    rc = pool_get_s32(pc, &m->num_indices); if (rc) return rc;
    if (m->num_indices < 0) return DTS89_ERR_RANGE;
    if (m->num_indices > 0) {
        const dts89_u16 *i16;
        dts89_s32 *idx;
        rc = pool_take16_ptr(pc, &i16, (dts89_u32)m->num_indices); if (rc) return rc;
        idx = (dts89_s32*)dts89_arena_alloc(arena, (dts89_u32)m->num_indices * (dts89_u32)sizeof(dts89_s32), 4u);
        if (!idx) return DTS89_ERR_NO_SPACE;
        for (n = 0u; n < (dts89_u32)m->num_indices; ++n) idx[n] = i16[n];
        m->indices = idx;
    }
    if (s->header.read_version < 20u) {
        rc = pool_skip32(pc, 3u); if (rc) return rc;
        rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;
    }
    rc = pool_get_s32(pc, &m->decal_start_primitive_count); if (rc) return rc;
    if (m->decal_start_primitive_count < 0) return DTS89_ERR_RANGE;
    if (m->decal_start_primitive_count > 0) {
        rc = pool_take_s32_copy(pc, arena, &m->decal_start_primitives, (dts89_u32)m->decal_start_primitive_count); if (rc) return rc;
    }
    if (s->header.read_version >= 19u && m->decal_start_primitive_count > 0) {
        rc = pool_take32_ptr(pc, &m->decal_texgen_s_bits, (dts89_u32)m->decal_start_primitive_count * 4u); if (rc) return rc;
        rc = pool_take32_ptr(pc, &m->decal_texgen_t_bits, (dts89_u32)m->decal_start_primitive_count * 4u); if (rc) return rc;
    }
    rc = pool_get_s32(pc, &m->decal_material_index); if (rc) return rc;
    rc = pool_check_guard(pc, g32, g16, g8); if (rc) return rc;
    return DTS89_OK;
}

static int decode_one_mesh(dts89_shape *s, dts89_pool_cursor *pc, dts89_arena *arena, dts89_mesh_view *m, dts89_s32 mesh_index, dts89_u32 *g32, dts89_u32 *g16, dts89_u32 *g8)
{
    int rc;
    dts89_u32 mesh_type;
    memset(m, 0, sizeof(*m));
    rc = pool_get32(pc, &mesh_type); if (rc) return rc;
    m->mesh_type = mesh_type;
    if (mesh_type == DTS89_MESH_NULL) return DTS89_OK;
    if (mesh_type == DTS89_MESH_DECAL) return decode_decal_mesh(s, pc, arena, m, g32, g16, g8);
    if (mesh_type == DTS89_MESH_SORTED) return decode_sorted_mesh(s, pc, arena, m, mesh_index, g32, g16, g8);
    if (mesh_type == DTS89_MESH_SKIN) {
        rc = decode_base_mesh(s, pc, arena, m, mesh_index, g32, g16, g8); if (rc) return rc;
        rc = decode_skin_extra(s, pc, arena, m, mesh_index, g32, g16, g8); if (rc) return rc;
        return DTS89_OK;
    }
    if (mesh_type == DTS89_MESH_STANDARD) return decode_base_mesh(s, pc, arena, m, mesh_index, g32, g16, g8);
    return DTS89_ERR_UNSUPPORTED;
}


static int reader_read_s32(dts89_reader *r, dts89_s32 *out_v)
{
    dts89_u32 t;
    int rc;
    if (!r || !out_v) return DTS89_ERR_BAD_ARG;
    rc = dts89_read_u32le(r, &t);
    if (rc) return rc;
    *out_v = (dts89_s32)t;
    return DTS89_OK;
}

static dts89_u32 sequence_dirty_flags(const dts89_sequence *q)
{
    dts89_u32 flags;
    flags = 0u;
    if (dts89_bitset_count(&q->rotation_matters) || dts89_bitset_count(&q->translation_matters) || dts89_bitset_count(&q->scale_matters)) flags |= DTS89_DIRTY_TRANSFORM;
    if (dts89_bitset_count(&q->vis_matters)) flags |= DTS89_DIRTY_VIS;
    if (dts89_bitset_count(&q->frame_matters)) flags |= DTS89_DIRTY_FRAME;
    if (dts89_bitset_count(&q->mat_frame_matters)) flags |= DTS89_DIRTY_MATFRAME;
    return flags;
}

static int decode_sequences(dts89_shape *s, const dts89_u8 *tail, dts89_u32 tail_bytes, dts89_arena *arena)
{
    dts89_reader r;
    dts89_s32 i;
    dts89_u32 nseq;
    int rc;
    dts89_reader_init(&r, tail, tail_bytes);
    rc = dts89_read_u32le(&r, &nseq);
    if (rc) return rc;
    if (nseq > 0x7FFFFFFFu || nseq > DTS89_DECODE_MAX_COUNT) return DTS89_ERR_RANGE;
    s->num_sequences = (dts89_s32)nseq;
    s->sequences = (dts89_sequence*)dts89_arena_alloc(arena, nseq * (dts89_u32)sizeof(dts89_sequence), 4u);
    if (!s->sequences && nseq) return DTS89_ERR_NO_SPACE;
    if (nseq) memset(s->sequences, 0, nseq * sizeof(dts89_sequence));
    for (i = 0; i < (dts89_s32)nseq; ++i) {
        dts89_sequence *q = &s->sequences[i];
        rc = reader_read_s32(&r, &q->name_index); if (rc) return rc;
        rc = dts89_read_u32le(&r, &q->flags); if (rc) return rc;
        rc = reader_read_s32(&r, &q->num_keyframes); if (rc) return rc;
        rc = dts89_read_u32le(&r, &q->duration_bits); if (rc) return rc;
        rc = reader_read_s32(&r, &q->priority); if (rc) return rc;
        rc = reader_read_s32(&r, &q->first_ground_frame); if (rc) return rc;
        rc = reader_read_s32(&r, &q->num_ground_frames); if (rc) return rc;
        rc = reader_read_s32(&r, &q->base_rotation); if (rc) return rc;
        rc = reader_read_s32(&r, &q->base_translation); if (rc) return rc;
        rc = reader_read_s32(&r, &q->base_scale); if (rc) return rc;
        rc = reader_read_s32(&r, &q->base_object_state); if (rc) return rc;
        rc = reader_read_s32(&r, &q->base_decal_state); if (rc) return rc;
        rc = reader_read_s32(&r, &q->first_trigger); if (rc) return rc;
        rc = reader_read_s32(&r, &q->num_triggers); if (rc) return rc;
        rc = dts89_read_u32le(&r, &q->tool_begin_bits); if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->rotation_matters); if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->translation_matters); if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->scale_matters); if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->decal_matters); if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->ifl_matters); if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->vis_matters); if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->frame_matters); if (rc) return rc;
        rc = dts89_bitset_read(&r, &q->mat_frame_matters); if (rc) return rc;
        q->dirty_flags = sequence_dirty_flags(q);
    }
    s->sequence_tail_start = tail + r.pos;
    s->sequence_tail_bytes = tail_bytes - r.pos;
    return DTS89_OK;
}

int dts89_make_v29_key(const char *seed, dts89_u32 seed_len, dts89_u8 *out_key, dts89_u32 key_len)
{
    dts89_u32 state;
    dts89_u32 i;
    if ((!seed && seed_len) || (!out_key && key_len)) return DTS89_ERR_BAD_ARG;
    state = 5381u;
    for (i = 0u; i < seed_len; ++i) {
        state = ((state << 5) + state) ^ (dts89_u8)seed[i];
    }
    for (i = 0u; i < key_len; ++i) {
        state = state * 1664525u + 1013904223u;
        out_key[i] = (dts89_u8)((state >> 16) & 255u);
    }
    return DTS89_OK;
}

void dts89_xor_bytes_at_offset(void *data, dts89_u32 byte_count, dts89_u32 start_offset, const dts89_u8 *key, dts89_u32 key_len)
{
    dts89_u8 *p;
    dts89_u32 i;
    if (!data || !key || key_len == 0u) return;
    p = (dts89_u8*)data;
    for (i = 0u; i < byte_count; ++i) {
        p[i] = (dts89_u8)(p[i] ^ key[(start_offset + i) % key_len]);
    }
}

static int decode_shape_internal(const void *bytes, dts89_u32 size, dts89_arena *arena, dts89_shape *out_shape, const dts89_u8 *key, dts89_u32 key_len)
{
    dts89_reader r;
    int rc;
    dts89_u32 mem_bytes;
    dts89_pool_cursor pc;
    dts89_s32 i;
    dts89_u32 g32;
    dts89_u32 g16;
    dts89_u32 g8;
    const dts89_u8 *mem_src;
    dts89_u8 *mem_tmp;
    if (!bytes || !arena || !out_shape) return DTS89_ERR_BAD_ARG;
    memset(out_shape, 0, sizeof(*out_shape));
    dts89_reader_init(&r, bytes, size);
    rc = dts89_decode_header(&r, &out_shape->header);
    if (rc) return rc;
    if (out_shape->header.read_version < 24u) return DTS89_ERR_UNSUPPORTED;
    if (out_shape->header.read_version > DTS89_VERSION_KNOWN_MAX) return DTS89_ERR_UNSUPPORTED;
    if (out_shape->header.read_version > DTS89_VERSION_CURRENT && (!key || key_len == 0u)) return DTS89_ERR_UNSUPPORTED;
    rc = dts89_mul_u32(out_shape->header.mem_buffer_dwords, 4u, &mem_bytes);
    if (rc) return rc;
    if (r.pos > size || mem_bytes > (size - r.pos)) return DTS89_ERR_TRUNCATED;
    mem_src = r.data + r.pos;
    if (out_shape->header.read_version > DTS89_VERSION_CURRENT) {
        mem_tmp = (dts89_u8*)dts89_arena_alloc(arena, mem_bytes, 4u);
        if (!mem_tmp && mem_bytes) return DTS89_ERR_NO_SPACE;
        if (mem_bytes) memcpy(mem_tmp, mem_src, (size_t)mem_bytes);
        dts89_xor_bytes_at_offset(mem_tmp, mem_bytes, 0u, key, key_len);
        mem_src = mem_tmp;
    }
    rc = decode_pools_to_arena(&out_shape->header, mem_src, mem_bytes, arena, &out_shape->pools);
    if (rc) return rc;
    r.pos += mem_bytes;
    pc.p32 = out_shape->pools.pool32;
    pc.p16 = out_shape->pools.pool16;
    pc.p8 = out_shape->pools.pool8;
    pc.left32 = out_shape->pools.count32;
    pc.left16 = out_shape->pools.count16;
    pc.left8 = out_shape->pools.count8;
    g32 = 0u;
    g16 = 0u;
    g8 = 0u;
    rc = decode_counts(out_shape, &pc);
    if (rc) return rc;
    rc = decode_validate_counts(out_shape);
    if (rc) return rc;
    rc = decode_arrays(out_shape, &pc, arena, &g32, &g16, &g8);
    if (rc) return rc;
    for (i = 0; i < out_shape->num_meshes; ++i) {
        rc = decode_one_mesh(out_shape, &pc, arena, &out_shape->meshes[i], i, &g32, &g16, &g8);
        if (rc) return rc;
    }
    rc = pool_check_guard(&pc, &g32, &g16, &g8);
    if (rc) return rc;
    rc = read_names(out_shape, &pc, arena);
    if (rc) return rc;
    rc = pool_check_guard(&pc, &g32, &g16, &g8);
    if (rc) return rc;
    rc = decode_sequences(out_shape, r.data + r.pos, size - r.pos, arena);
    if (rc) return rc;
    rc = decode_material_list(out_shape, out_shape->sequence_tail_start, out_shape->sequence_tail_bytes, arena);
    return rc;
}

int dts89_decode_shape(const void *bytes, dts89_u32 size, dts89_arena *arena, dts89_shape *out_shape)
{
    return decode_shape_internal(bytes, size, arena, out_shape, 0, 0u);
}

int dts89_decode_shape_with_key(const void *bytes, dts89_u32 size, dts89_arena *arena, dts89_shape *out_shape, const dts89_u8 *key, dts89_u32 key_len)
{
    return decode_shape_internal(bytes, size, arena, out_shape, key, key_len);
}

