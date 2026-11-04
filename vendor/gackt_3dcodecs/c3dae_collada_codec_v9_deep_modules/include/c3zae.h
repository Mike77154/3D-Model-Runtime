#ifndef C3ZAE_H
#define C3ZAE_H

#include "c3dae.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C3ZAE_OK                    0
#define C3ZAE_ERR_NULL             -101
#define C3ZAE_ERR_FORMAT           -102
#define C3ZAE_ERR_CAPACITY         -103
#define C3ZAE_ERR_UNSUPPORTED      -104
#define C3ZAE_ERR_NOT_FOUND        -105
#define C3ZAE_ERR_INFLATE          -106
#define C3ZAE_ERR_CRC              -107

#define C3ZAE_METHOD_STORE          0U
#define C3ZAE_METHOD_DEFLATE        8U

typedef struct c3zae_entry_tag {
    char name[C3DAE_PATH_MAX];
    c3dae_u32 local_header_offset;
    c3dae_u32 compressed_size;
    c3dae_u32 uncompressed_size;
    c3dae_u32 crc32;
    unsigned int method;
    unsigned int flags;
} c3zae_entry;

typedef struct c3zae_archive_tag {
    c3zae_entry *entries;
    c3dae_u32 entry_cap;
    c3dae_u32 entry_count;
} c3zae_archive;

void c3zae_archive_init(c3zae_archive *a,
                        c3zae_entry *entries,
                        c3dae_u32 entry_cap);

int c3zae_read_directory(const unsigned char *zip,
                         c3dae_u32 zip_len,
                         c3zae_archive *archive);

int c3zae_find_entry(const c3zae_archive *archive,
                     const char *path);

int c3zae_find_first_dae(const c3zae_archive *archive);

int c3zae_extract_entry(const unsigned char *zip,
                        c3dae_u32 zip_len,
                        const c3zae_entry *entry,
                        unsigned char *out,
                        c3dae_u32 out_cap,
                        c3dae_u32 *out_len);

int c3zae_extract_dae(const unsigned char *zae,
                      c3dae_u32 zae_len,
                      c3zae_entry *entries,
                      c3dae_u32 entry_cap,
                      unsigned char *scratch,
                      c3dae_u32 scratch_cap,
                      c3dae_u32 *dae_len,
                      char *dae_path,
                      unsigned int dae_path_cap);

int c3dae_load_zae_memory(c3dae_context *ctx,
                          const unsigned char *zae,
                          c3dae_u32 zae_len,
                          c3zae_entry *entries,
                          c3dae_u32 entry_cap,
                          unsigned char *dae_scratch,
                          c3dae_u32 dae_scratch_cap,
                          c3dae_model *out_model);

const char *c3zae_error_string(int code);

#ifdef __cplusplus
}
#endif

#endif
