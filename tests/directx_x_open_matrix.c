#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

#define WS_SIZE (8u * 1024u * 1024u)
static unsigned char g_ws[WS_SIZE];

int main(int argc, char **argv)
{
    int i;
    if (argc < 2) return 2;
    for (i = 1; i < argc; ++i) {
        modelrt_runtime rt;
        modelrt_handle h;
        int rc;
        modelrt_runtime_init(&rt);
        if (modelrt_register_builtin_providers(&rt) != MODELRT_OK) return 3;
        memset(g_ws, 0, sizeof(g_ws));
        modelrt_handle_init(&h, g_ws, sizeof(g_ws));
        rc = modelrt_open_file(&rt, &h, argv[i]);
        if (rc != MODELRT_OK) {
            printf("FAIL %s: %s\n", argv[i], modelrt_last_error(&rt));
            return 4;
        }
        if (h.info.codec_id != MODELRT_CODEC_DIRECTX_X) return 5;
        printf("PASS %s meshes=%u bones=%u clips=%u\n", argv[i],
               h.info.mesh_count, h.info.bone_count, h.info.animation_count);
        modelrt_close(&h);
    }
    return 0;
}
