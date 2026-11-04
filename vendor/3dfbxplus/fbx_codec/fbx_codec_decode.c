#include "fbx_codec_decode.h"

#include <string.h>

void fbx_codec_decode_result_init(FbxCodecDecodeResult *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
}

int fbx_codec_decode_memory(const void *data,
                            fbx_i64 size,
                            int build_scene,
                            FbxCodecDecodeResult *out)
{
    FbxContext *ctx;
    FbxNode *root;
    FbxScene *scene;

    if (!out) return 0;
    fbx_codec_decode_result_init(out);

    ctx = fbx_open_memory(data, size);
    if (!ctx) {
        out->error_code = FBX_ERROR_INVALID_ARGUMENT;
        out->error_message = "fbx_open_memory failed";
        return 0;
    }

    root = fbx_read_raw_tree(ctx);
    if (!root) {
        out->ctx = ctx;
        out->error_code = fbx_last_error_code(ctx);
        out->error_message = fbx_last_error(ctx);
        fbx_codec_decode_close(out);
        return 0;
    }

    scene = NULL;
    if (build_scene) {
        scene = fbx_scene_build(root);
        if (!scene) {
            out->ctx = ctx;
            out->root = root;
            out->error_code = FBX_ERROR_PARSE;
            out->error_message = "fbx_scene_build failed";
            fbx_codec_decode_close(out);
            return 0;
        }
    }

    out->ctx = ctx;
    out->root = root;
    out->scene = scene;
    out->is_binary = ctx->is_binary;
    out->version = ctx->version;
    out->error_code = FBX_ERROR_OK;
    out->error_message = "OK";
    return 1;
}

void fbx_codec_decode_close(FbxCodecDecodeResult *result)
{
    if (!result) return;
    if (result->scene) {
        fbx_scene_destroy(result->scene);
        result->scene = NULL;
    }
    if (result->ctx) {
        fbx_close(result->ctx, result->root);
    }
    result->ctx = NULL;
    result->root = NULL;
    result->is_binary = 0;
    result->version = 0;
}
