#include "mf3d_zip_crypto_aes.h"

/* ----------------------------- SHA-1 ----------------------------- */

static mf3d_u32 zca_rol32(mf3d_u32 x, int n)
{
    x &= 0xffffffffUL;
    return (mf3d_u32)(((x << n) | (x >> (32 - n))) & 0xffffffffUL);
}

static mf3d_u32 zca_be32(const mf3d_u8 *p)
{
    return (((mf3d_u32)p[0] << 24) | ((mf3d_u32)p[1] << 16) | ((mf3d_u32)p[2] << 8) | (mf3d_u32)p[3]) & 0xffffffffUL;
}

static void zca_put_be32(mf3d_u8 *p, mf3d_u32 v)
{
    p[0] = (mf3d_u8)(v >> 24);
    p[1] = (mf3d_u8)(v >> 16);
    p[2] = (mf3d_u8)(v >> 8);
    p[3] = (mf3d_u8)v;
}

static void zca_sha1_transform(mf3d_u32 st[5], const mf3d_u8 block[64])
{
    mf3d_u32 w[80];
    mf3d_u32 a, b, c, d, e, f, k, t;
    int i;
    for (i = 0; i < 16; ++i) w[i] = zca_be32(block + (mf3d_u32)i * 4UL);
    for (i = 16; i < 80; ++i) w[i] = zca_rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    a = st[0]; b = st[1]; c = st[2]; d = st[3]; e = st[4];
    for (i = 0; i < 80; ++i) {
        if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5a827999UL; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ed9eba1UL; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdcUL; }
        else { f = b ^ c ^ d; k = 0xca62c1d6UL; }
        t = (zca_rol32(a, 5) + f + e + k + w[i]) & 0xffffffffUL;
        e = d; d = c; c = zca_rol32(b, 30); b = a; a = t;
    }
    st[0] = (st[0] + a) & 0xffffffffUL;
    st[1] = (st[1] + b) & 0xffffffffUL;
    st[2] = (st[2] + c) & 0xffffffffUL;
    st[3] = (st[3] + d) & 0xffffffffUL;
    st[4] = (st[4] + e) & 0xffffffffUL;
}

void mf3d_zca_sha1_init(mf3d_zca_sha1 *ctx)
{
    if (!ctx) return;
    ctx->state[0] = 0x67452301UL;
    ctx->state[1] = 0xefcdab89UL;
    ctx->state[2] = 0x98badcfeUL;
    ctx->state[3] = 0x10325476UL;
    ctx->state[4] = 0xc3d2e1f0UL;
    ctx->count_hi = 0;
    ctx->count_lo = 0;
}

void mf3d_zca_sha1_update(mf3d_zca_sha1 *ctx, const mf3d_u8 *data, mf3d_u32 len)
{
    mf3d_u32 idx, part, i, oldlo;
    if (!ctx || (!data && len)) return;
    idx = (ctx->count_lo >> 3) & 63UL;
    oldlo = ctx->count_lo;
    ctx->count_lo = (ctx->count_lo + (len << 3)) & 0xffffffffUL;
    if (ctx->count_lo < oldlo) ctx->count_hi = (ctx->count_hi + 1UL) & 0xffffffffUL;
    ctx->count_hi = (ctx->count_hi + (len >> 29)) & 0xffffffffUL;
    part = 64UL - idx;
    i = 0;
    if (len >= part) {
        while (i < part) { ctx->buffer[idx + i] = data[i]; ++i; }
        zca_sha1_transform(ctx->state, ctx->buffer);
        while (i + 63UL < len) {
            zca_sha1_transform(ctx->state, data + i);
            i += 64UL;
        }
        idx = 0;
    }
    while (i < len) { ctx->buffer[idx++] = data[i++]; }
}

void mf3d_zca_sha1_final(mf3d_zca_sha1 *ctx, mf3d_u8 out20[20])
{
    mf3d_u8 pad[64];
    mf3d_u8 bits[8];
    mf3d_u32 idx, pad_len;
    int i;
    if (!ctx || !out20) return;
    for (i = 0; i < 64; ++i) pad[i] = 0;
    pad[0] = 0x80U;
    zca_put_be32(bits, ctx->count_hi);
    zca_put_be32(bits + 4, ctx->count_lo);
    idx = (ctx->count_lo >> 3) & 63UL;
    pad_len = (idx < 56UL) ? (56UL - idx) : (120UL - idx);
    mf3d_zca_sha1_update(ctx, pad, pad_len);
    mf3d_zca_sha1_update(ctx, bits, 8);
    for (i = 0; i < 5; ++i) zca_put_be32(out20 + i * 4, ctx->state[i]);
}

void mf3d_zca_hmac_sha1(const mf3d_u8 *key, mf3d_u32 key_len,
                        const mf3d_u8 *data, mf3d_u32 data_len,
                        mf3d_u8 out20[20])
{
    mf3d_u8 kopad[64];
    mf3d_u8 kipad[64];
    mf3d_u8 tk[20];
    mf3d_zca_sha1 ctx;
    mf3d_u32 i;
    if (!out20) return;
    if (!key && key_len) return;
    if (!data && data_len) return;
    if (key_len > 64UL) {
        mf3d_zca_sha1_init(&ctx);
        mf3d_zca_sha1_update(&ctx, key, key_len);
        mf3d_zca_sha1_final(&ctx, tk);
        key = tk;
        key_len = 20UL;
    }
    for (i = 0; i < 64UL; ++i) {
        kipad[i] = 0x36U;
        kopad[i] = 0x5cU;
    }
    for (i = 0; i < key_len; ++i) {
        kipad[i] = (mf3d_u8)(kipad[i] ^ key[i]);
        kopad[i] = (mf3d_u8)(kopad[i] ^ key[i]);
    }
    mf3d_zca_sha1_init(&ctx);
    mf3d_zca_sha1_update(&ctx, kipad, 64);
    mf3d_zca_sha1_update(&ctx, data, data_len);
    mf3d_zca_sha1_final(&ctx, tk);
    mf3d_zca_sha1_init(&ctx);
    mf3d_zca_sha1_update(&ctx, kopad, 64);
    mf3d_zca_sha1_update(&ctx, tk, 20);
    mf3d_zca_sha1_final(&ctx, out20);
}

int mf3d_zca_pbkdf2_hmac_sha1(const mf3d_u8 *password, mf3d_u32 password_len,
                              const mf3d_u8 *salt, mf3d_u32 salt_len,
                              mf3d_u32 iterations,
                              mf3d_u8 *out, mf3d_u32 out_len)
{
    mf3d_u8 u[20];
    mf3d_u8 t[20];
    mf3d_u8 salt_block[80];
    mf3d_u32 block, produced, take, i, j, sb_len;
    if ((!password && password_len) || (!salt && salt_len) || !out || iterations == 0) return MF3D_E_BADARG;
    if (salt_len + 4UL > sizeof(salt_block)) return MF3D_E_NOSPACE;
    produced = 0;
    block = 1;
    while (produced < out_len) {
        for (i = 0; i < salt_len; ++i) salt_block[i] = salt[i];
        salt_block[salt_len + 0] = (mf3d_u8)(block >> 24);
        salt_block[salt_len + 1] = (mf3d_u8)(block >> 16);
        salt_block[salt_len + 2] = (mf3d_u8)(block >> 8);
        salt_block[salt_len + 3] = (mf3d_u8)block;
        sb_len = salt_len + 4UL;
        mf3d_zca_hmac_sha1(password, password_len, salt_block, sb_len, u);
        for (j = 0; j < 20UL; ++j) t[j] = u[j];
        for (i = 1; i < iterations; ++i) {
            mf3d_zca_hmac_sha1(password, password_len, u, 20, u);
            for (j = 0; j < 20UL; ++j) t[j] = (mf3d_u8)(t[j] ^ u[j]);
        }
        take = out_len - produced;
        if (take > 20UL) take = 20UL;
        for (j = 0; j < take; ++j) out[produced + j] = t[j];
        produced += take;
        block++;
    }
    return MF3D_OK;
}

/* ----------------------------- AES encrypt block ----------------------------- */

static const mf3d_u8 zca_sbox[256] = {
0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static const mf3d_u32 zca_rcon[11] = {
0x00000000UL,0x01000000UL,0x02000000UL,0x04000000UL,0x08000000UL,0x10000000UL,
0x20000000UL,0x40000000UL,0x80000000UL,0x1b000000UL,0x36000000UL
};

static mf3d_u32 zca_get_be_word(const mf3d_u8 *p)
{
    return (((mf3d_u32)p[0] << 24) | ((mf3d_u32)p[1] << 16) | ((mf3d_u32)p[2] << 8) | (mf3d_u32)p[3]) & 0xffffffffUL;
}

static mf3d_u32 zca_sub_word(mf3d_u32 x)
{
    return ((mf3d_u32)zca_sbox[(x >> 24) & 255U] << 24) |
           ((mf3d_u32)zca_sbox[(x >> 16) & 255U] << 16) |
           ((mf3d_u32)zca_sbox[(x >> 8) & 255U] << 8) |
           ((mf3d_u32)zca_sbox[x & 255U]);
}

static mf3d_u32 zca_rot_word(mf3d_u32 x)
{
    x &= 0xffffffffUL;
    return ((x << 8) | (x >> 24)) & 0xffffffffUL;
}

int mf3d_zca_aes_set_key(mf3d_zca_aes *aes, const mf3d_u8 *key, mf3d_u32 key_len)
{
    mf3d_u32 nk, nr, i, temp;
    if (!aes || !key) return MF3D_E_BADARG;
    if (key_len == 16UL) { nk = 4; nr = 10; }
    else if (key_len == 24UL) { nk = 6; nr = 12; }
    else if (key_len == 32UL) { nk = 8; nr = 14; }
    else return MF3D_E_BADARG;
    aes->rounds = nr;
    for (i = 0; i < nk; ++i) aes->rk[i] = zca_get_be_word(key + i * 4UL);
    for (i = nk; i < 4UL * (nr + 1UL); ++i) {
        temp = aes->rk[i - 1UL];
        if ((i % nk) == 0UL) temp = zca_sub_word(zca_rot_word(temp)) ^ zca_rcon[i / nk];
        else if (nk > 6UL && (i % nk) == 4UL) temp = zca_sub_word(temp);
        aes->rk[i] = (aes->rk[i - nk] ^ temp) & 0xffffffffUL;
    }
    return MF3D_OK;
}

static mf3d_u8 zca_xtime(mf3d_u8 x)
{
    return (mf3d_u8)((x << 1) ^ ((x & 0x80U) ? 0x1bU : 0U));
}

static void zca_add_round_key(mf3d_u8 s[16], const mf3d_u32 *rk)
{
    int c;
    mf3d_u32 k;
    for (c = 0; c < 4; ++c) {
        k = rk[c];
        s[4 * c + 0] = (mf3d_u8)(s[4 * c + 0] ^ (mf3d_u8)(k >> 24));
        s[4 * c + 1] = (mf3d_u8)(s[4 * c + 1] ^ (mf3d_u8)(k >> 16));
        s[4 * c + 2] = (mf3d_u8)(s[4 * c + 2] ^ (mf3d_u8)(k >> 8));
        s[4 * c + 3] = (mf3d_u8)(s[4 * c + 3] ^ (mf3d_u8)k);
    }
}

static void zca_sub_bytes(mf3d_u8 s[16])
{
    int i;
    for (i = 0; i < 16; ++i) s[i] = zca_sbox[s[i]];
}

static void zca_shift_rows(mf3d_u8 s[16])
{
    mf3d_u8 t;
    t = s[1]; s[1] = s[5]; s[5] = s[9]; s[9] = s[13]; s[13] = t;
    t = s[2]; s[2] = s[10]; s[10] = t; t = s[6]; s[6] = s[14]; s[14] = t;
    t = s[15]; s[15] = s[11]; s[11] = s[7]; s[7] = s[3]; s[3] = t;
}

static void zca_mix_columns(mf3d_u8 s[16])
{
    int c;
    mf3d_u8 *p;
    mf3d_u8 a0, a1, a2, a3, t, u;
    for (c = 0; c < 4; ++c) {
        p = s + 4 * c;
        a0 = p[0]; a1 = p[1]; a2 = p[2]; a3 = p[3];
        t = (mf3d_u8)(a0 ^ a1 ^ a2 ^ a3);
        u = a0;
        p[0] = (mf3d_u8)(p[0] ^ t ^ zca_xtime((mf3d_u8)(a0 ^ a1)));
        p[1] = (mf3d_u8)(p[1] ^ t ^ zca_xtime((mf3d_u8)(a1 ^ a2)));
        p[2] = (mf3d_u8)(p[2] ^ t ^ zca_xtime((mf3d_u8)(a2 ^ a3)));
        p[3] = (mf3d_u8)(p[3] ^ t ^ zca_xtime((mf3d_u8)(a3 ^ u)));
    }
}

void mf3d_zca_aes_encrypt_block(const mf3d_zca_aes *aes,
                                const mf3d_u8 in16[16], mf3d_u8 out16[16])
{
    mf3d_u8 s[16];
    mf3d_u32 round;
    int i;
    if (!aes || !in16 || !out16) return;
    for (i = 0; i < 16; ++i) s[i] = in16[i];
    zca_add_round_key(s, aes->rk);
    for (round = 1; round < aes->rounds; ++round) {
        zca_sub_bytes(s);
        zca_shift_rows(s);
        zca_mix_columns(s);
        zca_add_round_key(s, aes->rk + round * 4UL);
    }
    zca_sub_bytes(s);
    zca_shift_rows(s);
    zca_add_round_key(s, aes->rk + aes->rounds * 4UL);
    for (i = 0; i < 16; ++i) out16[i] = s[i];
}

static void zca_ctr_put_le(mf3d_u8 ctr[16], mf3d_u32 v)
{
    int i;
    for (i = 0; i < 16; ++i) ctr[i] = 0;
    ctr[0] = (mf3d_u8)v;
    ctr[1] = (mf3d_u8)(v >> 8);
    ctr[2] = (mf3d_u8)(v >> 16);
    ctr[3] = (mf3d_u8)(v >> 24);
}

void mf3d_zca_aes_ctr_xcrypt(const mf3d_zca_aes *aes,
                             const mf3d_u8 *src, mf3d_u32 src_len,
                             mf3d_u8 *dst)
{
    mf3d_u8 ctr[16];
    mf3d_u8 ks[16];
    mf3d_u32 pos, block, i, take;
    if (!aes || (!src && src_len) || !dst) return;
    pos = 0;
    block = 1;
    while (pos < src_len) {
        zca_ctr_put_le(ctr, block);
        mf3d_zca_aes_encrypt_block(aes, ctr, ks);
        take = src_len - pos;
        if (take > 16UL) take = 16UL;
        for (i = 0; i < take; ++i) dst[pos + i] = (mf3d_u8)(src[pos + i] ^ ks[i]);
        pos += take;
        block++;
    }
}

/* ----------------------------- WinZip AES ----------------------------- */

static mf3d_u32 zca_strlen_u32(const char *s)
{
    mf3d_u32 n;
    n = 0;
    if (!s) return 0;
    while (s[n]) ++n;
    return n;
}

static int zca_auth_eq10(const mf3d_u8 *a, const mf3d_u8 *b)
{
    mf3d_u8 d;
    int i;
    d = 0;
    for (i = 0; i < 10; ++i) d = (mf3d_u8)(d | (a[i] ^ b[i]));
    return d == 0;
}

int mf3d_zca_winzip_aes_decrypt(const mf3d_zss_crypto_info *info,
                                const char *password,
                                const mf3d_u8 *src, mf3d_u32 src_len,
                                mf3d_u8 *dst, mf3d_u32 dst_cap,
                                mf3d_u32 *dst_len,
                                mf3d_u32 verify_auth_code)
{
    mf3d_u32 key_len, salt_len, need, data_len, pass_len;
    mf3d_u8 derived[MF3D_ZCA_MAX_DERIVED];
    mf3d_u8 auth[20];
    mf3d_zca_aes aes;
    int r;
    if (!info || !password || !src || !dst || !dst_len) return MF3D_E_BADARG;
    if (!info->is_winzip_aes) return MF3D_E_UNSUPPORTED;
    if (info->aes_strength == 1U) { key_len = 16UL; salt_len = 8UL; }
    else if (info->aes_strength == 2U) { key_len = 24UL; salt_len = 12UL; }
    else if (info->aes_strength == 3U) { key_len = 32UL; salt_len = 16UL; }
    else return MF3D_E_UNSUPPORTED;
    need = salt_len + 2UL + 10UL;
    if (src_len < need) return MF3D_E_ZIP;
    data_len = src_len - need;
    if (data_len > dst_cap) return MF3D_E_NOSPACE;
    pass_len = zca_strlen_u32(password);
    r = mf3d_zca_pbkdf2_hmac_sha1((const mf3d_u8*)password, pass_len, src, salt_len,
                                  MF3D_ZCA_WINZIP_ITERATIONS,
                                  derived, key_len * 2UL + 2UL);
    if (r != MF3D_OK) return r;
    if (derived[key_len * 2UL] != src[salt_len] || derived[key_len * 2UL + 1UL] != src[salt_len + 1UL]) return MF3D_E_ZIP;
    if (verify_auth_code) {
        mf3d_zca_hmac_sha1(derived + key_len, key_len, src + salt_len + 2UL, data_len, auth);
        if (!zca_auth_eq10(auth, src + salt_len + 2UL + data_len)) return MF3D_E_ZIP;
    }
    r = mf3d_zca_aes_set_key(&aes, derived, key_len);
    if (r != MF3D_OK) return r;
    mf3d_zca_aes_ctr_xcrypt(&aes, src + salt_len + 2UL, data_len, dst);
    *dst_len = data_len;
    return MF3D_OK;
}

int mf3d_zca_crypto_cb(void *user, const mf3d_zss_crypto_info *info,
                       const mf3d_u8 *src, mf3d_u32 src_len,
                       mf3d_u8 *dst, mf3d_u32 dst_cap,
                       mf3d_u32 *dst_len)
{
    mf3d_zca_options *opt;
    opt = (mf3d_zca_options*)user;
    if (!opt) return MF3D_E_BADARG;
    if (info && info->is_winzip_aes) {
        return mf3d_zca_winzip_aes_decrypt(info, opt->password, src, src_len,
                                           dst, dst_cap, dst_len,
                                           opt->verify_auth_code);
    }
    return MF3D_E_UNSUPPORTED;
}

void mf3d_zca_make_zss_options(mf3d_zss_options *zss,
                               mf3d_zca_options *zca,
                               const char *password,
                               mf3d_u8 *scratch,
                               mf3d_u32 scratch_size)
{
    if (!zss || !zca) return;
    zca->password = password;
    zca->verify_auth_code = 1UL;
    zss->password = password;
    zss->scratch = scratch;
    zss->scratch_size = scratch_size;
    zss->crypto_cb = mf3d_zca_crypto_cb;
    zss->crypto_user = zca;
}
