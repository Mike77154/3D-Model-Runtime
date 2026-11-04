#include "re4c3d/re4_animation.h"
#include "re4c3d/re4_base.h"
#include <string.h>

re4_err re4_seq_probe_read(re4_span src, re4_endian endian, re4_seq *out)
{
    if (out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    out->file_size = (re4_u32)src.size;
    if (src.size >= 4UL) out->first_word = re4_read_u32_e(src.data, endian);
    out->possible_command_count = (re4_u32)(src.size / 16UL);
    return RE4_OK;
}
