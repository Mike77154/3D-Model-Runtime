#include "dts89.h"
#include <stdio.h>

#define DTS89_DSQ_MAX_INPUT    (16u * 1024u * 1024u)
#define DTS89_DSQ_WORKSPACE    (16u * 1024u * 1024u)

static unsigned char g_input[DTS89_DSQ_MAX_INPUT];
static unsigned char g_workspace[DTS89_DSQ_WORKSPACE];

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
    dts89_dsq_file dsq;
    dts89_s32 i;
    int rc;
    if (argc < 2) {
        printf("usage: %s file.dsq\n", argv[0]);
        return 1;
    }
    f = fopen(argv[1], "rb");
    if (!f) {
        printf("cannot open input\n");
        return 2;
    }
    sz = file_size(f);
    if (sz < 0 || (unsigned long)sz > (unsigned long)DTS89_DSQ_MAX_INPUT) {
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
    rc = dts89_decode_dsq(g_input, (dts89_u32)sz, &arena, &dsq);
    if (rc) {
        printf("decode failed: %d\n", rc);
        return 5;
    }

    printf("version=%d exporter=%d nodes=%d objects=%d sequences=%d triggers=%d\n",
           (int)dsq.version, (int)dsq.exporter_version, (int)dsq.num_nodes,
           (int)dsq.num_objects, (int)dsq.num_sequences, (int)dsq.num_triggers);
    for (i = 0; i < dsq.num_nodes; ++i) {
        printf("node[%d]=%s\n", (int)i, dsq.node_names[i].ptr ? dsq.node_names[i].ptr : "");
    }
    for (i = 0; i < dsq.num_sequences; ++i) {
        printf("sequence[%d]=%s keyframes=%d flags=0x%08X\n",
               (int)i,
               dsq.sequences[i].name.ptr ? dsq.sequences[i].name.ptr : "",
               (int)dsq.sequences[i].num_keyframes,
               (unsigned)dsq.sequences[i].flags);
    }
    return 0;
}
