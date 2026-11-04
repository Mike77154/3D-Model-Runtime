#include <stdio.h>
#include <string.h>

#include "3dfbx.h"
#include "fbx_binary_writer.h"

#ifndef FBX_TOOL_OUT_CAPACITY
#define FBX_TOOL_OUT_CAPACITY (64u * 1024u * 1024u)
#endif

static unsigned char g_out[FBX_TOOL_OUT_CAPACITY];

static int parse_int_arg(const char *s, int fallback)
{
    int v;
    int neg;
    if (!s || !*s) return fallback;
    neg = 0;
    if (*s == '-') {
        neg = 1;
        s++;
    }
    v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    return neg ? -v : v;
}

int main(int argc, char **argv)
{
    const char *in_path;
    const char *out_path;
    FbxContext *ctx;
    FbxNode *root;
    FbxBinaryWriteOptions opt;
    FbxWriteBuffer wb;
    FBX_Result r;
    FILE *fp;

    if (argc < 3) {
        printf("Usage: %s <input.fbx> <output_binary.fbx> [version] [compress_arrays]\n", argv[0]);
        return 1;
    }

    in_path = argv[1];
    out_path = argv[2];
    fbx_binary_write_options_default(&opt);
    if (argc >= 4) opt.version = parse_int_arg(argv[3], 7400);
    if (argc >= 5) opt.compress_arrays = parse_int_arg(argv[4], 0) ? 1 : 0;

    ctx = fbx_open(in_path);
    if (!ctx) {
        printf("fbx_open failed: %s\n", in_path);
        return 2;
    }
    root = fbx_read_raw_tree(ctx);
    if (!root) {
        printf("fbx_read_raw_tree failed: %s\n", fbx_last_error(ctx));
        fbx_close(ctx, NULL);
        return 3;
    }

    fbx_write_buffer_init(&wb, g_out, sizeof(g_out));
    r = fbx_binary_write_tree(&wb, root, &opt);
    if (r != FBX_OK) {
        printf("fbx_binary_write_tree failed: %d\n", (int)r);
        fbx_close(ctx, root);
        return 4;
    }

    fp = fopen(out_path, "wb");
    if (!fp) {
        printf("cannot open output: %s\n", out_path);
        fbx_close(ctx, root);
        return 5;
    }
    if (wb.size != 0u && fwrite(g_out, 1u, wb.size, fp) != wb.size) {
        printf("write failed: %s\n", out_path);
        fclose(fp);
        fbx_close(ctx, root);
        return 6;
    }
    fclose(fp);
    printf("wrote %lu bytes version=%d compress_arrays=%d\n",
           (unsigned long)wb.size,
           opt.version,
           opt.compress_arrays);
    fbx_close(ctx, root);
    return 0;
}
