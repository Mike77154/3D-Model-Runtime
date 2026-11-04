/* fbx_importer.h */
#ifndef FBX_IMPORTER_H
#define FBX_IMPORTER_H

#include <stddef.h>
#ifndef FBX_NO_STDIO
#include <stdio.h>
#endif

#include "fbx_types.h"
#include "fbx_fixed.h"
#include "fbx_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FbxErrorCodeTag {
    FBX_ERROR_OK = 0,
    FBX_ERROR_INVALID_ARGUMENT = 1,
    FBX_ERROR_OUT_OF_MEMORY = 2,
    FBX_ERROR_IO = 3,
    FBX_ERROR_PARSE = 4,
    FBX_ERROR_UNSUPPORTED = 5,
    FBX_ERROR_LIMIT = 6,
    FBX_ERROR_DECOMPRESSION = 7,
    FBX_ERROR_STREAM = 8
} FbxErrorCode;

typedef struct FbxLimitsTag {
    int     max_depth;
    int     max_node_count;
    int     max_properties_per_node;
    int     max_children_per_node;
    fbx_u32 max_array_elements;
    fbx_u32 max_array_bytes;
    fbx_u32 max_vertices;
    fbx_u32 max_indices;
    int     max_bones;
    int     max_weights_per_vertex;
} FbxLimits;

/* Categoría lógica de propiedad (no es el type_code crudo de FBX) */
typedef enum FbxPropertyTypeTag {
    FBX_PROP_INT = 0,
    FBX_PROP_REAL,
    FBX_PROP_STRING,
    FBX_PROP_RAW,
    FBX_PROP_ARRAY_INT,
    FBX_PROP_ARRAY_REAL,
    FBX_PROP_UNKNOWN
} FbxPropertyType;

/* Propiedad FBX genérica.
 *
 * Campos relevantes:
 *  - type_code   → 'I','L','F','D','S','R','i','l','f','d', etc.
 *  - encoding    → 0 sin comprimir, 1 zlib (para arrays)
 *  - count       → nº de elementos (para arrays) o 1 para escalares
 *  - byte_length → longitud en bytes del payload (arrays/string/raw)
 *  - data        → buffer con los datos crudos (arena del importer)
 */
typedef struct FbxPropertyTag {
    char            type_code;
    FbxPropertyType type;
    fbx_i32         encoding;
    fbx_i32         count;
    fbx_i32         byte_length;
    void           *data;
} FbxProperty;

/* Nodo genérico del árbol FBX */
typedef struct FbxNodeTag {
    char              *name;
    int                property_count;
    FbxProperty       *properties;

    int                child_count;
    int                child_capacity;
    struct FbxNodeTag **children;
} FbxNode;

/* Contexto de lectura */
typedef enum FbxSourceKindTag {
    FBX_SOURCE_NONE = 0,
    FBX_SOURCE_FILE = 1,
    FBX_SOURCE_MEMORY = 2,
    FBX_SOURCE_STREAM = 3
} FbxSourceKind;

typedef size_t  (*FbxStreamReadFn)(void *user, void *dst, size_t size);
typedef int     (*FbxStreamSeekFn)(void *user, fbx_i64 offset, int origin);
typedef fbx_i64 (*FbxStreamTellFn)(void *user);
typedef fbx_i64 (*FbxStreamSizeFn)(void *user);

typedef struct FbxStreamCallbacksTag {
    FbxStreamReadFn read;
    FbxStreamSeekFn seek;
    FbxStreamTellFn tell;
    FbxStreamSizeFn size;
} FbxStreamCallbacks;

#define FBX_SEEK_SET 0
#define FBX_SEEK_CUR 1
#define FBX_SEEK_END 2

typedef struct FbxContextTag {
#ifndef FBX_NO_STDIO
    FILE   *fp;
#endif
    const fbx_u8 *mem;
    fbx_i64 mem_size;
    fbx_i64 mem_pos;

    void *stream_user;
    FbxStreamCallbacks stream;
    fbx_i64 stream_pos;
    fbx_i64 stream_size;
    fbx_u8 stream_prefix[32];
    size_t stream_prefix_size;
    size_t stream_prefix_pos;

    FbxSourceKind source_kind;
    int     is_binary;
    int     binary_big_endian;
    int     version;        /* version_raw (ej. 7400, 7500...). 0 si ASCII */
    fbx_i64 file_size;      /* bytes, -1 si desconocido */
    int     max_depth;      /* límite de recursión (protección) */
    int     node_count;
    FbxLimits limits;

    int     last_error;     /* 0 si OK */
    char    last_error_msg[256];
} FbxContext;

/* API */
FbxContext *fbx_open(const char *path);
FbxContext *fbx_open_memory(const void *data, fbx_i64 size);
FbxContext *fbx_open_stream(const FbxStreamCallbacks *callbacks, void *user);
FbxContext *fbx_open_stream_read(FbxStreamReadFn read_fn, void *user, fbx_i64 known_size);
void        fbx_close(FbxContext *ctx, FbxNode *root);

/* Lee la escena completa y devuelve el root del árbol. */
FbxNode    *fbx_read_scene(FbxContext *ctx);

/* Helpers */
void        fbx_print_tree(const FbxNode *node, int depth);
void        fbx_node_destroy(FbxNode *node);
const char *fbx_last_error(const FbxContext *ctx);

void        fbx_limits_default(FbxLimits *limits);
void        fbx_get_default_limits(FbxLimits *out_limits);
void        fbx_context_set_limits(FbxContext *ctx, const FbxLimits *limits);
int         fbx_last_error_code(const FbxContext *ctx);
const char *fbx_error_code_string(int code);
FbxNode    *fbx_read_raw_tree(FbxContext *ctx);

/* Low-level source helpers used by the parsers; they work with either a file
   source or a caller-owned memory slice. */
int         fbx_context_read(FbxContext *ctx, void *dst, size_t size);
int         fbx_context_getc(FbxContext *ctx);
int         fbx_context_seek(FbxContext *ctx, fbx_i64 offset, int origin);
fbx_i64     fbx_context_tell(FbxContext *ctx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FBX_IMPORTER_H */
