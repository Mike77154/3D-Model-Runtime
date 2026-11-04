#include <stdio.h>
#include "CSM_chiquisumother3d.h"

#ifndef CSM3D_TOOL_MAX_BYTES
#define CSM3D_TOOL_MAX_BYTES (8UL * 1024UL * 1024UL)
#endif

static char g_input[CSM3D_TOOL_MAX_BYTES + 1UL];
static char g_output[CSM3D_TOOL_MAX_BYTES + 1UL];
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

static int streq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

int main(int argc, char **argv)
{
    FILE *fp;
    unsigned long n;
    csm3d_u32 out_len;
    int r;
    int argi;
    int do_upper;
    int do_fill;
    int do_renumber;
    int first_frame;
    int step;

    if (argc < 3) {
        fputs("usage: csm_sanitize in.csm out.csm [--upper] [--fill] [--renumber first step]\n", stderr);
        return 2;
    }

    do_upper = 0;
    do_fill = 0;
    do_renumber = 0;
    first_frame = 1;
    step = 1;
    argi = 3;
    while (argi < argc) {
        if (streq(argv[argi], "--upper")) {
            do_upper = 1;
            argi++;
        } else if (streq(argv[argi], "--fill")) {
            do_fill = 1;
            argi++;
        } else if (streq(argv[argi], "--renumber")) {
            if (argi + 2 >= argc) {
                fputs("--renumber needs first and step\n", stderr);
                return 2;
            }
            if (!parse_int_arg(argv[argi + 1], &first_frame) || !parse_int_arg(argv[argi + 2], &step)) {
                fputs("bad --renumber arguments\n", stderr);
                return 2;
            }
            do_renumber = 1;
            argi += 3;
        } else {
            fputs("unknown option\n", stderr);
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
        fprintf(stderr, "decode failed: %s line=%d frame=%d marker=%d text=%s\n",
                csm3d_error_string(r), g_diag.line, g_diag.frame_index, g_diag.marker_index, g_diag.text);
        return 1;
    }

    if (do_upper) {
        r = csm3d_normalize_marker_names_upper(&g_doc);
        if (r != CSM3D_OK) {
            fprintf(stderr, "upper normalize failed: %s\n", csm3d_error_string(r));
            return 1;
        }
    }

    r = csm3d_repair_basic(&g_doc);
    if (r != CSM3D_OK) {
        fprintf(stderr, "repair failed: %s\n", csm3d_error_string(r));
        return 1;
    }

    if (do_fill) {
        r = csm3d_fill_missing_frames(&g_doc);
        if (r != CSM3D_OK) {
            fprintf(stderr, "fill failed: %s\n", csm3d_error_string(r));
            return 1;
        }
        r = csm3d_update_declared_metadata(&g_doc);
        if (r != CSM3D_OK) {
            fprintf(stderr, "metadata update failed: %s\n", csm3d_error_string(r));
            return 1;
        }
    }

    if (do_renumber) {
        r = csm3d_renumber_frames(&g_doc, first_frame, step);
        if (r != CSM3D_OK) {
            fprintf(stderr, "renumber failed: %s\n", csm3d_error_string(r));
            return 1;
        }
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
