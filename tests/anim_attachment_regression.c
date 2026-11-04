#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

#define WS (16u * 1024u * 1024u)
#define BWS (512u * 1024u)
static unsigned char target_ws[WS];
static unsigned char source_ws[WS];
static unsigned char bind_ws[BWS];

typedef struct cap_s { modelrt_vec3 first; int got; } cap;
static int on_tri(void *u, const modelrt_triangle_view *t)
{
    cap *c = (cap *)u;
    if (!c->got) { c->first = t->v[0].position; c->got = 1; }
    return 0;
}

int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle target;
    modelrt_handle source;
    modelrt_animation_binding binding;
    modelrt_animation_info ai;
    modelrt_visit_sink sink;
    const modelrt_animation_binding_info *bi;
    cap a, b;
    int rc;
    if (argc != 3) return 2;
    modelrt_runtime_init(&rt);
    if (modelrt_register_builtin_providers(&rt) != MODELRT_OK) return 3;
    modelrt_handle_init(&target, target_ws, sizeof(target_ws));
    modelrt_handle_init(&source, source_ws, sizeof(source_ws));
    if (modelrt_open_file(&rt, &target, argv[1]) != MODELRT_OK) { puts(modelrt_last_error(&rt)); return 4; }
    if (modelrt_open_file(&rt, &source, argv[2]) != MODELRT_OK) { puts(modelrt_last_error(&rt)); return 5; }
    if (modelrt_animation_count(&source) != 1) return 6;
    memset(&ai, 0, sizeof(ai));
    if (modelrt_animation_info_get(&source, 0, &ai) != MODELRT_OK) return 7;
    modelrt_animation_binding_init(&binding, bind_ws, sizeof(bind_ws));
    rc = modelrt_animation_bind(&binding, &target, &source);
    if (rc != MODELRT_OK) { printf("bind rc=%d\n", rc); return 8; }
    bi = modelrt_animation_binding_get_info(&binding);
    if (!bi || bi->matched_track_count == 0u) return 9;
    memset(&sink, 0, sizeof(sink)); sink.on_triangle = on_tri;
    memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
    if (modelrt_visit_bound_animation(&binding, 0, ai.start_time, &sink, &a) != MODELRT_OK) return 10;
    if (modelrt_visit_bound_animation(&binding, 0, ai.end_time, &sink, &b) != MODELRT_OK) return 11;
    if (!a.got || !b.got) return 12;
    printf("attachment OK clip=%s matched=%u/%u start=%ld end=%ld\n",
           ai.name, (unsigned)bi->matched_track_count, (unsigned)bi->source_track_count,
           (long)ai.start_time, (long)ai.end_time);
    modelrt_animation_unbind(&binding);
    modelrt_close(&source);
    modelrt_close(&target);
    return 0;
}
