#ifndef FBX_CODEC_WRITER_H
#define FBX_CODEC_WRITER_H

#include <stddef.h>
#include "fbx_importer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FbxCodecWriteStreamTag {
    void *user;
    int (*write)(void *user, const void *data, size_t size);
} FbxCodecWriteStream;

typedef struct FbxCodecWriteOptionsTag {
    int version;
    int emit_header_comment;
} FbxCodecWriteOptions;

void fbx_codec_write_options_default(FbxCodecWriteOptions *opt);
int  fbx_codec_write_ascii(const FbxNode *root,
                           const FbxCodecWriteStream *stream,
                           const FbxCodecWriteOptions *options);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FBX_CODEC_WRITER_H */
