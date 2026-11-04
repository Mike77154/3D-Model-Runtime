#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

#define WS_SIZE (4u * 1024u * 1024u)
static unsigned char g_ws[WS_SIZE];

typedef struct probe_s {
    int triangles;
    int bones;
    modelrt_triangle_view first;
} probe;

static int on_triangle(void *u, const modelrt_triangle_view *t)
{
    probe *p;
    p = (probe *)u;
    if (p->triangles == 0) p->first = *t;
    ++p->triangles;
    return 0;
}

static int on_bone(void *u, const modelrt_bone_view *b)
{
    probe *p;
    p = (probe *)u;
    ++p->bones;
    if (b->index == 0u) {
        if (strcmp(b->name, "Root") != 0 || b->parent != -1) return 1;
    }
    if (b->index == 1u) {
        if (strcmp(b->name, "Bone") != 0 || b->parent != 0) return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_visit_sink sink;
    modelrt_animation_info ai;
    probe p;
    const modelrt_model_info *info;
    int rc;
    if (argc != 2) return 2;
    modelrt_runtime_init(&rt);
    if (modelrt_register_builtin_providers(&rt) != MODELRT_OK) return 3;
    modelrt_handle_init(&h, g_ws, sizeof(g_ws));
    rc = modelrt_open_file(&rt, &h, argv[1]);
    if (rc != MODELRT_OK) {
        printf("open failed: %s\n", modelrt_last_error(&rt));
        return 4;
    }
    info = modelrt_get_info(&h);
    if (info == 0 || info->codec_id != MODELRT_CODEC_DIRECTX_X) return 5;
    if (info->mesh_count != 1u || info->bone_count != 2u || info->animation_count != 1u) return 6;
    if ((info->capabilities & MODELRT_CAP_SKINNING) == 0u) return 7;

    memset(&p, 0, sizeof(p));
    memset(&sink, 0, sizeof(sink));
    sink.on_triangle = on_triangle;
    sink.on_bone = on_bone;
    rc = modelrt_visit(&h, MODELRT_ANIM_BIND_POSE, 0, &sink, &p);
    if (rc != MODELRT_OK) return 8;
    if (p.triangles != 1 || p.bones != 2) return 9;

    /* SkinWeights offset and Bone combined are identity: bind pose stays put.
       Applying the owner Frame again would break this invariant on real rigs. */
    if (p.first.v[0].position.x != 0 || p.first.v[0].position.y != 0 || p.first.v[0].position.z != 0) return 10;
    if ((p.first.v[0].flags & MODELRT_VTX_SKIN) == 0u || p.first.v[0].bone[0] != 1) return 11;

    /* DirectX (2.25,-0.5) -> OpenGL (2.25,1.5), preserving tiled range. */
    if (p.first.v[0].uv0.x < (modelrt_q16)147452 || p.first.v[0].uv0.x > (modelrt_q16)147460 ||
        p.first.v[0].uv0.y < (modelrt_q16)98300 || p.first.v[0].uv0.y > (modelrt_q16)98308) return 12;

    rc = modelrt_animation_info_get(&h, 0, &ai);
    if (rc != MODELRT_OK) return 13;
    if (strcmp(ai.name, "Clip") != 0) return 14;
    if ((ai.flags & MODELRT_ANIM_UNBOUND) != 0u ||
        (ai.flags & MODELRT_ANIM_POSE_SAMPLER) == 0u) return 15;
    /* The file omits AnimTicksPerSecond. v0.5 recovers dense tick frames but
       intentionally does not invent a display/playback rate. */
    if (ai.frame_count != 11u || ai.first_tick != 0 || ai.last_tick != 10 ||
        ai.start_time != 0 || ai.end_time != 0) return 16;

    printf("DirectX .x v0.5 semantics OK: uv=(%d,%d) bones=%d skin=1 frames=%u\n",
           (int)p.first.v[0].uv0.x, (int)p.first.v[0].uv0.y, p.bones,
           (unsigned)ai.frame_count);
    modelrt_close(&h);
    return 0;
}
