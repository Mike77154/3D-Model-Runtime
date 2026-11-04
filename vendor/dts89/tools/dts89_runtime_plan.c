#include "dts89.h"
#include <stdio.h>

#define DTS89_PLAN_MAX_INPUT   (32u * 1024u * 1024u)
#define DTS89_PLAN_WORKSPACE   (16u * 1024u * 1024u)
#define DTS89_PLAN_MAX_SPANS   8u

static unsigned char g_input[DTS89_PLAN_MAX_INPUT];
static unsigned char g_workspace[DTS89_PLAN_WORKSPACE];
static dts89_render_span g_spans[DTS89_PLAN_MAX_SPANS];

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
    if (!f) {
        printf("cannot open input\n");
        return 3;
    }
    sz = file_size(f);
    if (sz < 0 || (unsigned long)sz > (unsigned long)DTS89_PLAN_MAX_INPUT) {
        printf("input too large\n");
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
    if (rc) {
        printf("runtime build failed: %d\n", rc);
        return 7;
    }
    rc = dts89_runtime_select_detail_from_pixel_size(&shape, &rt, dts89_fx_from_int((dts89_s32)pixel), &choice);
    if (rc) {
        printf("select detail failed: %d\n", rc);
        return 8;
    }
    rc = dts89_runtime_make_render_spans(&shape, &rt, &choice, 0u, g_spans, DTS89_PLAN_MAX_SPANS, &span_count);
    if (rc && rc != DTS89_ERR_NO_SPACE) {
        printf("span build failed: %d\n", rc);
        return 9;
    }
    printf("pixel=%u detail=%d intra=%u spans=%u trans=%s screen_error=%s\n", (unsigned)pixel, (int)choice.detail_level, (unsigned)choice.intra, (unsigned)span_count, (rt.flags & DTS89_SHAPE_HAS_TRANS) ? "yes" : "no", rt.use_detail_from_screen_error ? "yes" : "no");
    if (span_count > DTS89_PLAN_MAX_SPANS) span_count = DTS89_PLAN_MAX_SPANS;
    for (i = 0u; i < span_count; ++i) {
        dts89_u32 draws;
        rc = dts89_runtime_emit_draws_for_span(&shape, &g_spans[i], 0, 0u, &draws);
        if (rc) return 10;
        printf("span[%u]: detail=%d subshape=%d object_detail=%d objects=%d..%d alpha=%u flags=0x%08X draw_prims=%u\n", (unsigned)i, (int)g_spans[i].detail_level, (int)g_spans[i].subshape_index, (int)g_spans[i].object_detail, (int)g_spans[i].object_start, (int)g_spans[i].object_end, (unsigned)g_spans[i].alpha, (unsigned)g_spans[i].flags, (unsigned)draws);
    }
    return 0;
}
