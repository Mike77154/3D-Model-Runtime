#include <stdio.h>
#include "3dfbx.h"

int main(int argc, char **argv)
{
    FbxContext *ctx;
    FbxNode *root;
    FbxScene *scene;
    FbxModel *m;
    int targets;
    fbx_u32 indexes;
    fbx_u32 baked;

    if (argc < 2) return 2;
    ctx = fbx_open(argv[1]);
    if (!ctx) return 3;
    root = fbx_read_raw_tree(ctx);
    if (!root) {
        printf("read_error=%s\n", fbx_error_code_string(fbx_last_error_code(ctx)));
        fbx_close(ctx, root);
        return 4;
    }
    scene = fbx_scene_build(root);
    if (!scene) {
        fbx_close(ctx, root);
        return 5;
    }
    targets = 0;
    indexes = 0u;
    baked = 0u;
    for (m = scene->models; m; m = m->next) {
        if (m->mesh) {
            targets += m->mesh->morph_target_count;
            if (m->mesh->morph_target_count > 0) {
                indexes += m->mesh->morph_targets[0].index_count;
                baked += m->mesh->morph_targets[0].baked_vertex_count;
            }
        }
    }
    printf("morph_targets=%d first_index_count=%lu baked_vertices=%lu\n", targets, (unsigned long)indexes, (unsigned long)baked);
    fbx_scene_destroy(scene);
    fbx_close(ctx, root);
    return (targets == 1 && indexes == 2u && baked > 0u) ? 0 : 6;
}
