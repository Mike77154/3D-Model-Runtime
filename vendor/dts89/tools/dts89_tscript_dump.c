#include "dts89.h"
#include <stdio.h>

#define DTS89_TS_MAX_INPUT   (2u * 1024u * 1024u)
#define DTS89_TS_WORKSPACE   (4u * 1024u * 1024u)

static unsigned char g_input[DTS89_TS_MAX_INPUT];
static unsigned char g_workspace[DTS89_TS_WORKSPACE];

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
    dts89_ts_file file;
    dts89_s32 i, j;
    int rc;
    if (argc < 2) {
        printf("usage: %s materials.cs\n", argv[0]);
        return 1;
    }
    f = fopen(argv[1], "rb");
    if (!f) {
        printf("cannot open input\n");
        return 2;
    }
    sz = file_size(f);
    if (sz < 0 || (unsigned long)sz > (unsigned long)DTS89_TS_MAX_INPUT) {
        fclose(f);
        return 3;
    }
    if (fread(g_input, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        return 4;
    }
    fclose(f);

    dts89_arena_init(&arena, g_workspace, sizeof(g_workspace));
    rc = dts89_tscript_parse_singletons(g_input, (dts89_u32)sz, &arena, &file);
    if (rc) {
        printf("parse failed: %d\n", rc);
        return 5;
    }
    printf("objects=%d\n", (int)file.num_objects);
    for (i = 0; i < file.num_objects; ++i) {
        printf("[%d] %s(%s) assignments=%d\n",
               (int)i,
               file.objects[i].class_name.ptr,
               file.objects[i].object_name.ptr,
               (int)file.objects[i].num_assignments);
        for (j = 0; j < file.objects[i].num_assignments; ++j) {
            const dts89_ts_assignment *a = &file.objects[i].assignments[j];
            if (a->array_index >= 0)
                printf("  %s[%d] = %s%s%s\n", a->key.ptr, (int)a->array_index,
                       a->value_was_quoted ? "\"" : "", a->value.ptr,
                       a->value_was_quoted ? "\"" : "");
            else
                printf("  %s = %s%s%s\n", a->key.ptr,
                       a->value_was_quoted ? "\"" : "", a->value.ptr,
                       a->value_was_quoted ? "\"" : "");
        }
    }
    return 0;
}
