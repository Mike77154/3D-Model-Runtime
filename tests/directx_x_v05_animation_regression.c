#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

#define WS_SIZE (16u * 1024u * 1024u)
static unsigned char g_ws[WS_SIZE];

typedef struct tri_probe_s {
    int triangles;
    modelrt_triangle_view first;
} tri_probe;

static int on_triangle(void *u, const modelrt_triangle_view *t)
{
    tri_probe *p;
    p = (tri_probe *)u;
    if (p->triangles == 0) p->first = *t;
    ++p->triangles;
    return 0;
}

static int check_multiclip(const char *path)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_animation_info ai;
    modelrt_animation_frame_info fi;
    int rc;
    modelrt_runtime_init(&rt);
    if (modelrt_register_builtin_providers(&rt) != MODELRT_OK) return 10;
    modelrt_handle_init(&h, g_ws, sizeof(g_ws));
    rc = modelrt_open_file(&rt, &h, path);
    if (rc != MODELRT_OK) return 11;
    if (modelrt_animation_count(&h) != 2) return 12;

    memset(&ai, 0, sizeof(ai));
    if (modelrt_animation_info_get(&h, 0, &ai) != MODELRT_OK) return 13;
    if (strcmp(ai.name, "Walk") != 0 || ai.frame_count != 3u ||
        ai.source_key_count != 2u || ai.first_tick != 0 || ai.last_tick != 2) return 14;
    if ((ai.flags & MODELRT_ANIM_DENSE_FRAMES) == 0u ||
        (ai.flags & MODELRT_ANIM_POSE_SAMPLER) == 0u ||
        (ai.flags & MODELRT_ANIM_UNBOUND) != 0u) return 15;
    if ((ai.flags & MODELRT_ANIM_TICK_RATE_KNOWN) != 0u) return 16;

    memset(&ai, 0, sizeof(ai));
    if (modelrt_animation_info_get(&h, 1, &ai) != MODELRT_OK) return 17;
    if (strcmp(ai.name, "MatrixClip") != 0 || ai.frame_count != 3u ||
        ai.source_key_count != 2u || ai.first_tick != 10 || ai.last_tick != 12) return 18;
    memset(&fi, 0, sizeof(fi));
    if (modelrt_animation_frame_info_get(&h, 1, 1u, &fi) != MODELRT_OK) return 19;
    if (fi.frame_index != 1u || fi.tick != 11 || fi.time != 0 ||
        (fi.flags & MODELRT_ANIM_FRAME_TIME_KNOWN) != 0u) return 20;
    modelrt_close(&h);
    return 0;
}

static int check_timed_skin(const char *path)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_animation_info ai;
    modelrt_animation_frame_info fi;
    modelrt_visit_sink sink;
    tri_probe p;
    int rc;
    modelrt_runtime_init(&rt);
    if (modelrt_register_builtin_providers(&rt) != MODELRT_OK) return 30;
    modelrt_handle_init(&h, g_ws, sizeof(g_ws));
    rc = modelrt_open_file(&rt, &h, path);
    if (rc != MODELRT_OK) return 31;
    if (modelrt_animation_count(&h) != 1) return 32;
    memset(&ai, 0, sizeof(ai));
    if (modelrt_animation_info_get(&h, 0, &ai) != MODELRT_OK) return 33;
    if (strcmp(ai.name, "Move") != 0 || ai.frame_count != 3u ||
        ai.source_key_count != 2u || ai.first_tick != 0 || ai.last_tick != 2 ||
        ai.ticks_per_second != 2u) return 34;
    if ((ai.flags & MODELRT_ANIM_TICK_RATE_KNOWN) == 0u ||
        (ai.flags & MODELRT_ANIM_POSE_SAMPLER) == 0u ||
        (ai.flags & MODELRT_ANIM_UNBOUND) != 0u) return 35;
    if (ai.start_time != 0 || ai.end_time != MODELRT_Q16_ONE) return 36;
    memset(&fi, 0, sizeof(fi));
    if (modelrt_animation_frame_info_get(&h, 0, 1u, &fi) != MODELRT_OK) return 37;
    if (fi.tick != 1 || fi.time != (MODELRT_Q16_ONE / 2) ||
        (fi.flags & MODELRT_ANIM_FRAME_TIME_KNOWN) == 0u) return 38;

    memset(&sink, 0, sizeof(sink));
    memset(&p, 0, sizeof(p));
    sink.on_triangle = on_triangle;
    rc = modelrt_visit(&h, 0, (MODELRT_Q16_ONE / 2), &sink, &p);
    if (rc != MODELRT_OK) return 39;
    if (p.triangles != 1) return 40;
    if (p.first.v[0].position.x < (MODELRT_Q16_ONE - 8) ||
        p.first.v[0].position.x > (MODELRT_Q16_ONE + 8)) return 41;
    modelrt_close(&h);
    return 0;
}

int main(int argc, char **argv)
{
    int rc;
    if (argc != 3) return 2;
    rc = check_multiclip(argv[1]);
    if (rc != 0) { printf("multiclip failed: %d\n", rc); return rc; }
    rc = check_timed_skin(argv[2]);
    if (rc != 0) { printf("timed skin failed: %d\n", rc); return rc; }
    puts("DirectX .x v0.5 runtime: multiclip frames/timing + pose sampling OK");
    return 0;
}
