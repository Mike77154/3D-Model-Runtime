#include "re4c3d/re4_archive.h"
#include "re4c3d/re4_base.h"
#include <string.h>

/* DAT/MAP on RE4: count at +0, offset array at +0x10 (E3 BE: +4),
   and a parallel four-byte extension array directly after the offsets.
   File offsets are relative to the beginning of the DAT portion. */
re4_err re4_dat_read_indexed(re4_span src, re4_endian endian, re4_bool e3,
                             re4_arena *arena, re4_archive *out)
{
    re4_u32 count, i, off, next;
    re4_size table_at, table_end, offset_at, name_at;
    re4_archive_entry *entries;
    re4_size mark;
    if (arena == 0 || out == 0 || src.data == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0UL, 4UL)) return RE4_ERR_RANGE;
    if (src.size > 0xFFFFFFFFUL) return RE4_ERR_UNSUPPORTED;
    count = re4_read_u32_e(src.data, endian);
    if (count == 0U || count > 65535U) return RE4_ERR_BAD_FORMAT;
    table_at = e3 ? 4UL : 16UL;
    if ((re4_size)count > (0xFFFFFFFFUL / 8UL)) return RE4_ERR_RANGE;
    table_end = table_at + (re4_size)count * 8UL;
    if (!re4_span_ok(src, table_at, (re4_size)count * 8UL)) return RE4_ERR_RANGE;
    mark = arena->pos;
    entries = (re4_archive_entry *)re4_arena_push(arena, (re4_size)count * sizeof(re4_archive_entry), sizeof(void *));
    if (entries == 0) return RE4_ERR_NO_SPACE;
    for (i = 0U; i < count; ++i) {
        offset_at = table_at + (re4_size)i * 4UL;
        name_at = table_at + (re4_size)count * 4UL + (re4_size)i * 4UL;
        off = re4_read_u32_e(src.data + offset_at, endian);
        next = (i + 1U < count) ? re4_read_u32_e(src.data + offset_at + 4UL, endian) : (re4_u32)src.size;
        if (src.size > 0xFFFFFFFFUL || off < (re4_u32)table_end || next < off || !re4_span_ok(src, off, (re4_size)(next-off))) {
            arena->pos = mark;
            return RE4_ERR_BAD_FORMAT;
        }
        entries[i].offset = off;
        entries[i].packed_size = next-off;
        entries[i].unpacked_size = next-off;
        entries[i].flags = RE4_ARCHIVE_ENTRY_F_INFERRED_OFFSET | RE4_ARCHIVE_ENTRY_F_NAME_REF;
        entries[i].name.ptr = (const char *)(src.data + name_at);
        entries[i].name.len = 4UL;
    }
    out->kind = RE4_ARCHIVE_DAT_GUESS;
    out->entry_count = count;
    out->entries = entries;
    out->endian = endian;
    out->flags = e3 ? 0x110U : 0x100U;
    return RE4_OK;
}

/* UDAS has up to two 32-byte block descriptors beginning at +0x20.
   type 0 is embedded DAT; 4 is sound; 0xFFFFFFFE is an empty/end block.
   Descriptor values are relative to the whole UDAS and bounded strictly. */
re4_err re4_udas_read_parts(re4_span src, re4_endian endian, re4_udas_parts *out)
{
    re4_u32 i, type, length, offset;
    re4_size p;
    if (out == 0 || src.data == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0x20UL, 0x40UL)) return RE4_ERR_RANGE;
    out->sound_type = 0xFFFFFFFFU;
    for (i = 0U; i < 2U; ++i) {
        p = 0x20UL + (re4_size)i * 32UL;
        type = re4_read_u32_e(src.data + p, endian);
        if (type == 0xFFFFFFFFU) break;
        length = re4_read_u32_e(src.data + p + 4UL, endian);
        offset = re4_read_u32_e(src.data + p + 12UL, endian);
        if (offset < 0x60UL || !re4_span_ok(src, (re4_size)offset, (re4_size)length)) return RE4_ERR_BAD_FORMAT;
        if (type == 0U) {
            if (out->has_dat) return RE4_ERR_BAD_FORMAT;
            out->has_dat = RE4_TRUE;
            out->dat_offset = offset;
            out->dat_size = length;
        } else {
            if (out->has_sound) return RE4_ERR_BAD_FORMAT;
            out->has_sound = RE4_TRUE;
            out->sound_type = type;
            out->sound_offset = offset;
            out->sound_size = length;
        }
    }
    return out->has_dat ? RE4_OK : RE4_ERR_BAD_FORMAT;
}

static re4_bool re4_looks_yz2(re4_span data)
{
    re4_size i, tabs, lines;
    re4_u8 c;
    if (data.size < 33UL) return RE4_FALSE;
    tabs = 0UL;
    lines = 0UL;
    for (i = 0UL; i < 32UL; ++i) {
        c = data.data[i];
        if (c == 0U) return (tabs == 1UL && lines == 1UL) ? RE4_TRUE : RE4_FALSE;
        if (c == 9U && tabs == 0UL && lines == 0UL) { ++tabs; continue; }
        if (c == 10U && tabs == 1UL && lines == 0UL) { ++lines; continue; }
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            if (lines == 0UL) continue;
        }
        return RE4_FALSE;
    }
    return (tabs == 1UL && lines == 1UL) ? RE4_TRUE : RE4_FALSE;
}

/* Return embedded DAT entries in UDAS-relative coordinates, allowing
   re4_archive_entry_span() to operate on the original UDAS byte span. */
re4_err re4_udas_read_indexed(re4_span src, re4_endian endian, re4_bool e3,
                              re4_arena *arena, re4_archive *out, re4_udas_parts *parts)
{
    re4_udas_parts local_parts;
    re4_span embedded;
    re4_err err;
    re4_u32 i;
    re4_size mark;
    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    err = re4_udas_read_parts(src, endian, &local_parts);
    if (err != RE4_OK) return err;
    err = re4_span_sub(src, (re4_size)local_parts.dat_offset, (re4_size)local_parts.dat_size, &embedded);
    if (err != RE4_OK) return err;
    /* YZ2 ASCII prefix: compressed length TAB original length LF. */
    if (re4_looks_yz2(embedded)) return RE4_ERR_NEED_EXTERNAL_CODEC;
    mark = arena->pos;
    err = re4_dat_read_indexed(embedded, endian, e3, arena, out);
    if (err != RE4_OK) return err;
    for (i = 0U; i < out->entry_count; ++i) {
        if (out->entries[i].offset > RE4_U32_MAX - local_parts.dat_offset) {
            arena->pos = mark;
            memset(out, 0, sizeof(*out));
            return RE4_ERR_RANGE;
        }
        out->entries[i].offset += local_parts.dat_offset;
    }
    out->kind = RE4_ARCHIVE_UDAS_GUESS;
    out->flags |= 0x200U;
    if (parts != 0) *parts = local_parts;
    return RE4_OK;
}
