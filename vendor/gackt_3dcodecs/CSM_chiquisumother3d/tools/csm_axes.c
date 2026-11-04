#include <stdio.h>
#include "CSM_chiquisumother3d.h"

#ifndef CSM3D_TOOL_MAX_BYTES
#define CSM3D_TOOL_MAX_BYTES (8UL * 1024UL * 1024UL)
#endif

#ifndef CSM3D_TOOL_MAX_OUT_BYTES
#define CSM3D_TOOL_MAX_OUT_BYTES (12UL * 1024UL * 1024UL)
#endif

static char g_input[CSM3D_TOOL_MAX_BYTES + 1UL];
static char g_out[CSM3D_TOOL_MAX_OUT_BYTES + 1UL];
static CSM3D_Doc g_doc;
static CSM3D_Diagnostic g_diag;

static int parse_axis(const char *s)
{
    if (s == 0 || s[0] == '\0') return -1;
    if (s[0] == 'x' || s[0] == 'X') return CSM3D_AXIS_X;
    if (s[0] == 'y' || s[0] == 'Y') return CSM3D_AXIS_Y;
    if (s[0] == 'z' || s[0] == 'Z') return CSM3D_AXIS_Z;
    return -1;
}

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
    unsigned long input_len;
    csm3d_u32 out_len;
    int from_axis;
    int to_axis;
    int r;

    if (argc < 5) {
        fputs("usage: csm_axes input.csm output.csm from_up_axis to_up_axis\n", stderr);
        fputs("axis values: x y z\n", stderr);
        return 2;
    }
    from_axis = parse_axis(argv[3]);
    to_axis = parse_axis(argv[4]);
    if (from_axis < 0 || to_axis < 0) {
        fputs("bad axis; use x, y, or z\n", stderr);
        return 2;
    }
    if (!read_file(argv[1], g_input, CSM3D_TOOL_MAX_BYTES, &input_len)) {
        fputs("could not read input or input is too large\n", stderr);
        return 2;
    }
    r = csm3d_decode(g_input, (csm3d_u32)input_len, &g_doc, &g_diag);
    if (r != CSM3D_OK) {
        fprintf(stderr, "decode failed: %s line=%d text=%s\n", csm3d_error_string(r), g_diag.line, g_diag.text);
        return 1;
    }
    r = csm3d_convert_up_axis(&g_doc, from_axis, to_axis);
    if (r != CSM3D_OK) {
        fprintf(stderr, "axis conversion failed: %s\n", csm3d_error_string(r));
        return 1;
    }
    r = csm3d_encode(&g_doc, g_out, (csm3d_u32)CSM3D_TOOL_MAX_OUT_BYTES, &out_len, &g_diag);
    if (r != CSM3D_OK) {
        fprintf(stderr, "encode failed: %s text=%s\n", csm3d_error_string(r), g_diag.text);
        return 1;
    }
    if (!write_file(argv[2], g_out, (unsigned long)out_len)) {
        fputs("could not write output\n", stderr);
        return 2;
    }
    printf("converted up axis\n");
    return 0;
}
