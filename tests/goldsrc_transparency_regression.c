#include "../include/model_runtime89.h"
#include <stdio.h>
#include <string.h>

#define WS_BYTES (2u * 1024u * 1024u)
static unsigned char g_ws[WS_BYTES];
static unsigned char g_rgba[64];

typedef struct capture_s {
    int got_material;
    modelrt_u32 flags;
} capture;

static int on_material(void *user, const modelrt_material_view *material)
{
    capture *c;
    c = (capture *)user;
    if (material->index == 0u) {
        c->got_material = 1;
        c->flags = material->flags;
    }
    return 0;
}

int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_texture_info ti;
    modelrt_visit_sink sink;
    capture c;
    modelrt_u32 w;
    modelrt_u32 he;
    size_t used;
    int rc;

    if (argc != 2) return 2;
    modelrt_runtime_init(&rt);
    modelrt_register_builtin_providers(&rt);
    modelrt_handle_init(&h, g_ws, sizeof(g_ws));
    rc = modelrt_open_file(&rt, &h, argv[1]);
    if (rc != MODELRT_OK) {
        printf("open failed: %s\n", modelrt_last_error(&rt));
        return 3;
    }
    if (h.info.codec_id != MODELRT_CODEC_GOLDSRC) return 4;
    if (modelrt_texture_count(&h) != 1) return 5;

    memset(&ti, 0, sizeof(ti));
    rc = modelrt_texture_info_get(&h, 0, &ti);
    if (rc != MODELRT_OK) return 6;
    if ((ti.flags & MODELRT_MAT_ALPHA_MASK) == 0u) {
        printf("masked flag missing in texture info: %08x\n", (unsigned)ti.flags);
        return 7;
    }

    memset(&sink, 0, sizeof(sink));
    sink.on_material = on_material;
    memset(&c, 0, sizeof(c));
    rc = modelrt_visit(&h, MODELRT_ANIM_BIND_POSE, 0, &sink, &c);
    if (rc != MODELRT_OK) return 8;
    if (!c.got_material || (c.flags & MODELRT_MAT_ALPHA_MASK) == 0u) {
        printf("masked flag missing in material view: got=%d flags=%08x\n",
               c.got_material, (unsigned)c.flags);
        return 9;
    }

    w = he = 0u;
    used = 0u;
    memset(g_rgba, 0, sizeof(g_rgba));
    rc = modelrt_texture_decode_rgba(&h, 0, g_rgba, sizeof(g_rgba), &w, &he, &used);
    if (rc != MODELRT_OK) return 10;
    if (w != 2u || he != 2u || used != 16u) return 11;
    if (g_rgba[3] != 255u || g_rgba[7] != 0u ||
        g_rgba[11] != 255u || g_rgba[15] != 0u) {
        printf("unexpected alpha bytes: %u %u %u %u\n",
               (unsigned)g_rgba[3], (unsigned)g_rgba[7],
               (unsigned)g_rgba[11], (unsigned)g_rgba[15]);
        return 12;
    }

    printf("GoldSrc transparency regression OK: flags=%08x alpha=%u,%u,%u,%u\n",
           (unsigned)c.flags,
           (unsigned)g_rgba[3], (unsigned)g_rgba[7],
           (unsigned)g_rgba[11], (unsigned)g_rgba[15]);
    modelrt_close(&h);
    return 0;
}
