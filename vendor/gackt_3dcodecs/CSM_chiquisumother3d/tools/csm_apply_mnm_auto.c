#include <stdio.h>
#include "CSM_chiquisumother3d.h"

#ifndef CSM3D_TOOL_MAX_BYTES
#define CSM3D_TOOL_MAX_BYTES (8UL * 1024UL * 1024UL)
#endif

static char g_input[CSM3D_TOOL_MAX_BYTES + 1UL];
static char g_mnm[CSM3D_TOOL_MAX_BYTES + 1UL];
static char g_output[CSM3D_TOOL_MAX_BYTES + 1UL];
static CSM3D_Doc g_doc;
static CSM3D_MarkerMap g_map;
static CSM3D_Diagnostic g_diag;

static int read_all(const char *path, char *buf, unsigned long cap, unsigned long *out_len)
{
    FILE *fp;
    unsigned long n;

    fp = fopen(path, "rb");
    if (fp == 0) return 0;
    n = (unsigned long)fread(buf, 1, cap, fp);
    fclose(fp);
    if (n == cap) return 0;
    buf[n] = '\0';
    if (out_len != 0) *out_len = n;
    return 1;
}

int main(int argc, char **argv)
{
    unsigned long in_len;
    unsigned long mnm_len;
    csm3d_u32 out_len;
    int r;
    int direction;
    FILE *fp;

    if (argc < 4) {
        fputs("usage: csm_apply_mnm_auto in.csm map.mnm out.csm\n", stderr);
        return 2;
    }
    if (!read_all(argv[1], g_input, CSM3D_TOOL_MAX_BYTES, &in_len)) {
        fputs("could not read input CSM or tool buffer too small\n", stderr);
        return 2;
    }
    if (!read_all(argv[2], g_mnm, CSM3D_TOOL_MAX_BYTES, &mnm_len)) {
        fputs("could not read MNM or tool buffer too small\n", stderr);
        return 2;
    }

    r = csm3d_decode(g_input, (csm3d_u32)in_len, &g_doc, &g_diag);
    if (r != CSM3D_OK) {
        fprintf(stderr, "decode failed: %s line=%d text=%s\n", csm3d_error_string(r), g_diag.line, g_diag.text);
        return 1;
    }
    r = csm3d_mnm_decode(g_mnm, (csm3d_u32)mnm_len, &g_map, &g_diag);
    if (r != CSM3D_OK) {
        fprintf(stderr, "mnm decode failed: %s line=%d text=%s\n", csm3d_error_string(r), g_diag.line, g_diag.text);
        return 1;
    }
    r = csm3d_apply_marker_map_best(&g_doc, &g_map, &direction, &g_diag);
    if (r != CSM3D_OK) {
        fprintf(stderr, "apply failed: %s text=%s\n", csm3d_error_string(r), g_diag.text);
        return 1;
    }
    r = csm3d_encode(&g_doc, g_output, (csm3d_u32)CSM3D_TOOL_MAX_BYTES, &out_len, &g_diag);
    if (r != CSM3D_OK) {
        fprintf(stderr, "encode failed: %s text=%s\n", csm3d_error_string(r), g_diag.text);
        return 1;
    }
    fp = fopen(argv[3], "wb");
    if (fp == 0) {
        fputs("could not open output\n", stderr);
        return 2;
    }
    fwrite(g_output, 1, (size_t)out_len, fp);
    fclose(fp);
    printf("applied MNM direction: %s\n", direction == CSM3D_MNM_DIR_TARGET_TO_SOURCE ? "target-to-source" : "source-to-target");
    return 0;
}
