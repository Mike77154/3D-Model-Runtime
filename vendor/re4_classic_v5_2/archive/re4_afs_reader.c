#include "re4c3d/re4_archive.h"
#include "re4c3d/re4_base.h"
#include <string.h>

re4_err re4_afs_read(re4_span src, re4_arena *arena, re4_archive *out)
{
    re4_u32 n;
    re4_u32 i;
    re4_archive_entry *entries;
    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!re4_span_ok(src, 0, 8)) return RE4_ERR_RANGE;
    if (src.data[0] != 'A' || src.data[1] != 'F' || src.data[2] != 'S') return RE4_ERR_BAD_MAGIC;
    n = re4_read_u32_le(src.data + 4);
    if (n > 65535U) return RE4_ERR_BAD_FORMAT;
    if (!re4_span_ok(src, 8, (re4_size)n * 8UL)) return RE4_ERR_RANGE;
    entries = (re4_archive_entry *)re4_arena_push(arena, (re4_size)n * sizeof(re4_archive_entry), sizeof(re4_u32));
    if (n != 0 && entries == 0) return RE4_ERR_NO_SPACE;
    for (i = 0; i < n; ++i) {
        const re4_u8 *p;
        p = src.data + 8UL + ((re4_size)i * 8UL);
        entries[i].offset = re4_read_u32_le(p + 0);
        entries[i].packed_size = re4_read_u32_le(p + 4);
        entries[i].unpacked_size = entries[i].packed_size;
        entries[i].flags = 0;
        entries[i].name.ptr = 0;
        entries[i].name.len = 0;
        if (!re4_span_ok(src, (re4_size)entries[i].offset, (re4_size)entries[i].packed_size)) return RE4_ERR_RANGE;
    }
    out->kind = RE4_ARCHIVE_AFS;
    out->entry_count = n;
    out->entries = entries;
    out->endian = RE4_ENDIAN_LITTLE;
    return RE4_OK;
}
