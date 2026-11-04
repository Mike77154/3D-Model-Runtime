#include "fbx_arrays.h"
#include "fbx_alloc.h"
#ifndef FBX_NO_ZLIB
#include "fbx_mini_zlib.h"
#endif

#include <string.h>

/* FBX binary stores arrays little-endian.
   If you target big-endian platforms, we byteswap on read. */
static int fbx_is_little_endian(void) {
    unsigned int x = 1u;
    return *((const unsigned char*)&x) == 1u;
}

static fbx_u32 fbx_bswap32(fbx_u32 v) {
    return ((v & 0x000000FFu) << 24) |
           ((v & 0x0000FF00u) << 8)  |
           ((v & 0x00FF0000u) >> 8)  |
           ((v & 0xFF000000u) >> 24);
}

static fbx_u64 fbx_bswap64(fbx_u64 v) {
    /* Avoid 64-bit integer literal suffixes for maximal C89 compiler compatibility. */
    return ((v & ((fbx_u64)0xFFu        )) << 56) |
           ((v & ((fbx_u64)0xFFu <<  8 )) << 40) |
           ((v & ((fbx_u64)0xFFu << 16 )) << 24) |
           ((v & ((fbx_u64)0xFFu << 24 )) <<  8) |
           ((v & ((fbx_u64)0xFFu << 32 )) >>  8) |
           ((v & ((fbx_u64)0xFFu << 40 )) >> 24) |
           ((v & ((fbx_u64)0xFFu << 48 )) >> 40) |
           ((v & ((fbx_u64)0xFFu << 56 )) >> 56);
}


/* Devuelve un buffer con los bytes del array (descomprimido si hace falta).
   - out_buf: apunta a un buffer usable
   - out_size: tamaño del buffer
   - out_needs_release: 1 si el caller debe fbx_release(out_buf), 0 si apunta a prop->data
*/
static FBX_Result fbx_get_array_bytes(const FbxProperty *prop,
                                      fbx_u32 expected_bytes,
                                      unsigned char **out_buf,
                                      fbx_u32 *out_size,
                                      int *out_needs_release)
{
    if (!prop || !out_buf || !out_size || !out_needs_release) {
        return FBX_ERR_INVALID_ARGUMENT;
    }

    *out_buf = NULL;
    *out_size = 0;
    *out_needs_release = 0;

    if (prop->encoding == 0) {
        if (!prop->data) {
            if (expected_bytes == 0u) return FBX_OK;
            return FBX_ERR_DECOMPRESSION_FAILED;
        }
        if ((fbx_u32)prop->byte_length < expected_bytes) {
            return FBX_ERR_DECOMPRESSION_FAILED;
        }
        *out_buf = (unsigned char*)prop->data;
        *out_size = (fbx_u32)prop->byte_length;
        *out_needs_release = 0;
        return FBX_OK;
    }

    if (prop->encoding != 1) {
        return FBX_ERR_UNSUPPORTED_TYPE;
    }

#if defined(FBX_NO_ZLIB)
    (void)expected_bytes;
    return FBX_ERR_UNSUPPORTED_TYPE;
#else
    {
        unsigned char *dest;
        size_t dest_len;
        int zerr;

        if (expected_bytes == 0u) {
            *out_buf = NULL;
            *out_size = 0u;
            *out_needs_release = 0;
            return FBX_OK;
        }
        if (!prop->data || prop->byte_length <= 0) return FBX_ERR_DECOMPRESSION_FAILED;

        dest = (unsigned char*)fbx_alloc(expected_bytes);
        if (!dest) return FBX_ERR_OUT_OF_MEMORY;

        dest_len = 0u;
        zerr = fbx_mini_zlib_decode((const fbx_u8*)prop->data,
                                    (size_t)prop->byte_length,
                                    (fbx_u8*)dest,
                                    (size_t)expected_bytes,
                                    &dest_len);
        if (zerr != FBX_MINIZ_OK) {
            fbx_release(dest);
            return FBX_ERR_DECOMPRESSION_FAILED;
        }

        if (dest_len < (size_t)expected_bytes || dest_len > (size_t)FBX_U32_MAX) {
            fbx_release(dest);
            return FBX_ERR_DECOMPRESSION_FAILED;
        }

        *out_buf = dest;
        *out_size = (fbx_u32)dest_len;
        *out_needs_release = 1;
        return FBX_OK;
    }
#endif
}

/* ---- int32[] (accepts 'i' or 'l') ---- */
FBX_Result fbx_property_as_int32_array(const FbxProperty *prop,
                                       fbx_i32 **out_data,
                                       fbx_u32 *out_count)
{
    fbx_u32 count;
    fbx_u32 elem_size;
    fbx_u32 expected_bytes;
    unsigned char *buf;
    fbx_u32 buf_size;
    int needs_release;
    FBX_Result r;
    fbx_i32 *arr;
    fbx_u32 i;
    int little;

    if (!prop || !out_data || !out_count) return FBX_ERR_INVALID_ARGUMENT;
    *out_data = NULL;
    *out_count = 0;

    if (prop->count < 0) return FBX_ERR_DECOMPRESSION_FAILED;
    count = (fbx_u32)prop->count;

    if (prop->type_code == 'i') {
        elem_size = 4u;
    } else if (prop->type_code == 'l') {
        elem_size = 8u;
    } else {
        return FBX_ERR_UNSUPPORTED_TYPE;
    }

    if (count != 0u && elem_size > FBX_U32_MAX / count) return FBX_ERR_DECOMPRESSION_FAILED;
    expected_bytes = count * elem_size;

    r = fbx_get_array_bytes(prop, expected_bytes, &buf, &buf_size, &needs_release);
    if (r != FBX_OK) return r;

    arr = (fbx_i32*)fbx_alloc((size_t)count * sizeof(fbx_i32));
    if (!arr) {
        if (needs_release) fbx_release(buf);
        return FBX_ERR_OUT_OF_MEMORY;
    }

    little = fbx_is_little_endian();

    if (elem_size == 4u) {
        for (i = 0; i < count; ++i) {
            fbx_u32 u;
            memcpy(&u, buf + (size_t)i * 4u, 4u);
            if (!little) u = fbx_bswap32(u);
            arr[i] = (fbx_i32)u;
        }
    } else {
        for (i = 0; i < count; ++i) {
            fbx_u64 u;
            fbx_i64 v;
            memcpy(&u, buf + (size_t)i * 8u, 8u);
            if (!little) u = fbx_bswap64(u);
            v = (fbx_i64)u;
            if (v < (fbx_i64)(-2147483647l - 1l)) v = (fbx_i64)(-2147483647l - 1l);
            if (v > (fbx_i64)2147483647l) v = (fbx_i64)2147483647l;
            arr[i] = (fbx_i32)v;
        }
    }

    if (needs_release) fbx_release(buf);

    *out_data = arr;
    *out_count = count;
    return FBX_OK;
}

/* ---- int64[] (accepts 'l' or 'i') ---- */
FBX_Result fbx_property_as_int64_array(const FbxProperty *prop,
                                       fbx_i64 **out_data,
                                       fbx_u32 *out_count)
{
    fbx_u32 count;
    fbx_u32 elem_size;
    fbx_u32 expected_bytes;
    unsigned char *buf;
    fbx_u32 buf_size;
    int needs_release;
    FBX_Result r;
    fbx_i64 *arr;
    fbx_u32 i;
    int little;

    if (!prop || !out_data || !out_count) return FBX_ERR_INVALID_ARGUMENT;
    *out_data = NULL;
    *out_count = 0;

    if (prop->count < 0) return FBX_ERR_DECOMPRESSION_FAILED;
    count = (fbx_u32)prop->count;

    if (prop->type_code == 'l') {
        elem_size = 8u;
    } else if (prop->type_code == 'i') {
        elem_size = 4u;
    } else {
        return FBX_ERR_UNSUPPORTED_TYPE;
    }

    if (count != 0u && elem_size > FBX_U32_MAX / count) return FBX_ERR_DECOMPRESSION_FAILED;
    expected_bytes = count * elem_size;

    r = fbx_get_array_bytes(prop, expected_bytes, &buf, &buf_size, &needs_release);
    if (r != FBX_OK) return r;

    arr = (fbx_i64*)fbx_alloc((size_t)count * sizeof(fbx_i64));
    if (!arr) {
        if (needs_release) fbx_release(buf);
        return FBX_ERR_OUT_OF_MEMORY;
    }

    little = fbx_is_little_endian();

    if (elem_size == 8u) {
        for (i = 0; i < count; ++i) {
            fbx_u64 u;
            memcpy(&u, buf + (size_t)i * 8u, 8u);
            if (!little) u = fbx_bswap64(u);
            arr[i] = (fbx_i64)u;
        }
    } else {
        for (i = 0; i < count; ++i) {
            fbx_u32 u;
            memcpy(&u, buf + (size_t)i * 4u, 4u);
            if (!little) u = fbx_bswap32(u);
            arr[i] = (fbx_i64)(fbx_i32)u;
        }
    }

    if (needs_release) fbx_release(buf);

    *out_data = arr;
    *out_count = count;
    return FBX_OK;
}

/* ---- fbx_real[] (accepts binary 'f'/'d' and ASCII/runtime 'r') ---- */
FBX_Result fbx_property_as_real_array(const FbxProperty *prop,
                                      fbx_real **out_data,
                                      fbx_u32 *out_count)
{
    fbx_u32 count;
    fbx_u32 elem_size;
    fbx_u32 expected_bytes;
    unsigned char *buf;
    fbx_u32 buf_size;
    int needs_release;
    FBX_Result r;
    fbx_real *arr;
    fbx_u32 i;
    int little;

    if (!prop || !out_data || !out_count) return FBX_ERR_INVALID_ARGUMENT;
    *out_data = NULL;
    *out_count = 0;

    if (prop->count < 0) return FBX_ERR_DECOMPRESSION_FAILED;
    count = (fbx_u32)prop->count;

    if (prop->type_code == 'r') {
        if (count != 0u && prop->byte_length < (fbx_i32)(count * (fbx_u32)sizeof(fbx_real))) {
            return FBX_ERR_DECOMPRESSION_FAILED;
        }
        arr = (fbx_real*)fbx_alloc((size_t)count * sizeof(fbx_real));
        if (!arr) return FBX_ERR_OUT_OF_MEMORY;
        if (count > 0u) memcpy(arr, prop->data, (size_t)count * sizeof(fbx_real));
        *out_data = arr;
        *out_count = count;
        return FBX_OK;
    }

    if (prop->type_code == 'f') {
        elem_size = 4u;
    } else if (prop->type_code == 'd') {
        elem_size = 8u;
    } else {
        return FBX_ERR_UNSUPPORTED_TYPE;
    }

    if (count != 0u && elem_size > FBX_U32_MAX / count) return FBX_ERR_DECOMPRESSION_FAILED;
    expected_bytes = count * elem_size;

    r = fbx_get_array_bytes(prop, expected_bytes, &buf, &buf_size, &needs_release);
    if (r != FBX_OK) return r;

    arr = (fbx_real*)fbx_alloc((size_t)count * sizeof(fbx_real));
    if (!arr) {
        if (needs_release) fbx_release(buf);
        return FBX_ERR_OUT_OF_MEMORY;
    }

    little = fbx_is_little_endian();

    if (elem_size == 4u) {
        for (i = 0; i < count; ++i) {
            fbx_u32 u;
            memcpy(&u, buf + (size_t)i * 4u, 4u);
            if (!little) u = fbx_bswap32(u);
            arr[i] = fbx_real_from_ieee32_bits(u);
        }
    } else {
        for (i = 0; i < count; ++i) {
            fbx_u64 u;
            memcpy(&u, buf + (size_t)i * 8u, 8u);
            if (!little) u = fbx_bswap64(u);
            arr[i] = fbx_real_from_ieee64_bits(u);
        }
    }

    if (needs_release) fbx_release(buf);

    *out_data = arr;
    *out_count = count;
    return FBX_OK;
}

FBX_Result fbx_property_as_real64_array(const FbxProperty *prop,
                                        fbx_real **out_data,
                                        fbx_u32 *out_count)
{
    return fbx_property_as_real_array(prop, out_data, out_count);
}

FBX_Result fbx_property_as_vec3_array(const FbxProperty *prop,
                                       fbx_real **out_xyz,
                                       fbx_u32 *out_count)
{
    fbx_real *fbx_reals;
    fbx_u32 count;
    FBX_Result r;

    if (!prop || !out_xyz || !out_count) return FBX_ERR_INVALID_ARGUMENT;
    *out_xyz = NULL;
    *out_count = 0;

    fbx_reals = NULL;
    count = 0;

    /* Reutilizamos el reader genérico de fbx_real[] (acepta f/d) */
    r = fbx_property_as_real_array(prop, &fbx_reals, &count);
    if (r != FBX_OK) return r;

    if ((count % 3u) != 0u) {
        fbx_release(fbx_reals);
        return FBX_ERR_DECOMPRESSION_FAILED;
    }

    *out_xyz = fbx_reals;
    *out_count = count / 3u;
    return FBX_OK;
}

void fbx_arrays_release(void *ptr) {
    fbx_release(ptr);
}
