#ifndef MF3D_EXT_SECURE_CONTENT_H
#define MF3D_EXT_SECURE_CONTENT_H

#include "../src/mf3d.h"
#include "zip_crypto_aes/mf3d_zip_crypto_gcm.h"

#ifndef MF3D_EXT_MAX_SC_CONSUMERS
#define MF3D_EXT_MAX_SC_CONSUMERS 64
#endif
#ifndef MF3D_EXT_MAX_SC_GROUPS
#define MF3D_EXT_MAX_SC_GROUPS 128
#endif
#ifndef MF3D_EXT_MAX_SC_RESOURCES
#define MF3D_EXT_MAX_SC_RESOURCES 512
#endif
#ifndef MF3D_EXT_MAX_SC_RIGHTS
#define MF3D_EXT_MAX_SC_RIGHTS 256
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_sc_consumer_s {
    char consumerid[96];
    char keyid[96];
} mf3d_sc_consumer;

typedef struct mf3d_sc_group_s {
    char keyuuid[48];
    mf3d_u32 first_resource;
    mf3d_u32 resource_count;
    mf3d_u32 first_right;
    mf3d_u32 right_count;
} mf3d_sc_group;

typedef struct mf3d_sc_resource_s {
    char keyuuid[48];
    char path[192];
    char encryptionalgorithm[96];
    char compression[32];
    mf3d_u8 iv[32];
    mf3d_u32 iv_len;
    mf3d_u8 tag[32];
    mf3d_u32 tag_len;
    mf3d_u8 aad[128];
    mf3d_u32 aad_len;
} mf3d_sc_resource;

typedef struct mf3d_sc_accessright_s {
    char keyuuid[48];
    mf3d_u32 consumerindex;
    char wrappingalgorithm[128];
    char mgfalgorithm[96];
    char digestmethod[96];
    char cipher_b64[256];
} mf3d_sc_accessright;

typedef struct mf3d_secure_content_s {
    char uuid[48];
    mf3d_sc_consumer consumers[MF3D_EXT_MAX_SC_CONSUMERS];
    mf3d_sc_group groups[MF3D_EXT_MAX_SC_GROUPS];
    mf3d_sc_resource resources[MF3D_EXT_MAX_SC_RESOURCES];
    mf3d_sc_accessright rights[MF3D_EXT_MAX_SC_RIGHTS];
    mf3d_u32 consumer_count, group_count, resource_count, right_count;
} mf3d_secure_content;

void mf3d_secure_content_init(mf3d_secure_content *s);
int mf3d_secure_content_parse_keystore_xml(char *xml, mf3d_secure_content *out_s);
const mf3d_sc_resource *mf3d_secure_content_find_resource(const mf3d_secure_content *s, const char *path);
int mf3d_secure_content_base64_decode(const char *src, mf3d_u8 *dst, mf3d_u32 dst_cap, mf3d_u32 *dst_len);
int mf3d_secure_content_decrypt_aes256_gcm(const mf3d_sc_resource *res,
                                           const mf3d_u8 cek[32],
                                           const mf3d_u8 *cipher, mf3d_u32 cipher_len,
                                           mf3d_u8 *plain, mf3d_u32 plain_cap,
                                           mf3d_u32 *plain_len);

#ifdef __cplusplus
}
#endif
#endif
