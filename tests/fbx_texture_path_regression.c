#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    static unsigned char workspace[16u * 1024u * 1024u];
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_texture_info ti;
    const char *path;
    int rc;
    path = argc > 1 ? argv[1] : "tests/fbx_texture_paths.fbx";
    modelrt_runtime_init(&rt);
    modelrt_register_builtin_providers(&rt);
    modelrt_handle_init(&h, workspace, sizeof(workspace));
    rc = modelrt_open_file(&rt, &h, path);
    if (rc != MODELRT_OK) {
        printf("open=%d %s\n", rc, modelrt_last_error(&rt));
        return 1;
    }
    memset(&ti, 0, sizeof(ti));
    rc = modelrt_texture_info_get(&h, 0, &ti);
    if (rc != MODELRT_OK) {
        modelrt_close(&h);
        printf("texture-info=%d\n", rc);
        return 2;
    }
    if (strcmp(ti.source_path, "C:/old_project/textures/modern.jpg") != 0 ||
        strcmp(ti.relative_path, "textures/modern.jpg") != 0) {
        printf("bad paths source='%s' relative='%s' preferred='%s'\n",
               ti.source_path, ti.relative_path, ti.path);
        modelrt_close(&h);
        return 3;
    }
    printf("FBX texture path regression OK: source='%s' relative='%s'\n",
           ti.source_path, ti.relative_path);
    modelrt_close(&h);
    return 0;
}
