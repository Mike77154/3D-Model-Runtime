/* RE4 GCA3 / GCAX archive index, with footer metadata support.
   Copyright (c) 2026; original clean-room implementation.
   GCAX samples st1..st5 and ss have:
     bytes [0..15] : 'GCAX', reserved u32, u64 GCA3 footer offset
     bytes [16..footer_offset) : packed file payloads concatenated in record order
     bytes [footer_offset..EOF) : GCA3 header, 32-byte file records, names
   IMPORTANT: the legacy interpretation (payload after GCA3 footer) was wrong.
*/
#include "re4c3d/re4_archive.h"
#include "re4c3d/re4_base.h"
#include <string.h>

static re4_bool re4_is_magic4(re4_span src, re4_size off,
                              char a, char b, char c, char d)
{
    if (!re4_span_ok(src, off, 4UL)) return RE4_FALSE;
    return src.data[off] == (re4_u8)a &&
           src.data[off + 1UL] == (re4_u8)b &&
           src.data[off + 2UL] == (re4_u8)c &&
           src.data[off + 3UL] == (re4_u8)d ? RE4_TRUE : RE4_FALSE;
}

/* `meta` starts with GCA3. In GCAX mode it is the FOOTER only, and
   file spans are absolute offsets into the ORIGINAL archive. Names
   reference this metadata buffer: keep it alive while using the index.
   In raw GCA3 mode the old header-first sequential layout is retained. */
static re4_err re4_gca_parse_meta(re4_span meta, re4_u32 archive_size,
                                 re4_u32 footer_offset, re4_bool footer_mode,
                                 re4_arena *arena, re4_archive *out)
{
    re4_u32 count, i, packed, unpacked, flags;
    re4_size pos, data_pos, data_limit, table_size, rec, mark, nlen;
    re4_archive_entry *entries;
    re4_err er;
    if (arena == 0 || out == 0 || meta.data == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_is_magic4(meta, 0UL, 'G', 'C', 'A', '3')) return RE4_ERR_BAD_MAGIC;
    if (!re4_span_ok(meta, 0UL, 32UL)) return RE4_ERR_RANGE;
    if (re4_read_u32_le(meta.data + 28UL) != 0U) return RE4_ERR_UNSUPPORTED;
    count = re4_read_u32_le(meta.data + 24UL);
    if (count == 0U || count > 65535U) return RE4_ERR_BAD_FORMAT;
    table_size = (re4_size)count * 32UL;
    if (!re4_span_ok(meta, 32UL, table_size)) return RE4_ERR_RANGE;
    mark = arena->pos;
    entries = (re4_archive_entry *)re4_arena_push(arena,
                   (re4_size)count * sizeof(re4_archive_entry), sizeof(void *));
    if (entries == 0) return RE4_ERR_NO_SPACE;
    pos = 32UL + table_size;
    for (i = 0U; i < count; ++i) {
        if (!re4_span_ok(meta, pos, 2UL)) { er = RE4_ERR_RANGE; goto bad; }
        nlen = re4_read_u16_le(meta.data + pos);
        pos += 2UL;
        if (!re4_span_ok(meta, pos, nlen)) { er = RE4_ERR_RANGE; goto bad; }
        entries[i].name.ptr = (const char *)(meta.data + pos);
        entries[i].name.len = nlen;
        entries[i].flags = RE4_ARCHIVE_ENTRY_F_NAME_REF |
                           RE4_ARCHIVE_ENTRY_F_INFERRED_OFFSET;
        pos += nlen;
    }
    if (footer_mode) {
        if (pos != meta.size || footer_offset < 16U ||
            (re4_size)footer_offset > (re4_size)archive_size) {
            er = RE4_ERR_BAD_FORMAT; goto bad;
        }
        data_pos = 16UL;
        data_limit = (re4_size)footer_offset;
    } else {
        data_pos = pos;
        data_limit = (re4_size)archive_size;
    }
    for (i = 0U; i < count; ++i) {
        rec = 32UL + (re4_size)i * 32UL;
        flags = re4_read_u32_le(meta.data + rec + 4UL);
        if (re4_read_u32_le(meta.data + rec + 20UL) != 0U ||
            re4_read_u32_le(meta.data + rec + 28UL) != 0U) {
            er = RE4_ERR_UNSUPPORTED; goto bad;
        }
        unpacked = re4_read_u32_le(meta.data + rec + 16UL);
        packed = re4_read_u32_le(meta.data + rec + 24UL);
        if (packed == 0U && unpacked != 0U) packed = unpacked;
        if (data_pos > data_limit || (re4_size)packed > data_limit - data_pos) {
            er = RE4_ERR_RANGE; goto bad;
        }
        entries[i].offset = (re4_u32)data_pos;
        entries[i].packed_size = packed;
        entries[i].unpacked_size = unpacked;
        entries[i].flags |= (flags << 8);
        if (packed != unpacked)
            entries[i].flags |= RE4_ARCHIVE_ENTRY_F_COMPRESSED_HINT;
        data_pos += (re4_size)packed;
    }
    /* GCAX GCA3 is a footer index over the entire contiguous data section.
       Requiring an exact fit catches truncated/corrupted indices. */
    if (footer_mode && data_pos != data_limit) {
        er = RE4_ERR_BAD_FORMAT; goto bad;
    }
    out->kind = RE4_ARCHIVE_GCA;
    out->entry_count = count;
    out->entries = entries;
    out->endian = RE4_ENDIAN_LITTLE;
    out->flags = footer_mode ? 0x23U : 3U;
    return RE4_OK;
bad:
    arena->pos = mark;
    memset(out, 0, sizeof(*out));
    return er;
}

re4_err re4_gca_probe(re4_span src, re4_archive *out)
{
    if (out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (re4_is_magic4(src, 0UL, 'G', 'C', 'A', 'X') ||
        re4_is_magic4(src, 0UL, 'G', 'C', 'A', '3')) {
        out->kind = RE4_ARCHIVE_GCA;
        out->endian = RE4_ENDIAN_LITTLE;
        return RE4_OK;
    }
    return RE4_ERR_BAD_MAGIC;
}

/* The caller has already read the GCA3 footer using standard file I/O,
   and supplies the original GCAX file size and absolute footer offset.
   Suitable for Win32 without loading large archives into RAM. */
re4_err re4_gca_read_footer(re4_span footer, re4_u32 archive_size,
                            re4_u32 footer_offset, re4_arena *arena,
                            re4_archive *out)
{
    if (footer_offset < 16U || footer_offset > archive_size ||
        footer.size != (re4_size)(archive_size - footer_offset))
        return RE4_ERR_RANGE;
    return re4_gca_parse_meta(footer, archive_size, footer_offset,
                              RE4_TRUE, arena, out);
}

re4_err re4_gca_read(re4_span src, re4_arena *arena, re4_archive *out)
{
    re4_u32 meta_off;
    re4_span footer;
    re4_err er;
    if (arena == 0 || out == 0 || src.data == 0) return RE4_ERR_NULL;
    if (src.size > 0xFFFFFFFFUL) return RE4_ERR_UNSUPPORTED;
    if (re4_is_magic4(src, 0UL, 'G', 'C', 'A', 'X')) {
        if (!re4_span_ok(src, 8UL, 8UL)) return RE4_ERR_RANGE;
        if (re4_read_u32_le(src.data + 12UL) != 0U) return RE4_ERR_UNSUPPORTED;
        meta_off = re4_read_u32_le(src.data + 8UL);
        if (meta_off < 16U || (re4_size)meta_off > src.size) return RE4_ERR_RANGE;
        er = re4_span_sub(src, (re4_size)meta_off,
                          src.size - (re4_size)meta_off, &footer);
        if (er != RE4_OK) return er;
        return re4_gca_read_footer(footer, (re4_u32)src.size,
                                    meta_off, arena, out);
    }
    if (re4_is_magic4(src, 0UL, 'G', 'C', 'A', '3'))
        return re4_gca_parse_meta(src, (re4_u32)src.size, 0U,
                                  RE4_FALSE, arena, out);
    return RE4_ERR_BAD_MAGIC;
}
