#include <stdio.h>
#include "CSM_chiquisumother3d.h"

#ifndef CSM3D_TOOL_MAX_BYTES
#define CSM3D_TOOL_MAX_BYTES (8UL * 1024UL * 1024UL)
#endif

static char g_input[CSM3D_TOOL_MAX_BYTES + 1UL];
static CSM3D_Doc g_doc;
static CSM3D_Diagnostic g_diag;

static void print_fx(csm3d_i32 v)
{
    char tmp[40];
    if (csm3d_fx_to_text(v, tmp, (csm3d_u32)sizeof(tmp)) != 0U) {
        fputs(tmp, stdout);
    } else {
        fputs("0.000", stdout);
    }
}

int main(int argc, char **argv)
{
    FILE *fp;
    unsigned long n;
    int r;
    CSM3D_MotionQuality q;
    csm3d_i32 stationary_epsilon;
    csm3d_i32 high_speed_threshold;

    if (argc < 2) {
        fputs("usage: csm_quality file.csm [stationary_epsilon_units] [high_speed_units_per_second]\n", stderr);
        return 2;
    }

    stationary_epsilon = 1L;
    high_speed_threshold = 100000L;
    if (argc > 2) {
        r = csm3d_fx_parse(argv[2], &stationary_epsilon);
        if (r != CSM3D_OK) {
            fputs("bad stationary epsilon\n", stderr);
            return 2;
        }
    }
    if (argc > 3) {
        r = csm3d_fx_parse(argv[3], &high_speed_threshold);
        if (r != CSM3D_OK) {
            fputs("bad high-speed threshold\n", stderr);
            return 2;
        }
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

    r = csm3d_compute_motion_quality(&g_doc, stationary_epsilon, high_speed_threshold, &q);
    if (r != CSM3D_OK) {
        fprintf(stderr, "quality failed: %s\n", csm3d_error_string(r));
        return 1;
    }

    printf("markers,%d\n", q.marker_count);
    printf("stationary_markers,%d\n", q.stationary_marker_count);
    printf("moving_markers,%d\n", q.moving_marker_count);
    printf("high_speed_markers,%d\n", q.high_speed_marker_count);
    printf("total_path_approx,"); print_fx(q.total_path_length); printf("\n");
    printf("max_path_marker,%d,%s,", q.max_path_marker_index,
           (q.max_path_marker_index >= 0 ? g_doc.marker_name[q.max_path_marker_index] : "")); print_fx(q.max_path_length); printf("\n");
    printf("max_speed_marker,%d,%s,frame_index,%d,", q.max_speed_marker_index,
           (q.max_speed_marker_index >= 0 ? g_doc.marker_name[q.max_speed_marker_index] : ""), q.max_speed_frame_index); print_fx(q.max_marker_speed); printf("\n");
    return 0;
}
