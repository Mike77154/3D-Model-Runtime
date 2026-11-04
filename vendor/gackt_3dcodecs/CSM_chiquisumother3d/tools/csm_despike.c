#include <stdio.h>
#include "CSM_chiquisumother3d.h"

#ifndef CSM3D_TOOL_MAX_BYTES
#define CSM3D_TOOL_MAX_BYTES (8UL * 1024UL * 1024UL)
#endif

static char g_input[CSM3D_TOOL_MAX_BYTES + 1UL];
static char g_output[CSM3D_TOOL_MAX_BYTES + 1UL];
static CSM3D_Doc g_doc;
static CSM3D_Diagnostic g_diag;

static int parse_long_arg(const char *s, csm3d_i32 *out_value)
{
    int sign;
    csm3d_i32 value;
    int any;

    if (s == 0 || out_value == 0) return 0;
    sign = 1;
    value = 0;
    any = 0;
    if (*s == '-') { sign = -1; s++; }
    while (*s != '\0') {
        if (*s < '0' || *s > '9') return 0;
        any = 1;
        value = value * 10L + (csm3d_i32)(*s - '0');
        s++;
    }
    if (!any) return 0;
    *out_value = value * (csm3d_i32)sign;
    return 1;
}

int main(int argc, char **argv)
{
    FILE *fp;
    unsigned long n;
    csm3d_u32 out_len;
    csm3d_i32 threshold;
    int r;
    int replaced;

    if (argc < 4) {
        fputs("usage: csm_despike in.csm out.csm max_units_per_second_scaled\n", stderr);
        return 2;
    }
    if (!parse_long_arg(argv[3], &threshold)) {
        fputs("bad threshold\n", stderr);
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
    r = csm3d_sort_frames(&g_doc);
    if (r != CSM3D_OK) return 1;
    r = csm3d_despike_linear(&g_doc, threshold, &replaced);
    if (r != CSM3D_OK) return 1;
    r = csm3d_update_declared_metadata(&g_doc);
    if (r != CSM3D_OK) return 1;
    r = csm3d_encode(&g_doc, g_output, (csm3d_u32)CSM3D_TOOL_MAX_BYTES, &out_len, &g_diag);
    if (r != CSM3D_OK) {
        fprintf(stderr, "encode failed: %s text=%s\n", csm3d_error_string(r), g_diag.text);
        return 1;
    }
    fp = fopen(argv[2], "wb");
    if (fp == 0) {
        fputs("could not open output\n", stderr);
        return 2;
    }
    fwrite(g_output, 1, (size_t)out_len, fp);
    fclose(fp);
    printf("despiked markers: %d\n", replaced);
    return 0;
}
