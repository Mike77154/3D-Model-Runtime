#ifndef ACC3D_ENABLE_STDIO
#define ACC3D_ENABLE_STDIO
#endif
#include "acc3d.h"
#include <stdio.h>

#ifndef ACC3D_FILE_BUFFER_CAP
#define ACC3D_FILE_BUFFER_CAP (4UL * 1024UL * 1024UL)
#endif

static char g_file_buffer[ACC3D_FILE_BUFFER_CAP];
static struct acc3d_model g_model;

static void usage(void)
{
    puts("usage: acc3d_dump [-strip] input.acc [out.obj]");
}

int main(int argc, char **argv)
{
    const char *in_path;
    const char *out_path;
    size_t size;
    unsigned long opt;
    int argi;
    int r;

    opt = ACC3D_OPT_NONE;
    out_path = (const char *)0;
    argi = 1;
    if (argc < 2) {
        usage();
        return 2;
    }
    if (argi < argc && argv[argi][0] == '-' && argv[argi][1] == 's') {
        opt |= ACC3D_OPT_STRIP_AS_STRIP;
        ++argi;
    }
    if (argi >= argc) {
        usage();
        return 2;
    }
    in_path = argv[argi++];
    if (argi < argc) out_path = argv[argi++];

    r = acc3d_load_file(in_path, g_file_buffer, sizeof(g_file_buffer), &size);
    if (r != ACC3D_OK) {
        printf("load failed: %s\n", acc3d_result_string(r));
        return 1;
    }
    r = acc3d_decode_memory(g_file_buffer, size, opt, &g_model);
    if (r != ACC3D_OK) {
        printf("decode failed at line %ld: %s diag=0x%lx\n", g_model.line, acc3d_result_string(r), g_model.diag_bits);
        return 1;
    }
    printf("ACC3D ok: version=%ld acc_like=%ld strip_suffix=%ld diag=0x%lx\n",
        g_model.version_hex, g_model.is_acc_like, g_model.header_has_strip_suffix, g_model.diag_bits);
    printf("materials=%ld objects=%ld vertices=%ld surfaces=%ld refs=%ld triangles=%ld\n",
        g_model.material_count, g_model.object_count, g_model.vertex_count,
        g_model.surface_count, g_model.ref_count, g_model.triangle_count);
    if (out_path != (const char *)0) {
        r = acc3d_write_obj_file(out_path, &g_model);
        if (r != ACC3D_OK) {
            printf("obj write failed: %s\n", acc3d_result_string(r));
            return 1;
        }
        printf("wrote OBJ: %s\n", out_path);
    }
    return 0;
}
