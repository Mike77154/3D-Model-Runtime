#include "vrml97_codec.h"

#include <stdio.h>
#include <string.h>

#ifndef VRML97_TOOL_MAX_INPUT
#define VRML97_TOOL_MAX_INPUT (4UL * 1024UL * 1024UL)
#endif

static char g_input[VRML97_TOOL_MAX_INPUT];
static vrml97_scene g_scene;

static unsigned long read_file_fixed(const char *path, char *buf, unsigned long cap, int *ok)
{
    FILE *f;
    unsigned long n;
    unsigned long got;

    *ok = 0;
    f = fopen(path, "rb");
    if (f == 0) {
        return 0;
    }
    n = 0;
    while (n < cap) {
        got = (unsigned long)fread(buf + n, 1u, (size_t)(cap - n), f);
        n += got;
        if (got == 0) {
            break;
        }
    }
    fclose(f);
    *ok = 1;
    return n;
}

int main(int argc, char **argv)
{
    int ok;
    int status;
    unsigned long len;
    const char *wrl_path;
    const char *obj_path;
    const char *mtl_path;

    if (argc < 3) {
        fputs("usage: vrml97_to_obj input.wrl output.obj [output.mtl]\n", stderr);
        return 2;
    }
    wrl_path = argv[1];
    obj_path = argv[2];
    mtl_path = argc >= 4 ? argv[3] : "out.mtl";
    len = read_file_fixed(wrl_path, g_input, VRML97_TOOL_MAX_INPUT, &ok);
    if (!ok) {
        fputs("could not read input\n", stderr);
        return 1;
    }
    status = vrml97_parse(&g_scene, g_input, len);
    if (status != VRML97_OK) {
        fprintf(stderr, "parse failed: %s at %d:%d: %s\n", vrml97_status_string(status), g_scene.error_line, g_scene.error_column, g_scene.last_error);
        return 1;
    }
    status = vrml97_write_obj_file(&g_scene, obj_path, mtl_path);
    if (status != VRML97_OK) {
        fprintf(stderr, "write failed: %s\n", vrml97_status_string(status));
        return 1;
    }
    printf("triangles=%lu lines=%lu points=%lu meshes=%d materials=%d textures=%d viewpoints=%d lights=%d backgrounds=%d fogs=%d navinfos=%d worldinfos=%d inlines=%d protos=%d proto_instances=%d routes=%d static_choices=%d concave_faces=%d runtime_nodes=%d unsupported_nodes=%d warnings=%d mask=0x%08lx\n",
           g_scene.triangles_count,
           g_scene.lines_count,
           g_scene.points_count,
           g_scene.mesh_count,
           g_scene.material_count,
           g_scene.texture_count,
           g_scene.viewpoint_count,
           g_scene.light_count,
           g_scene.background_count,
           g_scene.fog_count,
           g_scene.navigation_info_count,
           g_scene.world_info_count,
           g_scene.inline_count,
           g_scene.proto_count,
           g_scene.proto_instance_count,
           g_scene.route_count,
           g_scene.static_choice_count,
           g_scene.concave_face_count,
           g_scene.runtime_node_count,
           g_scene.unsupported_node_count,
           g_scene.warning_count,
           g_scene.warnings);
    return 0;
}
