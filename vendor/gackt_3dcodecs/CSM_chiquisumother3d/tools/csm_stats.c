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

static void print_vec3(CSM3D_Vec3 p)
{
    print_fx(p.x);
    fputc(',', stdout);
    print_fx(p.y);
    fputc(',', stdout);
    print_fx(p.z);
}

int main(int argc, char **argv)
{
    FILE *fp;
    unsigned long n;
    int r;
    int i;
    CSM3D_MarkerStats stats;
    CSM3D_Vec3 center;

    if (argc < 2) {
        fputs("usage: csm_stats file.csm\n", stderr);
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

    printf("marker,index,kind,path_approx,max_speed_approx,max_speed_frame,min_xyz,max_xyz,first_xyz,last_xyz\n");
    i = 0;
    while (i < g_doc.marker_count) {
        r = csm3d_compute_marker_stats(&g_doc, i, &stats);
        if (r != CSM3D_OK) {
            return 1;
        }
        printf("%s,%d,%s,", g_doc.marker_name[i], i, csm3d_marker_kind_string(csm3d_marker_kind_from_name(g_doc.marker_name[i])));
        print_fx(stats.approximate_path_length);
        fputc(',', stdout);
        print_fx(stats.approximate_max_speed);
        printf(",%d,", stats.max_speed_frame_index);
        print_vec3(stats.bounds.min_point);
        fputc(',', stdout);
        print_vec3(stats.bounds.max_point);
        fputc(',', stdout);
        print_vec3(stats.first_point);
        fputc(',', stdout);
        print_vec3(stats.last_point);
        fputc('\n', stdout);
        i++;
    }

    if (g_doc.frame_count > 0) {
        r = csm3d_compute_frame_center(&g_doc, 0, &center);
        if (r == CSM3D_OK) {
            printf("# frame0_center,");
            print_vec3(center);
            fputc('\n', stdout);
        }
    }
    return 0;
}
