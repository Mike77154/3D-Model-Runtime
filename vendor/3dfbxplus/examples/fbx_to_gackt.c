#include <stdio.h>

#include "3dfbx.h"

static int write_file(const char *path, const void *data, fbx_u32 size)
{
    FILE *fp;
    fp = fopen(path, "wb");
    if (!fp) return 0;
    if (fwrite(data, 1u, (size_t)size, fp) != (size_t)size) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

int main(int argc, char **argv)
{
    const char *in_path;
    const char *out_path;
    FbxContext *ctx;
    FbxNode *root;
    FbxScene *scene;
    fbx_u32 need;
    fbx_u32 wrote;
    void *buffer;
    int rc;
    FbxGacktInfo info;

    if (argc < 3) {
        printf("Usage: %s input.fbx output.gackt\n", argv[0]);
        return 1;
    }

    in_path = argv[1];
    out_path = argv[2];

    ctx = fbx_open(in_path);
    if (!ctx) {
        printf("open failed\n");
        return 1;
    }

    root = fbx_read_raw_tree(ctx);
    if (!root) {
        printf("read failed: %s\n", fbx_last_error(ctx));
        fbx_close(ctx, NULL);
        return 1;
    }

    scene = fbx_scene_build(root);
    if (!scene) {
        printf("scene build failed\n");
        fbx_close(ctx, root);
        return 1;
    }

    need = fbx_gackt_calc_scene_size(scene, FBX_GACKT_FLAG_DEFAULT | FBX_GACKT_FLAG_BAKE_TRANSFORMS);
    if (need == 0u) {
        printf("gackt size failed\n");
        fbx_scene_destroy(scene);
        fbx_close(ctx, root);
        return 1;
    }

    buffer = fbx_alloc((size_t)need);
    if (!buffer) {
        printf("buffer alloc failed: need=%lu\n", (unsigned long)need);
        fbx_scene_destroy(scene);
        fbx_close(ctx, root);
        return 1;
    }

    wrote = 0u;
    rc = fbx_gackt_write_scene(scene, buffer, need, &wrote,
                               FBX_GACKT_FLAG_DEFAULT | FBX_GACKT_FLAG_BAKE_TRANSFORMS);
    if (rc != FBX_GACKT_OK) {
        printf("gackt write failed: %s\n", fbx_gackt_error_string(rc));
        fbx_scene_destroy(scene);
        fbx_close(ctx, root);
        return 1;
    }

    rc = fbx_gackt_validate_memory(buffer, wrote, &info);
    if (rc != FBX_GACKT_OK) {
        printf("gackt validate failed: %s\n", fbx_gackt_error_string(rc));
        fbx_scene_destroy(scene);
        fbx_close(ctx, root);
        return 1;
    }

    if (!write_file(out_path, buffer, wrote)) {
        printf("write file failed\n");
        fbx_scene_destroy(scene);
        fbx_close(ctx, root);
        return 1;
    }

    printf("GACKT: meshes=%lu vertices=%lu indices=%lu bytes=%lu\n",
           (unsigned long)info.mesh_count,
           (unsigned long)info.total_vertex_count,
           (unsigned long)info.total_index_count,
           (unsigned long)info.file_size);

    fbx_scene_destroy(scene);
    fbx_close(ctx, root);
    return 0;
}
