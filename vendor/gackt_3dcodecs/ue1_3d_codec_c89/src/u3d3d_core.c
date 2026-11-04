#include "u3d3d.h"

u3d3d_fx u3d3d_fx_from_i32(u3d3d_i32 v)
{
    return (u3d3d_fx)(v << U3D3D_FX_SHIFT);
}

u3d3d_i32 u3d3d_fx_to_i32_floor(u3d3d_fx v)
{
    return (u3d3d_i32)(v >> U3D3D_FX_SHIFT);
}

u3d3d_fx u3d3d_fx_mul(u3d3d_fx a, u3d3d_fx b)
{
    /*
       Avoid 64-bit math for strict C89 targets.  This fixed multiply is
       intentionally conservative and is only used by optional OBJ scaling.
       Inputs used here are small model coordinates and scale factors.
    */
    u3d3d_i32 ai;
    u3d3d_i32 af;
    u3d3d_i32 bi;
    u3d3d_i32 bf;
    u3d3d_i32 sign;
    u3d3d_u32 ua;
    u3d3d_u32 ub;
    u3d3d_u32 prod_i;
    u3d3d_u32 prod_f;
    u3d3d_u32 mid;
    u3d3d_u32 res;

    sign = 0;
    if (a < 0) {
        sign = !sign;
        ua = (u3d3d_u32)(-a);
    } else {
        ua = (u3d3d_u32)a;
    }
    if (b < 0) {
        sign = !sign;
        ub = (u3d3d_u32)(-b);
    } else {
        ub = (u3d3d_u32)b;
    }

    ai = (u3d3d_i32)(ua >> 16);
    af = (u3d3d_i32)(ua & 0xffffu);
    bi = (u3d3d_i32)(ub >> 16);
    bf = (u3d3d_i32)(ub & 0xffffu);

    prod_i = ((u3d3d_u32)ai) * ((u3d3d_u32)bi);
    mid = (((u3d3d_u32)ai) * ((u3d3d_u32)bf)) +
          (((u3d3d_u32)bi) * ((u3d3d_u32)af));
    prod_f = (((u3d3d_u32)af) * ((u3d3d_u32)bf)) >> 16;
    res = (prod_i << 16) + mid + prod_f;

    if (sign) {
        return (u3d3d_fx)(-(u3d3d_i32)res);
    }
    return (u3d3d_fx)res;
}

u3d3d_u16 u3d3d_get_le16(const u3d3d_u8 *p)
{
    return (u3d3d_u16)((u3d3d_u16)p[0] | ((u3d3d_u16)p[1] << 8));
}

u3d3d_u32 u3d3d_get_le32(const u3d3d_u8 *p)
{
    u3d3d_u32 v;
    v = (u3d3d_u32)p[0];
    v |= ((u3d3d_u32)p[1]) << 8;
    v |= ((u3d3d_u32)p[2]) << 16;
    v |= ((u3d3d_u32)p[3]) << 24;
    return v;
}

void u3d3d_put_le16(u3d3d_u8 *p, u3d3d_u16 v)
{
    p[0] = (u3d3d_u8)(v & 255u);
    p[1] = (u3d3d_u8)((v >> 8) & 255u);
}

void u3d3d_put_le32(u3d3d_u8 *p, u3d3d_u32 v)
{
    p[0] = (u3d3d_u8)(v & 255u);
    p[1] = (u3d3d_u8)((v >> 8) & 255u);
    p[2] = (u3d3d_u8)((v >> 16) & 255u);
    p[3] = (u3d3d_u8)((v >> 24) & 255u);
}

u3d3d_i32 u3d3d_unpack_signed(u3d3d_u32 bits, unsigned width)
{
    u3d3d_u32 sign;
    u3d3d_u32 range;
    u3d3d_u32 mask;

    if (width == 0u || width >= 32u) {
        return (u3d3d_i32)bits;
    }

    range = (u3d3d_u32)1u << width;
    sign = (u3d3d_u32)1u << (width - 1u);
    mask = range - 1u;
    bits &= mask;
    if ((bits & sign) != 0u) {
        return (u3d3d_i32)(bits - range);
    }
    return (u3d3d_i32)bits;
}

u3d3d_u32 u3d3d_pack_classic_vertex_i32(u3d3d_i32 x, u3d3d_i32 y, u3d3d_i32 z)
{
    u3d3d_u32 px;
    u3d3d_u32 py;
    u3d3d_u32 pz;

    px = ((u3d3d_u32)x) & 0x7ffu;
    py = ((u3d3d_u32)y) & 0x7ffu;
    pz = ((u3d3d_u32)z) & 0x3ffu;
    return px | (py << 11) | (pz << 22);
}

void u3d3d_unpack_classic_vertex(u3d3d_u32 packed, u3d3d_vertex *out_v)
{
    u3d3d_i32 x;
    u3d3d_i32 y;
    u3d3d_i32 z;

    if (out_v == (u3d3d_vertex *)0) {
        return;
    }

    x = u3d3d_unpack_signed(packed & 0x7ffu, 11u);
    y = u3d3d_unpack_signed((packed >> 11) & 0x7ffu, 11u);
    z = u3d3d_unpack_signed((packed >> 22) & 0x3ffu, 10u);
    out_v->x = u3d3d_fx_from_i32(x);
    out_v->y = u3d3d_fx_from_i32(y);
    out_v->z = u3d3d_fx_from_i32(z);
}

int u3d3d_pack_classic_vertex_fx(const u3d3d_vertex *v, u3d3d_u32 *out_packed)
{
    u3d3d_i32 x;
    u3d3d_i32 y;
    u3d3d_i32 z;

    if (v == (const u3d3d_vertex *)0 || out_packed == (u3d3d_u32 *)0) {
        return U3D3D_ERR_NULL;
    }

    if ((v->x & 0xffff) != 0 || (v->y & 0xffff) != 0 || (v->z & 0xffff) != 0) {
        return U3D3D_ERR_RANGE;
    }

    x = u3d3d_fx_to_i32_floor(v->x);
    y = u3d3d_fx_to_i32_floor(v->y);
    z = u3d3d_fx_to_i32_floor(v->z);

    if (x < -1024 || x > 1023 || y < -1024 || y > 1023 || z < -512 || z > 511) {
        return U3D3D_ERR_RANGE;
    }

    *out_packed = u3d3d_pack_classic_vertex_i32(x, y, z);
    return U3D3D_OK;
}

void u3d3d_model_init(u3d3d_model *model,
                      u3d3d_triangle *triangles,
                      u3d3d_u16 triangle_capacity)
{
    if (model == (u3d3d_model *)0) {
        return;
    }
    model->vertex_count = 0u;
    model->triangle_count = 0u;
    model->frame_count = 0u;
    model->frame_size = 0u;
    model->variant = U3D3D_VARIANT_CLASSIC;
    model->reserved0 = 0u;
    model->triangles = triangles;
    model->triangle_capacity = triangle_capacity;
}

int u3d3d_decode_d(const u3d3d_u8 *d_bytes,
                   u3d3d_u32 d_size,
                   u3d3d_model *model)
{
    u3d3d_u16 num_tri;
    u3d3d_u16 num_vert;
    u3d3d_u32 expected;
    u3d3d_u32 off;
    u3d3d_u16 i;
    u3d3d_triangle *t;

    if (d_bytes == (const u3d3d_u8 *)0 || model == (u3d3d_model *)0) {
        return U3D3D_ERR_NULL;
    }
    if (d_size < U3D3D_D_HEADER_SIZE) {
        return U3D3D_ERR_TOO_SHORT;
    }

    num_tri = u3d3d_get_le16(d_bytes + 0u);
    num_vert = u3d3d_get_le16(d_bytes + 2u);
    if (num_tri == 0u || num_vert < 3u) {
        return U3D3D_ERR_BAD_COUNT;
    }
    if (model->triangles == (u3d3d_triangle *)0) {
        return U3D3D_ERR_NULL;
    }
    if (num_tri > model->triangle_capacity) {
        return U3D3D_ERR_CAPACITY;
    }

    expected = U3D3D_D_HEADER_SIZE + ((u3d3d_u32)num_tri * U3D3D_D_TRI_SIZE);
    if (d_size < expected) {
        return U3D3D_ERR_SIZE_MISMATCH;
    }

    model->vertex_count = num_vert;
    model->triangle_count = num_tri;
    model->variant = U3D3D_VARIANT_CLASSIC;

    off = U3D3D_D_HEADER_SIZE;
    for (i = 0u; i < num_tri; ++i) {
        t = &model->triangles[i];
        t->v[0] = u3d3d_get_le16(d_bytes + off + 0u);
        t->v[1] = u3d3d_get_le16(d_bytes + off + 2u);
        t->v[2] = u3d3d_get_le16(d_bytes + off + 4u);
        t->type = d_bytes[off + 6u];
        t->color = d_bytes[off + 7u];
        t->uv[0][0] = d_bytes[off + 8u];
        t->uv[0][1] = d_bytes[off + 9u];
        t->uv[1][0] = d_bytes[off + 10u];
        t->uv[1][1] = d_bytes[off + 11u];
        t->uv[2][0] = d_bytes[off + 12u];
        t->uv[2][1] = d_bytes[off + 13u];
        t->texture_num = d_bytes[off + 14u];
        t->flags = d_bytes[off + 15u];
        off += U3D3D_D_TRI_SIZE;
    }

    return U3D3D_OK;
}

int u3d3d_decode_a_header(const u3d3d_u8 *a_bytes,
                          u3d3d_u32 a_size,
                          u3d3d_model *model)
{
    u3d3d_u16 num_frames;
    u3d3d_u16 frame_size;
    u3d3d_u32 min_size;

    if (a_bytes == (const u3d3d_u8 *)0 || model == (u3d3d_model *)0) {
        return U3D3D_ERR_NULL;
    }
    if (a_size < U3D3D_A_HEADER_SIZE) {
        return U3D3D_ERR_TOO_SHORT;
    }
    if (model->vertex_count == 0u) {
        return U3D3D_ERR_BAD_COUNT;
    }

    num_frames = u3d3d_get_le16(a_bytes + 0u);
    frame_size = u3d3d_get_le16(a_bytes + 2u);
    if (num_frames == 0u) {
        return U3D3D_ERR_BAD_COUNT;
    }
    if (frame_size != (u3d3d_u16)(model->vertex_count * U3D3D_VERTEX_BYTES)) {
        return U3D3D_ERR_SIZE_MISMATCH;
    }

    min_size = U3D3D_A_HEADER_SIZE + ((u3d3d_u32)num_frames * (u3d3d_u32)frame_size);
    if (a_size < min_size) {
        return U3D3D_ERR_SIZE_MISMATCH;
    }

    model->frame_count = num_frames;
    model->frame_size = frame_size;
    return U3D3D_OK;
}

int u3d3d_decode_a_frame(const u3d3d_u8 *a_bytes,
                         u3d3d_u32 a_size,
                         const u3d3d_model *model,
                         u3d3d_u16 frame_index,
                         u3d3d_vertex *vertices,
                         u3d3d_u16 vertex_capacity)
{
    u3d3d_u32 off;
    u3d3d_u16 i;
    u3d3d_u32 packed;

    if (a_bytes == (const u3d3d_u8 *)0 || model == (const u3d3d_model *)0 ||
        vertices == (u3d3d_vertex *)0) {
        return U3D3D_ERR_NULL;
    }
    if (model->vertex_count == 0u || model->frame_count == 0u) {
        return U3D3D_ERR_BAD_COUNT;
    }
    if (vertex_capacity < model->vertex_count) {
        return U3D3D_ERR_CAPACITY;
    }
    if (frame_index >= model->frame_count) {
        return U3D3D_ERR_FRAME;
    }

    off = U3D3D_A_HEADER_SIZE + ((u3d3d_u32)frame_index * (u3d3d_u32)model->frame_size);
    if (a_size < off + (u3d3d_u32)model->frame_size) {
        return U3D3D_ERR_SIZE_MISMATCH;
    }

    for (i = 0u; i < model->vertex_count; ++i) {
        packed = u3d3d_get_le32(a_bytes + off);
        u3d3d_unpack_classic_vertex(packed, &vertices[i]);
        off += U3D3D_VERTEX_BYTES;
    }
    return U3D3D_OK;
}

int u3d3d_decode_pair_frame(const u3d3d_u8 *d_bytes,
                            u3d3d_u32 d_size,
                            const u3d3d_u8 *a_bytes,
                            u3d3d_u32 a_size,
                            u3d3d_u16 frame_index,
                            u3d3d_model *model,
                            u3d3d_vertex *vertices,
                            u3d3d_u16 vertex_capacity)
{
    int rc;

    rc = u3d3d_decode_d(d_bytes, d_size, model);
    if (rc != U3D3D_OK) {
        return rc;
    }
    rc = u3d3d_decode_a_header(a_bytes, a_size, model);
    if (rc != U3D3D_OK) {
        return rc;
    }
    rc = u3d3d_decode_a_frame(a_bytes, a_size, model, frame_index, vertices, vertex_capacity);
    return rc;
}

int u3d3d_build_diag(const u3d3d_u8 *d_bytes,
                     u3d3d_u32 d_size,
                     const u3d3d_u8 *a_bytes,
                     u3d3d_u32 a_size,
                     const u3d3d_model *model,
                     u3d3d_diag *diag)
{
    u3d3d_u16 i;
    u3d3d_u16 j;
    const u3d3d_triangle *t;
    u3d3d_u16 maxv;
    u3d3d_u16 bad;
    u3d3d_u16 placeholders;
    u3d3d_u8 maxtex;

    (void)d_bytes;
    (void)d_size;
    (void)a_bytes;
    (void)a_size;

    if (model == (const u3d3d_model *)0 || diag == (u3d3d_diag *)0) {
        return U3D3D_ERR_NULL;
    }

    diag->d_expected_size = U3D3D_D_HEADER_SIZE + ((u3d3d_u32)model->triangle_count * U3D3D_D_TRI_SIZE);
    diag->a_expected_min_size = U3D3D_A_HEADER_SIZE;
    diag->a_expected_full_size = U3D3D_A_HEADER_SIZE +
        ((u3d3d_u32)model->frame_count * (u3d3d_u32)model->frame_size);
    maxv = 0u;
    bad = 0u;
    placeholders = 0u;
    maxtex = 0u;

    for (i = 0u; i < model->triangle_count; ++i) {
        t = &model->triangles[i];
        if (t->texture_num > maxtex) {
            maxtex = t->texture_num;
        }
        if (t->type == U3D3D_MF_WEAPON_PLACEHOLDER) {
            ++placeholders;
        }
        for (j = 0u; j < 3u; ++j) {
            if (t->v[j] > maxv) {
                maxv = t->v[j];
            }
            if (t->v[j] >= model->vertex_count) {
                ++bad;
            }
        }
    }

    diag->max_vertex_index = maxv;
    diag->bad_triangle_indices = bad;
    diag->weapon_placeholders = placeholders;
    diag->max_texture_slot = maxtex;
    return U3D3D_OK;
}

const char *u3d3d_error_string(int code)
{
    switch (code) {
    case U3D3D_OK: return "ok";
    case U3D3D_ERR_NULL: return "null argument";
    case U3D3D_ERR_TOO_SHORT: return "input too short";
    case U3D3D_ERR_BAD_COUNT: return "bad count";
    case U3D3D_ERR_CAPACITY: return "caller buffer capacity too small";
    case U3D3D_ERR_SIZE_MISMATCH: return "file size/count mismatch";
    case U3D3D_ERR_RANGE: return "value out of representable range";
    case U3D3D_ERR_FRAME: return "frame index out of range";
    case U3D3D_ERR_UNSUPPORTED: return "unsupported variant or feature";
    case U3D3D_ERR_IO: return "i/o error";
    case U3D3D_ERR_PARSE: return "parse error";
    default: return "unknown error";
    }
}

const char *u3d3d_triangle_type_name(u3d3d_u8 type)
{
    switch (type) {
    case U3D3D_MF_NORMAL_OS: return "normal_one_sided";
    case U3D3D_MF_NORMAL_TS: return "normal_two_sided";
    case U3D3D_MF_TRANSLUCENT_TS: return "translucent_two_sided";
    case U3D3D_MF_MASKED_TS: return "masked_two_sided";
    case U3D3D_MF_MODULATED_TS: return "modulated_two_sided";
    case U3D3D_MF_WEAPON_PLACEHOLDER: return "weapon_placeholder";
    default: return "unknown";
    }
}
