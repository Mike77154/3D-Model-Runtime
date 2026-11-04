#include "../include/model_runtime89.h"
#include <stdio.h>
#include <string.h>

#define WS_BYTES (8u * 1024u * 1024u)
static unsigned char g_ws[WS_BYTES];

typedef struct capture_s {
    int got_triangle;
    modelrt_vec3 p;
    modelrt_vec3 n;
} capture;

static int on_triangle(void *user, const modelrt_triangle_view *triangle)
{
    capture *c;
    c = (capture *)user;
    if (!c->got_triangle) {
        c->got_triangle = 1;
        c->p = triangle->v[0].position;
        c->n = triangle->v[0].normal;
    }
    return 0;
}

int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_visit_sink sink;
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
    memset(&sink, 0, sizeof(sink));
    memset(&c, 0, sizeof(c));
    sink.on_triangle = on_triangle;
    rc = modelrt_visit(&h, MODELRT_ANIM_BIND_POSE, 0, &sink, &c);
    if (rc != MODELRT_OK || !c.got_triangle) return 5;

    /* Native SMD point (1,2,3), Z-up, becomes runtime (1,3,-2), Y-up. */
    if (c.p.x != MODELRT_Q16_ONE ||
        c.p.y != MODELRT_Q16_ONE * 3 ||
        c.p.z != -MODELRT_Q16_ONE * 2) {
        printf("axis point mismatch: %ld %ld %ld\n",
               (long)c.p.x, (long)c.p.y, (long)c.p.z);
        return 6;
    }
    /* Native normal +Z becomes runtime +Y. */
    if (c.n.x != 0 || c.n.y != MODELRT_Q16_ONE || c.n.z != 0) {
        printf("axis normal mismatch: %ld %ld %ld\n",
               (long)c.n.x, (long)c.n.y, (long)c.n.z);
        return 7;
    }
    printf("GoldSrc axis regression OK: native Z-up -> runtime Y-up\n");
    modelrt_close(&h);
    return 0;
}
