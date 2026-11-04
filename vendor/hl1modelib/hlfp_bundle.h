#ifndef HLFP_BUNDLE_H
#define HLFP_BUNDLE_H

#include "hlfp_goldsrc.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Portable GoldSrc MDL bundle loader. No filesystem calls or allocation.
 * A host/engine provides buffers and a file reader callback.
 *
 * Reader contract: return HLFP_OK only when the complete file has been
 * written and *out_size is set to its actual byte count. Other return codes
 * (including missing files) are errors. The buffers must stay alive for as
 * long as the parsed hlfp_mdl is in use.
 */
typedef int (*hlfp_bundle_read_fn)(void *user,
                                   const char *path,
                                   hlfp_u8 *dst,
                                   size_t capacity,
                                   size_t *out_size);

typedef struct hlfp_bundle_buffers_s {
    hlfp_u8 *main_bytes;
    size_t main_capacity;
    hlfp_u8 *texture_bytes;
    size_t texture_capacity;
    hlfp_u8 *sequence_pool;
    size_t sequence_capacity;
} hlfp_bundle_buffers;

typedef struct hlfp_bundle_stats_s {
    size_t main_bytes;
    size_t texture_bytes;
    size_t sequence_bytes;
    int has_external_textures;
    int sequence_files_loaded;
} hlfp_bundle_stats;

int hlfp_bundle_load(const char *path,
                     hlfp_bundle_read_fn read_file,
                     void *read_user,
                     const hlfp_bundle_buffers *buffers,
                     hlfp_mdl *out_mdl,
                     hlfp_bundle_stats *stats,
                     hlfp_error *err);

#ifdef __cplusplus
}
#endif

#endif
