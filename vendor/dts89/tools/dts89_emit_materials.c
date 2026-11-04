#include "dts89.h"
#include <stdio.h>

#define DTS89_EMIT_MAX_INPUT   (32u * 1024u * 1024u)
#define DTS89_EMIT_WORKSPACE   (16u * 1024u * 1024u)
#define DTS89_EMIT_OUTPUT      (8u * 1024u * 1024u)

static unsigned char g_input[DTS89_EMIT_MAX_INPUT];
static unsigned char g_workspace[DTS89_EMIT_WORKSPACE];
static unsigned char g_output[DTS89_EMIT_OUTPUT];

static long file_size(FILE *f)
{
    long cur;
    long end;
    cur = ftell(f);
    fseek(f, 0, SEEK_END);
    end = ftell(f);
    fseek(f, cur, SEEK_SET);
    return end;
}

int main(int argc, char **argv)
{
    FILE *f;
    FILE *outf;
    long sz;
    dts89_arena arena;
    dts89_shape shape;
    dts89_writer w;
    const char *ext;
    int rc;
    if (argc < 2) {
        printf("usage: %s file.dts [out.cs] [default_ext]\n", argv[0]);
        return 1;
    }
    ext = (argc >= 4) ? argv[3] : ".png";
    f = fopen(argv[1], "rb");
    if (!f) {
        printf("cannot open input\n");
        return 2;
    }
    sz = file_size(f);
    if (sz < 0 || (unsigned long)sz > (unsigned long)DTS89_EMIT_MAX_INPUT) {
        printf("input too large\n");
        fclose(f);
        return 3;
    }
    if (fread(g_input, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        return 4;
    }
    fclose(f);

    dts89_arena_init(&arena, g_workspace, sizeof(g_workspace));
    rc = dts89_decode_shape(g_input, (dts89_u32)sz, &arena, &shape);
    if (rc) {
        printf("decode failed: %d\n", rc);
        return 5;
    }
    dts89_writer_init(&w, g_output, sizeof(g_output));
    rc = dts89_emit_materials_cs(&shape, ext, &w);
    if (rc) {
        printf("emit failed: %d\n", rc);
        return 6;
    }

    outf = (argc >= 3) ? fopen(argv[2], "wb") : stdout;
    if (!outf) {
        printf("cannot open output\n");
        return 7;
    }
    fwrite(g_output, 1, (size_t)w.pos, outf);
    if (outf != stdout) fclose(outf);
    return 0;
}
