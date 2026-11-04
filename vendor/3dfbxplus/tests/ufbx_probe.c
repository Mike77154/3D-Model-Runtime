#include "ufbx.h"

#include <stdio.h>
#include <stddef.h>

int main(int argc, char **argv)
{
    ufbx_error error;
    ufbx_scene *scene;
    size_t i;
    size_t vertices;
    size_t indices;
    size_t embedded_textures;
    size_t embedded_bytes;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.fbx>\n", argv[0]);
        return 2;
    }

    scene = ufbx_load_file(argv[1], NULL, &error);
    if (!scene) {
        printf("status=read_failed\n");
        printf("error=%s\n", error.description.data ? error.description.data : "ufbx error");
        return 3;
    }

    vertices = 0;
    indices = 0;
    for (i = 0; i < scene->meshes.count; ++i) {
        vertices += scene->meshes.data[i]->num_vertices;
        indices += scene->meshes.data[i]->num_indices;
    }
    embedded_textures = 0;
    embedded_bytes = 0;
    for (i = 0; i < scene->videos.count; ++i) {
        if (scene->videos.data[i]->content.size > 0) {
            embedded_textures += 1;
            embedded_bytes += scene->videos.data[i]->content.size;
        }
    }

    printf("status=ok\n");
    printf("is_binary=%d\n", scene->metadata.ascii ? 0 : 1);
    printf("version=%u\n", (unsigned)scene->metadata.version);
    printf("models=%lu\n", (unsigned long)scene->nodes.count);
    printf("meshes=%lu\n", (unsigned long)scene->meshes.count);
    printf("cameras=%lu\n", (unsigned long)scene->cameras.count);
    printf("lights=%lu\n", (unsigned long)scene->lights.count);
    printf("lines=%lu\n", (unsigned long)scene->line_curves.count);
    printf("curves=%lu\n", (unsigned long)scene->nurbs_curves.count);
    printf("surfaces=%lu\n", (unsigned long)scene->nurbs_surfaces.count);
    printf("vertices=%lu\n", (unsigned long)vertices);
    printf("indices=%lu\n", (unsigned long)indices);
    printf("materials=%lu\n", (unsigned long)scene->materials.count);
    printf("textures=%lu\n", (unsigned long)scene->textures.count);
    printf("videos=%lu\n", (unsigned long)scene->videos.count);
    printf("embedded_textures=%lu\n", (unsigned long)embedded_textures);
    printf("embedded_bytes=%lu\n", (unsigned long)embedded_bytes);
    printf("skeleton_bones=%lu\n", (unsigned long)scene->bones.count);
    printf("skin_clusters=%lu\n", (unsigned long)scene->skin_clusters.count);
    printf("morph_targets=%lu\n", (unsigned long)scene->blend_shapes.count);
    printf("blend_channels=%lu\n", (unsigned long)scene->blend_channels.count);
    printf("anim_clips=%lu\n", (unsigned long)scene->anim_stacks.count);
    printf("anim_layers=%lu\n", (unsigned long)scene->anim_layers.count);
    printf("anim_curves=%lu\n", (unsigned long)scene->anim_curves.count);

    ufbx_free_scene(scene);
    return 0;
}
