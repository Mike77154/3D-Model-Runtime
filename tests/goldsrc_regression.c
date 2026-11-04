#include "../include/model_runtime89.h"
#include <stdio.h>
#include <string.h>

#define WS_BYTES (8u * 1024u * 1024u)
static unsigned char g_ws[WS_BYTES];

typedef struct capture_s {
    int got_root;
    modelrt_vec3 root;
    int triangles;
} capture;

static int on_bone(void *user, const modelrt_bone_view *bone)
{
    capture *c;
    c = (capture *)user;
    if (bone->index == 0u) {
        c->got_root = 1;
        c->root = bone->position;
    }
    return 0;
}

static int on_triangle(void *user, const modelrt_triangle_view *triangle)
{
    capture *c;
    (void)triangle;
    c = (capture *)user;
    c->triangles += 1;
    return 0;
}

int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_visit_sink sink;
    modelrt_animation_info ai;
    capture c;
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
    if (modelrt_animation_count(&h) < 1) return 5;
    memset(&ai, 0, sizeof(ai));
    if (modelrt_animation_info_get(&h, 0, &ai) != MODELRT_OK) return 6;
    if (ai.end_time != MODELRT_Q16_ONE) {
        printf("unexpected duration=%ld\n", (long)ai.end_time);
        return 7;
    }
    memset(&sink, 0, sizeof(sink));
    sink.on_bone = on_bone;
    sink.on_triangle = on_triangle;
    memset(&c, 0, sizeof(c));
    rc = modelrt_visit(&h, 0, MODELRT_Q16_ONE, &sink, &c);
    if (rc != MODELRT_OK) return 8;
    if (!c.got_root || c.triangles <= 0) return 9;
    /* The source sequence translates root X by +100, but marks STUDIO_X.
       GoldSrc removes that motion channel from the skeleton pose. */
    if (c.root.x != 0) {
        printf("root motion leak x=%ld\n", (long)c.root.x);
        return 10;
    }
    printf("GoldSrc regression OK: duration=%ld root_x=%ld triangles=%d\n",
           (long)ai.end_time, (long)c.root.x, c.triangles);
    modelrt_close(&h);
    return 0;
}
