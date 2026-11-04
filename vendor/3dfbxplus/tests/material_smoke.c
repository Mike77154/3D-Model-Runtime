#include <stdio.h>
#include "3dfbx.h"

int main(int argc, char **argv)
{
    FbxContext *ctx;
    FbxNode *root;
    FbxScene *scene;
    FbxModel *m;
    int mesh_material_slots;
    int mesh_submeshes;
    int texture_count;

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

    mesh_material_slots = 0;
    mesh_submeshes = 0;
    for (m = scene->models; m; m = m->next) {
        if (m->mesh) {
            mesh_material_slots += m->mesh->material_slot_count;
            mesh_submeshes += (int)m->mesh->submesh_count;
        }
    }

    texture_count = 0;
    if (scene->material_count > 0 && scene->materials && scene->materials[0]) {
        texture_count = scene->materials[0]->texture_count;
    }

    printf("materials=%d textures=%d mesh_material_slots=%d submeshes=%d\n",
           scene->material_count, texture_count, mesh_material_slots, mesh_submeshes);

    fbx_scene_destroy(scene);
    fbx_close(ctx, root);
    return (mesh_material_slots >= 1 && texture_count >= 1) ? 0 : 6;
}
