#include <stdio.h>
#include "CSM_chiquisumother3d.h"

#ifndef CSM3D_TOOL_MAX_BYTES
#define CSM3D_TOOL_MAX_BYTES (8UL * 1024UL * 1024UL)
#endif

#ifndef CSM3D_TOOL_MAX_OUT_BYTES
#define CSM3D_TOOL_MAX_OUT_BYTES (12UL * 1024UL * 1024UL)
#endif

static char g_csm[CSM3D_TOOL_MAX_BYTES + 1UL];
static char g_mnm[CSM3D_TOOL_MAX_BYTES + 1UL];
static char g_out[CSM3D_TOOL_MAX_OUT_BYTES + 1UL];
static CSM3D_Doc g_doc;
static CSM3D_MarkerMap g_map;
static CSM3D_Diagnostic g_diag;

static int read_file(const char *path, char *buf, unsigned long cap, unsigned long *out_len)
{
    FILE *fp;
    unsigned long n;

    fp = fopen(path, "rb");
    if (fp == 0) {
        return 0;
    }
    n = (unsigned long)fread(buf, 1, cap, fp);
    fclose(fp);
    if (n == cap) {
        return 0;
    }
    buf[n] = '\0';
    if (out_len != 0) {
        *out_len = n;
    }
    return 1;
}

static int write_file(const char *path, const char *buf, unsigned long len)
{
    FILE *fp;
    unsigned long n;

    fp = fopen(path, "wb");
    if (fp == 0) {
        return 0;
    }
    n = (unsigned long)fwrite(buf, 1, len, fp);
    fclose(fp);
    return n == len;
}

int main(int argc, char **argv)
{
    unsigned long csm_len;
    unsigned long mnm_len;
    csm3d_u32 out_len;
    int r;

    if (argc < 4) {
        fputs("usage: csm_apply_mnm input.csm map.mnm output.csm\n", stderr);
        return 2;
    }
    if (!read_file(argv[1], g_csm, CSM3D_TOOL_MAX_BYTES, &csm_len)) {
        fputs("could not read CSM input or input is too large\n", stderr);
        return 2;
    }
    if (!read_file(argv[2], g_mnm, CSM3D_TOOL_MAX_BYTES, &mnm_len)) {
        fputs("could not read MNM input or input is too large\n", stderr);
        return 2;
    }

    r = csm3d_decode(g_csm, (csm3d_u32)csm_len, &g_doc, &g_diag);
    if (r != CSM3D_OK) {
        fprintf(stderr, "CSM decode failed: %s line=%d text=%s\n", csm3d_error_string(r), g_diag.line, g_diag.text);
        return 1;
    }
    r = csm3d_mnm_decode(g_mnm, (csm3d_u32)mnm_len, &g_map, &g_diag);
    if (r != CSM3D_OK) {
        fprintf(stderr, "MNM decode failed: %s line=%d text=%s\n", csm3d_error_string(r), g_diag.line, g_diag.text);
        return 1;
    }
    r = csm3d_apply_marker_map(&g_doc, &g_map, &g_diag);
    if (r != CSM3D_OK) {
        fprintf(stderr, "MNM apply failed: %s marker=%d text=%s\n", csm3d_error_string(r), g_diag.marker_index, g_diag.text);
        return 1;
    }
    r = csm3d_encode(&g_doc, g_out, (csm3d_u32)CSM3D_TOOL_MAX_OUT_BYTES, &out_len, &g_diag);
    if (r != CSM3D_OK) {
        fprintf(stderr, "encode failed: %s text=%s\n", csm3d_error_string(r), g_diag.text);
        return 1;
    }
    if (!write_file(argv[3], g_out, (unsigned long)out_len)) {
        fputs("could not write output\n", stderr);
        return 2;
    }

    printf("applied %d marker map entries; markers=%d frames=%d\n", g_map.entry_count, g_doc.marker_count, g_doc.frame_count);
    return 0;
}
