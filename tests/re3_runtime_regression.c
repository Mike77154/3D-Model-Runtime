#include "../include/model_runtime89.h"
#include <stdio.h>
#include <string.h>

#define WS_BYTES (12u * 1024u * 1024u)
static unsigned char g_ws[WS_BYTES];
static unsigned int g_triangles;
static unsigned int g_parts;
static unsigned int g_materials;
static modelrt_q16 g_first_x;
static int g_have_first;

static int on_tri(void *u, const modelrt_triangle_view *t)
{
    (void)u;
    if (!g_have_first) { g_first_x = t->v[0].position.x; g_have_first = 1; }
    ++g_triangles; return 0;
}
static int on_part(void *u, const modelrt_rigid_part_view *p)
{
    (void)u; (void)p; ++g_parts; return 0;
}
static int on_mat(void *u, const modelrt_material_view *m)
{
    (void)u; (void)m; ++g_materials; return 0;
}

int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_visit_sink sink;
    const modelrt_model_info *info;
    modelrt_animation_info ai;
    int rc;
    int i;
    int saw_bound;
    if (argc != 2) return 2;
    modelrt_runtime_init(&rt);
    if (modelrt_register_builtin_providers(&rt) != MODELRT_OK) return 3;
    modelrt_handle_init(&h, g_ws, sizeof(g_ws));
    rc = modelrt_open_file(&rt, &h, argv[1]);
    if (rc != MODELRT_OK) { printf("open fail: %s\n", modelrt_last_error(&rt)); return 4; }
    info = modelrt_get_info(&h);
    if (info == 0 || info->codec_id != MODELRT_CODEC_RE3) return 5;
    if (strstr(modelrt_get_provider_name(&h), "v0.28.0") == 0) return 12;
    if (info->mesh_count == 0u || info->animation_count == 0u) return 6;
    saw_bound = 0;
    for (i = 0; i < modelrt_animation_count(&h); ++i) {
        if (modelrt_animation_info_get(&h, i, &ai) != MODELRT_OK) return 7;
        if ((ai.flags & MODELRT_ANIM_UNBOUND) == 0u) saw_bound = 1;
    }
    if (!saw_bound) return 8;
    memset(&sink, 0, sizeof(sink));
    sink.on_triangle = on_tri;
    sink.on_rigid_part = on_part;
    sink.on_material = on_mat;
    rc = modelrt_visit(&h, MODELRT_ANIM_BIND_POSE, 0, &sink, 0);
    if (rc != MODELRT_OK) return 9;
    if (g_triangles != 6u) return 10; /* body+aux, each tri + quad=>3 */
    if (g_materials == 0u) return 11;
    /* V028 reference vertex: no DAT, root joint X=10, mesh vertex X=0. */
    if (!g_have_first || g_first_x != (modelrt_q16)(10 * MODELRT_Q16_ONE)) {
        printf("wrong reference vertex x=%ld\n", (long)g_first_x);
        return 13;
    }
    g_have_first = 0; g_triangles = 0u;
    rc = modelrt_visit(&h, 0, 0, &sink, 0);
    if (rc != MODELRT_OK || g_triangles != 6u) {
        printf("animated visit failure rc=%d tris=%u\n", rc, g_triangles);
        return 14;
    }
    printf("RE3 runtime regression OK: provider=%s meshes=%u clips=%u tris=%u mats=%u parts=%u\n",
           modelrt_get_provider_name(&h), (unsigned)info->mesh_count,
           (unsigned)info->animation_count, g_triangles, g_materials, g_parts);
    modelrt_close(&h);
    return 0;
}
