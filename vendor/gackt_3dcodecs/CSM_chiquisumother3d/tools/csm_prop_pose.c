#include <stdio.h>
#include "CSM_chiquisumother3d.h"

#ifndef CSM3D_TOOL_MAX_BYTES
#define CSM3D_TOOL_MAX_BYTES (8UL * 1024UL * 1024UL)
#endif

static char g_input[CSM3D_TOOL_MAX_BYTES + 1UL];
static CSM3D_Doc g_doc;
static CSM3D_Diagnostic g_diag;

static int side_from_text(const char *s)
{
    if (s == 0) return -1;
    if ((s[0] == 'l' || s[0] == 'L') && s[1] == '\0') return CSM3D_PROP_LEFT;
    if ((s[0] == 'm' || s[0] == 'M') && s[1] == '\0') return CSM3D_PROP_MIDDLE;
    if ((s[0] == 'r' || s[0] == 'R') && s[1] == '\0') return CSM3D_PROP_RIGHT;
    if ((s[0] == 'l' || s[0] == 'L') && (s[1] == 'e' || s[1] == 'E')) return CSM3D_PROP_LEFT;
    if ((s[0] == 'm' || s[0] == 'M') && (s[1] == 'i' || s[1] == 'I')) return CSM3D_PROP_MIDDLE;
    if ((s[0] == 'r' || s[0] == 'R') && (s[1] == 'i' || s[1] == 'I')) return CSM3D_PROP_RIGHT;
    return -1;
}

static void print_fx(csm3d_i32 v)
{
    char tmp[40];
    if (csm3d_fx_to_text(v, tmp, (csm3d_u32)sizeof(tmp)) != 0U) {
        fputs(tmp, stdout);
    } else {
        fputs("0.000", stdout);
    }
}

static void print_vec3_csv(CSM3D_Vec3 p)
{
    print_fx(p.x); fputc(',', stdout);
    print_fx(p.y); fputc(',', stdout);
    print_fx(p.z);
}

int main(int argc, char **argv)
{
    FILE *fp;
    unsigned long n;
    int r;
    int side;
    int f;
    CSM3D_PropPose pose;

    if (argc < 3) {
        fputs("usage: csm_prop_pose file.csm left|middle|right\n", stderr);
        return 2;
    }
    side = side_from_text(argv[2]);
    if (side < 0) {
        fputs("bad side; use left, middle, or right\n", stderr);
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

    printf("frame_number,frame_index,side,origin_x,origin_y,origin_z,bottom_to_top_x,bottom_to_top_y,bottom_to_top_z,bottom_to_middle_x,bottom_to_middle_y,bottom_to_middle_z,rough_normal_x,rough_normal_y,rough_normal_z,length_approx\n");
    f = 0;
    while (f < g_doc.frame_count) {
        r = csm3d_compute_prop_pose_at_frame(&g_doc, side, f, &pose);
        if (r != CSM3D_OK) {
            fprintf(stderr, "prop pose failed: %s\n", csm3d_error_string(r));
            return 1;
        }
        printf("%d,%d,%s,", g_doc.frame[f].frame_number, f, csm3d_prop_side_name(side));
        print_vec3_csv(pose.origin); fputc(',', stdout);
        print_vec3_csv(pose.bottom_to_top); fputc(',', stdout);
        print_vec3_csv(pose.bottom_to_middle); fputc(',', stdout);
        print_vec3_csv(pose.rough_plane_normal); fputc(',', stdout);
        print_fx(pose.approximate_length); fputc('\n', stdout);
        f++;
    }
    return 0;
}
