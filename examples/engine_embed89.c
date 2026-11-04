/* Example: reuse model_runtime89 independently of the Win32/OpenGL viewer. */
#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

/* Supply separate workspaces per simultaneously open asset. */
static unsigned char model_memory[64u * 1024u * 1024u];
static unsigned long n_triangles;
static int example_triangle(void *context, const modelrt_triangle_view *triangle)
{
    (void)context;
    (void)triangle; /* Convert the triangle into your renderer vertex buffer. */
    ++n_triangles;
    return 0;
}
int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_visit_sink sink;
    int result;
    if (argc != 2) {
        puts("usage: engine_embed89 <model.psk|model.mdl|model.fbx|...>");
        return 2;
    }
    modelrt_runtime_init(&rt);
    if (modelrt_register_builtin_providers(&rt) != MODELRT_OK) return 3;
    modelrt_handle_init(&h, model_memory, sizeof(model_memory));
    result = modelrt_open_file(&rt, &h, argv[1]);
    if (result != MODELRT_OK) {
        fprintf(stderr, "open: %s\n", modelrt_last_error(&rt));
        return 4;
    }
    memset(&sink, 0, sizeof(sink));
    sink.on_triangle = example_triangle;
    result = modelrt_visit(&h, MODELRT_ANIM_BIND_POSE, 0, &sink, 0);
    printf("codec=%u triangles=%lu status=%d\n", (unsigned)h.info.codec_id, n_triangles, result);
    modelrt_close(&h);
    return result == MODELRT_OK ? 0 : 5;
}
