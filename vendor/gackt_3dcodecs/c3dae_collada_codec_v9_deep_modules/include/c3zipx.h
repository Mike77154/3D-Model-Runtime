#ifndef C3ZIPX_H
#define C3ZIPX_H

#include "c3dae.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C3ZIPX_OK                         0
#define C3ZIPX_ERR_NULL                -201
#define C3ZIPX_ERR_FORMAT              -202
#define C3ZIPX_ERR_CAPACITY            -203
#define C3ZIPX_ERR_UNSUPPORTED         -204
#define C3ZIPX_ERR_NOT_FOUND           -205
#define C3ZIPX_ERR_INFLATE             -206
#define C3ZIPX_ERR_CRC                 -207
#define C3ZIPX_ERR_PASSWORD            -208
#define C3ZIPX_ERR_BAD_PASSWORD        -209
#define C3ZIPX_ERR_NEED_CODEC          -210
#define C3ZIPX_ERR_STRONG_ENCRYPTION   -211
#define C3ZIPX_ERR_RANGE64             -212

#define C3ZIPX_METHOD_STORE              0U
#define C3ZIPX_METHOD_DEFLATE            8U
#define C3ZIPX_METHOD_BZIP2             12U
#define C3ZIPX_METHOD_LZMA              14U
#define C3ZIPX_METHOD_AES               99U

#define C3ZIPX_FLAG_ENCRYPTED            1U
#define C3ZIPX_FLAG_DATA_DESCRIPTOR      8U
#define C3ZIPX_FLAG_STRONG_ENCRYPTION   64U
#define C3ZIPX_FLAG_UTF8              2048U

#define C3ZIPX_FEATURE_ZIP64             1U
#define C3ZIPX_FEATURE_AES               2U
#define C3ZIPX_FEATURE_STRONG            4U
#define C3ZIPX_FEATURE_ZIPCRYPTO         8U

#define C3ZIPX_AES_NONE                  0U
#define C3ZIPX_AES_128                   1U
#define C3ZIPX_AES_192                   2U
#define C3ZIPX_AES_256                   3U

#ifndef C3ZIPX_LZMA_MAX_PROPS
#define C3ZIPX_LZMA_MAX_PROPS           16U
#endif

typedef struct c3zipx_u64_tag {
    c3dae_u32 lo;
    c3dae_u32 hi;
} c3zipx_u64;

typedef struct c3zipx_entry_tag {
    char name[C3DAE_PATH_MAX];
    c3zipx_u64 local_header_offset;
    c3zipx_u64 compressed_size;
    c3zipx_u64 uncompressed_size;
    c3dae_u32 crc32;
    unsigned int method;
    unsigned int actual_method;
    unsigned int flags;
    unsigned int version_needed;
    unsigned int mod_time;
    unsigned int mod_date;
    unsigned int features;
    unsigned int aes_strength;
    unsigned int aes_vendor_version;
    unsigned int disk_start;
} c3zipx_entry;

typedef struct c3zipx_archive_tag {
    c3zipx_entry *entries;
    c3dae_u32 entry_cap;
    c3dae_u32 entry_count;
    unsigned int is_zip64;
} c3zipx_archive;

typedef int (*c3zipx_codec_fn)(const unsigned char *in,
                              c3dae_u32 in_len,
                              unsigned char *out,
                              c3dae_u32 out_cap,
                              c3dae_u32 expected_out_len,
                              c3dae_u32 *out_len,
                              void *user);

typedef struct c3zipx_codecs_tag {
    c3zipx_codec_fn bzip2_decode;
    c3zipx_codec_fn lzma_decode;
    void *user;
} c3zipx_codecs;

typedef struct c3zipx_lzma_info_tag {
    unsigned int sdk_major;
    unsigned int sdk_minor;
    unsigned int props_size;
    unsigned char props[C3ZIPX_LZMA_MAX_PROPS];
    c3dae_u32 payload_offset;
} c3zipx_lzma_info;

void c3zipx_archive_init(c3zipx_archive *a,
                         c3zipx_entry *entries,
                         c3dae_u32 entry_cap);

void c3zipx_codecs_init(c3zipx_codecs *c);

int c3zipx_read_directory(const unsigned char *zip,
                          c3dae_u32 zip_len,
                          c3zipx_archive *archive);

int c3zipx_find_entry(const c3zipx_archive *archive,
                      const char *path);

int c3zipx_find_first_dae(const c3zipx_archive *archive);

int c3zipx_extract_entry(const unsigned char *zip,
                         c3dae_u32 zip_len,
                         const c3zipx_entry *entry,
                         const char *password,
                         const c3zipx_codecs *codecs,
                         unsigned char *out,
                         c3dae_u32 out_cap,
                         unsigned char *work,
                         c3dae_u32 work_cap,
                         c3dae_u32 *out_len);

int c3zipx_extract_dae(const unsigned char *zae,
                       c3dae_u32 zae_len,
                       c3zipx_entry *entries,
                       c3dae_u32 entry_cap,
                       const char *password,
                       const c3zipx_codecs *codecs,
                       unsigned char *scratch,
                       c3dae_u32 scratch_cap,
                       unsigned char *work,
                       c3dae_u32 work_cap,
                       c3dae_u32 *dae_len,
                       char *dae_path,
                       unsigned int dae_path_cap);

int c3dae_load_zae_zipx_memory(c3dae_context *ctx,
                               const unsigned char *zae,
                               c3dae_u32 zae_len,
                               c3zipx_entry *entries,
                               c3dae_u32 entry_cap,
                               const char *password,
                               const c3zipx_codecs *codecs,
                               unsigned char *dae_scratch,
                               c3dae_u32 dae_scratch_cap,
                               unsigned char *work,
                               c3dae_u32 work_cap,
                               c3dae_model *out_model);

int c3zipx_lzma_method14_parse_header(const unsigned char *data,
                                      c3dae_u32 data_len,
                                      c3zipx_lzma_info *info);

int c3zipx_u64_fits_u32(c3zipx_u64 v);
c3dae_u32 c3zipx_u64_to_u32(c3zipx_u64 v);
const char *c3zipx_method_name(unsigned int method);
const char *c3zipx_error_string(int code);

#ifdef __cplusplus
}
#endif

#endif
