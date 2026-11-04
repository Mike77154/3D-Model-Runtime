#include "re4c3d/re4_archive.h"
#include "re4c3d/re4_base.h"
#include <string.h>

static re4_bool re4_has_gca_magic(re4_span src)
{
    if (src.size < 4UL) return RE4_FALSE;
    if (src.data[0] == 'G' && src.data[1] == 'C' && src.data[2] == 'A') return RE4_TRUE;
    return RE4_FALSE;
}

static re4_err re4_try_offset_size_table(re4_span src, re4_endian e, re4_archive_kind kind,
                                         re4_arena *arena, re4_archive *out)
{
    re4_u32 count;
    re4_u32 i;
    re4_archive_entry *entries;
    re4_size table_size;
    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0, 8)) return RE4_ERR_RANGE;
    count = re4_read_u32_e(src.data + 0, e);
    if (count == 0 || count > 65535U) return RE4_ERR_BAD_FORMAT;
    table_size = 4UL + ((re4_size)count * 8UL);
    if (!re4_span_ok(src, 0, table_size)) return RE4_ERR_RANGE;
    entries = (re4_archive_entry *)re4_arena_push(arena, (re4_size)count * sizeof(re4_archive_entry), sizeof(re4_u32));
    if (entries == 0) return RE4_ERR_NO_SPACE;
    for (i = 0; i < count; ++i) {
        const re4_u8 *p;
        p = src.data + 4UL + ((re4_size)i * 8UL);
        entries[i].offset = re4_read_u32_e(p + 0, e);
        entries[i].packed_size = re4_read_u32_e(p + 4, e);
        entries[i].unpacked_size = entries[i].packed_size;
        entries[i].flags = 0;
        entries[i].name.ptr = 0;
        entries[i].name.len = 0;
        if (entries[i].offset < table_size) return RE4_ERR_BAD_FORMAT;
        if (!re4_span_ok(src, (re4_size)entries[i].offset, (re4_size)entries[i].packed_size)) return RE4_ERR_RANGE;
    }
    out->kind = kind;
    out->entry_count = count;
    out->entries = entries;
    out->endian = e;
    out->flags = 1U;
    return RE4_OK;
}

static re4_err re4_try_offset_only_table(re4_span src, re4_endian e, re4_archive_kind kind,
                                         re4_arena *arena, re4_archive *out)
{
    re4_u32 count;
    re4_u32 i;
    re4_archive_entry *entries;
    re4_size table_size;
    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0, 8)) return RE4_ERR_RANGE;
    count = re4_read_u32_e(src.data + 0, e);
    if (count == 0 || count > 65535U) return RE4_ERR_BAD_FORMAT;
    table_size = 4UL + ((re4_size)count * 4UL);
    if (!re4_span_ok(src, 0, table_size)) return RE4_ERR_RANGE;
    entries = (re4_archive_entry *)re4_arena_push(arena, (re4_size)count * sizeof(re4_archive_entry), sizeof(re4_u32));
    if (entries == 0) return RE4_ERR_NO_SPACE;
    for (i = 0; i < count; ++i) {
        re4_u32 off;
        re4_u32 next;
        off = re4_read_u32_e(src.data + 4UL + ((re4_size)i * 4UL), e);
        if (i + 1U < count) next = re4_read_u32_e(src.data + 4UL + ((re4_size)(i + 1U) * 4UL), e);
        else {
            if (src.size > 0xFFFFFFFFUL) return RE4_ERR_UNSUPPORTED;
            next = (re4_u32)src.size;
        }
        if ((re4_size)off < table_size || next < off) return RE4_ERR_BAD_FORMAT;
        if (!re4_span_ok(src, (re4_size)off, (re4_size)(next - off))) return RE4_ERR_RANGE;
        entries[i].offset = off;
        entries[i].packed_size = next - off;
        entries[i].unpacked_size = entries[i].packed_size;
        entries[i].flags = RE4_ARCHIVE_ENTRY_F_INFERRED_OFFSET;
        entries[i].name.ptr = 0;
        entries[i].name.len = 0;
    }
    out->kind = kind;
    out->entry_count = count;
    out->entries = entries;
    out->endian = e;
    out->flags = 2U;
    return RE4_OK;
}

static re4_err re4_dat_or_udas_read_guess_kind(re4_span src, re4_endian endian, re4_archive_kind kind,
                                               re4_arena *arena, re4_archive *out)
{
    re4_size mark;
    re4_err er;
    if (arena == 0) return RE4_ERR_NULL;
    if (re4_has_gca_magic(src)) return re4_gca_read(src, arena, out);
    mark = arena->pos;
    er = re4_try_offset_size_table(src, endian, kind, arena, out);
    if (er == RE4_OK) return RE4_OK;
    arena->pos = mark;
    er = re4_try_offset_only_table(src, endian, kind, arena, out);
    return er;
}

re4_err re4_dat_read_guess(re4_span src, re4_endian endian, re4_arena *arena, re4_archive *out)
{
    return re4_dat_or_udas_read_guess_kind(src, endian, RE4_ARCHIVE_DAT_GUESS, arena, out);
}

re4_err re4_udas_read_guess(re4_span src, re4_endian endian, re4_arena *arena, re4_archive *out)
{
    return re4_dat_or_udas_read_guess_kind(src, endian, RE4_ARCHIVE_UDAS_GUESS, arena, out);
}
