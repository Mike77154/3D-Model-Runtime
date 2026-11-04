#ifndef MF3D_ZIP_CRYPTO_AES_H
#define MF3D_ZIP_CRYPTO_AES_H

/* Optional WinZip AES crypto helper for mf3d ZIP security.
   C89, no malloc/free/realloc, no float/double.

   Implements the common WinZip AES extra field 0x9901 path:
   PBKDF2-HMAC-SHA1(1000), AES-CTR, HMAC-SHA1-80 authentication.
   It is separated from the core so strict tiny builds may omit it. */

#include "../../src/mf3d.h"
#include "../zip_secure_span/mf3d_zip_secure_span.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MF3D_ZCA_SHA1_SIZE 20U
#define MF3D_ZCA_AES_BLOCK 16U
#define MF3D_ZCA_MAX_KEY   32U
#define MF3D_ZCA_MAX_DERIVED 66U
#define MF3D_ZCA_WINZIP_ITERATIONS 1000U

typedef struct mf3d_zca_sha1_s {
    mf3d_u32 state[5];
    mf3d_u32 count_hi;
    mf3d_u32 count_lo;
    mf3d_u8 buffer[64];
} mf3d_zca_sha1;

typedef struct mf3d_zca_aes_s {
    mf3d_u32 rk[60];
    mf3d_u32 rounds;
} mf3d_zca_aes;

typedef struct mf3d_zca_options_s {
    const char *password;
    mf3d_u32 verify_auth_code;
} mf3d_zca_options;

void mf3d_zca_sha1_init(mf3d_zca_sha1 *ctx);
void mf3d_zca_sha1_update(mf3d_zca_sha1 *ctx, const mf3d_u8 *data, mf3d_u32 len);
void mf3d_zca_sha1_final(mf3d_zca_sha1 *ctx, mf3d_u8 out20[20]);
void mf3d_zca_hmac_sha1(const mf3d_u8 *key, mf3d_u32 key_len,
                        const mf3d_u8 *data, mf3d_u32 data_len,
                        mf3d_u8 out20[20]);
int mf3d_zca_pbkdf2_hmac_sha1(const mf3d_u8 *password, mf3d_u32 password_len,
                              const mf3d_u8 *salt, mf3d_u32 salt_len,
                              mf3d_u32 iterations,
                              mf3d_u8 *out, mf3d_u32 out_len);

int mf3d_zca_aes_set_key(mf3d_zca_aes *aes, const mf3d_u8 *key, mf3d_u32 key_len);
void mf3d_zca_aes_encrypt_block(const mf3d_zca_aes *aes,
                                const mf3d_u8 in16[16], mf3d_u8 out16[16]);
void mf3d_zca_aes_ctr_xcrypt(const mf3d_zca_aes *aes,
                             const mf3d_u8 *src, mf3d_u32 src_len,
                             mf3d_u8 *dst);

int mf3d_zca_winzip_aes_decrypt(const mf3d_zss_crypto_info *info,
                                const char *password,
                                const mf3d_u8 *src, mf3d_u32 src_len,
                                mf3d_u8 *dst, mf3d_u32 dst_cap,
                                mf3d_u32 *dst_len,
                                mf3d_u32 verify_auth_code);

int mf3d_zca_crypto_cb(void *user, const mf3d_zss_crypto_info *info,
                       const mf3d_u8 *src, mf3d_u32 src_len,
                       mf3d_u8 *dst, mf3d_u32 dst_cap,
                       mf3d_u32 *dst_len);

void mf3d_zca_make_zss_options(mf3d_zss_options *zss,
                               mf3d_zca_options *zca,
                               const char *password,
                               mf3d_u8 *scratch,
                               mf3d_u32 scratch_size);

#ifdef __cplusplus
}
#endif
#endif
