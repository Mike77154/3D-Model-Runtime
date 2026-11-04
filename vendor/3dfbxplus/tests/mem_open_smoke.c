#include <stdio.h>
#include "3dfbx.h"

static unsigned char g_buf[65536];

int main(int argc, char **argv)
{
    FILE *fp;
    size_t n;
    FbxContext *ctx;
    FbxNode *root;
    FbxScene *scene;
    int meshes;
    FbxModel *m;

    if (argc < 2) return 2;
    fp = fopen(argv[1], "rb");
    if (!fp) return 3;
    n = fread(g_buf, 1u, sizeof(g_buf), fp);
    fclose(fp);
    if (n == 0u) return 4;

    ctx = fbx_open_memory(g_buf, (fbx_i64)n);
    if (!ctx) return 5;
    root = fbx_read_raw_tree(ctx);
    if (!root) return 6;
    scene = fbx_scene_build(root);
    if (!scene) return 7;

    meshes = 0;
    for (m = scene->models; m; m = m->next) if (m->mesh) meshes++;
    printf("version=%d meshes=%d\n", ctx->version, meshes);

    fbx_scene_destroy(scene);
    fbx_close(ctx, root);
    return (meshes == 1) ? 0 : 8;
}
