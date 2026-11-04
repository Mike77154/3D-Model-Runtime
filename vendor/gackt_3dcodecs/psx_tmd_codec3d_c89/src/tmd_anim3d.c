#include "tmd_anim3d.h"

static const tmd_i16 tmd_sin_deg_q12_table[91] = {
    0, 71, 143, 214, 286, 357, 428, 499, 570, 641, 711, 782, 852,
    921, 991, 1060, 1129, 1198, 1266, 1334, 1401, 1468, 1534, 1600, 1666, 1731,
    1796, 1860, 1923, 1986, 2048, 2110, 2171, 2231, 2290, 2349, 2408, 2465, 2522,
    2578, 2633, 2687, 2741, 2793, 2845, 2896, 2946, 2996, 3044, 3091, 3138, 3183,
    3228, 3271, 3314, 3355, 3396, 3435, 3474, 3511, 3547, 3582, 3617, 3650, 3681,
    3712, 3742, 3770, 3798, 3824, 3849, 3873, 3896, 3917, 3937, 3956, 3974, 3991,
    4006, 4021, 4034, 4046, 4056, 4065, 4074, 4080, 4086, 4090, 4094, 4095, 4096
};

static int tmd_anim_range_ok(tmd_u32 size, tmd_u32 off, tmd_u32 len)
{
    if (off > size) return 0;
    if (len > size) return 0;
    if (off > (size - len)) return 0;
    return 1;
}

static tmd_u32 tmd_anim_read_u32le_raw(const tmd_u8 *p)
{
    tmd_u32 v;
    v = ((tmd_u32)p[0]);
    v |= ((tmd_u32)p[1]) << 8;
    v |= ((tmd_u32)p[2]) << 16;
    v |= ((tmd_u32)p[3]) << 24;
    return v;
}

static tmd_i16 tmd_anim_i16_from_u16(tmd_u16 v)
{
    return (tmd_i16)v;
}

static tmd_i32 tmd_anim_i32_from_u32(tmd_u32 v)
{
    return (tmd_i32)v;
}

static int tmd_anim_read_u32(const tmd_u8 *data, tmd_u32 size, tmd_u32 off, tmd_u32 *out)
{
    if (data == TMD_NULL || out == TMD_NULL) return TMD_ERR_NULL;
    if (!tmd_anim_range_ok(size, off, 4UL)) return TMD_ERR_RANGE;
    *out = tmd_anim_read_u32le_raw(data + off);
    return TMD_ERR_OK;
}

static tmd_i32 tmd_fx_mul(tmd_i32 a, tmd_i32 b)
{
    tmd_i32 prod;
    prod = a * b;
    if (prod >= 0) return (prod + (TMD_FX_ONE >> 1)) >> TMD_FX_SHIFT;
    return -(((-prod) + (TMD_FX_ONE >> 1)) >> TMD_FX_SHIFT);
}

static tmd_i32 tmd_lerp_i32(tmd_i32 a, tmd_i32 b, tmd_i32 t_fx12)
{
    return a + tmd_fx_mul(b - a, t_fx12);
}


static tmd_i32 tmd_mod_i32(tmd_i32 v, tmd_i32 m)
{
    tmd_i32 r;
    if (m == 0) return 0;
    r = v % m;
    if (r < 0) r += m;
    return r;
}

static tmd_i32 tmd_sin_deg_fx12(tmd_i32 deg_fx12)
{
    tmd_i32 deg;
    tmd_i32 frac;
    tmd_i32 q;
    tmd_i32 base;
    tmd_i32 next;
    tmd_i32 val;
    int sign;

    deg = deg_fx12 >> TMD_FX_SHIFT;
    frac = deg_fx12 & (TMD_FX_ONE - 1);
    deg = tmd_mod_i32(deg, 360L);
    sign = 1;

    if (deg >= 180L) {
        deg -= 180L;
        sign = -1;
    }
    if (deg > 90L) deg = 180L - deg;

    q = deg;
    base = (tmd_i32)tmd_sin_deg_q12_table[(int)q];
    if (q >= 90L) next = base;
    else next = (tmd_i32)tmd_sin_deg_q12_table[(int)(q + 1L)];
    val = tmd_lerp_i32(base, next, frac);
    return (sign < 0) ? -val : val;
}

static tmd_i32 tmd_cos_deg_fx12(tmd_i32 deg_fx12)
{
    return tmd_sin_deg_fx12(deg_fx12 + (90L << TMD_FX_SHIFT));
}

static tmd_i16 tmd_clamp_i16(tmd_i32 v)
{
    if (v > 32767L) return (tmd_i16)32767;
    if (v < -32768L) return (tmd_i16)-32768;
    return (tmd_i16)v;
}

void tmd_transform_identity(TMD_Transform *tr)
{
    if (tr == TMD_NULL) return;
    tr->rx_deg_fx12 = 0L;
    tr->ry_deg_fx12 = 0L;
    tr->rz_deg_fx12 = 0L;
    tr->sx_fx12 = TMD_FX_ONE;
    tr->sy_fx12 = TMD_FX_ONE;
    tr->sz_fx12 = TMD_FX_ONE;
    tr->tx = 0L;
    tr->ty = 0L;
    tr->tz = 0L;
    tr->flags = (tmd_u16)(TMD_TRANSFORMF_HAS_ROT | TMD_TRANSFORMF_HAS_SCALE | TMD_TRANSFORMF_HAS_TRANS);
}

void tmd_matrix_identity(TMD_Matrix34 *m)
{
    int r;
    int c;
    if (m == TMD_NULL) return;
    for (r = 0; r < 3; ++r) {
        for (c = 0; c < 3; ++c) {
            m->m[r][c] = (r == c) ? TMD_FX_ONE : 0L;
        }
        m->t[r] = 0L;
    }
}

int tmd_transform_lerp(const TMD_Transform *a,
                       const TMD_Transform *b,
                       tmd_i32 t_fx12,
                       TMD_Transform *out_tr)
{
    if (a == TMD_NULL || b == TMD_NULL || out_tr == TMD_NULL) return TMD_ERR_NULL;
    out_tr->rx_deg_fx12 = tmd_lerp_i32(a->rx_deg_fx12, b->rx_deg_fx12, t_fx12);
    out_tr->ry_deg_fx12 = tmd_lerp_i32(a->ry_deg_fx12, b->ry_deg_fx12, t_fx12);
    out_tr->rz_deg_fx12 = tmd_lerp_i32(a->rz_deg_fx12, b->rz_deg_fx12, t_fx12);
    out_tr->sx_fx12 = tmd_lerp_i32(a->sx_fx12, b->sx_fx12, t_fx12);
    out_tr->sy_fx12 = tmd_lerp_i32(a->sy_fx12, b->sy_fx12, t_fx12);
    out_tr->sz_fx12 = tmd_lerp_i32(a->sz_fx12, b->sz_fx12, t_fx12);
    out_tr->tx = tmd_lerp_i32(a->tx, b->tx, t_fx12);
    out_tr->ty = tmd_lerp_i32(a->ty, b->ty, t_fx12);
    out_tr->tz = tmd_lerp_i32(a->tz, b->tz, t_fx12);
    out_tr->flags = (tmd_u16)(a->flags | b->flags);
    return TMD_ERR_OK;
}

int tmd_matrix_from_transform(const TMD_Transform *tr,
                              TMD_Matrix34 *out_matrix)
{
    tmd_i32 sx;
    tmd_i32 sy;
    tmd_i32 sz;
    tmd_i32 cx;
    tmd_i32 sxn;
    tmd_i32 cy;
    tmd_i32 syn;
    tmd_i32 cz;
    tmd_i32 szn;
    tmd_i32 a;
    tmd_i32 b;
    tmd_i32 c;
    tmd_i32 d;

    if (tr == TMD_NULL || out_matrix == TMD_NULL) return TMD_ERR_NULL;

    sx = tr->sx_fx12;
    sy = tr->sy_fx12;
    sz = tr->sz_fx12;
    cx = tmd_cos_deg_fx12(tr->rx_deg_fx12);
    sxn = tmd_sin_deg_fx12(tr->rx_deg_fx12);
    cy = tmd_cos_deg_fx12(tr->ry_deg_fx12);
    syn = tmd_sin_deg_fx12(tr->ry_deg_fx12);
    cz = tmd_cos_deg_fx12(tr->rz_deg_fx12);
    szn = tmd_sin_deg_fx12(tr->rz_deg_fx12);

    a = tmd_fx_mul(cz, cy);
    b = tmd_fx_mul(tmd_fx_mul(cz, syn), sxn) - tmd_fx_mul(szn, cx);
    c = tmd_fx_mul(tmd_fx_mul(cz, syn), cx) + tmd_fx_mul(szn, sxn);
    d = tmd_fx_mul(szn, cy);

    out_matrix->m[0][0] = tmd_fx_mul(a, sx);
    out_matrix->m[0][1] = tmd_fx_mul(b, sy);
    out_matrix->m[0][2] = tmd_fx_mul(c, sz);

    a = tmd_fx_mul(tmd_fx_mul(szn, syn), sxn) + tmd_fx_mul(cz, cx);
    b = tmd_fx_mul(tmd_fx_mul(szn, syn), cx) - tmd_fx_mul(cz, sxn);
    out_matrix->m[1][0] = tmd_fx_mul(d, sx);
    out_matrix->m[1][1] = tmd_fx_mul(a, sy);
    out_matrix->m[1][2] = tmd_fx_mul(b, sz);

    out_matrix->m[2][0] = tmd_fx_mul(-syn, sx);
    out_matrix->m[2][1] = tmd_fx_mul(tmd_fx_mul(cy, sxn), sy);
    out_matrix->m[2][2] = tmd_fx_mul(tmd_fx_mul(cy, cx), sz);

    out_matrix->t[0] = tr->tx;
    out_matrix->t[1] = tr->ty;
    out_matrix->t[2] = tr->tz;
    return TMD_ERR_OK;
}

int tmd_matrix_mul(const TMD_Matrix34 *parent,
                   const TMD_Matrix34 *local,
                   TMD_Matrix34 *out_matrix)
{
    TMD_Matrix34 tmp;
    int r;
    int c;
    if (parent == TMD_NULL || local == TMD_NULL || out_matrix == TMD_NULL) return TMD_ERR_NULL;

    for (r = 0; r < 3; ++r) {
        for (c = 0; c < 3; ++c) {
            tmp.m[r][c] = tmd_fx_mul(parent->m[r][0], local->m[0][c]) +
                          tmd_fx_mul(parent->m[r][1], local->m[1][c]) +
                          tmd_fx_mul(parent->m[r][2], local->m[2][c]);
        }
        tmp.t[r] = parent->t[r] +
                   tmd_fx_mul(parent->m[r][0], local->t[0]) +
                   tmd_fx_mul(parent->m[r][1], local->t[1]) +
                   tmd_fx_mul(parent->m[r][2], local->t[2]);
    }

    *out_matrix = tmp;
    return TMD_ERR_OK;
}

int tmd_skeleton_build_world(const TMD_Bone *bones,
                             tmd_u32 bone_count,
                             const TMD_Transform *local_pose,
                             TMD_Matrix34 *world_slots,
                             tmd_u32 world_slot_count)
{
    tmd_u32 i;
    TMD_Matrix34 local_m;
    int r;

    if (bones == TMD_NULL || local_pose == TMD_NULL || world_slots == TMD_NULL) return TMD_ERR_NULL;
    if (bone_count > world_slot_count) return TMD_ERR_OUTPUT_OVERFLOW;

    for (i = 0UL; i < bone_count; ++i) {
        r = tmd_matrix_from_transform(local_pose + i, &local_m);
        if (r != TMD_ERR_OK) return r;
        if (bones[i].parent_index == TMD_BONE_NO_PARENT) {
            world_slots[i] = local_m;
        } else {
            tmd_i32 parent;
            parent = (tmd_i32)bones[i].parent_index;
            if (parent < 0 || (tmd_u32)parent >= i) return TMD_ERR_BAD_SKELETON;
            r = tmd_matrix_mul(world_slots + parent, &local_m, world_slots + i);
            if (r != TMD_ERR_OK) return r;
        }
    }
    return TMD_ERR_OK;
}

int tmd_transform_vertex(const TMD_Matrix34 *matrix,
                         const TMD_Vertex *in_vertex,
                         TMD_Vertex *out_vertex)
{
    tmd_i32 x;
    tmd_i32 y;
    tmd_i32 z;
    tmd_i32 ox;
    tmd_i32 oy;
    tmd_i32 oz;
    if (matrix == TMD_NULL || in_vertex == TMD_NULL || out_vertex == TMD_NULL) return TMD_ERR_NULL;

    x = (tmd_i32)in_vertex->x;
    y = (tmd_i32)in_vertex->y;
    z = (tmd_i32)in_vertex->z;
    ox = matrix->t[0] + tmd_fx_mul(matrix->m[0][0], x) +
                         tmd_fx_mul(matrix->m[0][1], y) +
                         tmd_fx_mul(matrix->m[0][2], z);
    oy = matrix->t[1] + tmd_fx_mul(matrix->m[1][0], x) +
                         tmd_fx_mul(matrix->m[1][1], y) +
                         tmd_fx_mul(matrix->m[1][2], z);
    oz = matrix->t[2] + tmd_fx_mul(matrix->m[2][0], x) +
                         tmd_fx_mul(matrix->m[2][1], y) +
                         tmd_fx_mul(matrix->m[2][2], z);
    out_vertex->x = tmd_clamp_i16(ox);
    out_vertex->y = tmd_clamp_i16(oy);
    out_vertex->z = tmd_clamp_i16(oz);
    return TMD_ERR_OK;
}

int tmd_anim_sample_keyframes(const TMD_AnimKey *keys,
                              tmd_u32 key_count,
                              tmd_u32 frame,
                              TMD_Transform *local_pose,
                              tmd_u32 local_pose_count)
{
    tmd_u32 bone;
    tmd_u32 i;
    const TMD_AnimKey *prev;
    const TMD_AnimKey *next;
    tmd_i32 t;

    if (keys == TMD_NULL || local_pose == TMD_NULL) return TMD_ERR_NULL;

    for (bone = 0UL; bone < local_pose_count; ++bone) {
        prev = TMD_NULL;
        next = TMD_NULL;
        for (i = 0UL; i < key_count; ++i) {
            if ((tmd_u32)keys[i].bone_index != bone) continue;
            if (keys[i].frame <= frame) {
                if (prev == TMD_NULL || keys[i].frame >= prev->frame) prev = keys + i;
            }
            if (keys[i].frame >= frame) {
                if (next == TMD_NULL || keys[i].frame <= next->frame) next = keys + i;
            }
        }
        if (prev != TMD_NULL && next != TMD_NULL) {
            if (prev->frame == next->frame) {
                local_pose[bone] = prev->transform;
            } else {
                t = (tmd_i32)(((frame - prev->frame) << TMD_FX_SHIFT) / (next->frame - prev->frame));
                tmd_transform_lerp(&prev->transform, &next->transform, t, local_pose + bone);
            }
        } else if (prev != TMD_NULL) {
            local_pose[bone] = prev->transform;
        } else if (next != TMD_NULL) {
            local_pose[bone] = next->transform;
        }
    }
    return TMD_ERR_OK;
}

int tmd_tod_parse(TMD_TOD_Context *ctx,
                  const void *bytes,
                  tmd_u32 byte_count)
{
    tmd_u32 w0;
    tmd_u32 w1;
    if (ctx == TMD_NULL || bytes == TMD_NULL) return TMD_ERR_NULL;
    ctx->data = (const tmd_u8 *)bytes;
    ctx->size = byte_count;
    ctx->header.id = 0U;
    ctx->header.version = 0U;
    ctx->header.resolution_ticks = 0U;
    ctx->header.frame_count = 0UL;
    if (byte_count < 8UL) return TMD_ERR_TOO_SMALL;
    w0 = tmd_anim_read_u32le_raw(ctx->data + 0UL);
    w1 = tmd_anim_read_u32le_raw(ctx->data + 4UL);
    ctx->header.id = (tmd_u8)(w0 & 0xffUL);
    ctx->header.version = (tmd_u8)((w0 >> 8) & 0xffUL);
    ctx->header.resolution_ticks = (tmd_u16)((w0 >> 16) & 0xffffUL);
    ctx->header.frame_count = w1;
    if (ctx->header.id != TMD_TOD_ID) return TMD_ERR_BAD_TOD;
    return TMD_ERR_OK;
}

int tmd_tod_get_frame(const TMD_TOD_Context *ctx,
                      tmd_u32 frame_index,
                      TMD_TOD_Frame *out_frame)
{
    tmd_u32 off;
    tmd_u32 i;
    tmd_u32 w0;
    tmd_u32 w1;
    tmd_u16 frame_size_words;
    tmd_u16 packet_count;
    tmd_u32 frame_bytes;

    if (ctx == TMD_NULL || out_frame == TMD_NULL) return TMD_ERR_NULL;
    if (frame_index >= ctx->header.frame_count) return TMD_ERR_RANGE;

    off = 8UL;
    for (i = 0UL; i <= frame_index; ++i) {
        if (!tmd_anim_range_ok(ctx->size, off, 8UL)) return TMD_ERR_BAD_TOD;
        w0 = tmd_anim_read_u32le_raw(ctx->data + off);
        w1 = tmd_anim_read_u32le_raw(ctx->data + off + 4UL);
        frame_size_words = (tmd_u16)(w0 & 0xffffUL);
        packet_count = (tmd_u16)((w0 >> 16) & 0xffffUL);
        if (frame_size_words < 2U) return TMD_ERR_BAD_TOD;
        frame_bytes = ((tmd_u32)frame_size_words) * 4UL;
        if (!tmd_anim_range_ok(ctx->size, off, frame_bytes)) return TMD_ERR_BAD_TOD;
        if (i == frame_index) {
            out_frame->frame_offset = off;
            out_frame->packet_offset = off + 8UL;
            out_frame->frame_size_words = frame_size_words;
            out_frame->packet_count = packet_count;
            out_frame->frame_number = w1;
            return TMD_ERR_OK;
        }
        off += frame_bytes;
    }
    return TMD_ERR_RANGE;
}

int tmd_tod_get_packet(const TMD_TOD_Context *ctx,
                       const TMD_TOD_Frame *frame,
                       tmd_u32 packet_index,
                       TMD_TOD_Packet *out_packet)
{
    tmd_u32 off;
    tmd_u32 frame_end;
    tmd_u32 i;
    tmd_u32 h;
    tmd_u32 packet_bytes;
    tmd_u8 length_words;

    if (ctx == TMD_NULL || frame == TMD_NULL || out_packet == TMD_NULL) return TMD_ERR_NULL;
    if (packet_index >= (tmd_u32)frame->packet_count) return TMD_ERR_RANGE;

    off = frame->packet_offset;
    frame_end = frame->frame_offset + ((tmd_u32)frame->frame_size_words * 4UL);

    for (i = 0UL; i <= packet_index; ++i) {
        if (off > frame_end || !tmd_anim_range_ok(ctx->size, off, 4UL)) return TMD_ERR_BAD_TOD;
        h = tmd_anim_read_u32le_raw(ctx->data + off);
        length_words = (tmd_u8)((h >> 24) & 0xffUL);
        if (length_words == 0U) return TMD_ERR_BAD_TOD;
        packet_bytes = ((tmd_u32)length_words) * 4UL;
        if (off + packet_bytes > frame_end) return TMD_ERR_BAD_TOD;
        if (!tmd_anim_range_ok(ctx->size, off, packet_bytes)) return TMD_ERR_BAD_TOD;
        if (i == packet_index) {
            out_packet->packet_offset = off;
            out_packet->object_id = (tmd_u16)(h & 0xffffUL);
            out_packet->packet_type = (tmd_u8)((h >> 16) & 0x0fUL);
            out_packet->flag = (tmd_u8)((h >> 20) & 0x0fUL);
            out_packet->length_words = length_words;
            return TMD_ERR_OK;
        }
        off += packet_bytes;
    }
    return TMD_ERR_RANGE;
}

int tmd_tod_decode_rst_packet(const TMD_TOD_Context *ctx,
                              const TMD_TOD_Packet *packet,
                              TMD_Transform *out_transform)
{
    tmd_u32 off;
    tmd_u32 end;
    tmd_u32 w;
    int r;

    if (ctx == TMD_NULL || packet == TMD_NULL || out_transform == TMD_NULL) return TMD_ERR_NULL;
    if (packet->packet_type != TMD_TOD_PACKET_COORD_RST) return TMD_ERR_UNSUPPORTED_TOD_PACKET;

    tmd_transform_identity(out_transform);
    out_transform->flags = 0U;
    if ((packet->flag & TMD_TOD_FLAG_RST_DELTA) != 0U) out_transform->flags |= TMD_TRANSFORMF_DELTA;

    off = packet->packet_offset + 4UL;
    end = packet->packet_offset + ((tmd_u32)packet->length_words * 4UL);

    if ((packet->flag & TMD_TOD_FLAG_RST_ROT) != 0U) {
        if (off + 12UL > end) return TMD_ERR_BAD_TOD;
        r = tmd_anim_read_u32(ctx->data, ctx->size, off, &w);
        if (r != TMD_ERR_OK) return r;
        out_transform->rx_deg_fx12 = tmd_anim_i32_from_u32(w);
        r = tmd_anim_read_u32(ctx->data, ctx->size, off + 4UL, &w);
        if (r != TMD_ERR_OK) return r;
        out_transform->ry_deg_fx12 = tmd_anim_i32_from_u32(w);
        r = tmd_anim_read_u32(ctx->data, ctx->size, off + 8UL, &w);
        if (r != TMD_ERR_OK) return r;
        out_transform->rz_deg_fx12 = tmd_anim_i32_from_u32(w);
        out_transform->flags |= TMD_TRANSFORMF_HAS_ROT;
        off += 12UL;
    }

    if ((packet->flag & TMD_TOD_FLAG_RST_SCALE) != 0U) {
        if (off + 8UL > end) return TMD_ERR_BAD_TOD;
        r = tmd_anim_read_u32(ctx->data, ctx->size, off, &w);
        if (r != TMD_ERR_OK) return r;
        out_transform->sx_fx12 = (tmd_i32)tmd_anim_i16_from_u16((tmd_u16)(w & 0xffffUL));
        out_transform->sy_fx12 = (tmd_i32)tmd_anim_i16_from_u16((tmd_u16)((w >> 16) & 0xffffUL));
        r = tmd_anim_read_u32(ctx->data, ctx->size, off + 4UL, &w);
        if (r != TMD_ERR_OK) return r;
        out_transform->sz_fx12 = (tmd_i32)tmd_anim_i16_from_u16((tmd_u16)(w & 0xffffUL));
        out_transform->flags |= TMD_TRANSFORMF_HAS_SCALE;
        off += 8UL;
    }

    if ((packet->flag & TMD_TOD_FLAG_RST_TRANS) != 0U) {
        if (off + 12UL > end) return TMD_ERR_BAD_TOD;
        r = tmd_anim_read_u32(ctx->data, ctx->size, off, &w);
        if (r != TMD_ERR_OK) return r;
        out_transform->tx = tmd_anim_i32_from_u32(w);
        r = tmd_anim_read_u32(ctx->data, ctx->size, off + 4UL, &w);
        if (r != TMD_ERR_OK) return r;
        out_transform->ty = tmd_anim_i32_from_u32(w);
        r = tmd_anim_read_u32(ctx->data, ctx->size, off + 8UL, &w);
        if (r != TMD_ERR_OK) return r;
        out_transform->tz = tmd_anim_i32_from_u32(w);
        out_transform->flags |= TMD_TRANSFORMF_HAS_TRANS;
        off += 12UL;
    }

    if (off > end) return TMD_ERR_BAD_TOD;
    return TMD_ERR_OK;
}

static void tmd_apply_partial_transform(TMD_Transform *dst, const TMD_Transform *src)
{
    if ((src->flags & TMD_TRANSFORMF_DELTA) != 0U) {
        if ((src->flags & TMD_TRANSFORMF_HAS_ROT) != 0U) {
            dst->rx_deg_fx12 += src->rx_deg_fx12;
            dst->ry_deg_fx12 += src->ry_deg_fx12;
            dst->rz_deg_fx12 += src->rz_deg_fx12;
            dst->flags |= TMD_TRANSFORMF_HAS_ROT;
        }
        if ((src->flags & TMD_TRANSFORMF_HAS_SCALE) != 0U) {
            dst->sx_fx12 = tmd_fx_mul(dst->sx_fx12, src->sx_fx12);
            dst->sy_fx12 = tmd_fx_mul(dst->sy_fx12, src->sy_fx12);
            dst->sz_fx12 = tmd_fx_mul(dst->sz_fx12, src->sz_fx12);
            dst->flags |= TMD_TRANSFORMF_HAS_SCALE;
        }
        if ((src->flags & TMD_TRANSFORMF_HAS_TRANS) != 0U) {
            dst->tx += src->tx;
            dst->ty += src->ty;
            dst->tz += src->tz;
            dst->flags |= TMD_TRANSFORMF_HAS_TRANS;
        }
    } else {
        if ((src->flags & TMD_TRANSFORMF_HAS_ROT) != 0U) {
            dst->rx_deg_fx12 = src->rx_deg_fx12;
            dst->ry_deg_fx12 = src->ry_deg_fx12;
            dst->rz_deg_fx12 = src->rz_deg_fx12;
            dst->flags |= TMD_TRANSFORMF_HAS_ROT;
        }
        if ((src->flags & TMD_TRANSFORMF_HAS_SCALE) != 0U) {
            dst->sx_fx12 = src->sx_fx12;
            dst->sy_fx12 = src->sy_fx12;
            dst->sz_fx12 = src->sz_fx12;
            dst->flags |= TMD_TRANSFORMF_HAS_SCALE;
        }
        if ((src->flags & TMD_TRANSFORMF_HAS_TRANS) != 0U) {
            dst->tx = src->tx;
            dst->ty = src->ty;
            dst->tz = src->tz;
            dst->flags |= TMD_TRANSFORMF_HAS_TRANS;
        }
    }
}

int tmd_tod_apply_frame(const TMD_TOD_Context *ctx,
                        tmd_u32 frame_index,
                        TMD_Transform *local_pose,
                        tmd_u32 local_pose_count,
                        tmd_u32 *out_applied,
                        tmd_u32 *out_skipped)
{
    TMD_TOD_Frame frame;
    TMD_TOD_Packet packet;
    TMD_Transform tr;
    tmd_u32 i;
    tmd_u32 applied;
    tmd_u32 skipped;
    int r;

    if (ctx == TMD_NULL || local_pose == TMD_NULL) return TMD_ERR_NULL;
    r = tmd_tod_get_frame(ctx, frame_index, &frame);
    if (r != TMD_ERR_OK) return r;

    applied = 0UL;
    skipped = 0UL;
    for (i = 0UL; i < (tmd_u32)frame.packet_count; ++i) {
        r = tmd_tod_get_packet(ctx, &frame, i, &packet);
        if (r != TMD_ERR_OK) return r;
        if (packet.packet_type != TMD_TOD_PACKET_COORD_RST) {
            skipped++;
            continue;
        }
        if ((tmd_u32)packet.object_id >= local_pose_count) {
            skipped++;
            continue;
        }
        r = tmd_tod_decode_rst_packet(ctx, &packet, &tr);
        if (r != TMD_ERR_OK) return r;
        tmd_apply_partial_transform(local_pose + packet.object_id, &tr);
        applied++;
    }
    if (out_applied != TMD_NULL) *out_applied = applied;
    if (out_skipped != TMD_NULL) *out_skipped = skipped;
    return TMD_ERR_OK;
}

static tmd_u32 tmd_isqrt_u32(tmd_u32 x)
{
    tmd_u32 op;
    tmd_u32 res;
    tmd_u32 one;

    op = x;
    res = 0UL;
    one = 1UL << 30;
    while (one > op) one >>= 2;
    while (one != 0UL) {
        if (op >= res + one) {
            op -= res + one;
            res = res + (one << 1);
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

void tmd_quat_identity(TMD_Quat *q)
{
    if (q == TMD_NULL) return;
    q->x_fx12 = 0L;
    q->y_fx12 = 0L;
    q->z_fx12 = 0L;
    q->w_fx12 = TMD_FX_ONE;
}

int tmd_quat_normalize(TMD_Quat *q)
{
    tmd_u32 sum;
    tmd_u32 len;
    if (q == TMD_NULL) return TMD_ERR_NULL;

    sum = (tmd_u32)(q->x_fx12 * q->x_fx12) +
          (tmd_u32)(q->y_fx12 * q->y_fx12) +
          (tmd_u32)(q->z_fx12 * q->z_fx12) +
          (tmd_u32)(q->w_fx12 * q->w_fx12);
    len = tmd_isqrt_u32(sum);
    if (len == 0UL) {
        tmd_quat_identity(q);
        return TMD_ERR_BAD_ANIMATION;
    }
    q->x_fx12 = (q->x_fx12 * TMD_FX_ONE) / (tmd_i32)len;
    q->y_fx12 = (q->y_fx12 * TMD_FX_ONE) / (tmd_i32)len;
    q->z_fx12 = (q->z_fx12 * TMD_FX_ONE) / (tmd_i32)len;
    q->w_fx12 = (q->w_fx12 * TMD_FX_ONE) / (tmd_i32)len;
    return TMD_ERR_OK;
}

int tmd_quat_from_euler_deg_fx12(tmd_i32 rx_deg_fx12,
                                 tmd_i32 ry_deg_fx12,
                                 tmd_i32 rz_deg_fx12,
                                 TMD_Quat *out_q)
{
    tmd_i32 hx;
    tmd_i32 hy;
    tmd_i32 hz;
    tmd_i32 cx;
    tmd_i32 sx;
    tmd_i32 cy;
    tmd_i32 sy;
    tmd_i32 cz;
    tmd_i32 sz;
    tmd_i32 a;
    tmd_i32 b;

    if (out_q == TMD_NULL) return TMD_ERR_NULL;

    hx = rx_deg_fx12 / 2L;
    hy = ry_deg_fx12 / 2L;
    hz = rz_deg_fx12 / 2L;
    cx = tmd_cos_deg_fx12(hx);
    sx = tmd_sin_deg_fx12(hx);
    cy = tmd_cos_deg_fx12(hy);
    sy = tmd_sin_deg_fx12(hy);
    cz = tmd_cos_deg_fx12(hz);
    sz = tmd_sin_deg_fx12(hz);

    a = tmd_fx_mul(cx, cy);
    b = tmd_fx_mul(sx, sy);
    out_q->w_fx12 = tmd_fx_mul(a, cz) + tmd_fx_mul(b, sz);
    out_q->x_fx12 = tmd_fx_mul(tmd_fx_mul(sx, cy), cz) - tmd_fx_mul(tmd_fx_mul(cx, sy), sz);
    out_q->y_fx12 = tmd_fx_mul(tmd_fx_mul(cx, sy), cz) + tmd_fx_mul(tmd_fx_mul(sx, cy), sz);
    out_q->z_fx12 = tmd_fx_mul(a, sz) - tmd_fx_mul(b, cz);
    return tmd_quat_normalize(out_q);
}

int tmd_quat_nlerp(const TMD_Quat *a,
                   const TMD_Quat *b,
                   tmd_i32 t_fx12,
                   TMD_Quat *out_q)
{
    tmd_i32 dot;
    tmd_i32 bx;
    tmd_i32 by;
    tmd_i32 bz;
    tmd_i32 bw;

    if (a == TMD_NULL || b == TMD_NULL || out_q == TMD_NULL) return TMD_ERR_NULL;

    bx = b->x_fx12;
    by = b->y_fx12;
    bz = b->z_fx12;
    bw = b->w_fx12;
    dot = tmd_fx_mul(a->x_fx12, bx) + tmd_fx_mul(a->y_fx12, by) +
          tmd_fx_mul(a->z_fx12, bz) + tmd_fx_mul(a->w_fx12, bw);
    if (dot < 0L) {
        bx = -bx;
        by = -by;
        bz = -bz;
        bw = -bw;
    }
    out_q->x_fx12 = tmd_lerp_i32(a->x_fx12, bx, t_fx12);
    out_q->y_fx12 = tmd_lerp_i32(a->y_fx12, by, t_fx12);
    out_q->z_fx12 = tmd_lerp_i32(a->z_fx12, bz, t_fx12);
    out_q->w_fx12 = tmd_lerp_i32(a->w_fx12, bw, t_fx12);
    return tmd_quat_normalize(out_q);
}

int tmd_matrix_from_quat_transform(const TMD_Quat *q,
                                   const TMD_Transform *tr,
                                   TMD_Matrix34 *out_matrix)
{
    tmd_i32 x2;
    tmd_i32 y2;
    tmd_i32 z2;
    tmd_i32 xy;
    tmd_i32 xz;
    tmd_i32 yz;
    tmd_i32 wx;
    tmd_i32 wy;
    tmd_i32 wz;
    tmd_i32 sx;
    tmd_i32 sy;
    tmd_i32 sz;

    if (q == TMD_NULL || tr == TMD_NULL || out_matrix == TMD_NULL) return TMD_ERR_NULL;

    x2 = tmd_fx_mul(q->x_fx12, q->x_fx12);
    y2 = tmd_fx_mul(q->y_fx12, q->y_fx12);
    z2 = tmd_fx_mul(q->z_fx12, q->z_fx12);
    xy = tmd_fx_mul(q->x_fx12, q->y_fx12);
    xz = tmd_fx_mul(q->x_fx12, q->z_fx12);
    yz = tmd_fx_mul(q->y_fx12, q->z_fx12);
    wx = tmd_fx_mul(q->w_fx12, q->x_fx12);
    wy = tmd_fx_mul(q->w_fx12, q->y_fx12);
    wz = tmd_fx_mul(q->w_fx12, q->z_fx12);

    sx = tr->sx_fx12;
    sy = tr->sy_fx12;
    sz = tr->sz_fx12;

    out_matrix->m[0][0] = tmd_fx_mul(TMD_FX_ONE - ((y2 + z2) << 1), sx);
    out_matrix->m[0][1] = tmd_fx_mul((xy - wz) << 1, sy);
    out_matrix->m[0][2] = tmd_fx_mul((xz + wy) << 1, sz);
    out_matrix->m[1][0] = tmd_fx_mul((xy + wz) << 1, sx);
    out_matrix->m[1][1] = tmd_fx_mul(TMD_FX_ONE - ((x2 + z2) << 1), sy);
    out_matrix->m[1][2] = tmd_fx_mul((yz - wx) << 1, sz);
    out_matrix->m[2][0] = tmd_fx_mul((xz - wy) << 1, sx);
    out_matrix->m[2][1] = tmd_fx_mul((yz + wx) << 1, sy);
    out_matrix->m[2][2] = tmd_fx_mul(TMD_FX_ONE - ((x2 + y2) << 1), sz);
    out_matrix->t[0] = tr->tx;
    out_matrix->t[1] = tr->ty;
    out_matrix->t[2] = tr->tz;
    return TMD_ERR_OK;
}

int tmd_matrix_inverse_rigid(const TMD_Matrix34 *matrix,
                             TMD_Matrix34 *out_inverse)
{
    int r;
    int c;
    if (matrix == TMD_NULL || out_inverse == TMD_NULL) return TMD_ERR_NULL;

    for (r = 0; r < 3; ++r) {
        for (c = 0; c < 3; ++c) {
            out_inverse->m[r][c] = matrix->m[c][r];
        }
    }
    for (r = 0; r < 3; ++r) {
        out_inverse->t[r] = -(tmd_fx_mul(out_inverse->m[r][0], matrix->t[0]) +
                              tmd_fx_mul(out_inverse->m[r][1], matrix->t[1]) +
                              tmd_fx_mul(out_inverse->m[r][2], matrix->t[2]));
    }
    return TMD_ERR_OK;
}

int tmd_skin_vertices_rigid(const TMD_Vertex *base_vertices,
                            tmd_u32 vertex_count,
                            const tmd_u16 *vertex_bone_indices,
                            const TMD_Matrix34 *skin_matrices,
                            tmd_u32 skin_matrix_count,
                            TMD_Vertex *out_vertices,
                            tmd_u32 out_vertex_count)
{
    tmd_u32 i;
    tmd_u32 bi;
    int r;
    if (base_vertices == TMD_NULL || vertex_bone_indices == TMD_NULL ||
        skin_matrices == TMD_NULL || out_vertices == TMD_NULL) return TMD_ERR_NULL;
    if (vertex_count > out_vertex_count) return TMD_ERR_OUTPUT_OVERFLOW;
    for (i = 0UL; i < vertex_count; ++i) {
        bi = (tmd_u32)vertex_bone_indices[i];
        if (bi >= skin_matrix_count) return TMD_ERR_BAD_SKELETON;
        r = tmd_transform_vertex(skin_matrices + bi, base_vertices + i, out_vertices + i);
        if (r != TMD_ERR_OK) return r;
    }
    return TMD_ERR_OK;
}

int tmd_pose_copy_bind(const TMD_Bone *bones,
                       tmd_u32 bone_count,
                       TMD_Transform *local_pose,
                       tmd_u32 local_pose_count)
{
    tmd_u32 i;
    if (bones == TMD_NULL || local_pose == TMD_NULL) return TMD_ERR_NULL;
    if (bone_count > local_pose_count) return TMD_ERR_OUTPUT_OVERFLOW;
    for (i = 0UL; i < bone_count; ++i) local_pose[i] = bones[i].bind_local;
    return TMD_ERR_OK;
}

int tmd_pose_blend(const TMD_Transform *a_pose,
                   const TMD_Transform *b_pose,
                   tmd_u32 pose_count,
                   tmd_i32 t_fx12,
                   TMD_Transform *out_pose,
                   tmd_u32 out_pose_count)
{
    tmd_u32 i;
    int r;
    if (a_pose == TMD_NULL || b_pose == TMD_NULL || out_pose == TMD_NULL) return TMD_ERR_NULL;
    if (pose_count > out_pose_count) return TMD_ERR_OUTPUT_OVERFLOW;
    for (i = 0UL; i < pose_count; ++i) {
        r = tmd_transform_lerp(a_pose + i, b_pose + i, t_fx12, out_pose + i);
        if (r != TMD_ERR_OK) return r;
    }
    return TMD_ERR_OK;
}

int tmd_tod_find_frame_by_number(const TMD_TOD_Context *ctx,
                                 tmd_u32 frame_number,
                                 tmd_u32 *out_frame_index,
                                 TMD_TOD_Frame *out_frame)
{
    tmd_u32 i;
    TMD_TOD_Frame frame;
    int r;
    if (ctx == TMD_NULL) return TMD_ERR_NULL;
    if (out_frame_index == TMD_NULL && out_frame == TMD_NULL) return TMD_ERR_NULL;
    for (i = 0UL; i < ctx->header.frame_count; ++i) {
        r = tmd_tod_get_frame(ctx, i, &frame);
        if (r != TMD_ERR_OK) return r;
        if (frame.frame_number == frame_number) {
            if (out_frame_index != TMD_NULL) *out_frame_index = i;
            if (out_frame != TMD_NULL) *out_frame = frame;
            return TMD_ERR_OK;
        }
    }
    return TMD_ERR_RANGE;
}
