#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

static unsigned char workspace[4u * 1024u * 1024u];
static int got_triangle;
static modelrt_triangle_view captured;

static int on_triangle(void *user, const modelrt_triangle_view *tri)
{
    (void)user;
    if (!got_triangle) {
        captured = *tri;
        got_triangle = 1;
    }
    return 0;
}

static int expect_q16(const char *name, modelrt_q16 got, modelrt_q16 want)
{
    if (got != want) {
        printf("%s mismatch: got=%ld want=%ld\n", name, (long)got, (long)want);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_visit_sink sink;
    int rc;

    if (argc != 2) return 1;
    got_triangle = 0;
    memset(&captured, 0, sizeof(captured));
    modelrt_runtime_init(&rt);
    modelrt_register_builtin_providers(&rt);
    modelrt_handle_init(&h, workspace, sizeof(workspace));
    rc = modelrt_open_file(&rt, &h, argv[1]);
    if (rc != MODELRT_OK) {
        printf("open failed: %s\n", modelrt_last_error(&rt));
        return 2;
    }
    memset(&sink, 0, sizeof(sink));
    sink.on_triangle = on_triangle;
    rc = modelrt_visit(&h, MODELRT_ANIM_BIND_POSE, 0, &sink, 0);
    if (rc != MODELRT_OK || !got_triangle) {
        printf("visit failed rc=%d got_triangle=%d\n", rc, got_triangle);
        return 3;
    }

    /* Texture requests UV layer 1.  Layer 0 is deliberately all zeroes. */
    if (expect_q16("v0.u", captured.v[0].uv0.x, 16384L)) return 4;
    if (expect_q16("v0.v", captured.v[0].uv0.y, 16384L)) return 5;

    /* Preserve repeat/tiling coordinates outside 0..1 while flipping V. */
    if (expect_q16("v1.u", captured.v[1].uv0.x, 147456L)) return 6;
    if (expect_q16("v1.v", captured.v[1].uv0.y, 98304L)) return 7;
    if (expect_q16("v2.u", captured.v[2].uv0.x, -16384L)) return 8;
    if (expect_q16("v2.v", captured.v[2].uv0.y, -32768L)) return 9;

    printf("XPS UV regression OK: texture_uv_layer=1 V_flipped=1 tiling_preserved=1\n");
    modelrt_close(&h);
    return 0;
}
