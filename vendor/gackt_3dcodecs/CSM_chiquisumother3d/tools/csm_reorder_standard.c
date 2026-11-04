#include <stdio.h>
#include "CSM_chiquisumother3d.h"

#ifndef CSM3D_TOOL_MAX_BYTES
#define CSM3D_TOOL_MAX_BYTES (8UL * 1024UL * 1024UL)
#endif
#ifndef CSM3D_TOOL_OUT_BYTES
#define CSM3D_TOOL_OUT_BYTES (16UL * 1024UL * 1024UL)
#endif

static char g_input[CSM3D_TOOL_MAX_BYTES + 1UL];
static char g_output[CSM3D_TOOL_OUT_BYTES];
static CSM3D_Doc g_doc;
static CSM3D_Diagnostic g_diag;

int main(int argc, char **argv)
{
    FILE *fp;
    unsigned long n;
    csm3d_u32 out_len;
    int r;

    if (argc < 3) {
        fputs("usage: csm_reorder_standard input.csm output.csm\n", stderr);
        return 2;
    }

    fp = fopen(argv[1], "rb");
    if (fp == 0) {
        fputs("could not open input\n", stderr);
        return 2;
    }
    n = (unsigned long)fread(g_input, 1, CSM3D_TOOL_MAX_BYTES, fp);
    fclose(fp);
    if (n == CSM3D_TOOL_MAX_BYTES) {
        fputs("input may be larger than tool buffer\n", stderr);
        return 2;
    }
    g_input[n] = '\0';

    r = csm3d_decode(g_input, (csm3d_u32)n, &g_doc, &g_diag);
    if (r != CSM3D_OK) {
        fprintf(stderr, "decode failed: %s line=%d text=%s\n", csm3d_error_string(r), g_diag.line, g_diag.text);
        return 1;
    }

    r = csm3d_reorder_markers_to_standard(&g_doc);
    if (r != CSM3D_OK) {
        fprintf(stderr, "reorder failed: %s\n", csm3d_error_string(r));
        return 1;
    }

    r = csm3d_encode(&g_doc, g_output, (csm3d_u32)sizeof(g_output), &out_len, &g_diag);
    if (r != CSM3D_OK) {
        fprintf(stderr, "encode failed: %s text=%s\n", csm3d_error_string(r), g_diag.text);
        return 1;
    }

    fp = fopen(argv[2], "wb");
    if (fp == 0) {
        fputs("could not open output\n", stderr);
        return 2;
    }
    fwrite(g_output, 1, out_len, fp);
    fclose(fp);
    return 0;
}
