/* Optional tiny file dumper. This wrapper uses stdio for convenience; the decoder core does not. */
#include "gxsi_decoder3d.h"
#include <stdio.h>
#include <string.h>

#ifndef GXSI_TOOL_MAX_FILE
#define GXSI_TOOL_MAX_FILE (1024UL * 1024UL * 8UL)
#endif

static char g_file_buffer[GXSI_TOOL_MAX_FILE];

static void wr(void *user, const char *text) {
    FILE *f = (FILE *)user;
    fputs(text, f);
}

int main(int argc, char **argv) {
    FILE *f;
    long n;
    GXSI_Scene scene;
    GXSI_Options opt;
    int rc;

    if (argc < 2) {
        fputs("usage: gxsi_file_dump model.xsi [out.obj]\n", stderr);
        return 1;
    }
    f = fopen(argv[1], "rb");
    if (!f) {
        fputs("cannot open input\n", stderr);
        return 1;
    }
    n = (long)fread(g_file_buffer, 1, sizeof(g_file_buffer), f);
    fclose(f);
    if (n <= 0) {
        fputs("empty input\n", stderr);
        return 1;
    }

    gxsi_default_options(&opt);
    rc = gxsi_decode_auto(g_file_buffer, (size_t)n, &scene, &opt);
    if (rc != GXSI_OK) {
        fprintf(stderr, "decode failed: %s\n", gxsi_status_string(rc));
        return 2;
    }
    fprintf(stderr, "decoded: meshes=%u vertices=%u normals=%u uvs=%u triangles=%u materials=%u bones=%u weights=%u fcurves=%u keys=%u nurbs=%u cp=%u\n",
            scene.mesh_count, scene.vertex_count, scene.normal_count, scene.texcoord_count,
            scene.triangle_count, scene.material_count, scene.bone_count, scene.weight_count,
            scene.fcurve_count, scene.fcurve_key_count, scene.nurbs_count, scene.nurbs_cp_count);

    if (argc >= 3) {
        FILE *out = fopen(argv[2], "wb");
        if (!out) {
            fputs("cannot open output\n", stderr);
            return 3;
        }
        gxsi_write_obj(&scene, wr, out);
        fclose(out);
    }
    return 0;
}
