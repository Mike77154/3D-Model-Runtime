#include "u3d3d.h"
#include <stdio.h>

#ifndef U3D3D_TOOL_MAX_BYTES
#define U3D3D_TOOL_MAX_BYTES 1048576u
#endif
#ifndef U3D3D_TOOL_MAX_TRIS
#define U3D3D_TOOL_MAX_TRIS 32768u
#endif

static u3d3d_u8 g_d_bytes[U3D3D_TOOL_MAX_BYTES];
static u3d3d_u8 g_a_bytes[U3D3D_TOOL_MAX_BYTES];
static u3d3d_triangle g_tris[U3D3D_TOOL_MAX_TRIS];

int main(int argc, char **argv)
{
    u3d3d_u32 d_size;
    u3d3d_u32 a_size;
    u3d3d_model model;
    u3d3d_diag diag;
    int rc;

    if (argc < 3) {
        fprintf(stderr, "usage: u3d3d_info model_d.3d model_a.3d\n");
        return 2;
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
    rc = u3d3d_decode_d(g_d_bytes, d_size, &model);
    if (rc != U3D3D_OK) {
        fprintf(stderr, "decode d failed: %s\n", u3d3d_error_string(rc));
        return 1;
    }
    rc = u3d3d_decode_a_header(g_a_bytes, a_size, &model);
    if (rc != U3D3D_OK) {
        fprintf(stderr, "decode a header failed: %s\n", u3d3d_error_string(rc));
        return 1;
    }
    rc = u3d3d_build_diag(g_d_bytes, d_size, g_a_bytes, a_size, &model, &diag);
    if (rc != U3D3D_OK) {
        fprintf(stderr, "diag failed: %s\n", u3d3d_error_string(rc));
        return 1;
    }

    printf("u3d3d info\n");
    printf("  vertices: %lu\n", (unsigned long)model.vertex_count);
    printf("  triangles: %lu\n", (unsigned long)model.triangle_count);
    printf("  frames: %lu\n", (unsigned long)model.frame_count);
    printf("  frame_size: %lu\n", (unsigned long)model.frame_size);
    printf("  expected d size: %lu\n", (unsigned long)diag.d_expected_size);
    printf("  expected a size: %lu\n", (unsigned long)diag.a_expected_full_size);
    printf("  max vertex index: %lu\n", (unsigned long)diag.max_vertex_index);
    printf("  bad indices: %lu\n", (unsigned long)diag.bad_triangle_indices);
    printf("  max texture slot: %lu\n", (unsigned long)diag.max_texture_slot);
    printf("  weapon placeholders: %lu\n", (unsigned long)diag.weapon_placeholders);
    return diag.bad_triangle_indices == 0u ? 0 : 3;
}
