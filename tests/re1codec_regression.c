#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

#define WS_SIZE (1024u * 1024u)
#define TEX_SIZE (256u * 256u * 4u)

static unsigned char workspace[WS_SIZE];
static unsigned char texture[TEX_SIZE];
static unsigned long triangles;
static unsigned long bones;
static unsigned long parts;
static unsigned long materials;
static modelrt_vec3 first_pos;
static int have_first_pos;

static int on_triangle(void *u, const modelrt_triangle_view *t)
{
    (void)u;
    if (!have_first_pos) { first_pos = t->v[0].position; have_first_pos = 1; }
    ++triangles;
    return 0;
}
static int on_bone(void *u, const modelrt_bone_view *b)
{
    (void)u; (void)b; ++bones; return 0;
}
static int on_part(void *u, const modelrt_rigid_part_view *p)
{
    (void)u; (void)p; ++parts; return 0;
}
static int on_material(void *u, const modelrt_material_view *m)
{
    (void)u; (void)m; ++materials; return 0;
}

int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_visit_sink sink;
    const modelrt_model_info *info;
    modelrt_animation_info ai;
    modelrt_texture_info ti;
    modelrt_u32 w;
    modelrt_u32 hh;
    modelrt_vec3 bind_pos;
    size_t bytes;
    int rc;
    if (argc != 2) return 2;
    modelrt_runtime_init(&rt);
    rc = modelrt_register_builtin_providers(&rt);
    if (rc != MODELRT_OK) return 3;
    modelrt_handle_init(&h, workspace, sizeof(workspace));
    rc = modelrt_open_file(&rt, &h, argv[1]);
    if (rc != MODELRT_OK) {
        printf("open failed: %s\n", modelrt_last_error(&rt));
        return 4;
    }
    info = modelrt_get_info(&h);
    if (info == 0 || info->codec_id != MODELRT_CODEC_RE) return 5;
    if (strcmp(info->codec_name, "RE1codec v0.3-mvref") != 0) return 6;
    if (info->mesh_count != 1u || info->bone_count != 0u ||
        info->rigid_part_count != 1u || info->animation_count != 2u ||
        info->texture_count != 1u) return 7;
    if ((info->capabilities & MODELRT_CAP_EMBEDDED_TEX) == 0u ||
        (info->capabilities & MODELRT_CAP_RIGID_HIERARCHY) == 0u ||
        (info->capabilities & MODELRT_CAP_SKELETON) != 0u ||
        (info->capabilities & MODELRT_CAP_SKINNING) != 0u) return 8;
    memset(&sink, 0, sizeof(sink));
    sink.on_triangle = on_triangle;
    sink.on_bone = on_bone;
    sink.on_rigid_part = on_part;
    sink.on_material = on_material;
    have_first_pos = 0;
    rc = modelrt_visit(&h, MODELRT_ANIM_BIND_POSE, 0, &sink, 0);
    if (rc != MODELRT_OK || triangles != 1u || bones != 0u || parts != 1u ||
        materials == 0u || !have_first_pos) return 9;
    bind_pos = first_pos;

    /* Bank B must be the first playable clip. */
    rc = modelrt_animation_info_get(&h, 0, &ai);
    if (rc != MODELRT_OK || ai.end_time <= 0 ||
        strstr(ai.name, "Bank B") == 0 ||
        (ai.flags & MODELRT_ANIM_UNBOUND) != 0u) return 10;

    /* Bank A remains visible as metadata but must be explicitly unbound. */
    rc = modelrt_animation_info_get(&h, 1, &ai);
    if (rc != MODELRT_OK || strstr(ai.name, "Bank A") == 0 ||
        (ai.flags & MODELRT_ANIM_UNBOUND) == 0u) return 11;

    /* The fixture deliberately gives Bank A a +100 centi pivot while Bank B
       has zero. Selecting Bank A must NOT move/deform the TMD mesh. */
    triangles = 0u; bones = 0u; parts = 0u; materials = 0u; have_first_pos = 0;
    rc = modelrt_visit(&h, 1, 0, &sink, 0);
    if (rc != MODELRT_OK || triangles != 1u || bones != 0u || parts != 1u || !have_first_pos) return 12;
    if (first_pos.x != bind_pos.x || first_pos.y != bind_pos.y || first_pos.z != bind_pos.z) return 13;

    /* Bank B clip still drives the creature hierarchy normally. */
    triangles = 0u; bones = 0u; parts = 0u; materials = 0u; have_first_pos = 0;
    rc = modelrt_visit(&h, 0, 0, &sink, 0);
    if (rc != MODELRT_OK || triangles != 1u || bones != 0u || parts != 1u) return 14;
    rc = modelrt_texture_info_get(&h, 0, &ti);
    if (rc != MODELRT_OK || ti.kind != MODELRT_TEXTURE_EMBEDDED ||
        ti.width != 256u || ti.height != 256u) return 15;
    rc = modelrt_texture_decode_rgba(&h, 0, texture, sizeof(texture), &w, &hh, &bytes);
    if (rc != MODELRT_OK || w != 256u || hh != 256u || bytes != sizeof(texture)) return 16;
    if (texture[3] == 0u) return 17;
    printf("RE1codec v0.3 runtime regression OK: meshes=%u bones=%u parts=%u clips=%u textures=%u tri=%lu tex=%ux%u\n",
           (unsigned)info->mesh_count, (unsigned)info->bone_count,
           (unsigned)info->rigid_part_count,
           (unsigned)info->animation_count, (unsigned)info->texture_count,
           triangles, (unsigned)w, (unsigned)hh);
    modelrt_close(&h);
    return 0;
}
