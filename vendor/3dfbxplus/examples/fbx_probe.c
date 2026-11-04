#include <stdio.h>
#include <stdlib.h>
#include "3dfbx.h"

static void print_i64(const char *key, fbx_i64 value)
{
    printf("%s=%ld\n", key, (long)value);
}

int main(int argc, char **argv)
{
    FbxContext *ctx;
    FbxNode *root;
    FbxScene *scene;
    FbxModel *m;
    int models;
    int meshes;
    int cameras;
    int lights;
    int lines;
    int curves;
    int surfaces;
    int materials;
    int textures;
    int embedded_textures;
    fbx_u32 vertices;
    fbx_u32 indices;
    fbx_u32 uv_sets;
    fbx_u32 submeshes;
    fbx_u32 material_slots;
    fbx_u32 skin_weights;
    int morph_targets;
    int anim_tracks;
    int anim_layers;
    int morph_tracks;
    int unique_meshes;
    fbx_u32 embedded_bytes;
    int i;
    int j;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.fbx>\n", argv[0]);
        return 2;
    }

    ctx = fbx_open(argv[1]);
    if (!ctx) {
        printf("status=open_failed\n");
        return 3;
    }
    root = fbx_read_scene(ctx);
    if (!root) {
        printf("status=read_failed\n");
        printf("error=%s\n", fbx_last_error(ctx));
        fbx_close(ctx, NULL);
        return 4;
    }
    scene = fbx_scene_build(root);
    if (!scene) {
        printf("status=scene_failed\n");
        fbx_close(ctx, root);
        return 5;
    }

    models = 0;
    meshes = 0;
    cameras = 0;
    lights = 0;
    lines = 0;
    curves = 0;
    surfaces = 0;
    vertices = 0u;
    indices = 0u;
    uv_sets = 0u;
    submeshes = 0u;
    material_slots = 0u;
    skin_weights = 0u;
    morph_targets = 0;
    for (m = scene->models; m; m = m->next) {
        models += 1;
        if (m->type == FBX_MODEL_TYPE_MESH) meshes += 1;
        else if (m->type == FBX_MODEL_TYPE_CAMERA) cameras += 1;
        else if (m->type == FBX_MODEL_TYPE_LIGHT) lights += 1;
        else if (m->type == FBX_MODEL_TYPE_LINE) lines += 1;
        else if (m->type == FBX_MODEL_TYPE_NURBS_CURVE) curves += 1;
        else if (m->type == FBX_MODEL_TYPE_NURBS_SURFACE) surfaces += 1;
        if (m->mesh) {
            vertices += m->mesh->vertex_count;
            indices += m->mesh->index_count;
            uv_sets += m->mesh->uv_set_count;
            submeshes += m->mesh->submesh_count;
            material_slots += (fbx_u32)m->mesh->material_slot_count;
            morph_targets += m->mesh->morph_target_count;
            if (m->mesh->skin) skin_weights += m->mesh->skin->weight_count;
        }
    }

    unique_meshes = fbx_scene_count_unique_meshes(scene);
    meshes = unique_meshes;
    if (scene->camera_count > cameras) cameras = scene->camera_count;
    if (scene->light_count > lights) lights = scene->light_count;
    if (scene->line_count > lines) lines = scene->line_count;
    if (scene->curve_count > curves) curves = scene->curve_count;
    if (scene->surface_count > surfaces) surfaces = scene->surface_count;

    materials = scene->material_count;
    textures = 0;
    embedded_textures = 0;
    embedded_bytes = 0u;
    if (scene->texture_count > 0 || scene->video_count > 0) {
        textures = scene->texture_count;
        for (j = 0; j < scene->video_count; ++j) {
            if (scene->videos[j].embedded) {
                embedded_textures += 1;
                embedded_bytes += scene->videos[j].embedded_size;
            }
        }
    } else {
        for (i = 0; i < scene->material_count; ++i) {
            FbxMaterial *mat;
            mat = scene->materials[i];
            if (!mat) continue;
            textures += mat->texture_count;
            for (j = 0; j < mat->texture_count; ++j) {
                if (mat->textures[j].embedded) {
                    embedded_textures += 1;
                    embedded_bytes += mat->textures[j].embedded_size;
                }
            }
        }
    }

    anim_tracks = 0;
    anim_layers = 0;
    morph_tracks = 0;
    for (i = 0; i < scene->anim_clip_count; ++i) {
        if (!scene->anim_clips[i]) continue;
        anim_tracks += scene->anim_clips[i]->track_count;
        anim_layers += scene->anim_clips[i]->layer_count;
        morph_tracks += scene->anim_clips[i]->morph_track_count;
    }
    if (scene->anim_layer_object_count > anim_layers) anim_layers = scene->anim_layer_object_count;

    printf("status=ok\n");
    printf("is_binary=%d\n", ctx->is_binary);
    printf("version=%d\n", ctx->version);
    printf("models=%d\n", models);
    printf("meshes=%d\n", meshes);
    printf("cameras=%d\n", cameras);
    printf("lights=%d\n", lights);
    printf("lines=%d\n", lines);
    printf("curves=%d\n", curves);
    printf("surfaces=%d\n", surfaces);
    printf("vertices=%u\n", (unsigned)vertices);
    printf("indices=%u\n", (unsigned)indices);
    printf("uv_sets=%u\n", (unsigned)uv_sets);
    printf("submeshes=%u\n", (unsigned)submeshes);
    printf("material_slots=%u\n", (unsigned)material_slots);
    printf("materials=%d\n", materials);
    printf("textures=%d\n", textures);
    printf("embedded_textures=%d\n", embedded_textures);
    printf("embedded_bytes=%u\n", (unsigned)embedded_bytes);
    print_i64("skeleton_bones", scene->skeleton ? (fbx_i64)scene->skeleton->bone_count : (fbx_i64)0);
    printf("skin_weights=%u\n", (unsigned)skin_weights);
    printf("morph_targets=%d\n", morph_targets);
    printf("anim_clips=%d\n", scene->anim_clip_count);
    printf("anim_tracks=%d\n", anim_tracks);
    printf("anim_layers=%d\n", anim_layers);
    printf("morph_tracks=%d\n", morph_tracks);
    printf("curve_geometries=%d\n", scene->line_count + scene->curve_count);
    printf("surface_geometries=%d\n", scene->surface_count);

    fbx_scene_destroy(scene);
    fbx_close(ctx, root);
    return 0;
}
