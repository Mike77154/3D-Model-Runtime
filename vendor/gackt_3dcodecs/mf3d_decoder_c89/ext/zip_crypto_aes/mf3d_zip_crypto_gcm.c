#include "mf3d_zip_crypto_gcm.h"

static void xor16(mf3d_u8 a[16], const mf3d_u8 b[16])
{
    int i;
    for (i = 0; i < 16; ++i) a[i] = (mf3d_u8)(a[i] ^ b[i]);
}

static void rshift1(mf3d_u8 v[16])
{
    int i;
    mf3d_u8 carry, ncarry;
    carry = 0;
    for (i = 0; i < 16; ++i) {
        ncarry = (mf3d_u8)(v[i] & 1U);
        v[i] = (mf3d_u8)((v[i] >> 1) | (carry ? 0x80U : 0U));
        carry = ncarry;
    }
}

static int bit_msb(const mf3d_u8 x[16], int bit)
{
    return (x[bit >> 3] >> (7 - (bit & 7))) & 1;
}

static void gcm_mul(const mf3d_u8 x[16], const mf3d_u8 y[16], mf3d_u8 out[16])
{
    mf3d_u8 z[16], v[16];
    int i, j, lsb;
    for (i = 0; i < 16; ++i) { z[i] = 0; v[i] = y[i]; }
    for (i = 0; i < 128; ++i) {
        if (bit_msb(x, i)) xor16(z, v);
        lsb = v[15] & 1U;
        rshift1(v);
        if (lsb) v[0] = (mf3d_u8)(v[0] ^ 0xe1U);
    }
    for (j = 0; j < 16; ++j) out[j] = z[j];
}

static void put_be64_from_u32bits(mf3d_u8 out[8], mf3d_u32 byte_len)
{
    mf3d_u32 hi, lo;
    hi = (byte_len >> 29) & 7UL;
    lo = (byte_len << 3) & 0xffffffffUL;
    out[0] = 0; out[1] = 0; out[2] = 0; out[3] = (mf3d_u8)hi;
    out[4] = (mf3d_u8)(lo >> 24); out[5] = (mf3d_u8)(lo >> 16);
    out[6] = (mf3d_u8)(lo >> 8); out[7] = (mf3d_u8)lo;
}

static void ghash_update(mf3d_u8 y[16], const mf3d_u8 h[16], const mf3d_u8 *data, mf3d_u32 len)
{
    mf3d_u8 block[16], tmp[16];
    mf3d_u32 i, take;
    while (len > 0) {
        for (i = 0; i < 16; ++i) block[i] = 0;
        take = len < 16UL ? len : 16UL;
        for (i = 0; i < take; ++i) block[i] = data[i];
        xor16(y, block);
        gcm_mul(y, h, tmp);
        for (i = 0; i < 16; ++i) y[i] = tmp[i];
        data += take;
        len -= take;
    }
}

static void ghash_all(const mf3d_u8 h[16], const mf3d_u8 *aad, mf3d_u32 aad_len,
                      const mf3d_u8 *c, mf3d_u32 c_len, mf3d_u8 out[16])
{
    mf3d_u8 y[16], lenblk[16], tmp[16];
    int i;
    for (i = 0; i < 16; ++i) y[i] = 0;
    if (aad_len) ghash_update(y, h, aad, aad_len);
    if (c_len) ghash_update(y, h, c, c_len);
    put_be64_from_u32bits(lenblk, aad_len);
    put_be64_from_u32bits(lenblk + 8, c_len);
    xor16(y, lenblk);
    gcm_mul(y, h, tmp);
    for (i = 0; i < 16; ++i) out[i] = tmp[i];
}

static void inc32(mf3d_u8 ctr[16])
{
    int i;
    for (i = 15; i >= 12; --i) { ctr[i] = (mf3d_u8)(ctr[i] + 1U); if (ctr[i] != 0) break; }
}

int mf3d_zcg_init(mf3d_zcg_ctx *ctx, const mf3d_u8 *key, mf3d_u32 key_len,
                  const mf3d_u8 *iv, mf3d_u32 iv_len,
                  const mf3d_u8 *aad, mf3d_u32 aad_len)
{
    mf3d_u8 zero[16], s[16];
    int r, i;
    (void)aad; (void)aad_len;
    if (!ctx || !key || !iv) return MF3D_E_BADARG;
    r = mf3d_zca_aes_set_key(&ctx->aes, key, key_len);
    if (r != MF3D_OK) return r;
    for (i = 0; i < 16; ++i) zero[i] = 0;
    mf3d_zca_aes_encrypt_block(&ctx->aes, zero, ctx->h);
    if (iv_len == 12UL) {
        for (i = 0; i < 12; ++i) ctx->j0[i] = iv[i];
        ctx->j0[12] = 0; ctx->j0[13] = 0; ctx->j0[14] = 0; ctx->j0[15] = 1;
    } else {
        ghash_all(ctx->h, 0, 0, iv, iv_len, s);
        for (i = 0; i < 16; ++i) ctx->j0[i] = s[i];
    }
    return MF3D_OK;
}

void mf3d_zcg_crypt(mf3d_zcg_ctx *ctx, const mf3d_u8 *src, mf3d_u32 len, mf3d_u8 *dst)
{
    mf3d_u8 ctr[16], ks[16];
    mf3d_u32 i, j, take;
    if (!ctx || (!src && len) || (!dst && len)) return;
    for (i = 0; i < 16; ++i) ctr[i] = ctx->j0[i];
    i = 0;
    while (i < len) {
        inc32(ctr);
        mf3d_zca_aes_encrypt_block(&ctx->aes, ctr, ks);
        take = len - i;
        if (take > 16UL) take = 16UL;
        for (j = 0; j < take; ++j) dst[i + j] = (mf3d_u8)(src[i + j] ^ ks[j]);
        i += take;
    }
}

void mf3d_zcg_tag(mf3d_zcg_ctx *ctx, const mf3d_u8 *aad, mf3d_u32 aad_len,
                  const mf3d_u8 *ciphertext, mf3d_u32 cipher_len,
                  mf3d_u8 tag16[16])
{
    mf3d_u8 s[16], ej0[16];
    int i;
    if (!ctx || !tag16) return;
    ghash_all(ctx->h, aad, aad_len, ciphertext, cipher_len, s);
    mf3d_zca_aes_encrypt_block(&ctx->aes, ctx->j0, ej0);
    for (i = 0; i < 16; ++i) tag16[i] = (mf3d_u8)(s[i] ^ ej0[i]);
}

int mf3d_zcg_decrypt_verify(const mf3d_u8 *key, mf3d_u32 key_len,
                            const mf3d_u8 *iv, mf3d_u32 iv_len,
                            const mf3d_u8 *aad, mf3d_u32 aad_len,
                            const mf3d_u8 *cipher, mf3d_u32 cipher_len,
                            const mf3d_u8 tag16[16],
                            mf3d_u8 *plain, mf3d_u32 plain_cap,
                            mf3d_u32 *plain_len)
{
    mf3d_zcg_ctx ctx;
    mf3d_u8 calc[16];
    int i, diff, r;
    if (!key || !iv || (!cipher && cipher_len) || !tag16 || (!plain && cipher_len)) return MF3D_E_BADARG;
    if (cipher_len > plain_cap) return MF3D_E_NOSPACE;
    r = mf3d_zcg_init(&ctx, key, key_len, iv, iv_len, aad, aad_len);
    if (r != MF3D_OK) return r;
    mf3d_zcg_tag(&ctx, aad, aad_len, cipher, cipher_len, calc);
    diff = 0;
    for (i = 0; i < 16; ++i) diff |= (int)(calc[i] ^ tag16[i]);
    if (diff) return MF3D_E_RANGE;
    mf3d_zcg_crypt(&ctx, cipher, cipher_len, plain);
    if (plain_len) *plain_len = cipher_len;
    return MF3D_OK;
}

int mf3d_zcg_encrypt_tag(const mf3d_u8 *key, mf3d_u32 key_len,
                         const mf3d_u8 *iv, mf3d_u32 iv_len,
                         const mf3d_u8 *aad, mf3d_u32 aad_len,
                         const mf3d_u8 *plain, mf3d_u32 plain_len,
                         mf3d_u8 *cipher, mf3d_u32 cipher_cap,
                         mf3d_u8 tag16[16])
{
    mf3d_zcg_ctx ctx;
    int r;
    if (!key || !iv || (!plain && plain_len) || (!cipher && plain_len) || !tag16) return MF3D_E_BADARG;
    if (plain_len > cipher_cap) return MF3D_E_NOSPACE;
    r = mf3d_zcg_init(&ctx, key, key_len, iv, iv_len, aad, aad_len);
    if (r != MF3D_OK) return r;
    mf3d_zcg_crypt(&ctx, plain, plain_len, cipher);
    mf3d_zcg_tag(&ctx, aad, aad_len, cipher, plain_len, tag16);
    return MF3D_OK;
}
