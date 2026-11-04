#include "fbx_binary_writer.h"
#include "fbx_mini_zlib.h"

#include <string.h>

#define FBX_BINARY_MAGIC_SIZE 23u

static const fbx_u8 fbx_binary_magic[FBX_BINARY_MAGIC_SIZE] = {
    'K','a','y','d','a','r','a',' ','F','B','X',' ','B','i','n','a','r','y',' ',' ',
    0x00u, 0x1au, 0x00u
};

void fbx_write_buffer_init(FbxWriteBuffer *buffer, void *memory, size_t capacity)
{
    if (!buffer) return;
    buffer->data = (fbx_u8*)memory;
    buffer->capacity = capacity;
    buffer->size = 0u;
    buffer->overflowed = 0;
}

void fbx_binary_write_options_default(FbxBinaryWriteOptions *options)
{
    if (!options) return;
    options->version = 7400;
    options->emit_root_node = 0;
    options->compress_arrays = 0;
}

static int fbx_w_reserve(FbxWriteBuffer *buffer, size_t bytes)
{
    if (!buffer || (!buffer->data && buffer->capacity != 0u)) return 0;
    if (bytes > buffer->capacity - buffer->size) {
        buffer->overflowed = 1;
        return 0;
    }
    return 1;
}

static int fbx_w_bytes(FbxWriteBuffer *buffer, const void *data, size_t bytes)
{
    if (!fbx_w_reserve(buffer, bytes)) return 0;
    if (bytes != 0u && data) memcpy(buffer->data + buffer->size, data, bytes);
    buffer->size += bytes;
    return 1;
}

static int fbx_w_u8(FbxWriteBuffer *buffer, fbx_u8 v)
{
    return fbx_w_bytes(buffer, &v, 1u);
}

static int fbx_w_u16(FbxWriteBuffer *buffer, fbx_u16 v)
{
    fbx_u8 b[2];
    b[0] = (fbx_u8)(v & 0xffu);
    b[1] = (fbx_u8)((v >> 8) & 0xffu);
    return fbx_w_bytes(buffer, b, 2u);
}

static int fbx_w_u32(FbxWriteBuffer *buffer, fbx_u32 v)
{
    fbx_u8 b[4];
    b[0] = (fbx_u8)(v & 0xffu);
    b[1] = (fbx_u8)((v >> 8) & 0xffu);
    b[2] = (fbx_u8)((v >> 16) & 0xffu);
    b[3] = (fbx_u8)((v >> 24) & 0xffu);
    return fbx_w_bytes(buffer, b, 4u);
}

static int fbx_w_u64(FbxWriteBuffer *buffer, fbx_u64 v)
{
    if (!fbx_w_u32(buffer, (fbx_u32)(v & (fbx_u64)0xffffffffu))) return 0;
    return fbx_w_u32(buffer, (fbx_u32)(v >> 32));
}

static void fbx_patch_u32(FbxWriteBuffer *buffer, size_t at, fbx_u32 v)
{
    if (!buffer || at + 4u > buffer->capacity) return;
    buffer->data[at + 0u] = (fbx_u8)(v & 0xffu);
    buffer->data[at + 1u] = (fbx_u8)((v >> 8) & 0xffu);
    buffer->data[at + 2u] = (fbx_u8)((v >> 16) & 0xffu);
    buffer->data[at + 3u] = (fbx_u8)((v >> 24) & 0xffu);
}

static void fbx_patch_u64(FbxWriteBuffer *buffer, size_t at, fbx_u64 v)
{
    fbx_patch_u32(buffer, at, (fbx_u32)(v & (fbx_u64)0xffffffffu));
    fbx_patch_u32(buffer, at + 4u, (fbx_u32)(v >> 32));
}

static int fbx_version_uses_64(int version)
{
    return version >= 7500;
}

static int fbx_write_null_record(FbxWriteBuffer *buffer, int version)
{
    int i;
    int bytes;
    bytes = fbx_version_uses_64(version) ? 25 : 13;
    for (i = 0; i < bytes; ++i) {
        if (!fbx_w_u8(buffer, 0u)) return 0;
    }
    return 1;
}

static size_t fbx_array_elem_size(char type_code)
{
    switch (type_code) {
        case 'f': return 4u;
        case 'd': return 8u;
        case 'l': return 8u;
        case 'i': return 4u;
        case 'b': return 1u;
        case 'c': return 1u;
        default: return 0u;
    }
}

static FBX_Result fbx_write_array_raw_or_compressed(FbxWriteBuffer *buffer,
                                                    const FbxProperty *prop,
                                                    char output_type,
                                                    const FbxBinaryWriteOptions *opt)
{
    fbx_u32 count;
    fbx_u32 byte_len;

    if (!buffer || !prop) return FBX_ERR_INVALID_ARGUMENT;
    if (prop->count < 0 || prop->byte_length < 0) return FBX_ERR_UNSUPPORTED_TYPE;
    count = (fbx_u32)prop->count;
    byte_len = (fbx_u32)prop->byte_length;

    if (!fbx_w_u8(buffer, (fbx_u8)output_type)) return FBX_ERR_OUT_OF_MEMORY;
    if (!fbx_w_u32(buffer, count)) return FBX_ERR_OUT_OF_MEMORY;

    if (prop->encoding == 1) {
        if (!fbx_w_u32(buffer, 1u)) return FBX_ERR_OUT_OF_MEMORY;
        if (!fbx_w_u32(buffer, byte_len)) return FBX_ERR_OUT_OF_MEMORY;
        if (byte_len != 0u && !prop->data) return FBX_ERR_INVALID_ARGUMENT;
        if (!fbx_w_bytes(buffer, prop->data, (size_t)byte_len)) return FBX_ERR_OUT_OF_MEMORY;
        return FBX_OK;
    }

    if (opt && opt->compress_arrays && byte_len != 0u && prop->data) {
        size_t len_patch;
        size_t written;
        int zr;
        if (!fbx_w_u32(buffer, 1u)) return FBX_ERR_OUT_OF_MEMORY;
        len_patch = buffer->size;
        if (!fbx_w_u32(buffer, 0u)) return FBX_ERR_OUT_OF_MEMORY;
        if (!fbx_w_reserve(buffer, fbx_mini_zlib_store_bound((size_t)byte_len))) return FBX_ERR_OUT_OF_MEMORY;
        written = 0u;
        zr = fbx_mini_zlib_store_encode((const fbx_u8*)prop->data,
                                        (size_t)byte_len,
                                        buffer->data + buffer->size,
                                        buffer->capacity - buffer->size,
                                        &written);
        if (zr != FBX_MINIZ_OK || written > (size_t)FBX_U32_MAX) return FBX_ERR_DECOMPRESSION_FAILED;
        buffer->size += written;
        fbx_patch_u32(buffer, len_patch, (fbx_u32)written);
        return FBX_OK;
    }

    if (!fbx_w_u32(buffer, 0u)) return FBX_ERR_OUT_OF_MEMORY;
    if (!fbx_w_u32(buffer, byte_len)) return FBX_ERR_OUT_OF_MEMORY;
    if (byte_len != 0u && !prop->data) return FBX_ERR_INVALID_ARGUMENT;
    if (!fbx_w_bytes(buffer, prop->data, (size_t)byte_len)) return FBX_ERR_OUT_OF_MEMORY;
    return FBX_OK;
}

static FBX_Result fbx_write_real_array64(FbxWriteBuffer *buffer, const FbxProperty *prop)
{
    fbx_u32 count;
    fbx_u32 i;
    const fbx_real *vals;

    if (!buffer || !prop) return FBX_ERR_INVALID_ARGUMENT;
    if (prop->count < 0) return FBX_ERR_UNSUPPORTED_TYPE;
    count = (fbx_u32)prop->count;
    vals = (const fbx_real*)prop->data;

    if (!fbx_w_u8(buffer, (fbx_u8)'d')) return FBX_ERR_OUT_OF_MEMORY;
    if (!fbx_w_u32(buffer, count)) return FBX_ERR_OUT_OF_MEMORY;
    if (!fbx_w_u32(buffer, 0u)) return FBX_ERR_OUT_OF_MEMORY;
    if (!fbx_w_u32(buffer, count * 8u)) return FBX_ERR_OUT_OF_MEMORY;
    if (count != 0u && !vals) return FBX_ERR_INVALID_ARGUMENT;
    for (i = 0u; i < count; ++i) {
        if (!fbx_w_u64(buffer, fbx_real_to_ieee64_bits(vals[i]))) return FBX_ERR_OUT_OF_MEMORY;
    }
    return FBX_OK;
}

static FBX_Result fbx_write_property(FbxWriteBuffer *buffer,
                                     const FbxProperty *prop,
                                     const FbxBinaryWriteOptions *opt)
{
    char tc;
    if (!buffer || !prop) return FBX_ERR_INVALID_ARGUMENT;
    tc = prop->type_code;

    if (tc == 'r' && prop->type == FBX_PROP_ARRAY_REAL) return fbx_write_real_array64(buffer, prop);
    if (tc == 'r' && prop->type == FBX_PROP_REAL) {
        const fbx_real *v;
        if (!prop->data) return FBX_ERR_INVALID_ARGUMENT;
        v = (const fbx_real*)prop->data;
        if (!fbx_w_u8(buffer, (fbx_u8)'D')) return FBX_ERR_OUT_OF_MEMORY;
        if (!fbx_w_u64(buffer, fbx_real_to_ieee64_bits(*v))) return FBX_ERR_OUT_OF_MEMORY;
        return FBX_OK;
    }

    if (tc == 'S' || tc == 'R') {
        if (prop->byte_length < 0) return FBX_ERR_UNSUPPORTED_TYPE;
        if (!fbx_w_u8(buffer, (fbx_u8)tc)) return FBX_ERR_OUT_OF_MEMORY;
        if (!fbx_w_u32(buffer, (fbx_u32)prop->byte_length)) return FBX_ERR_OUT_OF_MEMORY;
        if (prop->byte_length != 0 && !prop->data) return FBX_ERR_INVALID_ARGUMENT;
        if (!fbx_w_bytes(buffer, prop->data, (size_t)prop->byte_length)) return FBX_ERR_OUT_OF_MEMORY;
        return FBX_OK;
    }
    if (tc == 'Y') {
        fbx_i16 v;
        if (!prop->data) return FBX_ERR_INVALID_ARGUMENT;
        memcpy(&v, prop->data, sizeof(v));
        if (!fbx_w_u8(buffer, (fbx_u8)'Y')) return FBX_ERR_OUT_OF_MEMORY;
        if (!fbx_w_u16(buffer, (fbx_u16)v)) return FBX_ERR_OUT_OF_MEMORY;
        return FBX_OK;
    }
    if (tc == 'C') {
        fbx_u8 v2;
        if (!prop->data) return FBX_ERR_INVALID_ARGUMENT;
        v2 = ((const fbx_u8*)prop->data)[0];
        if (!fbx_w_u8(buffer, (fbx_u8)'C')) return FBX_ERR_OUT_OF_MEMORY;
        if (!fbx_w_u8(buffer, v2)) return FBX_ERR_OUT_OF_MEMORY;
        return FBX_OK;
    }
    if (tc == 'I') {
        fbx_i32 v3;
        if (!prop->data) return FBX_ERR_INVALID_ARGUMENT;
        memcpy(&v3, prop->data, sizeof(v3));
        if (!fbx_w_u8(buffer, (fbx_u8)'I')) return FBX_ERR_OUT_OF_MEMORY;
        if (!fbx_w_u32(buffer, (fbx_u32)v3)) return FBX_ERR_OUT_OF_MEMORY;
        return FBX_OK;
    }
    if (tc == 'L') {
        fbx_i64 v4;
        if (!prop->data) return FBX_ERR_INVALID_ARGUMENT;
        memcpy(&v4, prop->data, sizeof(v4));
        if (!fbx_w_u8(buffer, (fbx_u8)'L')) return FBX_ERR_OUT_OF_MEMORY;
        if (!fbx_w_u64(buffer, (fbx_u64)v4)) return FBX_ERR_OUT_OF_MEMORY;
        return FBX_OK;
    }
    if (tc == 'F' || tc == 'D') {
        const fbx_real *rv;
        if (!prop->data) return FBX_ERR_INVALID_ARGUMENT;
        rv = (const fbx_real*)prop->data;
        if (tc == 'F') {
            if (!fbx_w_u8(buffer, (fbx_u8)'F')) return FBX_ERR_OUT_OF_MEMORY;
            if (!fbx_w_u32(buffer, fbx_real_to_ieee32_bits(*rv))) return FBX_ERR_OUT_OF_MEMORY;
        } else {
            if (!fbx_w_u8(buffer, (fbx_u8)'D')) return FBX_ERR_OUT_OF_MEMORY;
            if (!fbx_w_u64(buffer, fbx_real_to_ieee64_bits(*rv))) return FBX_ERR_OUT_OF_MEMORY;
        }
        return FBX_OK;
    }
    if (fbx_array_elem_size(tc) != 0u) return fbx_write_array_raw_or_compressed(buffer, prop, tc, opt);
    return FBX_ERR_UNSUPPORTED_TYPE;
}

static FBX_Result fbx_write_node(FbxWriteBuffer *buffer,
                                 const FbxNode *node,
                                 const FbxBinaryWriteOptions *opt)
{
    size_t start;
    size_t end_patch;
    size_t props_len_patch;
    size_t props_start;
    size_t props_end;
    size_t name_len;
    int i;
    int version;
    int use64;

    if (!buffer || !node || !node->name) return FBX_ERR_INVALID_ARGUMENT;
    version = opt ? opt->version : 7400;
    use64 = fbx_version_uses_64(version);
    name_len = strlen(node->name);
    if (name_len > 255u || node->property_count < 0 || node->child_count < 0) return FBX_ERR_UNSUPPORTED_TYPE;

    start = buffer->size;
    end_patch = start;
    if (use64) {
        if (!fbx_w_u64(buffer, 0u)) return FBX_ERR_OUT_OF_MEMORY;
        if (!fbx_w_u64(buffer, (fbx_u64)node->property_count)) return FBX_ERR_OUT_OF_MEMORY;
        props_len_patch = buffer->size;
        if (!fbx_w_u64(buffer, 0u)) return FBX_ERR_OUT_OF_MEMORY;
    } else {
        if (!fbx_w_u32(buffer, 0u)) return FBX_ERR_OUT_OF_MEMORY;
        if (!fbx_w_u32(buffer, (fbx_u32)node->property_count)) return FBX_ERR_OUT_OF_MEMORY;
        props_len_patch = buffer->size;
        if (!fbx_w_u32(buffer, 0u)) return FBX_ERR_OUT_OF_MEMORY;
    }
    if (!fbx_w_u8(buffer, (fbx_u8)name_len)) return FBX_ERR_OUT_OF_MEMORY;
    if (!fbx_w_bytes(buffer, node->name, name_len)) return FBX_ERR_OUT_OF_MEMORY;

    props_start = buffer->size;
    for (i = 0; i < node->property_count; ++i) {
        FBX_Result r;
        r = fbx_write_property(buffer, &node->properties[i], opt);
        if (r != FBX_OK) return r;
    }
    props_end = buffer->size;

    for (i = 0; i < node->child_count; ++i) {
        FBX_Result r2;
        r2 = fbx_write_node(buffer, node->children[i], opt);
        if (r2 != FBX_OK) return r2;
    }
    if (node->child_count > 0) {
        if (!fbx_write_null_record(buffer, version)) return FBX_ERR_OUT_OF_MEMORY;
    }

    if (use64) {
        fbx_patch_u64(buffer, end_patch, (fbx_u64)buffer->size);
        fbx_patch_u64(buffer, props_len_patch, (fbx_u64)(props_end - props_start));
    } else {
        if (buffer->size > (size_t)FBX_U32_MAX || props_end - props_start > (size_t)FBX_U32_MAX) return FBX_ERR_UNSUPPORTED_TYPE;
        fbx_patch_u32(buffer, end_patch, (fbx_u32)buffer->size);
        fbx_patch_u32(buffer, props_len_patch, (fbx_u32)(props_end - props_start));
    }
    return FBX_OK;
}

FBX_Result fbx_binary_write_tree(FbxWriteBuffer *buffer,
                                 const FbxNode *root,
                                 const FbxBinaryWriteOptions *options)
{
    FbxBinaryWriteOptions local;
    const FbxBinaryWriteOptions *opt;
    int i;

    if (!buffer || !root) return FBX_ERR_INVALID_ARGUMENT;
    if (options) opt = options;
    else {
        fbx_binary_write_options_default(&local);
        opt = &local;
    }
    if (opt->version < 5000 || opt->version > 7999) return FBX_ERR_UNSUPPORTED_TYPE;
    if (!fbx_w_bytes(buffer, fbx_binary_magic, FBX_BINARY_MAGIC_SIZE)) return FBX_ERR_OUT_OF_MEMORY;
    if (!fbx_w_u32(buffer, (fbx_u32)opt->version)) return FBX_ERR_OUT_OF_MEMORY;

    if (!opt->emit_root_node && root->name && strcmp(root->name, "Root") == 0) {
        for (i = 0; i < root->child_count; ++i) {
            FBX_Result r;
            r = fbx_write_node(buffer, root->children[i], opt);
            if (r != FBX_OK) return r;
        }
    } else {
        FBX_Result r2;
        r2 = fbx_write_node(buffer, root, opt);
        if (r2 != FBX_OK) return r2;
    }
    if (!fbx_write_null_record(buffer, opt->version)) return FBX_ERR_OUT_OF_MEMORY;
    return buffer->overflowed ? FBX_ERR_OUT_OF_MEMORY : FBX_OK;
}
