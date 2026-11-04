#include <stdio.h>
#include <string.h>

#include "3dfbx.h"

static int file_write_cb(void *user, const void *data, size_t size)
{
    FILE *fp;
    fp = (FILE*)user;
    return fwrite(data, 1u, size, fp) == size;
}

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
    FbxCodecWriteStream stream;
    FbxCodecWriteOptions opt;
    FILE *fp;

    if (argc < 3) {
        printf("Usage: %s <input.fbx> <output_ascii.fbx> [version]\n", argv[0]);
        return 1;
    }

    in_path = argv[1];
    out_path = argv[2];
    fbx_codec_write_options_default(&opt);
    if (argc >= 4) opt.version = parse_int_arg(argv[3], 7400);

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

    fp = fopen(out_path, "wb");
    if (!fp) {
        printf("cannot open output: %s\n", out_path);
        fbx_close(ctx, root);
        return 4;
    }
    stream.user = fp;
    stream.write = file_write_cb;
    if (!fbx_codec_write_ascii(root, &stream, &opt)) {
        printf("fbx_codec_write_ascii failed\n");
        fclose(fp);
        fbx_close(ctx, root);
        return 5;
    }
    fclose(fp);
    printf("wrote ASCII FBX: %s version=%d\n", out_path, opt.version);
    fbx_close(ctx, root);
    return 0;
}
