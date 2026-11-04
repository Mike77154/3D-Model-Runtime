/* fbx_importer.c - high-level API + common tree utilities */
#include "fbx_importer.h"
#include "fbx_alloc.h"
#include "fbx_importer_ascii.h"
#include "fbx_importer_bin.h"

#include <string.h>

#ifndef FBX_NO_STDIO
# if defined(_MSC_VER)
#  include <io.h>
# elif defined(__APPLE__) || defined(__linux__) || defined(__unix__)
#  include <sys/types.h>
extern int fseeko(FILE *stream, off_t offset, int whence);
extern off_t ftello(FILE *stream);
# endif
#endif

/* ===== Error handling ===== */

static void fbx_set_error(FbxContext *ctx, int code, const char *msg) {
    if (!ctx) return;
    ctx->last_error = code;
    if (msg) {
        strncpy(ctx->last_error_msg, msg, sizeof(ctx->last_error_msg) - 1);
        ctx->last_error_msg[sizeof(ctx->last_error_msg) - 1] = '\0';
    } else {
        ctx->last_error_msg[0] = '\0';
    }
}

const char *fbx_last_error(const FbxContext *ctx) {
    if (!ctx) return "(no context)";
    if (ctx->last_error == 0) {
        if (ctx->last_error_msg[0] != '\0') return ctx->last_error_msg;
        return "OK";
    }
    if (ctx->last_error_msg[0] == '\0') return "(error)";
    return ctx->last_error_msg;
}

void fbx_limits_default(FbxLimits *limits)
{
    if (!limits) return;
    limits->max_depth = FBX_DEFAULT_MAX_DEPTH;
    limits->max_node_count = FBX_DEFAULT_MAX_NODE_COUNT;
    limits->max_properties_per_node = FBX_DEFAULT_MAX_PROPERTIES_PER_NODE;
    limits->max_children_per_node = FBX_DEFAULT_MAX_CHILDREN_PER_NODE;
    limits->max_array_elements = FBX_DEFAULT_MAX_ARRAY_ELEMENTS;
    limits->max_array_bytes = FBX_DEFAULT_MAX_ARRAY_BYTES;
    limits->max_vertices = FBX_DEFAULT_MAX_VERTICES;
    limits->max_indices = FBX_DEFAULT_MAX_INDICES;
    limits->max_bones = FBX_DEFAULT_MAX_BONES;
    limits->max_weights_per_vertex = FBX_DEFAULT_MAX_WEIGHTS_PER_VERTEX;
}

void fbx_context_set_limits(FbxContext *ctx, const FbxLimits *limits)
{
    if (!ctx) return;
    if (limits) {
        ctx->limits = *limits;
    } else {
        fbx_limits_default(&ctx->limits);
    }
    ctx->max_depth = ctx->limits.max_depth;
}

void fbx_get_default_limits(FbxLimits *out_limits)
{
    fbx_limits_default(out_limits);
}

const char *fbx_error_code_string(int code)
{
    switch (code) {
        case FBX_ERROR_OK: return "OK";
        case FBX_ERROR_INVALID_ARGUMENT: return "invalid argument";
        case FBX_ERROR_OUT_OF_MEMORY: return "out of importer arena";
        case FBX_ERROR_IO: return "I/O error";
        case FBX_ERROR_PARSE: return "parse error";
        case FBX_ERROR_UNSUPPORTED: return "unsupported FBX feature";
        case FBX_ERROR_LIMIT: return "safety limit reached";
        case FBX_ERROR_DECOMPRESSION: return "array decompression failed";
        case FBX_ERROR_STREAM: return "stream source error";
        default: return "unknown error";
    }
}

int fbx_last_error_code(const FbxContext *ctx)
{
    if (!ctx) return FBX_ERROR_INVALID_ARGUMENT;
    return ctx->last_error;
}

/* ===== Source helpers: file or caller-owned memory ===== */

#ifndef FBX_NO_STDIO
static int fbx_file_seek64(FILE *fp, fbx_i64 offset, int origin) {
    int std_origin;
    if (origin == FBX_SEEK_SET) std_origin = SEEK_SET;
    else if (origin == FBX_SEEK_CUR) std_origin = SEEK_CUR;
    else std_origin = SEEK_END;
# if defined(_MSC_VER)
    return _fseeki64(fp, offset, std_origin);
# elif defined(__APPLE__) || defined(__linux__) || defined(__unix__)
    return fseeko(fp, (off_t)offset, std_origin);
# else
    return fseek(fp, (long)offset, std_origin);
# endif
}

static fbx_i64 fbx_file_tell64(FILE *fp) {
# if defined(_MSC_VER)
    return (fbx_i64)_ftelli64(fp);
# elif defined(__APPLE__) || defined(__linux__) || defined(__unix__)
    return (fbx_i64)ftello(fp);
# else
    return (fbx_i64)ftell(fp);
# endif
}
#endif

int fbx_context_read(FbxContext *ctx, void *dst, size_t size)
{
    unsigned char *out;
    size_t got;
    if (!ctx || !dst) return 0;
    if (size == 0u) return 1;

    if (ctx->source_kind == FBX_SOURCE_MEMORY) {
        if (ctx->mem_pos < 0 || ctx->mem_size < 0) return 0;
        if ((fbx_i64)size > ctx->mem_size - ctx->mem_pos) return 0;
        memcpy(dst, ctx->mem + ctx->mem_pos, size);
        ctx->mem_pos += (fbx_i64)size;
        return 1;
    }

    if (ctx->source_kind == FBX_SOURCE_STREAM && ctx->stream.read) {
        out = (unsigned char *)dst;
        got = 0u;
        while (got < size) {
            size_t n;
            if (ctx->stream_prefix_pos < ctx->stream_prefix_size) {
                n = ctx->stream_prefix_size - ctx->stream_prefix_pos;
                if (n > size - got) n = size - got;
                memcpy(out + got, ctx->stream_prefix + ctx->stream_prefix_pos, n);
                ctx->stream_prefix_pos += n;
                got += n;
                ctx->stream_pos += (fbx_i64)n;
                continue;
            }
            n = ctx->stream.read(ctx->stream_user, out + got, size - got);
            if (n == 0u) return 0;
            got += n;
            ctx->stream_pos += (fbx_i64)n;
        }
        return 1;
    }

#ifndef FBX_NO_STDIO
    if (ctx->source_kind == FBX_SOURCE_FILE && ctx->fp) {
        return fread(dst, 1u, size, ctx->fp) == size;
    }
#endif

    return 0;
}

int fbx_context_getc(FbxContext *ctx)
{
    unsigned char c;
    if (!fbx_context_read(ctx, &c, 1u)) return -1;
    return (int)c;
}

static int fbx_stream_discard_forward(FbxContext *ctx, fbx_i64 amount)
{
    unsigned char scratch[256];
    while (amount > 0) {
        size_t step;
        step = (amount > (fbx_i64)sizeof(scratch)) ? sizeof(scratch) : (size_t)amount;
        if (!fbx_context_read(ctx, scratch, step)) return -1;
        amount -= (fbx_i64)step;
    }
    return 0;
}

int fbx_context_seek(FbxContext *ctx, fbx_i64 offset, int origin)
{
    fbx_i64 base;
    fbx_i64 target;
    if (!ctx) return -1;

    if (ctx->source_kind == FBX_SOURCE_MEMORY) {
        if (origin == FBX_SEEK_SET) base = 0;
        else if (origin == FBX_SEEK_CUR) base = ctx->mem_pos;
        else if (origin == FBX_SEEK_END) base = ctx->mem_size;
        else return -1;
        target = base + offset;
        if (target < 0 || target > ctx->mem_size) return -1;
        ctx->mem_pos = target;
        return 0;
    }

    if (ctx->source_kind == FBX_SOURCE_STREAM) {
        if (ctx->stream.seek) {
            int r;
            r = ctx->stream.seek(ctx->stream_user, offset, origin);
            if (r == 0) {
                if (ctx->stream.tell) ctx->stream_pos = ctx->stream.tell(ctx->stream_user);
                else if (origin == FBX_SEEK_SET) ctx->stream_pos = offset;
                else if (origin == FBX_SEEK_CUR) ctx->stream_pos += offset;
                else ctx->stream_pos = (fbx_i64)-1;
            }
            return r;
        }

        if (origin == FBX_SEEK_SET) {
            if (ctx->stream_pos < 0 || offset < ctx->stream_pos) return -1;
            return fbx_stream_discard_forward(ctx, offset - ctx->stream_pos);
        }
        if (origin == FBX_SEEK_CUR && offset >= 0) {
            return fbx_stream_discard_forward(ctx, offset);
        }
        return -1;
    }

#ifndef FBX_NO_STDIO
    if (ctx->source_kind == FBX_SOURCE_FILE && ctx->fp) {
        return fbx_file_seek64(ctx->fp, offset, origin);
    }
#endif

    return -1;
}

fbx_i64 fbx_context_tell(FbxContext *ctx)
{
    if (!ctx) return (fbx_i64)-1;
    if (ctx->source_kind == FBX_SOURCE_MEMORY) return ctx->mem_pos;
    if (ctx->source_kind == FBX_SOURCE_STREAM) {
        return ctx->stream_pos;
    }
#ifndef FBX_NO_STDIO
    if (ctx->source_kind == FBX_SOURCE_FILE && ctx->fp) return fbx_file_tell64(ctx->fp);
#endif
    return (fbx_i64)-1;
}

static fbx_i64 fbx_context_size(FbxContext *ctx)
{
    if (!ctx) return (fbx_i64)-1;
    if (ctx->source_kind == FBX_SOURCE_MEMORY) return ctx->mem_size;
    if (ctx->source_kind == FBX_SOURCE_STREAM) {
        if (ctx->stream.size) return ctx->stream.size(ctx->stream_user);
        return ctx->stream_size;
    }

#ifndef FBX_NO_STDIO
    {
        fbx_i64 cur;
        fbx_i64 end;
        cur = fbx_context_tell(ctx);
    if (cur < 0) cur = 0;
    if (fbx_context_seek(ctx, 0, FBX_SEEK_END) != 0) {
        (void)fbx_context_seek(ctx, cur, FBX_SEEK_SET);
        return (fbx_i64)-1;
    }
        end = fbx_context_tell(ctx);
        (void)fbx_context_seek(ctx, cur, FBX_SEEK_SET);
        return end;
    }
#else
    return (fbx_i64)-1;
#endif
}

static int fbx_context_read_stream_direct(FbxContext *ctx, void *dst, size_t size)
{
    unsigned char *out;
    size_t got;
    if (!ctx || !dst || ctx->source_kind != FBX_SOURCE_STREAM || !ctx->stream.read) return 0;
    out = (unsigned char*)dst;
    got = 0u;
    while (got < size) {
        size_t n;
        n = ctx->stream.read(ctx->stream_user, out + got, size - got);
        if (n == 0u) return 0;
        got += n;
    }
    return 1;
}

static fbx_u32 fbx_u32_from_be4(const unsigned char b[4])
{
    return ((fbx_u32)b[0] << 24)
         | ((fbx_u32)b[1] << 16)
         | ((fbx_u32)b[2] << 8)
         |  (fbx_u32)b[3];
}

static fbx_u32 fbx_u32_from_le4(const unsigned char b[4])
{
    return  (fbx_u32)b[0]
         | ((fbx_u32)b[1] << 8)
         | ((fbx_u32)b[2] << 16)
         | ((fbx_u32)b[3] << 24);
}

static int fbx_version_is_plausible(fbx_u32 ver)
{
    return ver >= 3000u && ver <= 9999u;
}

static int fbx_header_is_binary(const unsigned char hdr[23])
{
    static const unsigned char magic_prefix[22] = {
        'K','a','y','d','a','r','a',' ','F','B','X',' ','B','i','n','a','r','y',' ',' ',0x00,0x1A
    };
    return memcmp(hdr, magic_prefix, 22u) == 0 && (hdr[22] == 0x00u || hdr[22] == 0x01u);
}

static void fbx_set_binary_version_from_bytes(FbxContext *ctx, const unsigned char b[4])
{
    fbx_u32 le;
    fbx_u32 be;
    le = fbx_u32_from_le4(b);
    be = fbx_u32_from_be4(b);
    ctx->binary_big_endian = 0;
    if (!fbx_version_is_plausible(le) && fbx_version_is_plausible(be)) {
        ctx->binary_big_endian = 1;
        ctx->version = (int)be;
    } else {
        ctx->version = (int)le;
    }
}


/* ===== ASCII version detection (best-effort) ===== */

static int fbx_ascii_sniff_version(FbxContext *ctx) {
    enum { FBX_SNIFF_MAX = 262144 };
    unsigned char *buf;
    size_t cap;
    size_t got;
    size_t i;
    fbx_i64 sz;

    if (!ctx) return 0;
    if (fbx_context_seek(ctx, 0, FBX_SEEK_SET) != 0) return 0;

    cap = FBX_SNIFF_MAX;
    sz = ctx->file_size;
    if (sz > 0 && (fbx_i64)cap > sz) cap = (size_t)sz;

    buf = (unsigned char*)fbx_alloc(cap + 1u);
    if (!buf) {
        (void)fbx_context_seek(ctx, 0, FBX_SEEK_SET);
        return 0;
    }

    got = 0u;
    while (got < cap) {
        int c;
        c = fbx_context_getc(ctx);
        if (c < 0) break;
        buf[got++] = (unsigned char)c;
    }
    buf[got] = '\0';

    for (i = 0u; i + 8u < got; ++i) {
        /* Header comments such as `; FBX 8.0.0 project file` can be more
           informative than the legacy `FBXVersion` field in synthetic/future
           files. Mirror ufbx/SDK behavior by keeping the highest plausible
           version found in the initial header area. */
        if (buf[i] == ';' && i + 6u < got &&
            buf[i+1u] == ' ' && buf[i+2u] == 'F' && buf[i+3u] == 'B' && buf[i+4u] == 'X' && buf[i+5u] == ' ') {
            size_t j;
            int major;
            int minor;
            int patch;
            int v;
            j = i + 6u;
            major = 0;
            minor = 0;
            patch = 0;
            if (j < got && buf[j] >= '0' && buf[j] <= '9') {
                while (j < got && buf[j] >= '0' && buf[j] <= '9') {
                    major = major * 10 + (int)(buf[j] - '0');
                    j++;
                }
                if (j < got && buf[j] == '.') {
                    j++;
                    while (j < got && buf[j] >= '0' && buf[j] <= '9') {
                        minor = minor * 10 + (int)(buf[j] - '0');
                        j++;
                    }
                }
                if (j < got && buf[j] == '.') {
                    j++;
                    while (j < got && buf[j] >= '0' && buf[j] <= '9') {
                        patch = patch * 10 + (int)(buf[j] - '0');
                        j++;
                    }
                }
                v = major * 1000 + minor * 100 + patch;
                if (v >= 8000 && v > ctx->version && fbx_version_is_plausible((fbx_u32)v)) ctx->version = v;
            }
        }

        if (i + 10u < got && buf[i] == 'F' && buf[i+1u] == 'B' && buf[i+2u] == 'X' &&
            buf[i+3u] == 'V' && buf[i+4u] == 'e' && buf[i+5u] == 'r' &&
            buf[i+6u] == 's' && buf[i+7u] == 'i' && buf[i+8u] == 'o' && buf[i+9u] == 'n') {
            size_t j;
            j = i + 10u;
            while (j < got && (buf[j] == ' ' || buf[j] == '\t' || buf[j] == '\r' || buf[j] == '\n' || buf[j] == ':')) {
                j++;
            }
            if (j < got && buf[j] >= '0' && buf[j] <= '9') {
                int v;
                v = 0;
                while (j < got && buf[j] >= '0' && buf[j] <= '9') {
                    int digit;
                    digit = (int)(buf[j] - '0');
                    if (v < 100000000) v = v * 10 + digit;
                    j++;
                }
                if (v > ctx->version && fbx_version_is_plausible((fbx_u32)v)) ctx->version = v;
            }
        }
    }

    fbx_release(buf);
    (void)fbx_context_seek(ctx, 0, FBX_SEEK_SET);
    return (ctx->version != 0);
}

/* ===== Tree destroy ===== */

static void fbx_property_destroy_internal(FbxProperty *p) {
    if (!p) return;
    if (p->data) {
        fbx_release(p->data);
        p->data = NULL;
    }
    p->type_code = 0;
    p->type = FBX_PROP_UNKNOWN;
    p->encoding = 0;
    p->count = 0;
    p->byte_length = 0;
}

void fbx_node_destroy(FbxNode *node) {
    int i;
    if (!node) return;

    if (node->name) {
        fbx_release(node->name);
        node->name = NULL;
    }

    if (node->properties) {
        for (i = 0; i < node->property_count; ++i) {
            fbx_property_destroy_internal(&node->properties[i]);
        }
        fbx_release(node->properties);
        node->properties = NULL;
        node->property_count = 0;
    }

    if (node->children) {
        for (i = 0; i < node->child_count; ++i) {
            fbx_node_destroy(node->children[i]);
        }
        fbx_release(node->children);
        node->children = NULL;
        node->child_capacity = 0;
        node->child_count = 0;
    }

    fbx_release(node);
}

/* ===== Debug print ===== */

void fbx_print_tree(const FbxNode *node, int depth) {
#ifndef FBX_NO_STDIO
    int i;
    if (!node) return;
    for (i = 0; i < depth; ++i) {
        fputs("  ", stdout);
    }
    printf("%s (props=%d, children=%d)\n",
           node->name ? node->name : "(null)",
           node->property_count,
           node->child_count);
    for (i = 0; i < node->child_count; ++i) {
        fbx_print_tree(node->children[i], depth + 1);
    }
#else
    (void)node;
    (void)depth;
#endif
}

/* ===== Open / Close / Read ===== */

static void fbx_context_init_common(FbxContext *ctx)
{
    if (!ctx) return;
#ifndef FBX_NO_STDIO
    ctx->fp = NULL;
#endif
    ctx->mem = NULL;
    ctx->mem_size = 0;
    ctx->mem_pos = 0;
    memset(&ctx->stream, 0, sizeof(ctx->stream));
    ctx->stream_user = NULL;
    ctx->stream_pos = 0;
    ctx->stream_size = (fbx_i64)-1;
    ctx->stream_prefix_size = 0u;
    ctx->stream_prefix_pos = 0u;
    ctx->source_kind = FBX_SOURCE_NONE;
    ctx->is_binary = 0;
    ctx->binary_big_endian = 0;
    ctx->version = 0;
    ctx->file_size = (fbx_i64)-1;
    fbx_limits_default(&ctx->limits);
    ctx->max_depth = ctx->limits.max_depth;
    ctx->node_count = 0;
    ctx->last_error = FBX_ERROR_OK;
    ctx->last_error_msg[0] = '\0';
}

static int fbx_detect_format(FbxContext *ctx)
{
    unsigned char hdr[23];
    unsigned char vb[4];

    if (!ctx) return 0;
    ctx->file_size = fbx_context_size(ctx);

    if (ctx->source_kind == FBX_SOURCE_STREAM) {
        if (!fbx_context_read_stream_direct(ctx, hdr, 23u)) {
            fbx_set_error(ctx, FBX_ERROR_IO, "Failed to read FBX stream header");
            return 0;
        }

        if (fbx_header_is_binary(hdr)) {
            ctx->is_binary = 1;
            if (!fbx_context_read_stream_direct(ctx, vb, 4u)) {
                fbx_set_error(ctx, FBX_ERROR_IO, "Failed to read FBX binary version");
                return 0;
            }
            fbx_set_binary_version_from_bytes(ctx, vb);
            ctx->stream_pos = 27;
            return 1;
        }

        ctx->is_binary = 0;
        ctx->version = 0;
        memcpy(ctx->stream_prefix, hdr, 23u);
        ctx->stream_prefix_size = 23u;
        ctx->stream_prefix_pos = 0u;
        ctx->stream_pos = 0;
        return 1;
    }

    if (!fbx_context_read(ctx, hdr, 23u)) {
        fbx_set_error(ctx, FBX_ERROR_IO, "Failed to read FBX header");
        return 0;
    }

    if (fbx_header_is_binary(hdr)) {
        ctx->is_binary = 1;
        if (!fbx_context_read(ctx, vb, 4u)) {
            fbx_set_error(ctx, FBX_ERROR_IO, "Failed to read FBX binary version");
            return 0;
        }
        fbx_set_binary_version_from_bytes(ctx, vb);
    } else {
        ctx->is_binary = 0;
        ctx->version = 0;
        if (fbx_context_seek(ctx, 0, FBX_SEEK_SET) != 0) {
            fbx_set_error(ctx, FBX_ERROR_STREAM, "ASCII FBX source requires seek or stream prefix replay");
            return 0;
        }
        (void)fbx_ascii_sniff_version(ctx);
    }
    return 1;
}

FbxContext *fbx_open(const char *path) {
#ifndef FBX_NO_STDIO
    FbxContext *ctx;

    if (!path) return NULL;

    ctx = (FbxContext*)fbx_alloc_zero_n(1u, sizeof(FbxContext));
    if (!ctx) return NULL;
    fbx_context_init_common(ctx);

    ctx->fp = fopen(path, "rb");
    if (!ctx->fp) {
        fbx_release(ctx);
        return NULL;
    }
    ctx->source_kind = FBX_SOURCE_FILE;

    if (!fbx_detect_format(ctx)) {
        fclose(ctx->fp);
        ctx->fp = NULL;
        fbx_release(ctx);
        return NULL;
    }

    return ctx;
#else
    (void)path;
    return NULL;
#endif
}

FbxContext *fbx_open_memory(const void *data, fbx_i64 size)
{
    FbxContext *ctx;
    if (!data || size <= 0) return NULL;

    ctx = (FbxContext*)fbx_alloc_zero_n(1u, sizeof(FbxContext));
    if (!ctx) return NULL;
    fbx_context_init_common(ctx);

    ctx->mem = (const fbx_u8 *)data;
    ctx->mem_size = size;
    ctx->mem_pos = 0;
    ctx->source_kind = FBX_SOURCE_MEMORY;

    if (!fbx_detect_format(ctx)) {
        fbx_release(ctx);
        return NULL;
    }

    return ctx;
}

FbxContext *fbx_open_stream(const FbxStreamCallbacks *callbacks, void *user)
{
    FbxContext *ctx;

    if (!callbacks || !callbacks->read) return NULL;

    ctx = (FbxContext*)fbx_alloc_zero_n(1u, sizeof(FbxContext));
    if (!ctx) return NULL;
    fbx_context_init_common(ctx);

    ctx->stream = *callbacks;
    ctx->stream_user = user;
    ctx->stream_pos = 0;
    ctx->stream_size = callbacks->size ? callbacks->size(user) : (fbx_i64)-1;
    ctx->source_kind = FBX_SOURCE_STREAM;

    if (!fbx_detect_format(ctx)) {
        fbx_release(ctx);
        return NULL;
    }

    return ctx;
}

FbxContext *fbx_open_stream_read(FbxStreamReadFn read_fn, void *user, fbx_i64 known_size)
{
    FbxContext *ctx;
    if (!read_fn) return NULL;

    ctx = (FbxContext*)fbx_alloc_zero_n(1u, sizeof(FbxContext));
    if (!ctx) return NULL;
    fbx_context_init_common(ctx);

    ctx->stream.read = read_fn;
    ctx->stream.seek = NULL;
    ctx->stream.tell = NULL;
    ctx->stream.size = NULL;
    ctx->stream_user = user;
    ctx->stream_pos = 0;
    ctx->stream_size = known_size;
    ctx->source_kind = FBX_SOURCE_STREAM;

    if (!fbx_detect_format(ctx)) {
        fbx_release(ctx);
        return NULL;
    }

    return ctx;
}


static int fbx_property_to_int(const FbxProperty *p, int *out_value)
{
    fbx_i64 v64;
    fbx_i32 v32;
    fbx_i16 v16;

    if (!p || !out_value || !p->data || p->type != FBX_PROP_INT) return 0;

    if (p->byte_length == (fbx_i32)sizeof(fbx_i64)) {
        memcpy(&v64, p->data, sizeof(fbx_i64));
        if (v64 < (fbx_i64)-2147483647 - 1 || v64 > (fbx_i64)2147483647) return 0;
        *out_value = (int)v64;
        return 1;
    }
    if (p->byte_length == (fbx_i32)sizeof(fbx_i32)) {
        memcpy(&v32, p->data, sizeof(fbx_i32));
        *out_value = (int)v32;
        return 1;
    }
    if (p->byte_length == (fbx_i32)sizeof(fbx_i16)) {
        memcpy(&v16, p->data, sizeof(fbx_i16));
        *out_value = (int)v16;
        return 1;
    }
    if (p->byte_length == 1) {
        *out_value = (int)((const fbx_u8*)p->data)[0];
        return 1;
    }
    return 0;
}

static int fbx_tree_find_int_node(const FbxNode *node, const char *name, int *out_value)
{
    int i;

    if (!node || !name || !out_value) return 0;

    if (node->name && strcmp(node->name, name) == 0 && node->property_count > 0) {
        if (fbx_property_to_int(&node->properties[0], out_value)) return 1;
    }

    for (i = 0; i < node->child_count; ++i) {
        if (fbx_tree_find_int_node(node->children[i], name, out_value)) return 1;
    }
    return 0;
}

void fbx_close(FbxContext *ctx, FbxNode *root) {
    if (root) {
        fbx_node_destroy(root);
    }

    if (ctx) {
#ifndef FBX_NO_STDIO
        if (ctx->fp) {
            fclose(ctx->fp);
            ctx->fp = NULL;
        }
#endif
        fbx_release(ctx);
    }
}

FbxNode *fbx_read_scene(FbxContext *ctx) {
    FbxNode *root;
    int ascii_version;

    if (!ctx || ctx->source_kind == FBX_SOURCE_NONE) return NULL;

    if (ctx->is_binary) {
        root = fbx_read_scene_binary(ctx);
        if (root) {
            ascii_version = 0;
            if (fbx_tree_find_int_node(root, "FBXVersion", &ascii_version) &&
                ascii_version > ctx->version) {
                ctx->version = ascii_version;
            }
        }
        return root;
    }

    root = fbx_read_scene_ascii(ctx);
    if (root && ctx->version == 0) {
        ascii_version = 0;
        if (fbx_tree_find_int_node(root, "FBXVersion", &ascii_version)) {
            ctx->version = ascii_version;
        }
    }
    return root;
}

FbxNode *fbx_read_raw_tree(FbxContext *ctx)
{
    return fbx_read_scene(ctx);
}
