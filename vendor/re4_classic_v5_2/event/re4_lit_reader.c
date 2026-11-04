#include "re4c3d/re4_event.h"
#include <string.h>

re4_err re4_lit_probe_read(re4_span src, re4_lit_probe *out)
{
    if (out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    out->file_size = (re4_u32)src.size;
    out->possible_record_count = (re4_u32)(src.size / 32UL);
    return RE4_OK;
}
