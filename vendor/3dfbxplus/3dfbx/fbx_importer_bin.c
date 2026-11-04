/* fbx_importer_bin.c */
#include "fbx_importer_bin.h"
#include "fbx_alloc.h"

#include <string.h>
#include <limits.h>

/* ----------------- Local error helper ----------------- */

static void fbx_set_error(FbxContext *ctx, int code, const char *msg)
{
    size_t n;

    if (!ctx) return;

    ctx->last_error = code;

    if (!msg) {
        ctx->last_error_msg[0] = '\0';
        return;
    }

    n = strlen(msg);
    if (n >= sizeof(ctx->last_error_msg)) {
        n = sizeof(ctx->last_error_msg) - 1;
    }
    memcpy(ctx->last_error_msg, msg, n);
    ctx->last_error_msg[n] = '\0';
}

/* Store a non-fatal message (warning/info). We keep last_error == 0 so
   callers that only check success via return values are not disrupted.
   fbx_last_error() can still surface last_error_msg.
*/
static void fbx_set_warning(FbxContext *ctx, const char *msg)
{
    size_t n;

    if (!ctx) return;

    /* Keep last_error at 0 (non-fatal). */
    ctx->last_error = 0;

    if (!msg) {
        ctx->last_error_msg[0] = '\0';
        return;
    }

    n = strlen(msg);
    if (n >= sizeof(ctx->last_error_msg)) {
        n = sizeof(ctx->last_error_msg) - 1;
    }
    memcpy(ctx->last_error_msg, msg, n);
    ctx->last_error_msg[n] = '\0';
}

/* ----------------- Low-level reads ----------------- */

static int fbx_read_bytes(FbxContext *ctx, void *dst, size_t size)
{
    return fbx_context_read(ctx, dst, size);
}

static int fbx_read_u8(FbxContext *ctx, fbx_u8 *out)
{
    fbx_u8 b;
    if (!fbx_read_bytes(ctx, &b, 1)) return 0;
    *out = b;
    return 1;
}

static int fbx_read_u16(FbxContext *ctx, fbx_u16 *out)
{
    fbx_u8 buf[2];
    if (!fbx_read_bytes(ctx, buf, 2)) return 0;
    if (ctx && ctx->binary_big_endian) {
        *out = (fbx_u16)(((fbx_u16)buf[0] << 8) | (fbx_u16)buf[1]);
    } else {
        *out = (fbx_u16)(buf[0] | ((fbx_u16)buf[1] << 8));
    }
    return 1;
}

static int fbx_read_u32(FbxContext *ctx, fbx_u32 *out)
{
    fbx_u8 buf[4];
    if (!fbx_read_bytes(ctx, buf, 4)) return 0;
    if (ctx && ctx->binary_big_endian) {
        *out = ((fbx_u32)buf[0] << 24)
             | ((fbx_u32)buf[1] << 16)
             | ((fbx_u32)buf[2] << 8)
             |  (fbx_u32)buf[3];
    } else {
        *out =  (fbx_u32)buf[0]
              | ((fbx_u32)buf[1] << 8)
              | ((fbx_u32)buf[2] << 16)
              | ((fbx_u32)buf[3] << 24);
    }
    return 1;
}

static int fbx_read_u64(FbxContext *ctx, fbx_u64 *out)
{
    fbx_u32 a;
    fbx_u32 b;
    if (!fbx_read_u32(ctx, &a)) return 0;
    if (!fbx_read_u32(ctx, &b)) return 0;
    if (ctx && ctx->binary_big_endian) {
        *out = ((fbx_u64)a << 32) | (fbx_u64)b;
    } else {
        *out = ((fbx_u64)b << 32) | (fbx_u64)a;
    }
    return 1;
}

static int fbx_read_i16(FbxContext *ctx, fbx_i16 *out)
{
    fbx_u16 u;
    if (!fbx_read_u16(ctx, &u)) return 0;
    *out = (fbx_i16)u;
    return 1;
}

static int fbx_read_i32(FbxContext *ctx, fbx_i32 *out)
{
    fbx_u32 u;
    if (!fbx_read_u32(ctx, &u)) return 0;
    *out = (fbx_i32)u;
    return 1;
}

static int fbx_read_i64(FbxContext *ctx, fbx_i64 *out)
{
    fbx_u64 u;
    if (!fbx_read_u64(ctx, &u)) return 0;
    *out = (fbx_i64)u;
    return 1;
}

static int fbx_read_real32(FbxContext *ctx, fbx_real *out)
{
    fbx_u32 bits;
    if (!fbx_read_u32(ctx, &bits)) return 0;
    *out = fbx_real_from_ieee32_bits(bits);
    return 1;
}

static int fbx_read_real64(FbxContext *ctx, fbx_real *out)
{
    fbx_u64 bits;
    if (!fbx_read_u64(ctx, &bits)) return 0;
    *out = fbx_real_from_ieee64_bits(bits);
    return 1;
}

/* ----------------- Node / property helpers ----------------- */

static void fbx_property_init(FbxProperty *p)
{
    if (!p) return;
    p->type_code = 0;
    p->type = FBX_PROP_UNKNOWN;
    p->encoding = 0;
    p->count = 0;
    p->byte_length = 0;
    p->data = NULL;
}

static FbxNode *fbx_node_create(const char *name)
{
    FbxNode *node;

    node = (FbxNode*)fbx_alloc_zero_n(1, sizeof(FbxNode));
    if (!node) return NULL;

    node->name = NULL;
    node->property_count = 0;
    node->properties = NULL;
    node->child_count = 0;
    node->child_capacity = 0;
    node->children = NULL;

    if (name) {
        size_t len = strlen(name);
        node->name = (char*)fbx_alloc(len + 1);
        if (!node->name) {
            fbx_release(node);
            return NULL;
        }
        memcpy(node->name, name, len + 1);
    }

    return node;
}

static int fbx_read_string(FbxContext *ctx, char **out, fbx_u32 *out_len)
{
    fbx_u32 len;
    char *buf;

    if (!fbx_read_u32(ctx, &len)) return 0;
    if (ctx && ctx->limits.max_array_bytes > 0 && len > ctx->limits.max_array_bytes) {
        fbx_set_error(ctx, FBX_ERROR_LIMIT, "FBX: string byte limit exceeded");
        return 0;
    }

    /* Allocate even for empty strings so callers can rely on non-NULL */
    buf = (char*)fbx_alloc((size_t)len + 1);
    if (!buf) {
        fbx_set_error(ctx, FBX_ERROR_OUT_OF_MEMORY, "FBX: out of memory reading string");
        return 0;
    }

    if (len > 0) {
        if (!fbx_read_bytes(ctx, buf, (size_t)len)) {
            fbx_release(buf);
            return 0;
        }
    }

    buf[len] = '\0';

    *out = buf;
    if (out_len) *out_len = len;
    return 1;
}

static int fbx_read_raw(FbxContext *ctx, void **out, fbx_u32 *out_len)
{
    fbx_u32 len;
    void *buf;

    if (!fbx_read_u32(ctx, &len)) return 0;
    if (ctx && ctx->limits.max_array_bytes > 0 && len > ctx->limits.max_array_bytes) {
        fbx_set_error(ctx, FBX_ERROR_LIMIT, "FBX: raw property byte limit exceeded");
        return 0;
    }

    if (len == 0) {
        *out = NULL;
        if (out_len) *out_len = 0;
        return 1;
    }

    buf = fbx_alloc((size_t)len);
    if (!buf) {
        fbx_set_error(ctx, FBX_ERROR_OUT_OF_MEMORY, "FBX: out of memory reading raw property");
        return 0;
    }

    if (!fbx_read_bytes(ctx, buf, (size_t)len)) {
        fbx_release(buf);
        return 0;
    }

    *out = buf;
    if (out_len) *out_len = len;
    return 1;
}


static void fbx_swap_bytes_in_place(unsigned char *p, fbx_u32 elem_size)
{
    fbx_u32 i;
    if (!p) return;
    for (i = 0u; i < elem_size / 2u; ++i) {
        unsigned char t;
        t = p[i];
        p[i] = p[elem_size - 1u - i];
        p[elem_size - 1u - i] = t;
    }
}

static void fbx_maybe_byteswap_array_payload(FbxContext *ctx,
                                             char type_code,
                                             fbx_i32 encoding,
                                             fbx_i32 count,
                                             fbx_i32 byte_len,
                                             void *data)
{
    fbx_u32 elem_size;
    fbx_u32 i;
    unsigned char *bytes;

    if (!ctx || !ctx->binary_big_endian) return;
    if (encoding != 0) return;
    if (!data || count <= 0 || byte_len <= 0) return;

    elem_size = 1u;
    if (type_code == 'f' || type_code == 'i') elem_size = 4u;
    else if (type_code == 'd' || type_code == 'l') elem_size = 8u;
    else if (type_code == 'b' || type_code == 'c') elem_size = 1u;
    else return;

    if (elem_size <= 1u) return;
    if ((fbx_u32)byte_len < (fbx_u32)count * elem_size) return;

    bytes = (unsigned char*)data;
    for (i = 0u; i < (fbx_u32)count; ++i) {
        fbx_swap_bytes_in_place(bytes + (size_t)i * elem_size, elem_size);
    }
}

static int fbx_read_array_payload(FbxContext *ctx,
                                  fbx_i32 *out_encoding,
                                  fbx_i32 *out_count,
                                  fbx_i32 *out_byte_len,
                                  void **out_data)
{
    /* FBX binary array header order is:
         uint32 arrayLength
         uint32 encoding   (0 = raw, 1 = zlib)
         uint32 byteLength (compressed length if encoding==1)
       Previous versions of this file accidentally swapped the first two.
     */
    fbx_u32 count_u;
    fbx_u32 encoding_u;
    fbx_u32 byte_len_u;
    void *buf;

    if (!fbx_read_u32(ctx, &count_u)) return 0;
    if (!fbx_read_u32(ctx, &encoding_u)) return 0;
    if (!fbx_read_u32(ctx, &byte_len_u)) return 0;

    if (ctx && ctx->limits.max_array_elements > 0 &&
        count_u > ctx->limits.max_array_elements) {
        fbx_set_error(ctx, FBX_ERROR_LIMIT, "FBX: array element limit exceeded");
        return 0;
    }
    if (ctx && ctx->limits.max_array_bytes > 0 &&
        byte_len_u > ctx->limits.max_array_bytes) {
        fbx_set_error(ctx, FBX_ERROR_LIMIT, "FBX: array payload byte limit exceeded");
        return 0;
    }

    if (byte_len_u == 0) {
        *out_encoding = (fbx_i32)encoding_u;
        *out_count = (fbx_i32)count_u;
        *out_byte_len = 0;
        *out_data = NULL;
        return 1;
    }

    buf = fbx_alloc((size_t)byte_len_u);
    if (!buf) {
        fbx_set_error(ctx, FBX_ERROR_OUT_OF_MEMORY, "FBX: out of memory reading array payload");
        return 0;
    }

    if (!fbx_read_bytes(ctx, buf, (size_t)byte_len_u)) {
        fbx_release(buf);
        return 0;
    }

    *out_encoding = (fbx_i32)encoding_u;
    *out_count = (fbx_i32)count_u;
    *out_byte_len = (fbx_i32)byte_len_u;
    *out_data = buf;
    return 1;
}

static int fbx_read_property(FbxContext *ctx, FbxProperty *out)
{
    fbx_u8 tc;

    if (!ctx || !out) return 0;

    fbx_property_init(out);

    if (!fbx_read_u8(ctx, &tc)) return 0;
    out->type_code = (char)tc;

    switch ((char)tc) {
        case 'Y': {
            fbx_i16 v;
            if (!fbx_read_i16(ctx, &v)) return 0;
            out->type = FBX_PROP_INT;
            out->count = 1;
            out->byte_length = (fbx_i32)sizeof(fbx_i16);
            out->data = fbx_alloc(sizeof(fbx_i16));
            if (!out->data) { fbx_set_error(ctx, FBX_ERROR_OUT_OF_MEMORY, "FBX: out of memory reading property"); return 0; }
            memcpy(out->data, &v, sizeof(fbx_i16));
            return 1;
        }
        case 'C': {
            fbx_u8 v;
            if (!fbx_read_u8(ctx, &v)) return 0;
            out->type = FBX_PROP_INT;
            out->count = 1;
            out->byte_length = 1;
            out->data = fbx_alloc(1);
            if (!out->data) { fbx_set_error(ctx, FBX_ERROR_OUT_OF_MEMORY, "FBX: out of memory reading property"); return 0; }
            ((fbx_u8*)out->data)[0] = v;
            return 1;
        }
        case 'I': {
            fbx_i32 v;
            if (!fbx_read_i32(ctx, &v)) return 0;
            out->type = FBX_PROP_INT;
            out->count = 1;
            out->byte_length = (fbx_i32)sizeof(fbx_i32);
            out->data = fbx_alloc(sizeof(fbx_i32));
            if (!out->data) { fbx_set_error(ctx, FBX_ERROR_OUT_OF_MEMORY, "FBX: out of memory reading property"); return 0; }
            memcpy(out->data, &v, sizeof(fbx_i32));
            return 1;
        }
        case 'L': {
            fbx_i64 v;
            if (!fbx_read_i64(ctx, &v)) return 0;
            out->type = FBX_PROP_INT;
            out->count = 1;
            out->byte_length = (fbx_i32)sizeof(fbx_i64);
            out->data = fbx_alloc(sizeof(fbx_i64));
            if (!out->data) { fbx_set_error(ctx, FBX_ERROR_OUT_OF_MEMORY, "FBX: out of memory reading property"); return 0; }
            memcpy(out->data, &v, sizeof(fbx_i64));
            return 1;
        }
        case 'F': {
            fbx_real v;
            if (!fbx_read_real32(ctx, &v)) return 0;
            out->type = FBX_PROP_REAL;
            out->count = 1;
            out->byte_length = (fbx_i32)sizeof(fbx_real);
            out->data = fbx_alloc(sizeof(fbx_real));
            if (!out->data) { fbx_set_error(ctx, FBX_ERROR_OUT_OF_MEMORY, "FBX: out of memory reading property"); return 0; }
            memcpy(out->data, &v, sizeof(fbx_real));
            return 1;
        }
        case 'D': {
            fbx_real v;
            if (!fbx_read_real64(ctx, &v)) return 0;
            out->type = FBX_PROP_REAL;
            out->count = 1;
            out->byte_length = (fbx_i32)sizeof(fbx_real);
            out->data = fbx_alloc(sizeof(fbx_real));
            if (!out->data) { fbx_set_error(ctx, FBX_ERROR_OUT_OF_MEMORY, "FBX: out of memory reading property"); return 0; }
            memcpy(out->data, &v, sizeof(fbx_real));
            return 1;
        }
        case 'S': {
            char *s;
            fbx_u32 len;
            if (!fbx_read_string(ctx, &s, &len)) return 0;
            out->type = FBX_PROP_STRING;
            out->count = 1;
            out->byte_length = (fbx_i32)len;
            out->data = s;
            return 1;
        }
        case 'R': {
            void *r;
            fbx_u32 len;
            if (!fbx_read_raw(ctx, &r, &len)) return 0;
            out->type = FBX_PROP_RAW;
            out->count = 1;
            out->byte_length = (fbx_i32)len;
            out->data = r;
            return 1;
        }
        /* Array types */
        case 'f':
        case 'd':
        case 'l':
        case 'i':
        case 'b':
        case 'c': {
            fbx_i32 encoding;
            fbx_i32 count;
            fbx_i32 byte_len;
            void *buf;

            if (!fbx_read_array_payload(ctx, &encoding, &count, &byte_len, &buf)) {
                return 0;
            }
            fbx_maybe_byteswap_array_payload(ctx, (char)tc, encoding, count, byte_len, buf);

            out->encoding = encoding;
            out->count = count;
            out->byte_length = byte_len;
            out->data = buf;

            if ((char)tc == 'f' || (char)tc == 'd') {
                out->type = FBX_PROP_ARRAY_REAL;
            } else {
                out->type = FBX_PROP_ARRAY_INT;
            }
            return 1;
        }
        default:
            /* Unsupported property type code.
               For maximum forwards-compatibility we let the caller decide how to
               recover (typically by skipping the rest of the property list using
               the node header's propertyListLen). */
            return 0;
    }
}

/* ----------------- Binary node reader ----------------- */

static FbxNode *fbx_read_binary_node(FbxContext *ctx, int depth)
{
    fbx_u64 end_offset64;
    fbx_u64 num_props64;
    fbx_u64 prop_list_len64;
    fbx_u8 name_len;
    char name_buf[256];
    FbxNode *node;

    if (!ctx || ctx->source_kind == FBX_SOURCE_NONE) return NULL;

    if (ctx->max_depth > 0 && depth > ctx->max_depth) {
        fbx_set_error(ctx, FBX_ERROR_LIMIT, "FBX: max_depth exceeded (possible corrupted file)");
        return NULL;
    }

    /* Version-dependent header sizes (FBX 7.5+ uses 64-bit offsets/counts) */
    if (ctx->version >= 7500) {
        if (!fbx_read_u64(ctx, &end_offset64)) return NULL;
        if (!fbx_read_u64(ctx, &num_props64)) return NULL;
        if (!fbx_read_u64(ctx, &prop_list_len64)) return NULL;
    } else {
        fbx_u32 end32, np32, pll32;
        if (!fbx_read_u32(ctx, &end32)) return NULL;
        if (!fbx_read_u32(ctx, &np32)) return NULL;
        if (!fbx_read_u32(ctx, &pll32)) return NULL;
        end_offset64 = (fbx_u64)end32;
        num_props64 = (fbx_u64)np32;
        prop_list_len64 = (fbx_u64)pll32;
    }

    if (!fbx_read_u8(ctx, &name_len)) return NULL;

    /* Null record check */
    if (end_offset64 == 0 && num_props64 == 0 && prop_list_len64 == 0 && name_len == 0) {
        return NULL;
    }

    if (ctx->limits.max_node_count > 0 &&
        ctx->node_count >= ctx->limits.max_node_count) {
        fbx_set_error(ctx, FBX_ERROR_LIMIT, "FBX: node count limit exceeded");
        return NULL;
    }
    ctx->node_count += 1;

    /* name_len es uint8 en el spec (0..255) y name_buf es 256, así que siempre cabe */

    if (name_len > 0) {
        if (!fbx_read_bytes(ctx, name_buf, (size_t)name_len)) return NULL;
    }
    name_buf[name_len] = '\0';

    node = fbx_node_create(name_buf);
    if (!node) {
        fbx_set_error(ctx, FBX_ERROR_OUT_OF_MEMORY, "FBX: out of memory creating node");
        return NULL;
    }

    if (num_props64 > (fbx_u64)INT_MAX ||
        (ctx->limits.max_properties_per_node > 0 &&
         num_props64 > (fbx_u64)ctx->limits.max_properties_per_node)) {
        fbx_set_error(ctx, FBX_ERROR_LIMIT, "FBX: too many properties in node");
        fbx_node_destroy(node);
        return NULL;
    }

    if (ctx->limits.max_array_bytes > 0 &&
        prop_list_len64 > (fbx_u64)ctx->limits.max_array_bytes) {
        fbx_set_error(ctx, FBX_ERROR_LIMIT, "FBX: property payload byte limit exceeded");
        fbx_node_destroy(node);
        return NULL;
    }

    if (num_props64 > 0) {
        int i;
        int nprops;
        fbx_i64 props_start;
        fbx_i64 props_end;

        nprops = (int)num_props64;
        node->properties = (FbxProperty*)fbx_alloc_zero_n((size_t)nprops, sizeof(FbxProperty));
        if (!node->properties) {
            fbx_set_error(ctx, FBX_ERROR_OUT_OF_MEMORY, "FBX: out of memory allocating properties");
            fbx_node_destroy(node);
            return NULL;
        }
        node->property_count = nprops;

        props_start = fbx_context_tell(ctx);
        if (props_start < 0) {
            fbx_set_error(ctx, FBX_ERROR_IO, "FBX: ftell failed");
            fbx_node_destroy(node);
            return NULL;
        }
        /* property list is a contiguous blob of length prop_list_len64 */
        props_end = props_start + (fbx_i64)prop_list_len64;

        for (i = 0; i < nprops; ++i) {
            if (!fbx_read_property(ctx, &node->properties[i])) {
                /* Non-fatal: skip to end of property list and continue.
                   This keeps the stream aligned even if we don't understand a
                   new/rare property type.
                */
                fbx_set_warning(ctx, "FBX: unknown/unsupported property type (skipping)" );

                /* Drop unread properties from the count so downstream code only
                   iterates over valid properties.
                */
                node->property_count = i;

                if (props_end > 0) {
                    (void)fbx_context_seek(ctx, props_end, FBX_SEEK_SET);
                }
                break;
            }
        }

        /* Defensive: ensure we're positioned at end of the property list. */
        if (props_end > 0) {
            fbx_i64 pos_after = fbx_context_tell(ctx);
            if (pos_after >= 0 && pos_after < props_end) {
                (void)fbx_context_seek(ctx, props_end, FBX_SEEK_SET);
            }
        }
    }

    /* Read children until we reach end_offset */
    while (1) {
        fbx_i64 pos;
        FbxNode *child;
        fbx_i64 pos2;

        pos = fbx_context_tell(ctx);
        if (pos < 0) {
            fbx_set_error(ctx, FBX_ERROR_IO, "FBX: ftell failed");
            fbx_node_destroy(node);
            return NULL;
        }

        if ((fbx_u64)pos >= end_offset64) {
            break;
        }

        child = fbx_read_binary_node(ctx, depth + 1);
        if (!child) {
            /* Could be null-record, padding, or error. Autodesk-era writers and
               several legacy/export-test files may leave padding after a null
               record before the parent end offset. If no fatal error has been
               recorded and the stream advanced, treat it as a terminator and
               align to the declared end offset. */
            pos2 = fbx_context_tell(ctx);
            if (pos2 < 0) {
                fbx_set_error(ctx, FBX_ERROR_IO, "FBX: ftell failed");
                fbx_node_destroy(node);
                return NULL;
            }
            if ((fbx_u64)pos2 >= end_offset64) {
                break;
            }
            if (ctx->last_error == 0 && pos2 > pos) {
                (void)fbx_context_seek(ctx, (fbx_i64)end_offset64, FBX_SEEK_SET);
                break;
            }
            /* Preserve the original fatal cause when the child reader set one. */
            if (ctx->last_error != 0) {
                fbx_node_destroy(node);
                return NULL;
            }
            /* Not enough progress -> error */
            fbx_set_error(ctx, FBX_ERROR_PARSE, "FBX: failed to read child node");
            fbx_node_destroy(node);
            return NULL;
        }

        if (ctx->limits.max_children_per_node > 0 &&
            node->child_count >= ctx->limits.max_children_per_node) {
            fbx_set_error(ctx, FBX_ERROR_LIMIT, "FBX: child count limit exceeded");
            fbx_node_destroy(child);
            fbx_node_destroy(node);
            return NULL;
        }

        if (node->child_count >= node->child_capacity) {
            FbxNode **tmp;
            int new_cap;
            new_cap = (node->child_capacity == 0) ? 4 : (node->child_capacity * 2);
            if (new_cap < node->child_count + 1) new_cap = node->child_count + 1;
            tmp = (FbxNode**)fbx_resize(node->children,
                                     (size_t)new_cap * sizeof(FbxNode*));
            if (!tmp) {
                fbx_set_error(ctx, FBX_ERROR_OUT_OF_MEMORY, "FBX: out of memory growing children");
                fbx_node_destroy(child);
                fbx_node_destroy(node);
                return NULL;
            }
            node->children = tmp;
            node->child_capacity = new_cap;
        }

        node->children[node->child_count] = child;
        node->child_count += 1;
    }

    /* Seek to end_offset if needed (defensive) */
    {
        fbx_i64 pos_end;
        pos_end = fbx_context_tell(ctx);
        if (pos_end >= 0 && (fbx_u64)pos_end < end_offset64) {
            (void)fbx_context_seek(ctx, (fbx_i64)end_offset64, FBX_SEEK_SET);
        }
    }

    return node;
}

/* ---- Public API ---- */

FbxNode *fbx_read_scene_binary(FbxContext *ctx)
{
    if (!ctx) return NULL;
    if (!ctx->is_binary) return NULL;

    /* FBX binario contiene múltiples nodos top-level.
       Creamos un root sintético que los contiene como hijos. */
    {
        FbxNode *root;
        root = fbx_node_create("Root");
        if (!root) {
            fbx_set_error(ctx, FBX_ERROR_OUT_OF_MEMORY, "FBX: out of memory creating synthetic root");
            return NULL;
        }

        while (1) {
            FbxNode *n;

            /* Limpiar error antes de leer el siguiente nodo top-level,
               así NULL puede significar 'null-record' (ok) o 'error'. */
            ctx->last_error = 0;
            ctx->last_error_msg[0] = '\0';

            n = fbx_read_binary_node(ctx, 0);
            if (!n) {
                /* null-record = fin. error = last_error no vacío */
                if (ctx->last_error) {
                    fbx_node_destroy(root);
                    return NULL;
                }
                break;
            }

            if (ctx->limits.max_children_per_node > 0 &&
                root->child_count >= ctx->limits.max_children_per_node) {
                fbx_set_error(ctx, FBX_ERROR_LIMIT, "FBX: root child count limit exceeded");
                fbx_node_destroy(n);
                fbx_node_destroy(root);
                return NULL;
            }

            if (root->child_count >= root->child_capacity) {
                FbxNode **tmp;
                int new_cap;
                new_cap = (root->child_capacity == 0) ? 4 : (root->child_capacity * 2);
                if (new_cap < root->child_count + 1) new_cap = root->child_count + 1;
                tmp = (FbxNode**)fbx_resize(root->children,
                                         (size_t)new_cap * sizeof(FbxNode*));
                if (!tmp) {
                    fbx_set_error(ctx, FBX_ERROR_OUT_OF_MEMORY, "FBX: out of memory growing root children");
                    fbx_node_destroy(n);
                    fbx_node_destroy(root);
                    return NULL;
                }
                root->children = tmp;
                root->child_capacity = new_cap;
            }

            root->children[root->child_count] = n;
            root->child_count += 1;
        }

        return root;
    }
}
