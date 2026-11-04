#include <stdio.h>
#include <stdlib.h>

#include "3dfbx.h"
#include "fbx_alloc.h"

/* C89-friendly printing for fbx_i64 without %lld (pedantic-safe). */
static void print_i64_dec(fbx_i64 v)
{
    char buf[64];
    int pos;
    fbx_u64 u;

    if (v == 0) {
        fputs("0", stdout);
        return;
    }

    if (v < 0) {
        fputc('-', stdout);
        /* convert using unsigned to avoid overflow on INT64_MIN */
        u = (fbx_u64)(~(fbx_u64)v) + 1u;
    } else {
        u = (fbx_u64)v;
    }

    pos = 0;
    while (u != 0 && pos < (int)(sizeof(buf) - 1)) {
        fbx_u64 q = u / 10u;
        fbx_u64 r = u - q * 10u;
        buf[pos++] = (char)('0' + (char)r);
        u = q;
    }

    while (pos > 0) {
        fputc(buf[--pos], stdout);
    }
}

static void dump_scene(const char *path)
{
    FbxContext *ctx;
    FbxNode *root;
    FbxScene *scene;
    FbxModel *m;
    FbxVersionInfo vinfo;
    FbxVersionSceneProfile profile;

    unsigned models;
    unsigned meshes;

    ctx = fbx_open(path);
    if (!ctx) {
        printf("fbx_open failed: %s\n", path);
        return;
    }

    root = fbx_read_scene(ctx);
    if (!root) {
        printf("fbx_read_scene failed: %s\n", fbx_last_error(ctx));
        fbx_close(ctx, NULL);
        return;
    }

    if (fbx_get_version_info(ctx, &vinfo) &&
        fbx_version_analyze_scene(root, &vinfo, &profile)) {
        printf("Version profile:\n");
        printf("  raw:       %d\n", (int)profile.info.version_raw);
        printf("  profile:   %s\n", fbx_version_get_profile(&profile.info));
        printf("  path:      %s\n", fbx_import_path_name(profile.import_path));
        printf("  flags:     0x%08x\n", (unsigned)profile.flags);
        printf("  embedded_model_mesh=%d modern_geometry=%d numeric_models=%d no_numeric_models=%d aux_roots=%d legacy_takes=%d string_connections=%d\n\n",
               profile.embedded_model_mesh_count,
               profile.modern_geometry_count,
               profile.numeric_model_id_count,
               profile.no_numeric_model_id_count,
               profile.aux_root_hint_count,
               profile.legacy_take_count,
               profile.string_connection_count);
    }

    scene = fbx_scene_build(root);
    if (!scene) {
        printf("fbx_scene_build failed\n");
        fbx_close(ctx, root);
        return;
    }

    models = 0;
    meshes = 0;
    for (m = scene->models; m; m = m->next) {
        models++;
        if (m->mesh) meshes++;
    }

    printf("FBX: %s\n", path);
    printf("  is_binary: %d\n", ctx->is_binary);
    printf("  version:   %d\n", ctx->version);
    printf("  models:    %u\n", models);
    printf("  meshes:    %u\n", meshes);
    printf("  skeleton:  %s", scene->skeleton ? "yes" : "no");
    if (scene->skeleton) {
        printf(" (%d bones)\n", scene->skeleton->bone_count);
    } else {
        printf("\n");
    }

    printf("\nMeshes:\n");
    for (m = scene->models; m; m = m->next) {
        if (!m->mesh) continue;
        fputs("- model_id=", stdout);
        print_i64_dec(m->id);
        printf(" name=%s vtx=%u idx=%u uv_sets=%u uv0=%s nrm=%s tan=%s bitan=%s skin=%s\n",
               m->name ? m->name : "(null)",
               (unsigned)m->mesh->vertex_count,
               (unsigned)m->mesh->index_count,
               (unsigned)m->mesh->uv_set_count,
               m->mesh->uvs ? "yes" : "no",
               m->mesh->normals ? "yes" : "no",
               m->mesh->tangents ? "yes" : "no",
               m->mesh->bitangents ? "yes" : "no",
               m->mesh->skin ? "yes" : "no");

        if (m->mesh->uv_set_count > 0 && m->mesh->uv_set_names) {
            unsigned s;
            for (s = 0; s < (unsigned)m->mesh->uv_set_count; ++s) {
                const char *nm = m->mesh->uv_set_names[s];
                printf("    uv[%u] name=%s\n", s, nm ? nm : "(null)");
            }
        }

        if (m->mesh->skin) {
            printf("    skin weights: %u\n", (unsigned)m->mesh->skin->weight_count);
        }
    }

    fbx_scene_destroy(scene);
    fbx_close(ctx, root);
}

int main(int argc, char **argv)
{
    const char *path;


    if (argc < 2) {
        path = "test.fbx";
        printf("Usage: %s <file.fbx>\n", argv[0]);
        printf("(no file given; trying '%s')\n\n", path);
    } else {
        path = argv[1];
    }

    dump_scene(path);
    return 0;
}
