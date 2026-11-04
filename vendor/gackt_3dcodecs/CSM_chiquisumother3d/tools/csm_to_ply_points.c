#include <stdio.h>
#include "CSM_chiquisumother3d.h"

#ifndef CSM3D_TOOL_MAX_BYTES
#define CSM3D_TOOL_MAX_BYTES (8UL * 1024UL * 1024UL)
#endif

static char g_input[CSM3D_TOOL_MAX_BYTES + 1UL];
static CSM3D_Doc g_doc;
static CSM3D_Diagnostic g_diag;

static int parse_int_arg(const char *s, int *out_value)
{
    int sign;
    int value;
    int any;

    if (s == 0 || out_value == 0) {
        return 0;
    }
    sign = 1;
    value = 0;
    any = 0;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    while (*s != '\0') {
        if (*s < '0' || *s > '9') {
            return 0;
        }
        any = 1;
        value = value * 10 + (*s - '0');
        s++;
    }
    if (!any) {
        return 0;
    }
    *out_value = value * sign;
    return 1;
}

static void file_i32(FILE *fp, csm3d_i32 v)
{
    fprintf(fp, "%ld", (long)v);
}

int main(int argc, char **argv)
{
    FILE *fp;
    unsigned long n;
    int r;
    int frame_index;
    int i;
    CSM3D_Vec3 p;

    if (argc < 3) {
        fputs("usage: csm_to_ply_points in.csm out.ply [stored_frame_index]\n", stderr);
        return 2;
    }

    frame_index = 0;
    if (argc >= 4) {
        if (!parse_int_arg(argv[3], &frame_index)) {
            fputs("bad frame index\n", stderr);
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
    if (frame_index < 0 || frame_index >= g_doc.frame_count) {
        fputs("frame index out of range\n", stderr);
        return 2;
    }

    fp = fopen(argv[2], "wb");
    if (fp == 0) {
        fputs("could not open output\n", stderr);
        return 2;
    }

    fprintf(fp, "ply\n");
    fprintf(fp, "format ascii 1.0\n");
    fprintf(fp, "comment CSM_chiquisumother3d point cloud\n");
    fprintf(fp, "comment source_frame_number %d stored_index %d\n", g_doc.frame[frame_index].frame_number, frame_index);
    fprintf(fp, "comment coordinates_are_fixed_x1000\n");
    i = 0;
    while (i < g_doc.marker_count) {
        fprintf(fp, "comment marker_%d %s\n", i, g_doc.marker_name[i]);
        i++;
    }
    fprintf(fp, "element vertex %d\n", g_doc.marker_count);
    fprintf(fp, "property int x_scaled_1000\n");
    fprintf(fp, "property int y_scaled_1000\n");
    fprintf(fp, "property int z_scaled_1000\n");
    fprintf(fp, "end_header\n");

    i = 0;
    while (i < g_doc.marker_count) {
        p = g_doc.frame[frame_index].point[i];
        file_i32(fp, p.x); fputc(' ', fp);
        file_i32(fp, p.y); fputc(' ', fp);
        file_i32(fp, p.z); fputc('\n', fp);
        i++;
    }
    fclose(fp);
    return 0;
}
