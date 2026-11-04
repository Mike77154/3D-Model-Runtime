#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

static int g_bones;
static int g_has_parent;
static int g_materials;
static int g_material_texture;

static int on_bone(void *user, const modelrt_bone_view *b)
{
    (void)user;
    g_bones++;
    if (b->parent >= 0) g_has_parent = 1;
    return 0;
}

static int on_material(void *user, const modelrt_material_view *m)
{
    (void)user;
    g_materials++;
    if (m->texture_index >= 0) g_material_texture = 1;
    return 0;
}

int main(int argc, char **argv)
{
    static unsigned char workspace[16u * 1024u * 1024u];
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_visit_sink sink;
    const modelrt_model_info *info;
    int rc;
    const char *path;
    path = argc > 1 ? argv[1] : "tests/rigged_test.fbx";
    modelrt_runtime_init(&rt);
    modelrt_register_builtin_providers(&rt);
    modelrt_handle_init(&h, workspace, sizeof(workspace));
    rc = modelrt_open_file(&rt, &h, path);
    if (rc != MODELRT_OK) { printf("open=%d %s\n", rc, modelrt_last_error(&rt)); return 1; }
    info = modelrt_get_info(&h);
    if (info == 0 || info->codec_id != MODELRT_CODEC_FBX || info->bone_count < 2u || info->texture_count < 1u) {
        printf("bad-info bones=%u textures=%u\n", info ? (unsigned)info->bone_count : 0u,
               info ? (unsigned)info->texture_count : 0u);
        modelrt_close(&h); return 2;
    }
    memset(&sink, 0, sizeof(sink));
    sink.on_bone = on_bone;
    sink.on_material = on_material;
    rc = modelrt_visit(&h, MODELRT_ANIM_BIND_POSE, 0, &sink, 0);
    modelrt_close(&h);
    if (rc != MODELRT_OK) { printf("visit=%d\n", rc); return 3; }
    if (g_bones < 2 || !g_has_parent || g_materials < 1 || !g_material_texture) {
        printf("bad-visit bones=%d parent=%d materials=%d texture=%d\n",
               g_bones, g_has_parent, g_materials, g_material_texture); return 4;
    }
    printf("FBX regression OK: bones=%d hierarchy=1 textured_material=1\n", g_bones);
    return 0;
}
