#include "mf3d_ext_deflate_storeblock.h"

typedef struct mf3d_bitrd_s {
    const mf3d_u8 *src;
    mf3d_u32 len;
    mf3d_u32 pos;
    mf3d_u32 bitbuf;
    int bitcnt;
} mf3d_bitrd;

static int pull_byte(mf3d_bitrd *b)
{
    if (b->pos >= b->len) return -1;
    return (int)b->src[b->pos++];
}

static int need_bits(mf3d_bitrd *b, int n)
{
    int c;
    while (b->bitcnt < n) {
        c = pull_byte(b);
        if (c < 0) return MF3D_E_INFLATE;
        b->bitbuf |= ((mf3d_u32)c) << b->bitcnt;
        b->bitcnt += 8;
    }
    return MF3D_OK;
}

static int get_bits(mf3d_bitrd *b, int n, mf3d_u32 *out)
{
    int r;
    r = need_bits(b, n);
    if (r != MF3D_OK) return r;
    *out = b->bitbuf & ((1UL << n) - 1UL);
    b->bitbuf >>= n;
    b->bitcnt -= n;
    return MF3D_OK;
}

static void align_byte(mf3d_bitrd *b)
{
    b->bitbuf = 0;
    b->bitcnt = 0;
}

int mf3d_inflate_raw_stored_blocks(void *user, const mf3d_u8 *src, mf3d_u32 src_len,
                                   mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len)
{
    mf3d_bitrd br;
    mf3d_u32 outp, final_bit, btype, len, nlen, i;
    int r;
    (void)user;
    if (!src || !dst || !dst_len) return MF3D_E_BADARG;
    br.src = src; br.len = src_len; br.pos = 0; br.bitbuf = 0; br.bitcnt = 0;
    outp = 0;
    final_bit = 0;
    while (!final_bit) {
        r = get_bits(&br, 1, &final_bit); if (r != MF3D_OK) return r;
        r = get_bits(&br, 2, &btype); if (r != MF3D_OK) return r;
        if (btype != 0) { *dst_len = outp; return MF3D_E_UNSUPPORTED; }
        align_byte(&br);
        if (br.pos + 4UL > br.len) return MF3D_E_INFLATE;
        len = (mf3d_u32)br.src[br.pos] | ((mf3d_u32)br.src[br.pos + 1] << 8);
        nlen = (mf3d_u32)br.src[br.pos + 2] | ((mf3d_u32)br.src[br.pos + 3] << 8);
        br.pos += 4;
        if (((len ^ 0xffffUL) & 0xffffUL) != (nlen & 0xffffUL)) return MF3D_E_INFLATE;
        if (outp + len > dst_cap) return MF3D_E_NOSPACE;
        if (br.pos + len > br.len) return MF3D_E_INFLATE;
        for (i = 0; i < len; ++i) dst[outp++] = br.src[br.pos++];
    }
    *dst_len = outp;
    return MF3D_OK;
}
