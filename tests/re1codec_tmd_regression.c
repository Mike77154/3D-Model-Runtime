#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

#define WS_SIZE (512u * 1024u)
static unsigned char workspace[WS_SIZE];
static unsigned long triangles;
static unsigned long parts;

static int on_triangle(void *u, const modelrt_triangle_view *t)
{
    (void)u; (void)t; ++triangles; return 0;
}
static int on_part(void *u, const modelrt_rigid_part_view *p)
{
    (void)u; (void)p; ++parts; return 0;
}

int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_visit_sink sink;
    const modelrt_model_info *info;
    modelrt_texture_info ti;
    int rc;
    if (argc != 2) return 2;
    modelrt_runtime_init(&rt);
    if (modelrt_register_builtin_providers(&rt) != MODELRT_OK) return 3;
    modelrt_handle_init(&h, workspace, sizeof(workspace));
    rc = modelrt_open_file(&rt, &h, argv[1]);
    if (rc != MODELRT_OK) return 4;
    info = modelrt_get_info(&h);
    if (info == 0 || info->codec_id != MODELRT_CODEC_RE) return 5;
    if (info->mesh_count != 1u || info->bone_count != 0u ||
        info->rigid_part_count != 0u || info->animation_count != 0u) return 6;
    if ((info->capabilities & MODELRT_CAP_RIGID_HIERARCHY) != 0u) return 7;
    if ((info->capabilities & MODELRT_CAP_EXTERNAL_TEX) == 0u) return 8;
    if (info->texture_count != 1u) return 9;
    memset(&ti, 0, sizeof(ti));
    if (modelrt_texture_info_get(&h, 0, &ti) != MODELRT_OK ||
        ti.kind != MODELRT_TEXTURE_EXTERNAL) return 10;
    memset(&sink, 0, sizeof(sink));
    sink.on_triangle = on_triangle;
    sink.on_rigid_part = on_part;
    rc = modelrt_visit(&h, MODELRT_ANIM_BIND_POSE, 0, &sink, 0);
    if (rc != MODELRT_OK || triangles != 1u || parts != 0u) return 11;
    printf("RE1codec v0.3 standalone TMD OK: meshes=%u parts=%u clips=%u textures=%u external_slot=1 tri=%lu\n",
           (unsigned)info->mesh_count, (unsigned)info->rigid_part_count,
           (unsigned)info->animation_count, (unsigned)info->texture_count,
           triangles);
    modelrt_close(&h);
    return 0;
}
