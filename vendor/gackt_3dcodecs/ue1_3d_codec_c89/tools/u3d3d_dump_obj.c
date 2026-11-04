#include "u3d3d.h"
#include <stdio.h>

#ifndef U3D3D_TOOL_MAX_BYTES
#define U3D3D_TOOL_MAX_BYTES 1048576u
#endif
#ifndef U3D3D_TOOL_MAX_TRIS
#define U3D3D_TOOL_MAX_TRIS 32768u
#endif
#ifndef U3D3D_TOOL_MAX_VERTS
#define U3D3D_TOOL_MAX_VERTS 65535u
#endif
#ifndef U3D3D_TOOL_MAX_UC_BYTES
#define U3D3D_TOOL_MAX_UC_BYTES 131072u
#endif

static u3d3d_u8 g_d_bytes[U3D3D_TOOL_MAX_BYTES];
static u3d3d_u8 g_a_bytes[U3D3D_TOOL_MAX_BYTES];
static u3d3d_u8 g_uc_bytes[U3D3D_TOOL_MAX_UC_BYTES + 1u];
static u3d3d_triangle g_tris[U3D3D_TOOL_MAX_TRIS];
static u3d3d_vertex g_verts[U3D3D_TOOL_MAX_VERTS];

int main(int argc, char **argv)
{
    u3d3d_u32 d_size;
    u3d3d_u32 a_size;
    u3d3d_u32 uc_size;
    u3d3d_model model;
    u3d3d_uc_info uc_info;
    u3d3d_uc_info *uc_ptr;
    FILE *out;
    int rc;
    u3d3d_u16 frame;

    if (argc < 5) {
        fprintf(stderr, "usage: u3d3d_dump_obj model_d.3d model_a.3d frame output.obj [model.uc]\n");
        return 2;
    }

    frame = (u3d3d_u16)0u;
    if (argv[3][0] >= '0' && argv[3][0] <= '9') {
        const char *p;
        unsigned long v;
        p = argv[3];
        v = 0ul;
        while (*p >= '0' && *p <= '9') {
            v = (v * 10ul) + (unsigned long)(*p - '0');
            ++p;
        }
        frame = (u3d3d_u16)v;
    }

    rc = u3d3d_fread_all(argv[1], g_d_bytes, U3D3D_TOOL_MAX_BYTES, &d_size);
    if (rc != U3D3D_OK) {
        fprintf(stderr, "read d failed: %s\n", u3d3d_error_string(rc));
        return 1;
    }
    rc = u3d3d_fread_all(argv[2], g_a_bytes, U3D3D_TOOL_MAX_BYTES, &a_size);
    if (rc != U3D3D_OK) {
        fprintf(stderr, "read a failed: %s\n", u3d3d_error_string(rc));
        return 1;
    }

    u3d3d_model_init(&model, g_tris, (u3d3d_u16)U3D3D_TOOL_MAX_TRIS);
    rc = u3d3d_decode_pair_frame(g_d_bytes, d_size, g_a_bytes, a_size, frame,
                                 &model, g_verts, (u3d3d_u16)U3D3D_TOOL_MAX_VERTS);
    if (rc != U3D3D_OK) {
        fprintf(stderr, "decode failed: %s\n", u3d3d_error_string(rc));
        return 1;
    }

    uc_ptr = (u3d3d_uc_info *)0;
    if (argc >= 6) {
        u3d3d_uc_info_init(&uc_info);
        rc = u3d3d_fread_all(argv[5], g_uc_bytes, U3D3D_TOOL_MAX_UC_BYTES, &uc_size);
        if (rc == U3D3D_OK) {
            g_uc_bytes[uc_size] = 0u;
            u3d3d_parse_uc_info((const char *)g_uc_bytes, uc_size, &uc_info);
            uc_ptr = &uc_info;
        }
    }

    out = fopen(argv[4], "wb");
    if (out == (FILE *)0) {
        fprintf(stderr, "cannot create obj\n");
        return 1;
    }
    rc = u3d3d_export_obj(out, &model, g_verts, "u3d3d_mesh", uc_ptr, 0u);
    fclose(out);
    if (rc != U3D3D_OK) {
        fprintf(stderr, "export obj failed: %s\n", u3d3d_error_string(rc));
        return 1;
    }
    return 0;
}
