#include <stdio.h>
#include "CSM_chiquisumother3d.h"

#ifndef CSM3D_TOOL_MAX_BYTES
#define CSM3D_TOOL_MAX_BYTES (8UL * 1024UL * 1024UL)
#endif

static char g_input[CSM3D_TOOL_MAX_BYTES + 1UL];
static CSM3D_Doc g_doc;
static CSM3D_Diagnostic g_diag;

static void file_fx(FILE *fp, csm3d_i32 v)
{
    char tmp[40];
    if (csm3d_fx_to_text(v, tmp, (csm3d_u32)sizeof(tmp)) != 0U) {
        fputs(tmp, fp);
    } else {
        fputs("0.000", fp);
    }
}

int main(int argc, char **argv)
{
    FILE *fp;
    unsigned long n;
    int r;
    int m;
    int f;
    int vbase;
    CSM3D_Vec3 p;

    if (argc < 3) {
        fputs("usage: csm_to_obj_paths in.csm out.obj\n", stderr);
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
    fp = fopen(argv[2], "wb");
    if (fp == 0) {
        fputs("could not open output\n", stderr);
        return 2;
    }
    fprintf(fp, "# marker paths exported by CSM_chiquisumother3d\n");
    m = 0;
    while (m < g_doc.marker_count) {
        fprintf(fp, "o %s\n", g_doc.marker_name[m]);
        f = 0;
        while (f < g_doc.frame_count) {
            p = g_doc.frame[f].point[m];
            fputs("v ", fp);
            file_fx(fp, p.x); fputc(' ', fp);
            file_fx(fp, p.y); fputc(' ', fp);
            file_fx(fp, p.z); fputc('\n', fp);
            f++;
        }
        if (g_doc.frame_count > 1) {
            vbase = m * g_doc.frame_count + 1;
            fputs("l", fp);
            f = 0;
            while (f < g_doc.frame_count) {
                fprintf(fp, " %d", vbase + f);
                f++;
            }
            fputc('\n', fp);
        }
        m++;
    }
    fclose(fp);
    return 0;
}
