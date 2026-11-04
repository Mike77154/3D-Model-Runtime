#include "re4c3d/re4_archive.h"
#include "re4c3d/re4_base.h"

re4_err re4_archive_entry_span(re4_span src, const re4_archive_entry *entry, re4_span *out)
{
    re4_size off;
    re4_size len;
    if (entry == 0 || out == 0) return RE4_ERR_NULL;
    off = (re4_size)entry->offset;
    len = (re4_size)entry->packed_size;
    return re4_span_sub(src, off, len, out);
}
