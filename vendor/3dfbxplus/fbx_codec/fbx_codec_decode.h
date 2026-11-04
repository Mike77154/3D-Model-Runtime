#ifndef FBX_CODEC_DECODE_H
#define FBX_CODEC_DECODE_H

#include "fbx_importer.h"
#include "fbx_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FbxCodecDecodeResultTag {
    FbxContext *ctx;
    FbxNode    *root;
    FbxScene   *scene;
    int         is_binary;
    int         version;
    int         error_code;
    const char *error_message;
} FbxCodecDecodeResult;

void fbx_codec_decode_result_init(FbxCodecDecodeResult *out);
int  fbx_codec_decode_memory(const void *data,
                             fbx_i64 size,
                             int build_scene,
                             FbxCodecDecodeResult *out);
void fbx_codec_decode_close(FbxCodecDecodeResult *result);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FBX_CODEC_DECODE_H */
