#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

#define WS_SIZE (16u * 1024u * 1024u)
static unsigned char g_ws[WS_SIZE];

typedef struct tri_probe_s { int triangles; modelrt_triangle_view first; } tri_probe;
static int on_triangle(void *u, const modelrt_triangle_view *t)
{
    tri_probe *p;
    p = (tri_probe *)u;
    if (p->triangles == 0) p->first = *t;
    ++p->triangles;
    return 0;
}

int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_animation_info ai;
    modelrt_visit_sink sink;
    tri_probe p;
    int rc;
    if (argc != 2) return 2;
    modelrt_runtime_init(&rt);
    if (modelrt_register_builtin_providers(&rt) != MODELRT_OK) return 3;
    modelrt_handle_init(&h, g_ws, sizeof(g_ws));
    rc = modelrt_open_file(&rt, &h, argv[1]);
    if (rc != MODELRT_OK) return 4;
    memset(&ai, 0, sizeof(ai));
    if (modelrt_animation_info_get(&h, 0, &ai) != MODELRT_OK) return 5;
    if (ai.frame_count != 3u || (ai.flags & MODELRT_ANIM_POSE_SAMPLER) == 0u ||
        (ai.flags & MODELRT_ANIM_TICK_RATE_KNOWN) != 0u) return 6;
    memset(&sink, 0, sizeof(sink));
    memset(&p, 0, sizeof(p));
    sink.on_triangle = on_triangle;
    rc = modelrt_visit_animation_frame(&h, 0, 1u, &sink, &p);
    if (rc != MODELRT_OK) return 7;
    if (p.triangles != 1) return 8;
    if (p.first.v[0].position.x < (MODELRT_Q16_ONE - 8) ||
        p.first.v[0].position.x > (MODELRT_Q16_ONE + 8)) return 9;
    puts("DirectX .x frame-addressed playback OK without AnimTicksPerSecond");
    modelrt_close(&h);
    return 0;
}
