#include "re4c3d/re4_texture.h"
#include "re4c3d/re4_base.h"
#include <string.h>

re4_err re4_eff_texture_refs_scan(re4_span src, re4_arena *arena, re4_tplcue *out)
{
    re4_span tail;
    re4_size off;
    if (out == 0 || arena == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    off = 0;
    while (off + 8UL <= src.size) {
        re4_u32 a;
        re4_u32 b;
        a = re4_read_u32_le(src.data + off);
        b = re4_read_u32_le(src.data + off + 4UL);
        if ((a < 65536U && b >= 65536U) || (b < 65536U && a >= 65536U)) {
            tail.data = src.data + off;
            tail.size = src.size - off;
            return re4_tplcue_read_le(tail, arena, out);
        }
        off += 4UL;
    }
    return RE4_ERR_BAD_FORMAT;
}
