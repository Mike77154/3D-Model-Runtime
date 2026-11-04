#include <stdio.h>
#include "CSM_chiquisumother3d.h"

#ifndef CSM3D_TOOL_MAX_BYTES
#define CSM3D_TOOL_MAX_BYTES (8UL * 1024UL * 1024UL)
#endif

static char g_input[CSM3D_TOOL_MAX_BYTES + 1UL];
static CSM3D_Doc g_doc;
static CSM3D_Diagnostic g_diag;
static CSM3D_MarkerCoverage g_cov;

int main(int argc, char **argv)
{
    FILE *fp;
    unsigned long n;
    int r;
    int score;
    int i;

    if (argc < 2) {
        fputs("usage: csm_marker_report file.csm\n", stderr);
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

    printf("probably_csm,%d\n", csm3d_is_probably_csm(g_input, (csm3d_u32)n, &score));
    printf("probe_score,%d\n", score);

    r = csm3d_decode(g_input, (csm3d_u32)n, &g_doc, &g_diag);
    if (r != CSM3D_OK) {
        fprintf(stderr, "decode failed: %s line=%d text=%s\n", csm3d_error_string(r), g_diag.line, g_diag.text);
        return 1;
    }
    r = csm3d_compute_marker_coverage(&g_doc, &g_cov);
    if (r != CSM3D_OK) return 1;

    printf("markers,%d\n", g_doc.marker_count);
    printf("standard_total,%d\n", g_cov.standard_marker_total);
    printf("standard_present,%d\n", g_cov.present_standard_count);
    printf("prop_present,%d\n", g_cov.present_prop_count);
    printf("unknown,%d\n", g_cov.unknown_count);
    printf("missing_standard_count,%d\n", g_cov.missing_standard_count);
    i = 0;
    while (i < g_cov.missing_standard_count && i < CSM3D_MAX_COVERAGE_MISSING) {
        printf("missing_standard,%s\n", g_cov.missing_standard[i]);
        i++;
    }
    return 0;
}
