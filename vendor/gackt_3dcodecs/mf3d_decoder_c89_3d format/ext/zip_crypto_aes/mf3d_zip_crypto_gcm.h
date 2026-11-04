#ifndef MF3D_ZIP_CRYPTO_GCM_H
#define MF3D_ZIP_CRYPTO_GCM_H

#include "mf3d_zip_crypto_aes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MF3D_ZCG_TAG_SIZE 16U

typedef struct mf3d_zcg_ctx_s {
    mf3d_zca_aes aes;
    mf3d_u8 h[16];
    mf3d_u8 j0[16];
} mf3d_zcg_ctx;

int mf3d_zcg_init(mf3d_zcg_ctx *ctx, const mf3d_u8 *key, mf3d_u32 key_len,
                  const mf3d_u8 *iv, mf3d_u32 iv_len,
                  const mf3d_u8 *aad, mf3d_u32 aad_len);
void mf3d_zcg_crypt(mf3d_zcg_ctx *ctx, const mf3d_u8 *src, mf3d_u32 len, mf3d_u8 *dst);
void mf3d_zcg_tag(mf3d_zcg_ctx *ctx, const mf3d_u8 *aad, mf3d_u32 aad_len,
                  const mf3d_u8 *ciphertext, mf3d_u32 cipher_len,
                  mf3d_u8 tag16[16]);
int mf3d_zcg_decrypt_verify(const mf3d_u8 *key, mf3d_u32 key_len,
                            const mf3d_u8 *iv, mf3d_u32 iv_len,
                            const mf3d_u8 *aad, mf3d_u32 aad_len,
                            const mf3d_u8 *cipher, mf3d_u32 cipher_len,
                            const mf3d_u8 tag16[16],
                            mf3d_u8 *plain, mf3d_u32 plain_cap,
                            mf3d_u32 *plain_len);
int mf3d_zcg_encrypt_tag(const mf3d_u8 *key, mf3d_u32 key_len,
                         const mf3d_u8 *iv, mf3d_u32 iv_len,
                         const mf3d_u8 *aad, mf3d_u32 aad_len,
                         const mf3d_u8 *plain, mf3d_u32 plain_len,
                         mf3d_u8 *cipher, mf3d_u32 cipher_cap,
                         mf3d_u8 tag16[16]);

#ifdef __cplusplus
}
#endif
#endif
