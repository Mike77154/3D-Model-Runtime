#include "re4c3d/re4_archive.h"
#include "re4c3d/re4_base.h"
#include <string.h>

#define RE4_LFS_CHUNK_SIZE 0x10000UL

const char *re4_lfs_kind_name(re4_lfs_kind kind)
{
    switch (kind) {
    case RE4_LFS_KIND_CHUNK_TABLE: return "chunk-table";
    case RE4_LFS_KIND_STREAM_XMEM: return "stream-xmem";
    default: return "unknown";
    }
}


static re4_bool re4_lfs_looks_like_chunk_table(re4_span src)
{
    re4_u32 m1le;
    re4_u32 m1be;
    re4_endian e;
    re4_u32 n;
    re4_size need;
    re4_u32 i;
    if (!re4_span_ok(src, 0, 20UL)) return RE4_FALSE;
    m1le = re4_read_u32_le(src.data + 0UL);
    m1be = re4_read_u32_be(src.data + 0UL);
    if (m1le == 0x584C4452U) e = RE4_ENDIAN_LITTLE;
    else if (m1be == 0x52444C58U) e = RE4_ENDIAN_BIG;
    else return RE4_FALSE;
    n = re4_read_u32_e(src.data + 16UL, e);
    if (n > 65535U) return RE4_FALSE;
    need = 20UL + ((re4_size)n * 8UL);
    if (!re4_span_ok(src, 0, need)) return RE4_FALSE;
    for (i = 0; i < n; ++i) {
        const re4_u8 *p;
        re4_u32 raw_off;
        re4_size ps;
        re4_size off;
        p = src.data + 20UL + ((re4_size)i * 8UL);
        ps = (re4_size)re4_read_u16_e(p + 0UL, e);
        if (ps == 0UL) ps = RE4_LFS_CHUNK_SIZE;
        raw_off = re4_read_u32_e(p + 4UL, e);
        off = 20UL + (re4_size)(raw_off & ~1U);
        if (!re4_span_ok(src, off, ps)) return RE4_FALSE;
    }
    return RE4_TRUE;
}

static re4_bool re4_lfs_is_rdlx_stream(re4_span src)
{
    re4_u32 raw_size;
    re4_u32 packed_size;
    re4_u32 dummy;
    if (re4_lfs_looks_like_chunk_table(src)) return RE4_FALSE;
    if (!re4_span_ok(src, 0, 32UL)) return RE4_FALSE;
    if (src.data[0] != 'R' || src.data[1] != 'D' || src.data[2] != 'L' || src.data[3] != 'X') return RE4_FALSE;
    dummy = re4_read_u32_le(src.data + 4UL);
    raw_size = re4_read_u32_le(src.data + 8UL);
    packed_size = re4_read_u32_le(src.data + 12UL);
    if (raw_size == 0 || packed_size == 0) return RE4_FALSE;
    if (!re4_span_ok(src, 32UL, (re4_size)packed_size)) return RE4_FALSE;
    if (dummy == 0xFEEEBAAAU && src.size == 32UL + (re4_size)packed_size) return RE4_TRUE;
    if (re4_read_u32_le(src.data + 28UL) == 0U && src.size == 32UL + (re4_size)packed_size) return RE4_TRUE;
    return RE4_FALSE;
}

static re4_err re4_lfs_read_stream(re4_span src, re4_lfs *out)
{
    memset(out, 0, sizeof(*out));
    out->kind = RE4_LFS_KIND_STREAM_XMEM;
    out->magic1 = re4_read_u32_le(src.data + 0UL);
    out->magic2 = re4_read_u32_le(src.data + 4UL);
    out->size_decompressed = re4_read_u32_le(src.data + 8UL);
    out->size_compressed = re4_read_u32_le(src.data + 12UL);
    out->stream_flags = re4_read_u32_le(src.data + 16UL);
    out->stream_offset = 32U;
    out->stream_packed_size = out->size_compressed;
    out->stream_unpacked_size = out->size_decompressed;
    out->endian = RE4_ENDIAN_LITTLE;
    return RE4_OK;
}

re4_err re4_lfs_read(re4_span src, re4_arena *arena, re4_lfs *out)
{
    re4_u32 m1le;
    re4_u32 m1be;
    re4_endian e;
    re4_u32 n;
    re4_size need;
    re4_u32 i;
    re4_lfs_chunk *chunks;

    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0, 20)) return RE4_ERR_RANGE;

    if (re4_lfs_is_rdlx_stream(src)) return re4_lfs_read_stream(src, out);

    m1le = re4_read_u32_le(src.data + 0);
    m1be = re4_read_u32_be(src.data + 0);
    if (m1le == 0x584C4452U) e = RE4_ENDIAN_LITTLE;
    else if (m1be == 0x52444C58U) e = RE4_ENDIAN_BIG;
    else return RE4_ERR_BAD_MAGIC;

    n = re4_read_u32_e(src.data + 16, e);
    if (n > 65535U) return RE4_ERR_BAD_FORMAT;
    need = 20UL + ((re4_size)n * 8UL);
    if (!re4_span_ok(src, 0, need)) return RE4_ERR_RANGE;
    chunks = (re4_lfs_chunk *)re4_arena_push(arena, (re4_size)n * sizeof(re4_lfs_chunk), sizeof(re4_u32));
    if (n != 0 && chunks == 0) return RE4_ERR_NO_SPACE;

    out->kind = RE4_LFS_KIND_CHUNK_TABLE;
    out->magic1 = re4_read_u32_e(src.data + 0, e);
    out->magic2 = re4_read_u32_e(src.data + 4, e);
    out->size_decompressed = re4_read_u32_e(src.data + 8, e);
    out->size_compressed = re4_read_u32_e(src.data + 12, e);
    out->chunk_count = n;
    out->chunks = chunks;
    out->endian = e;

    for (i = 0; i < n; ++i) {
        const re4_u8 *p;
        re4_u32 raw_off;
        p = src.data + 20UL + ((re4_size)i * 8UL);
        chunks[i].packed_size = re4_read_u16_e(p + 0, e);
        chunks[i].unpacked_size = re4_read_u16_e(p + 2, e);
        raw_off = re4_read_u32_e(p + 4, e);
        chunks[i].compressed = (re4_u8)(raw_off & 1U);
        chunks[i].offset = 20U + (raw_off & ~1U);
    }
    return RE4_OK;
}

re4_err re4_lfs_chunk_span(re4_span src, const re4_lfs_chunk *chunk, re4_span *out, re4_size *unpacked_size)
{
    re4_size ps;
    re4_size us;
    if (chunk == 0 || out == 0) return RE4_ERR_NULL;
    ps = chunk->packed_size == 0 ? RE4_LFS_CHUNK_SIZE : (re4_size)chunk->packed_size;
    us = chunk->unpacked_size == 0 ? RE4_LFS_CHUNK_SIZE : (re4_size)chunk->unpacked_size;
    if (!re4_span_ok(src, chunk->offset, ps)) return RE4_ERR_RANGE;
    out->data = src.data + chunk->offset;
    out->size = ps;
    if (unpacked_size != 0) *unpacked_size = us;
    return RE4_OK;
}

re4_err re4_lfs_extract(re4_span src, const re4_lfs *lfs,
                        re4_u8 *dst, re4_size dst_cap, re4_size *written,
                        re4_lfs_codec_fn codec, void *codec_user)
{
    re4_u32 i;
    re4_size pos;
    if (lfs == 0 || dst == 0 || written == 0) return RE4_ERR_NULL;
    pos = 0;

    if (lfs->kind == RE4_LFS_KIND_STREAM_XMEM) {
        re4_size out_len;
        if (!re4_span_ok(src, lfs->stream_offset, lfs->stream_packed_size)) return RE4_ERR_RANGE;
        if ((re4_size)lfs->stream_unpacked_size > dst_cap) return RE4_ERR_NO_SPACE;
        if (lfs->stream_flags != 0U || lfs->stream_packed_size != lfs->stream_unpacked_size) {
            if (codec == 0) return RE4_ERR_NEED_EXTERNAL_CODEC;
            out_len = (re4_size)lfs->stream_unpacked_size;
            if (codec(codec_user, src.data + lfs->stream_offset, lfs->stream_packed_size, dst, &out_len) != RE4_OK) {
                return RE4_ERR_BAD_FORMAT;
            }
            if (out_len != (re4_size)lfs->stream_unpacked_size) return RE4_ERR_BAD_FORMAT;
            *written = out_len;
            return RE4_OK;
        }
        memcpy(dst, src.data + lfs->stream_offset, (size_t)lfs->stream_unpacked_size);
        *written = lfs->stream_unpacked_size;
        return RE4_OK;
    }

    for (i = 0; i < lfs->chunk_count; ++i) {
        const re4_lfs_chunk *c;
        re4_size ps;
        re4_size us;
        c = &lfs->chunks[i];
        ps = c->packed_size == 0 ? RE4_LFS_CHUNK_SIZE : (re4_size)c->packed_size;
        us = c->unpacked_size == 0 ? RE4_LFS_CHUNK_SIZE : (re4_size)c->unpacked_size;
        if (i + 1U == lfs->chunk_count && lfs->size_decompressed != 0) {
            re4_size remaining;
            if ((re4_size)lfs->size_decompressed < pos) return RE4_ERR_BAD_FORMAT;
            remaining = (re4_size)lfs->size_decompressed - pos;
            if (remaining < us) us = remaining;
        }
        if (!re4_span_ok(src, (re4_size)c->offset, ps)) return RE4_ERR_RANGE;
        if (pos > dst_cap || us > dst_cap - pos) return RE4_ERR_NO_SPACE;
        if (c->compressed) {
            re4_size out_len;
            re4_err er;
            if (codec == 0) return RE4_ERR_NEED_EXTERNAL_CODEC;
            out_len = us;
            er = codec(codec_user, src.data + c->offset, ps, dst + pos, &out_len);
            if (er != RE4_OK) return er;
            if (out_len != us) return RE4_ERR_BAD_FORMAT;
        } else {
            if (ps < us) return RE4_ERR_BAD_FORMAT;
            memcpy(dst + pos, src.data + c->offset, (size_t)us);
        }
        pos += us;
    }
    *written = pos;
    return RE4_OK;
}

static void re4_write_u16_e(re4_u8 *p, re4_u16 v, re4_endian e)
{
    if (e == RE4_ENDIAN_BIG) {
        p[0] = (re4_u8)(v >> 8);
        p[1] = (re4_u8)(v & 255U);
    } else {
        p[0] = (re4_u8)(v & 255U);
        p[1] = (re4_u8)(v >> 8);
    }
}

static void re4_write_u32_e(re4_u8 *p, re4_u32 v, re4_endian e)
{
    if (e == RE4_ENDIAN_BIG) {
        p[0] = (re4_u8)(v >> 24);
        p[1] = (re4_u8)(v >> 16);
        p[2] = (re4_u8)(v >> 8);
        p[3] = (re4_u8)(v & 255U);
    } else {
        p[0] = (re4_u8)(v & 255U);
        p[1] = (re4_u8)(v >> 8);
        p[2] = (re4_u8)(v >> 16);
        p[3] = (re4_u8)(v >> 24);
    }
}

static re4_size re4_lfs_align_payload_offset(re4_size off)
{
    if (off < 4UL) return 4UL;
    return ((((off - 4UL) + 15UL) / 16UL) * 16UL) + 4UL;
}

re4_err re4_lfs_store_size(re4_size raw_size, re4_size *out_size, re4_u32 *out_chunks)
{
    re4_u32 n;
    re4_size pos;
    re4_size remaining;
    re4_size chunk;
    re4_u32 i;
    if (out_size == 0) return RE4_ERR_NULL;
    if (raw_size > (re4_size)0xFFFFFFFFUL) return RE4_ERR_RANGE;
    n = (re4_u32)((raw_size + (RE4_LFS_CHUNK_SIZE - 1UL)) / RE4_LFS_CHUNK_SIZE);
    if (raw_size == 0UL) n = 0U;
    if (n > 65535U) return RE4_ERR_RANGE;
    pos = 20UL + ((re4_size)n * 8UL);
    remaining = raw_size;
    for (i = 0; i < n; ++i) {
        pos = re4_lfs_align_payload_offset(pos);
        chunk = remaining > RE4_LFS_CHUNK_SIZE ? RE4_LFS_CHUNK_SIZE : remaining;
        if (chunk > ((re4_size)0xFFFFFFFFUL - pos)) return RE4_ERR_RANGE;
        pos += chunk;
        remaining -= chunk;
    }
    *out_size = pos;
    if (out_chunks != 0) *out_chunks = n;
    return RE4_OK;
}

re4_err re4_lfs_write_store(re4_span raw, re4_endian endian, re4_u8 *dst, re4_size dst_cap, re4_size *written)
{
    re4_size need;
    re4_u32 n;
    re4_u32 i;
    re4_size table_pos;
    re4_size data_pos;
    re4_size remaining;
    re4_err er;
    if (dst == 0 || written == 0) return RE4_ERR_NULL;
    er = re4_lfs_store_size(raw.size, &need, &n);
    if (er != RE4_OK) return er;
    if (dst_cap < need) return RE4_ERR_NO_SPACE;
    memset(dst, 0, (size_t)need);
    re4_write_u32_e(dst + 0UL, 0x584C4452U, endian);
    re4_write_u32_e(dst + 4UL, 0xFEEEBAAAU, endian);
    re4_write_u32_e(dst + 8UL, (re4_u32)raw.size, endian);
    re4_write_u32_e(dst + 12UL, (re4_u32)raw.size, endian);
    re4_write_u32_e(dst + 16UL, n, endian);
    table_pos = 20UL;
    data_pos = 20UL + ((re4_size)n * 8UL);
    remaining = raw.size;
    for (i = 0; i < n; ++i) {
        re4_size chunk;
        re4_u16 packed_field;
        re4_u16 unpacked_field;
        re4_u32 relative_off;
        data_pos = re4_lfs_align_payload_offset(data_pos);
        chunk = remaining > RE4_LFS_CHUNK_SIZE ? RE4_LFS_CHUNK_SIZE : remaining;
        packed_field = chunk == RE4_LFS_CHUNK_SIZE ? 0U : (re4_u16)chunk;
        unpacked_field = packed_field;
        relative_off = (re4_u32)(data_pos - 20UL);
        re4_write_u16_e(dst + table_pos + ((re4_size)i * 8UL) + 0UL, packed_field, endian);
        re4_write_u16_e(dst + table_pos + ((re4_size)i * 8UL) + 2UL, unpacked_field, endian);
        re4_write_u32_e(dst + table_pos + ((re4_size)i * 8UL) + 4UL, relative_off, endian);
        if (chunk != 0UL) memcpy(dst + data_pos, raw.data + (raw.size - remaining), (size_t)chunk);
        data_pos += chunk;
        remaining -= chunk;
    }
    *written = need;
    return RE4_OK;
}
