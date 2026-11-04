#include "dts89.h"
#include <stdio.h>

#define DTS89_EXAMPLE_MAX_INPUT   (32u * 1024u * 1024u)
#define DTS89_EXAMPLE_MAX_OUTPUT  (48u * 1024u * 1024u)
#define DTS89_EXAMPLE_WORKSPACE   (16u * 1024u * 1024u)
#define DTS89_EXAMPLE_DETACHED    (16u * 1024u * 1024u)

static unsigned char g_input[DTS89_EXAMPLE_MAX_INPUT];
static unsigned char g_output[DTS89_EXAMPLE_MAX_OUTPUT];
static unsigned char g_workspace[DTS89_EXAMPLE_WORKSPACE];
static unsigned char g_detached[DTS89_EXAMPLE_DETACHED];

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
    long sz;
    dts89_arena arena;
    dts89_arena detached;
    dts89_shape shape;
    dts89_writer w;
    int rc;
    if (argc < 3) {
        printf("usage: %s in.dts out.dts\n", argv[0]);
        return 1;
    }
    f = fopen(argv[1], "rb");
    if (!f) {
        printf("cannot open input\n");
        return 2;
    }
    sz = file_size(f);
    if (sz < 0 || (unsigned long)sz > (unsigned long)DTS89_EXAMPLE_MAX_INPUT) {
        printf("input too large for fixed buffers\n");
        fclose(f);
        return 3;
    }
    if (fread(g_input, 1, (size_t)sz, f) != (size_t)sz) {
        printf("read failed\n");
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

    dts89_arena_init(&detached, g_detached, sizeof(g_detached));
    rc = dts89_detach_shape(&shape, &detached);
    if (rc) {
        printf("detach failed: %d\n", rc);
        return 6;
    }

    printf("version=%u exporter=%u nodes=%d objects=%d meshes=%d names=%d seq=%d\n",
           (unsigned)shape.header.read_version,
           (unsigned)shape.header.exporter_version,
           (int)shape.num_nodes,
           (int)shape.num_objects,
           (int)shape.num_meshes,
           (int)shape.num_names,
           (int)shape.num_sequences);

    dts89_writer_init(&w, g_output, sizeof(g_output));
    rc = dts89_encode_shape(&shape, &w, 0);
    if (rc) {
        printf("encode failed: %d\n", rc);
        return 7;
    }

    f = fopen(argv[2], "wb");
    if (!f) {
        printf("cannot open output\n");
        return 8;
    }
    fwrite(g_output, 1, w.pos, f);
    fclose(f);
    return 0;
}
