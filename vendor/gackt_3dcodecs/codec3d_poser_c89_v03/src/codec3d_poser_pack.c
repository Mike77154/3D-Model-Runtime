#include "codec3d_poser_pack.h"
#include <string.h>

static int p3d_is_gzip(const unsigned char *p, unsigned long n)
{
    if (p == 0 || n < 2UL) return 0;
    return p[0] == 0x1fu && p[1] == 0x8bu;
}

static int p3d_is_zlib(const unsigned char *p, unsigned long n)
{
    unsigned int cmf, flg, v;
    if (p == 0 || n < 2UL) return 0;
    cmf = (unsigned int)p[0]; flg = (unsigned int)p[1]; v = cmf * 256u + flg;
    if ((cmf & 0x0Fu) != 8u) return 0;
    if ((cmf >> 4) > 7u) return 0;
    return (v % 31u) == 0u;
}

#ifdef P3D_USE_ZLIB
#include <zlib.h>

typedef struct P3D_ZArena_s {
    unsigned char mem[P3D_ZLIB_ARENA_SIZE];
    unsigned long used;
} P3D_ZArena;

static P3D_ZArena g_p3d_zarena;

static voidpf p3d_zalloc(voidpf opaque, uInt items, uInt size)
{
    P3D_ZArena *a;
    unsigned long need;
    unsigned long pos;
    unsigned long align;
    a = (P3D_ZArena *)opaque;
    if (a == 0) return Z_NULL;
    need = (unsigned long)items * (unsigned long)size;
    align = (unsigned long)sizeof(void *) - 1UL;
    pos = (a->used + align) & ~align;
    if (pos + need > (unsigned long)P3D_ZLIB_ARENA_SIZE) return Z_NULL;
    a->used = pos + need;
    return (voidpf)(a->mem + pos);
}

static void p3d_zfree(voidpf opaque, voidpf address)
{
    (void)opaque;
    (void)address;
}
#endif

int p3d_decode_packed_memory(P3D_Context *ctx,
                             const unsigned char *packed,
                             unsigned long packed_size,
                             unsigned char *scratch_out,
                             unsigned long scratch_out_size,
                             unsigned long *decoded_size)
{
    int gz;
    int zl;
#ifdef P3D_USE_ZLIB
    z_stream zs;
    int zr;
#endif
    if (ctx == 0 || packed == 0 || scratch_out == 0 || decoded_size == 0) return P3D_ERR_NULL;
    *decoded_size = 0UL;
    gz = p3d_is_gzip(packed, packed_size);
    zl = p3d_is_zlib(packed, packed_size);
    if (gz) ctx->flags |= P3D_FLAG_PACKED_GZIP;
    if (zl) ctx->flags |= P3D_FLAG_PACKED_ZLIB;
    if (!gz && !zl) return p3d_decode_memory(ctx, packed, packed_size);
#ifndef P3D_USE_ZLIB
    (void)scratch_out_size;
    ctx->flags |= P3D_FLAG_PACKED_UNSUPPORTED;
    return P3D_ERR_UNSUPPORTED;
#else
    memset(&zs, 0, sizeof(zs));
    memset(&g_p3d_zarena, 0, sizeof(g_p3d_zarena));
    zs.zalloc = p3d_zalloc;
    zs.zfree = p3d_zfree;
    zs.opaque = (voidpf)&g_p3d_zarena;
    zs.next_in = (Bytef *)packed;
    zs.avail_in = (uInt)packed_size;
    zs.next_out = scratch_out;
    zs.avail_out = (uInt)scratch_out_size;
    zr = inflateInit2(&zs, 47);
    if (zr != Z_OK) { ctx->flags |= P3D_FLAG_PACKED_UNSUPPORTED; return P3D_ERR_UNSUPPORTED; }
    zr = inflate(&zs, Z_FINISH);
    *decoded_size = (unsigned long)zs.total_out;
    inflateEnd(&zs);
    if (zr != Z_STREAM_END) { ctx->flags |= P3D_FLAG_PACKED_UNSUPPORTED; return P3D_ERR_OUTPUT_TOO_SMALL; }
    return p3d_decode_memory(ctx, scratch_out, *decoded_size);
#endif
}
