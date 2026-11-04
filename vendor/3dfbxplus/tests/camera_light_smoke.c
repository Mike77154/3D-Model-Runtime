#include <stdio.h>
#include "3dfbx.h"

int main(int argc, char **argv)
{
    FbxContext *ctx;
    FbxNode *root;
    FbxScene *scene;
    FbxModel *m;
    int cameras;
    int lights;
    int valid_cameras;
    int valid_lights;

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

    cameras = 0;
    lights = 0;
    valid_cameras = 0;
    valid_lights = 0;
    for (m = scene->models; m; m = m->next) {
        if (m->type == FBX_MODEL_TYPE_CAMERA) {
            cameras++;
            if (m->camera.valid) valid_cameras++;
        }
        if (m->type == FBX_MODEL_TYPE_LIGHT) {
            lights++;
            if (m->light.valid) valid_lights++;
        }
    }

    printf("cameras=%d valid_cameras=%d lights=%d valid_lights=%d\n",
           cameras, valid_cameras, lights, valid_lights);

    fbx_scene_destroy(scene);
    fbx_close(ctx, root);
    return (cameras == 1 && lights == 1 && valid_cameras == 1 && valid_lights == 1) ? 0 : 6;
}
