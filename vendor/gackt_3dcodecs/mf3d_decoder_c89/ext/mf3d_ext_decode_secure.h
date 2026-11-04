#ifndef MF3D_EXT_DECODE_SECURE_H
#define MF3D_EXT_DECODE_SECURE_H

#include "../src/mf3d.h"
#include "zip64_deflate/mf3d_zip64_deflate.h"
#include "zip_secure_span/mf3d_zip_secure_span.h"
#include "zip_crypto_aes/mf3d_zip_crypto_aes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_secure_decode_options_s {
    const char *password;
    mf3d_u8 *xml_scratch;
    mf3d_u32 xml_scratch_size;
    mf3d_u8 *compressed_scratch;
    mf3d_u32 compressed_scratch_size;
    mf3d_u32 verify_winzip_aes_auth;
} mf3d_secure_decode_options;

int mf3d_decode_secure_zip(const mf3d_u8 *data, mf3d_u32 size,
                           mf3d_model *out_model,
                           mf3d_secure_decode_options *opt);

int mf3d_decode_secure_zip_model_xml(const mf3d_u8 *data, mf3d_u32 size,
                                     mf3d_secure_decode_options *opt,
                                     mf3d_u8 *xml_out, mf3d_u32 xml_cap,
                                     mf3d_u32 *xml_len);

#ifdef __cplusplus
}
#endif
#endif
