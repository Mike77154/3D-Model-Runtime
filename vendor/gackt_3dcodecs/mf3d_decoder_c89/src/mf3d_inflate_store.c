#include "mf3d.h"

/* This file intentionally avoids heap use. ZIP method 0 is handled internally.
   For ZIP method 8, plug a raw-deflate callback through mf3d_options.
   This lets embedded/retro builds bind their own ROM/static inflater without
   pulling malloc-heavy zlib/miniz defaults into the decoder core. */
int mf3d_noheap_inflate_stub(void *user, const mf3d_u8 *src, mf3d_u32 src_len,
                             mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len)
{
    (void)user; (void)src; (void)src_len; (void)dst; (void)dst_cap;
    if (dst_len) *dst_len = 0;
    return MF3D_E_UNSUPPORTED;
}
