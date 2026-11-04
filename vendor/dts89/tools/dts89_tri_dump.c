#include "dts89.h"
#include <stdio.h>

#define DTS89_TRI_MAX_INPUT   (32u * 1024u * 1024u)
#define DTS89_TRI_WORKSPACE   (16u * 1024u * 1024u)
#define DTS89_TRI_MAX_SPANS   8u
#define DTS89_TRI_MAX_DRAWS   8192u

static unsigned char g_input[DTS89_TRI_MAX_INPUT];
static unsigned char g_workspace[DTS89_TRI_WORKSPACE];
static dts89_render_span g_spans[DTS89_TRI_MAX_SPANS];
static dts89_draw_command g_draws[DTS89_TRI_MAX_DRAWS];

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

static int parse_u32_arg(const char *s, dts89_u32 *out)
{
    dts89_u32 v;
    if (!s || !out) return 0;
    v = 0u;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        if (v > (0xFFFFFFFFu - (dts89_u32)(*s - '0')) / 10u) return 0;
        v = v * 10u + (dts89_u32)(*s - '0');
        s++;
    }
    *out = v;
    return 1;
}

int main(int argc, char **argv)
{
    FILE *f;
    long sz;
    dts89_arena arena;
    dts89_shape shape;
    dts89_runtime_options opt;
    dts89_runtime_shape rt;
    dts89_detail_choice choice;
    dts89_u32 pixel;
    dts89_u32 span_count;
    dts89_u32 i;
    dts89_u32 total_draws;
    dts89_u32 total_tris;
    int rc;
    if (argc < 2) {
        printf("usage: %s file.dts [pixel_size_int]\n", argv[0]);
        return 1;
    }
    pixel = 64u;
    if (argc > 2 && !parse_u32_arg(argv[2], &pixel)) {
        printf("bad pixel size\n");
        return 2;
    }
    f = fopen(argv[1], "rb");
    if (!f) return 3;
    sz = file_size(f);
    if (sz < 0 || (unsigned long)sz > (unsigned long)DTS89_TRI_MAX_INPUT) {
        fclose(f);
        return 4;
    }
    if (fread(g_input, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        return 5;
    }
    fclose(f);
    dts89_arena_init(&arena, g_workspace, sizeof(g_workspace));
    rc = dts89_decode_shape(g_input, (dts89_u32)sz, &arena, &shape);
    if (rc) {
        printf("decode failed: %d\n", rc);
        return 6;
    }
    dts89_runtime_options_default(&opt);
    rc = dts89_runtime_build(&shape, &opt, &arena, &rt);
    if (rc) return 7;
    rc = dts89_runtime_select_detail_from_pixel_size(&shape, &rt, dts89_fx_from_int((dts89_s32)pixel), &choice);
    if (rc) return 8;
    rc = dts89_runtime_make_render_spans(&shape, &rt, &choice, 0u, g_spans, DTS89_TRI_MAX_SPANS, &span_count);
    if (rc && rc != DTS89_ERR_NO_SPACE) return 9;
    printf("detail=%d spans=%u\n", (int)choice.detail_level, (unsigned)span_count);
    if (span_count > DTS89_TRI_MAX_SPANS) span_count = DTS89_TRI_MAX_SPANS;
    total_draws = 0u;
    total_tris = 0u;
    for (i = 0u; i < span_count; ++i) {
        dts89_u32 draw_count;
        dts89_u32 j;
        rc = dts89_runtime_emit_draws_for_span(&shape, &g_spans[i], g_draws, DTS89_TRI_MAX_DRAWS, &draw_count);
        if (rc && rc != DTS89_ERR_NO_SPACE) return 10;
        if (draw_count > DTS89_TRI_MAX_DRAWS) draw_count = DTS89_TRI_MAX_DRAWS;
        for (j = 0u; j < draw_count; ++j) {
            dts89_u32 tris;
            rc = dts89_runtime_count_triangles_for_draw(&shape, &g_draws[j], &tris);
            if (rc) return 11;
            total_tris += tris;
            total_draws++;
            printf("draw[%u]: object=%d mesh=%d prim=%d mat=%d type=0x%08X indices=%d tris=%u\n", (unsigned)total_draws - 1u, (int)g_draws[j].object_index, (int)g_draws[j].mesh_index, (int)g_draws[j].primitive_index, (int)g_draws[j].material_index, (unsigned)g_draws[j].primitive_type, (int)g_draws[j].index_count, (unsigned)tris);
        }
    }
    printf("totals draws=%u tris=%u\n", (unsigned)total_draws, (unsigned)total_tris);
    return 0;
}
