#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

static unsigned char workspace[32u * 1024u * 1024u];
static unsigned long g_triangles;
static unsigned long g_meshes;
static unsigned long g_vertices_reported;

static int on_mesh_begin(void *user, const modelrt_mesh_info *m)
{
    (void)user;
    ++g_meshes;
    g_vertices_reported += (unsigned long)m->vertex_count;
    if (m->triangle_count == 0u || m->vertex_count == 0u) return 7;
    return 0;
}

static int on_triangle(void *user, const modelrt_triangle_view *tri)
{
    (void)user;
    (void)tri;
    ++g_triangles;
    return 0;
}

static int run_one(const char *path)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_visit_sink sink;
    int rc;

    g_triangles = 0ul;
    g_meshes = 0ul;
    g_vertices_reported = 0ul;
    modelrt_runtime_init(&rt);
    modelrt_register_builtin_providers(&rt);
    modelrt_handle_init(&h, workspace, sizeof(workspace));
    rc = modelrt_open_file(&rt, &h, path);
    if (rc != MODELRT_OK) {
        printf("XPS open failed: %s: %s\n", path, modelrt_last_error(&rt));
        return 2;
    }
    if (strcmp(modelrt_get_provider_name(&h), "XPS") != 0) {
        printf("wrong provider: %s\n", modelrt_get_provider_name(&h));
        return 3;
    }
    memset(&sink, 0, sizeof(sink));
    sink.on_mesh_begin = on_mesh_begin;
    sink.on_triangle = on_triangle;
    rc = modelrt_visit(&h, MODELRT_ANIM_BIND_POSE, 0, &sink, 0);
    if (rc != MODELRT_OK) {
        printf("XPS visit failed: %s rc=%d\n", path, rc);
        return 4;
    }
    if (g_meshes == 0ul || g_triangles == 0ul || g_vertices_reported == 0ul) {
        printf("XPS geometry missing: %s meshes=%lu verts=%lu tris=%lu\n",
               path, g_meshes, g_vertices_reported, g_triangles);
        return 5;
    }
    printf("XPS regression OK: %s meshes=%lu verts=%lu tris=%lu bones=%lu\n",
           path, g_meshes, g_vertices_reported, g_triangles,
           (unsigned long)h.info.bone_count);
    modelrt_close(&h);
    return 0;
}

int main(int argc, char **argv)
{
    int i;
    int rc;
    if (argc < 2) return 1;
    for (i = 1; i < argc; ++i) {
        rc = run_one(argv[i]);
        if (rc != 0) return rc;
    }
    return 0;
}
