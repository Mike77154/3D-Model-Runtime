#include "fbx_codec_writer.h"
#include "fbx_arrays.h"
#include "fbx_fixed.h"

#include <string.h>

static int fbxw_write(const FbxCodecWriteStream *s, const void *data, size_t size)
{
    if (!s || !s->write) return 0;
    if (size == 0u) return 1;
    return s->write(s->user, data, size) != 0;
}

static int fbxw_cstr(const FbxCodecWriteStream *s, const char *text)
{
    if (!text) text = "";
    return fbxw_write(s, text, strlen(text));
}

static int fbxw_char(const FbxCodecWriteStream *s, char c)
{
    return fbxw_write(s, &c, 1u);
}

static int fbxw_indent(const FbxCodecWriteStream *s, int depth)
{
    int i;
    for (i = 0; i < depth; ++i) {
        if (!fbxw_cstr(s, "    ")) return 0;
    }
    return 1;
}

static int fbxw_u64_dec(const FbxCodecWriteStream *s, fbx_u64 v)
{
    char tmp[32];
    int n;
    n = 0;
    do {
        fbx_u64 q;
        fbx_u64 r;
        q = v / 10u;
        r = v - q * 10u;
        tmp[n++] = (char)('0' + (char)r);
        v = q;
    } while (v != 0u && n < (int)sizeof(tmp));
    while (n > 0) {
        if (!fbxw_char(s, tmp[--n])) return 0;
    }
    return 1;
}

static int fbxw_i64_dec(const FbxCodecWriteStream *s, fbx_i64 v)
{
    fbx_u64 u;
    if (v < 0) {
        if (!fbxw_char(s, '-')) return 0;
        u = (fbx_u64)(~(fbx_u64)v) + 1u;
    } else {
        u = (fbx_u64)v;
    }
    return fbxw_u64_dec(s, u);
}

static int fbxw_real_dec(const FbxCodecWriteStream *s, fbx_real v)
{
    fbx_i64 sv;
    fbx_u64 av;
    fbx_u64 ip;
    fbx_u64 frac;
    fbx_u64 dec;
    char digits[6];
    int i;

    sv = (fbx_i64)v;
    if (sv < 0) {
        if (!fbxw_char(s, '-')) return 0;
        av = (fbx_u64)(~(fbx_u64)sv) + 1u;
    } else {
        av = (fbx_u64)sv;
    }
    ip = av >> FBX_REAL_SHIFT;
    frac = av & (((fbx_u64)1u << FBX_REAL_SHIFT) - 1u);
    dec = (frac * (fbx_u64)1000000u + (fbx_u64)FBX_REAL_HALF) >> FBX_REAL_SHIFT;
    if (dec >= (fbx_u64)1000000u) {
        ip += 1u;
        dec -= (fbx_u64)1000000u;
    }
    if (!fbxw_u64_dec(s, ip)) return 0;
    if (!fbxw_char(s, '.')) return 0;
    for (i = 5; i >= 0; --i) {
        digits[i] = (char)('0' + (char)(dec % 10u));
        dec /= 10u;
    }
    return fbxw_write(s, digits, sizeof(digits));
}

static int fbxw_quoted(const FbxCodecWriteStream *s, const char *text)
{
    const char *p;
    if (!fbxw_char(s, '"')) return 0;
    if (text) {
        p = text;
        while (*p) {
            if (*p == '"' || *p == '\\') {
                if (!fbxw_char(s, '\\')) return 0;
                if (!fbxw_char(s, *p)) return 0;
            } else if (*p == '\n') {
                if (!fbxw_cstr(s, "\\n")) return 0;
            } else if (*p == '\r') {
                if (!fbxw_cstr(s, "\\r")) return 0;
            } else if (*p == '\t') {
                if (!fbxw_cstr(s, "\\t")) return 0;
            } else {
                if (!fbxw_char(s, *p)) return 0;
            }
            ++p;
        }
    }
    return fbxw_char(s, '"');
}

static int fbxw_prop_int_scalar(const FbxCodecWriteStream *s, const FbxProperty *p)
{
    if (!p || !p->data) return fbxw_cstr(s, "0");
    if (p->type_code == 'L' && p->byte_length >= (fbx_i32)sizeof(fbx_i64)) {
        fbx_i64 v;
        memcpy(&v, p->data, sizeof(v));
        return fbxw_i64_dec(s, v);
    }
    if (p->type_code == 'I' && p->byte_length >= (fbx_i32)sizeof(fbx_i32)) {
        fbx_i32 v;
        memcpy(&v, p->data, sizeof(v));
        return fbxw_i64_dec(s, (fbx_i64)v);
    }
    if (p->type_code == 'Y' && p->byte_length >= (fbx_i32)sizeof(fbx_i16)) {
        fbx_i16 v;
        memcpy(&v, p->data, sizeof(v));
        return fbxw_i64_dec(s, (fbx_i64)v);
    }
    if (p->type_code == 'C' && p->byte_length >= 1) {
        return fbxw_i64_dec(s, (fbx_i64)((const fbx_u8*)p->data)[0]);
    }
    return fbxw_cstr(s, "0");
}

static int fbxw_prop_real_scalar(const FbxCodecWriteStream *s, const FbxProperty *p)
{
    fbx_real v;
    if (!p || !p->data || p->byte_length < (fbx_i32)sizeof(fbx_real)) return fbxw_cstr(s, "0.0");
    memcpy(&v, p->data, sizeof(v));
    return fbxw_real_dec(s, v);
}

static int fbxw_prop_array_prefix(const FbxCodecWriteStream *s, fbx_u32 count)
{
    if (!fbxw_char(s, '*')) return 0;
    if (!fbxw_u64_dec(s, (fbx_u64)count)) return 0;
    return fbxw_cstr(s, " { a: ");
}

static int fbxw_prop_array_suffix(const FbxCodecWriteStream *s)
{
    return fbxw_cstr(s, " }");
}

static int fbxw_prop_array_i32(const FbxCodecWriteStream *s, const FbxProperty *p)
{
    fbx_i32 *arr;
    fbx_u32 count;
    fbx_u32 i;
    if (fbx_property_as_int32_array(p, &arr, &count) != FBX_OK) return fbxw_cstr(s, "*0 { a: }");
    if (!fbxw_prop_array_prefix(s, count)) return 0;
    for (i = 0u; i < count; ++i) {
        if (i != 0u && !fbxw_char(s, ',')) return 0;
        if (!fbxw_i64_dec(s, (fbx_i64)arr[i])) return 0;
    }
    fbx_release(arr);
    return fbxw_prop_array_suffix(s);
}

static int fbxw_prop_array_i64(const FbxCodecWriteStream *s, const FbxProperty *p)
{
    fbx_i64 *arr;
    fbx_u32 count;
    fbx_u32 i;
    if (fbx_property_as_int64_array(p, &arr, &count) != FBX_OK) return fbxw_cstr(s, "*0 { a: }");
    if (!fbxw_prop_array_prefix(s, count)) return 0;
    for (i = 0u; i < count; ++i) {
        if (i != 0u && !fbxw_char(s, ',')) return 0;
        if (!fbxw_i64_dec(s, arr[i])) return 0;
    }
    fbx_release(arr);
    return fbxw_prop_array_suffix(s);
}

static int fbxw_prop_array_real(const FbxCodecWriteStream *s, const FbxProperty *p)
{
    fbx_real *arr;
    fbx_u32 count;
    fbx_u32 i;
    if (fbx_property_as_real_array(p, &arr, &count) != FBX_OK) {
        if (fbx_property_as_real64_array(p, &arr, &count) != FBX_OK) return fbxw_cstr(s, "*0 { a: }");
    }
    if (!fbxw_prop_array_prefix(s, count)) return 0;
    for (i = 0u; i < count; ++i) {
        if (i != 0u && !fbxw_char(s, ',')) return 0;
        if (!fbxw_real_dec(s, arr[i])) return 0;
    }
    fbx_release(arr);
    return fbxw_prop_array_suffix(s);
}

static int fbxw_property(const FbxCodecWriteStream *s, const FbxProperty *p)
{
    if (!p) return 0;
    if (p->type == FBX_PROP_STRING) return fbxw_quoted(s, (const char*)p->data);
    if (p->type == FBX_PROP_RAW) {
        if (!fbxw_cstr(s, "RawBytes:")) return 0;
        return fbxw_i64_dec(s, (fbx_i64)p->byte_length);
    }
    if (p->type == FBX_PROP_INT) return fbxw_prop_int_scalar(s, p);
    if (p->type == FBX_PROP_REAL) return fbxw_prop_real_scalar(s, p);
    if (p->type == FBX_PROP_ARRAY_INT) {
        if (p->type_code == 'l') return fbxw_prop_array_i64(s, p);
        return fbxw_prop_array_i32(s, p);
    }
    if (p->type == FBX_PROP_ARRAY_REAL) return fbxw_prop_array_real(s, p);
    return fbxw_cstr(s, "0");
}

static int fbxw_node_ascii(const FbxNode *node, const FbxCodecWriteStream *s, int depth)
{
    int i;
    if (!node || !node->name) return 1;
    if (!fbxw_indent(s, depth)) return 0;
    if (!fbxw_cstr(s, node->name)) return 0;
    if (!fbxw_cstr(s, ": ")) return 0;
    for (i = 0; i < node->property_count; ++i) {
        if (i != 0 && !fbxw_cstr(s, ", ")) return 0;
        if (!fbxw_property(s, &node->properties[i])) return 0;
    }
    if (node->child_count > 0) {
        if (!fbxw_cstr(s, " {\n")) return 0;
        for (i = 0; i < node->child_count; ++i) {
            if (!fbxw_node_ascii(node->children[i], s, depth + 1)) return 0;
        }
        if (!fbxw_indent(s, depth)) return 0;
        if (!fbxw_cstr(s, "}\n")) return 0;
    } else {
        if (!fbxw_cstr(s, "\n")) return 0;
    }
    return 1;
}

void fbx_codec_write_options_default(FbxCodecWriteOptions *opt)
{
    if (!opt) return;
    opt->version = 7400;
    opt->emit_header_comment = 1;
}

int fbx_codec_write_ascii(const FbxNode *root,
                          const FbxCodecWriteStream *stream,
                          const FbxCodecWriteOptions *options)
{
    FbxCodecWriteOptions def_opt;
    const FbxCodecWriteOptions *opt;
    int i;

    if (!root || !stream || !stream->write) return 0;
    fbx_codec_write_options_default(&def_opt);
    opt = options ? options : &def_opt;

    if (opt->emit_header_comment) {
        if (!fbxw_cstr(stream, "; FBX ASCII exported by 3dfbx codec layer\n")) return 0;
        if (!fbxw_cstr(stream, "; Version: ")) return 0;
        if (!fbxw_i64_dec(stream, (fbx_i64)opt->version)) return 0;
        if (!fbxw_cstr(stream, "\n\n")) return 0;
    }

    if (root->name && strcmp(root->name, "Root") == 0) {
        for (i = 0; i < root->child_count; ++i) {
            if (!fbxw_node_ascii(root->children[i], stream, 0)) return 0;
        }
        return 1;
    }
    return fbxw_node_ascii(root, stream, 0);
}
