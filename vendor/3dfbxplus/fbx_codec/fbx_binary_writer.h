#ifndef FBX_BINARY_WRITER_H
#define FBX_BINARY_WRITER_H

#include <stddef.h>
#include "fbx_arrays.h"
#include "fbx_importer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FbxWriteBufferTag {
    fbx_u8 *data;
    size_t capacity;
    size_t size;
    int overflowed;
} FbxWriteBuffer;

typedef struct FbxBinaryWriteOptionsTag {
    int version;          /* e.g. 6100, 7400, 7500, 7700 */
    int emit_root_node;   /* 0: write children of synthetic Root; 1: write root itself */
    int compress_arrays;  /* 1: zlib-store encode uncompressed binary arrays when possible */
} FbxBinaryWriteOptions;

void       fbx_write_buffer_init(FbxWriteBuffer *buffer, void *memory, size_t capacity);
void       fbx_binary_write_options_default(FbxBinaryWriteOptions *options);
FBX_Result fbx_binary_write_tree(FbxWriteBuffer *buffer,
                                 const FbxNode *root,
                                 const FbxBinaryWriteOptions *options);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FBX_BINARY_WRITER_H */
