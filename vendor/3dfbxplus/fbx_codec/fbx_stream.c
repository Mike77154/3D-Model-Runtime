#include "fbx_stream.h"
#include "fbx_config.h"

#include <string.h>

int fbx_codec_decode_stream(const FbxCodecStreamSource *source,
                            int build_scene,
                            FbxCodecDecodeResult *out)
{
    FbxContext *ctx;
    FbxNode *root;
    FbxScene *scene;

    if (!out) return 0;
    fbx_codec_decode_result_init(out);
    if (!source || !source->read) {
        out->error_code = FBX_ERROR_INVALID_ARGUMENT;
        out->error_message = "invalid stream source";
        return 0;
    }

    ctx = fbx_open_stream_read(source->read, source->user, source->known_size);
    if (!ctx) {
        out->error_code = FBX_ERROR_IO;
        out->error_message = "fbx_open_stream failed";
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

typedef struct FbxStreamScannerTag {
    const FbxCodecStreamSource *source;
    const FbxStreamEvents *events;
    FbxStreamStats *stats;
    fbx_i64 pos;
    int version;
    int record_size;
} FbxStreamScanner;

static int fbx_scan_read(FbxStreamScanner *s, void *dst, size_t size)
{
    unsigned char *out;
    size_t got;
    size_t n;

    if (!s || !s->source || !s->source->read) return 0;
    out = (unsigned char*)dst;
    got = 0u;
    while (got < size) {
        n = s->source->read(s->source->user, out + got, size - got);
        if (n == 0u) return 0;
        got += n;
        s->pos += (fbx_i64)n;
        if (s->stats) s->stats->bytes_read += (fbx_i64)n;
    }
    return 1;
}

static int fbx_scan_skip(FbxStreamScanner *s, fbx_i64 amount)
{
    unsigned char scratch[256];
    size_t chunk;

    if (amount < 0) return 0;
    while (amount > 0) {
        chunk = sizeof(scratch);
        if (amount < (fbx_i64)chunk) chunk = (size_t)amount;
        if (!fbx_scan_read(s, scratch, chunk)) return 0;
        amount -= (fbx_i64)chunk;
    }
    return 1;
}

static fbx_u32 fbx_scan_le32(const unsigned char *b)
{
    return ((fbx_u32)b[0]) |
           ((fbx_u32)b[1] << 8) |
           ((fbx_u32)b[2] << 16) |
           ((fbx_u32)b[3] << 24);
}

static fbx_u64 fbx_scan_le64(const unsigned char *b)
{
    fbx_u64 lo;
    fbx_u64 hi;
    lo = (fbx_u64)fbx_scan_le32(b);
    hi = (fbx_u64)fbx_scan_le32(b + 4);
    return lo | (hi << 32);
}

static int fbx_scan_read_u8(FbxStreamScanner *s, unsigned char *v)
{
    return fbx_scan_read(s, v, 1u);
}

static int fbx_scan_read_u32(FbxStreamScanner *s, fbx_u32 *v)
{
    unsigned char b[4];
    if (!fbx_scan_read(s, b, 4u)) return 0;
    *v = fbx_scan_le32(b);
    return 1;
}

static int fbx_scan_read_u64(FbxStreamScanner *s, fbx_u64 *v)
{
    unsigned char b[8];
    if (!fbx_scan_read(s, b, 8u)) return 0;
    *v = fbx_scan_le64(b);
    return 1;
}

static int fbx_scan_read_header(FbxStreamScanner *s,
                                fbx_i64 *end_offset,
                                fbx_i64 *prop_count,
                                fbx_i64 *prop_bytes,
                                unsigned char *name_len)
{
    fbx_u32 u32;
    fbx_u64 u64;

    if (s->version >= 7500) {
        if (!fbx_scan_read_u64(s, &u64)) return 0;
        *end_offset = (fbx_i64)u64;
        if (!fbx_scan_read_u64(s, &u64)) return 0;
        *prop_count = (fbx_i64)u64;
        if (!fbx_scan_read_u64(s, &u64)) return 0;
        *prop_bytes = (fbx_i64)u64;
    } else {
        if (!fbx_scan_read_u32(s, &u32)) return 0;
        *end_offset = (fbx_i64)u32;
        if (!fbx_scan_read_u32(s, &u32)) return 0;
        *prop_count = (fbx_i64)u32;
        if (!fbx_scan_read_u32(s, &u32)) return 0;
        *prop_bytes = (fbx_i64)u32;
    }
    if (!fbx_scan_read_u8(s, name_len)) return 0;
    return 1;
}

static int fbx_scan_prop_payload(FbxStreamScanner *s,
                                 char type_code,
                                 FbxStreamPropertyInfo *info)
{
    fbx_u32 len;
    fbx_u32 count;
    fbx_u32 enc;
    fbx_u32 payload_len;
    fbx_i64 bytes;
    int is_array;

    info->type_code = type_code;
    info->count = 1;
    info->byte_length = 0;
    info->encoding = 0;
    is_array = 0;
    bytes = 0;

    switch (type_code) {
        case 'C': case 'B': bytes = 1; break;
        case 'Y': bytes = 2; break;
        case 'I': case 'F': bytes = 4; break;
        case 'L': case 'D': bytes = 8; break;
        case 'S': case 'R':
            if (!fbx_scan_read_u32(s, &len)) return 0;
            info->byte_length = (fbx_i64)len;
            return fbx_scan_skip(s, (fbx_i64)len);
        case 'b': case 'c': bytes = 1; is_array = 1; break;
        case 'y': bytes = 2; is_array = 1; break;
        case 'i': case 'f': bytes = 4; is_array = 1; break;
        case 'l': case 'd': bytes = 8; is_array = 1; break;
        default:
            return 0;
    }

    if (!is_array) {
        info->byte_length = bytes;
        return fbx_scan_skip(s, bytes);
    }

    if (!fbx_scan_read_u32(s, &count)) return 0;
    if (!fbx_scan_read_u32(s, &enc)) return 0;
    if (!fbx_scan_read_u32(s, &payload_len)) return 0;
    info->count = (fbx_i64)count;
    info->encoding = (int)enc;
    info->byte_length = (fbx_i64)payload_len;
    if (s->stats) {
        s->stats->arrays += 1;
        if (enc != 0u) s->stats->compressed_arrays += 1;
    }
    return fbx_scan_skip(s, (fbx_i64)payload_len);
}

static int fbx_scan_children(FbxStreamScanner *s, fbx_i64 stop, int depth)
{
    char name[256];
    fbx_i64 end_offset;
    fbx_i64 prop_count;
    fbx_i64 prop_bytes;
    fbx_i64 prop_start;
    fbx_i64 i;
    unsigned char name_len;
    unsigned char type;
    FbxStreamPropertyInfo info;

    if (depth > FBX_DEFAULT_MAX_DEPTH) return 0;
    if (s->stats && depth > s->stats->max_depth) s->stats->max_depth = depth;

    while (s->pos + (fbx_i64)s->record_size <= stop) {
        if (!fbx_scan_read_header(s, &end_offset, &prop_count, &prop_bytes, &name_len)) return 0;
        if (end_offset == 0 && prop_count == 0 && prop_bytes == 0 && name_len == 0) return 1;
        if (end_offset <= s->pos || end_offset > stop) return 0;
        if (prop_count < 0 || prop_count > FBX_DEFAULT_MAX_PROPERTIES_PER_NODE) return 0;
        if (prop_bytes < 0 || prop_bytes > (fbx_i64)FBX_DEFAULT_MAX_ARRAY_BYTES) return 0;
        if (!fbx_scan_read(s, name, (size_t)name_len)) return 0;
        name[name_len] = '\0';

        if (s->stats) s->stats->nodes += 1;
        if (s->events && s->events->begin_node) {
            if (!s->events->begin_node(s->events->user, name, depth, end_offset, prop_count, prop_bytes)) return 0;
        }

        prop_start = s->pos;
        for (i = 0; i < prop_count; ++i) {
            if (!fbx_scan_read_u8(s, &type)) return 0;
            if (!fbx_scan_prop_payload(s, (char)type, &info)) return 0;
            if (s->stats) s->stats->properties += 1;
            if (s->events && s->events->property) {
                if (!s->events->property(s->events->user, name, depth, &info)) return 0;
            }
        }
        if (s->pos - prop_start != prop_bytes) return 0;

        if (s->pos < end_offset) {
            if (!fbx_scan_children(s, end_offset, depth + 1)) return 0;
            if (s->pos < end_offset) {
                if (!fbx_scan_skip(s, end_offset - s->pos)) return 0;
            }
            if (s->pos != end_offset) return 0;
        }

        if (s->events && s->events->end_node) {
            if (!s->events->end_node(s->events->user, name, depth, end_offset)) return 0;
        }
    }
    return 1;
}

int fbx_stream_scan_binary(const FbxCodecStreamSource *source,
                           const FbxStreamEvents *events,
                           FbxStreamStats *stats)
{
    FbxStreamScanner s;
    unsigned char magic[23];
    fbx_u32 version;

    if (stats) memset(stats, 0, sizeof(*stats));
    if (!source || !source->read || source->known_size <= 27) return 0;
    memset(&s, 0, sizeof(s));
    s.source = source;
    s.events = events;
    s.stats = stats;

    if (!fbx_scan_read(&s, magic, 23u)) return 0;
    if (memcmp(magic, "Kaydara FBX Binary  \0\x1a\0", 23u) != 0) return 0;
    if (!fbx_scan_read_u32(&s, &version)) return 0;
    s.version = (int)version;
    s.record_size = (s.version >= 7500) ? 25 : 13;
    if (stats) {
        stats->is_binary = 1;
        stats->version = s.version;
    }
    return fbx_scan_children(&s, source->known_size, 0);
}
