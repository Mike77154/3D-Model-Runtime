#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

#define WS_SIZE (4u * 1024u * 1024u)
static unsigned char g_ws[WS_SIZE];

typedef struct cap_s {
    int triangles;
    int materials;
    int bones;
    modelrt_triangle_view tri;
} cap;

static int on_material(void *u, const modelrt_material_view *m)
{
    cap *c;
    c = (cap *)u;
    ++c->materials;
    if (m->index == 0u && m->texture_index != 0) return 1;
    return 0;
}

static int on_triangle(void *u, const modelrt_triangle_view *t)
{
    cap *c;
    c = (cap *)u;
    if (c->triangles == 0) c->tri = *t;
    ++c->triangles;
    return 0;
}

static int on_bone(void *u, const modelrt_bone_view *b)
{
    cap *c;
    c = (cap *)u;
    ++c->bones;
    if (b->index == 1u) {
        if (strcmp(b->name, "Bone") != 0) return 1;
        if (b->parent != 0) return 1;
        if (b->position.x != 10 * MODELRT_Q16_ONE) return 1;
        if (b->position.y != 2 * MODELRT_Q16_ONE) return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_visit_sink sink;
    modelrt_texture_info ti;
    modelrt_animation_info ai;
    const modelrt_model_info *info;
    cap c;
    int rc;
    if (argc != 2) return 2;
    modelrt_runtime_init(&rt);
    rc = modelrt_register_builtin_providers(&rt);
    if (rc != MODELRT_OK) return 3;
    modelrt_handle_init(&h, g_ws, sizeof(g_ws));
    rc = modelrt_open_file(&rt, &h, argv[1]);
    if (rc != MODELRT_OK) {
        printf("open failed: %s\n", modelrt_last_error(&rt));
        return 4;
    }
    info = modelrt_get_info(&h);
    if (info == 0 || info->codec_id != MODELRT_CODEC_DIRECTX_X) return 5;
    if (info->mesh_count != 1u || info->material_count != 1u ||
        info->texture_count != 1u || info->bone_count != 2u ||
        info->animation_count != 1u) return 6;
    if ((info->capabilities & MODELRT_CAP_SKINNING) == 0u) return 7;
    if ((info->capabilities & MODELRT_CAP_EXTERNAL_TEX) == 0u) return 8;

    memset(&c, 0, sizeof(c));
    memset(&sink, 0, sizeof(sink));
    sink.on_material = on_material;
    sink.on_triangle = on_triangle;
    sink.on_bone = on_bone;
    rc = modelrt_visit(&h, MODELRT_ANIM_BIND_POSE, 0, &sink, &c);
    if (rc != MODELRT_OK) return 9;
    if (c.triangles != 1 || c.materials != 1 || c.bones != 2) return 10;
    if (c.tri.material_index != 0) return 11;
    if (c.tri.v[0].position.x != 10 * MODELRT_Q16_ONE ||
        c.tri.v[0].position.y != 2 * MODELRT_Q16_ONE) return 12;
    if ((c.tri.v[0].flags & MODELRT_VTX_UV0) == 0u) return 13;
    if ((c.tri.v[0].flags & MODELRT_VTX_SKIN) == 0u || c.tri.v[0].bone[0] != 1) return 14;
    if ((c.tri.v[0].flags & MODELRT_VTX_COLOR) == 0u || c.tri.v[0].rgba[1] < 120u) return 15;

    rc = modelrt_texture_info_get(&h, 0, &ti);
    if (rc != MODELRT_OK) return 16;
    if (strstr(ti.path, "leaf.jpg") == 0) return 17;

    rc = modelrt_animation_info_get(&h, 0, &ai);
    if (rc != MODELRT_OK) return 18;
    if (strcmp(ai.name, "Walk") != 0) return 19;
    if ((ai.flags & MODELRT_ANIM_UNBOUND) != 0u ||
        (ai.flags & MODELRT_ANIM_POSE_SAMPLER) == 0u) return 20;
    if (ai.frame_count == 0u || ai.source_key_count == 0u) return 21;
    if (ai.end_time != MODELRT_Q16_ONE) return 22;

    printf("DirectX .x regression OK: provider=%s meshes=%u mats=%u tex=%u bones=%u clips=%u tri=%d\n",
           modelrt_get_provider_name(&h), info->mesh_count, info->material_count,
           info->texture_count, info->bone_count, info->animation_count, c.triangles);
    modelrt_close(&h);
    return 0;
}
