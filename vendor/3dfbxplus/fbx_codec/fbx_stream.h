#ifndef FBX_STREAM_H
#define FBX_STREAM_H

#include "fbx_codec_decode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FbxCodecStreamSourceTag {
    void *user;
    FbxStreamReadFn read;
    fbx_i64 known_size;
} FbxCodecStreamSource;

/* Full decoder entry point from a read-only incremental stream.
 * This builds the raw tree/scene and is useful for pipes or embedded VFS.
 */
int fbx_codec_decode_stream(const FbxCodecStreamSource *source,
                            int build_scene,
                            FbxCodecDecodeResult *out);

/* Lightweight event scanner for binary FBX.
 * It never materializes the FBX tree and it never seeks.  It validates the
 * binary node/property structure and reports node/property events while
 * reading forward only.  Array payloads are skipped as bytes, so callers can
 * use this for preflight, indexing, metering, and streaming diagnostics.
 */
typedef struct FbxStreamPropertyInfoTag {
    char type_code;
    fbx_i64 count;
    fbx_i64 byte_length;
    int encoding;
} FbxStreamPropertyInfo;

typedef struct FbxStreamStatsTag {
    int is_binary;
    int version;
    fbx_i64 nodes;
    fbx_i64 properties;
    fbx_i64 arrays;
    fbx_i64 compressed_arrays;
    fbx_i64 bytes_read;
    int max_depth;
} FbxStreamStats;

typedef int (*FbxStreamBeginNodeFn)(void *user,
                                   const char *name,
                                   int depth,
                                   fbx_i64 end_offset,
                                   fbx_i64 property_count,
                                   fbx_i64 property_bytes);
typedef int (*FbxStreamPropertyFn)(void *user,
                                   const char *node_name,
                                   int depth,
                                   const FbxStreamPropertyInfo *prop);
typedef int (*FbxStreamEndNodeFn)(void *user,
                                 const char *name,
                                 int depth,
                                 fbx_i64 end_offset);

typedef struct FbxStreamEventsTag {
    void *user;
    FbxStreamBeginNodeFn begin_node;
    FbxStreamPropertyFn property;
    FbxStreamEndNodeFn end_node;
} FbxStreamEvents;

int fbx_stream_scan_binary(const FbxCodecStreamSource *source,
                           const FbxStreamEvents *events,
                           FbxStreamStats *stats);

#ifdef __cplusplus
}
#endif

#endif /* FBX_STREAM_H */
