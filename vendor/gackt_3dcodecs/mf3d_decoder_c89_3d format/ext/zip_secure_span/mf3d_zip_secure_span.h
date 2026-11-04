#ifndef MF3D_ZIP_SECURE_SPAN_H
#define MF3D_ZIP_SECURE_SPAN_H

/* Optional ZIP security/split support for mf3d.
   C89, no malloc/free/realloc, no float/double.

   Scope:
   - real legacy PKWARE ZipCrypto decryption when caller supplies a password
   - split/spanned archive segment concatenation into caller-owned buffer
   - central-directory encryption / strong encryption / WinZip AES metadata detection
   - callback bridge for proprietary/strong crypto methods the tiny core should not own
*/

#include "mf3d.h"
#include "mf3d_zip64_deflate.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MF3D_ZSS_GP_ENCRYPTED       0x0001U
#define MF3D_ZSS_GP_STRONG          0x0040U
#define MF3D_ZSS_GP_UTF8            0x0800U
#define MF3D_ZSS_GP_CENTRAL_MASKED  0x2000U

#define MF3D_ZSS_METHOD_AES_EXTRA   99U
#define MF3D_ZSS_EXTRA_STRONG       0x0017U
#define MF3D_ZSS_EXTRA_AES          0x9901U

#define MF3D_ZSS_JOIN_STRIP_MARKER  1U
#define MF3D_ZSS_SPAN_SIG           0x08074b50UL
#define MF3D_ZSS_SPAN_TMP_SIG       0x30304b50UL

typedef struct mf3d_zss_span_s {
    const mf3d_u8 *data;
    mf3d_u32 size;
} mf3d_zss_span;

typedef struct mf3d_zss_crypto_info_s {
    mf3d_u16 gp_flag;
    mf3d_u16 method;
    mf3d_u16 aes_vendor_version;
    char aes_vendor_id[3];
    mf3d_u8 aes_strength;
    mf3d_u16 aes_actual_method;
    mf3d_u16 strong_format;
    mf3d_u16 strong_algid;
    mf3d_u16 strong_bitlen;
    mf3d_u16 strong_flags;
    mf3d_u32 is_encrypted;
    mf3d_u32 is_legacy_zipcrypto;
    mf3d_u32 is_strong;
    mf3d_u32 is_winzip_aes;
    mf3d_u32 local_header_masked;
} mf3d_zss_crypto_info;

typedef struct mf3d_zss_cdir_info_s {
    mf3d_u32 present;
    mf3d_u32 encrypted_or_compressed;
    mf3d_u32 masked_local_headers;
    mf3d_u16 compression_method;
    mf3d_z64_u64 compressed_size;
    mf3d_z64_u64 original_size;
    mf3d_u16 algid;
    mf3d_u16 bitlen;
    mf3d_u16 flags;
    mf3d_u16 hashid;
    mf3d_u16 hash_len;
    mf3d_z64_u64 archive_header_offset;
} mf3d_zss_cdir_info;

/* Optional bridge for AES/strong/private encryption. The callback receives the
   encrypted compressed data for one entry and must write decrypted compressed
   bytes to dst. It must not allocate if used in strict builds. */
typedef int (*mf3d_zss_crypto_fn)(void *user, const mf3d_zss_crypto_info *info,
                                 const mf3d_u8 *src, mf3d_u32 src_len,
                                 mf3d_u8 *dst, mf3d_u32 dst_cap,
                                 mf3d_u32 *dst_len);

typedef struct mf3d_zss_options_s {
    const char *password;
    mf3d_u8 *scratch;
    mf3d_u32 scratch_size;
    mf3d_zss_crypto_fn crypto_cb;
    void *crypto_user;
} mf3d_zss_options;

int mf3d_zss_join_spans(const mf3d_zss_span *spans, mf3d_u32 span_count,
                        mf3d_u32 flags, mf3d_u8 *dst, mf3d_u32 dst_cap,
                        mf3d_u32 *dst_len);

int mf3d_zss_is_split_marker(const mf3d_u8 *data, mf3d_u32 size);
int mf3d_zss_entry_crypto_info(const mf3d_u8 *zip_data, mf3d_u32 zip_size,
                               const mf3d_z64_entry *entry,
                               mf3d_zss_crypto_info *info);
int mf3d_zss_inspect_central_directory(const mf3d_u8 *zip_data, mf3d_u32 zip_size,
                                       mf3d_zss_cdir_info *info);

int mf3d_zss_extract(const mf3d_u8 *zip_data, mf3d_u32 zip_size,
                     mf3d_z64_archive *zip, int idx,
                     const mf3d_zss_options *opt,
                     mf3d_u8 *dst, mf3d_u32 dst_cap,
                     mf3d_u32 *dst_len);

int mf3d_zss_extract_named(const mf3d_u8 *zip_data, mf3d_u32 zip_size,
                           const char *name, const mf3d_zss_options *opt,
                           mf3d_u8 *dst, mf3d_u32 dst_cap,
                           mf3d_u32 *dst_len);

/* Low-level utility for tests/tools: decrypt one complete legacy ZipCrypto
   member payload. src includes the 12-byte encryption header. */
int mf3d_zss_zipcrypto_decrypt(const char *password,
                               const mf3d_u8 *src, mf3d_u32 src_len,
                               mf3d_u8 *dst, mf3d_u32 dst_cap,
                               mf3d_u32 *dst_len,
                               mf3d_u8 crc_high_check,
                               mf3d_u32 *header_ok);

#ifdef __cplusplus
}
#endif
#endif
