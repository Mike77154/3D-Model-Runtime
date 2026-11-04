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
        fputs("?", stdout);
    }
}

static void print_vec3(CSM3D_Vec3 p)
{
    print_fx(p.x);
    printf(" ");
    print_fx(p.y);
    printf(" ");
    print_fx(p.z);
}

static void print_analysis(const CSM3D_Analysis *a)
{
    printf("analysis flags: 0x%08lx\n", (unsigned long)a->flags);
    if ((a->flags & CSM3D_ANALYSIS_DECLARED_MARKER_MISMATCH) != 0UL) {
        printf("  warning: $NumMarkers differs from $Order count\n");
    }
    if ((a->flags & CSM3D_ANALYSIS_DUPLICATE_MARKERS) != 0UL) {
        printf("  warning: duplicate marker index %d and %d\n", a->duplicate_marker_a, a->duplicate_marker_b);
    }
    if ((a->flags & CSM3D_ANALYSIS_NON_MONOTONIC_FRAMES) != 0UL) {
        printf("  warning: non-monotonic frame at stored index %d\n", a->frame_order_break_index);
    }
    if ((a->flags & CSM3D_ANALYSIS_DUPLICATE_FRAMES) != 0UL) {
        printf("  warning: duplicate frame stored index %d and %d\n", a->duplicate_frame_a, a->duplicate_frame_b);
    }
    if ((a->flags & CSM3D_ANALYSIS_DECLARED_RANGE_MISMATCH) != 0UL) {
        printf("  warning: declared frame range differs from data range\n");
    }
    if ((a->flags & CSM3D_ANALYSIS_EMPTY_DECLARED_RANGE) != 0UL) {
        printf("  warning: declared frame range is empty/inverted\n");
    }
    if ((a->flags & CSM3D_ANALYSIS_UNKNOWN_MARKERS) != 0UL) {
        printf("  warning: %d marker name(s) are unknown to the built-in Character Studio table\n", a->unknown_marker_count);
    }
    if ((a->flags & CSM3D_ANALYSIS_INCOMPLETE_PROP_MARKERS) != 0UL) {
        printf("  warning: incomplete prop marker triplet; seen prop marker count=%d\n", a->incomplete_prop_marker_count);
    }
    if ((a->flags & CSM3D_ANALYSIS_LEFT_PROP_PRESENT) != 0UL) {
        printf("  prop: left marker triplet present\n");
    }
    if ((a->flags & CSM3D_ANALYSIS_MIDDLE_PROP_PRESENT) != 0UL) {
        printf("  prop: middle marker triplet present\n");
    }
    if ((a->flags & CSM3D_ANALYSIS_RIGHT_PROP_PRESENT) != 0UL) {
        printf("  prop: right marker triplet present\n");
    }
}

static void print_prop_info(const CSM3D_Doc *doc, int side)
{
    CSM3D_PropInfo info;
    int r;

    r = csm3d_compute_prop_info(doc, side, &info);
    if (r != CSM3D_OK) {
        return;
    }
    printf("prop %s: bottom=%d middle=%d top=%d approx_avg_len=", csm3d_prop_side_name(side), info.bottom_index, info.middle_index, info.top_index);
    print_fx(info.approximate_average_length);
    printf(" min=");
    print_fx(info.approximate_min_length);
    printf(" max=");
    print_fx(info.approximate_max_length);
    printf("\n");
    printf("prop %s bounds min: ", csm3d_prop_side_name(side));
    print_vec3(info.bounds.min_point);
    printf("\n");
    printf("prop %s bounds max: ", csm3d_prop_side_name(side));
    print_vec3(info.bounds.max_point);
    printf("\n");
}

int main(int argc, char **argv)
{
    FILE *fp;
    unsigned long n;
    int r;
    CSM3D_Bounds bounds;
    CSM3D_Analysis analysis;
    CSM3D_Vec3 vel;
    CSM3D_MarkerStats stats;
    int i;
    int kind;

    if (argc < 2) {
        fputs("usage: csm_probe file.csm\n", stderr);
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
        fprintf(stderr, "decode failed: %s line=%d frame=%d marker=%d text=%s\n",
                csm3d_error_string(r), g_diag.line, g_diag.frame_index, g_diag.marker_index, g_diag.text);
        return 1;
    }

    printf("CSM_chiquisumother3d probe v%d.%d.%d\n", CSM3D_VERSION_MAJOR, CSM3D_VERSION_MINOR, CSM3D_VERSION_PATCH);
    printf("markers: %d\n", g_doc.marker_count);
    printf("frames : %d\n", g_doc.frame_count);
    printf("first  : %d\n", g_doc.first_frame);
    printf("last   : %d\n", g_doc.last_frame);
    printf("rate   : %d\n", g_doc.rate);
    if (g_doc.declared_marker_count > 0) {
        printf("declared markers: %d\n", g_doc.declared_marker_count);
    }
    if (g_doc.actor[0] != '\0') {
        printf("actor  : %s\n", g_doc.actor);
    }
    if (g_doc.filename[0] != '\0') {
        printf("file   : %s\n", g_doc.filename);
    }

    printf("order  :");
    i = 0;
    while (i < g_doc.marker_count) {
        printf(" %s", g_doc.marker_name[i]);
        i++;
    }
    printf("\n");

    printf("marker classes:\n");
    i = 0;
    while (i < g_doc.marker_count) {
        kind = csm3d_marker_kind_from_name(g_doc.marker_name[i]);
        printf("  %2d %-12s %s\n", i, g_doc.marker_name[i], csm3d_marker_kind_string(kind));
        i++;
    }

    r = csm3d_compute_bounds(&g_doc, &bounds);
    if (r == CSM3D_OK) {
        printf("bounds min: ");
        print_vec3(bounds.min_point);
        printf("\n");
        printf("bounds max: ");
        print_vec3(bounds.max_point);
        printf("\n");
    }

    if (g_doc.frame_count > 1 && g_doc.marker_count > 0) {
        r = csm3d_compute_velocity(&g_doc, 0, 0, &vel);
        if (r == CSM3D_OK) {
            printf("velocity marker[0] frame[0->1] units/sec: ");
            print_vec3(vel);
            printf("\n");
        }
    }

    if (g_doc.marker_count > 0) {
        r = csm3d_compute_marker_stats(&g_doc, 0, &stats);
        if (r == CSM3D_OK) {
            printf("marker[0] path approx: ");
            print_fx(stats.approximate_path_length);
            printf(" max_speed: ");
            print_fx(stats.approximate_max_speed);
            printf(" at frame index %d\n", stats.max_speed_frame_index);
        }
    }

    r = csm3d_analyze(&g_doc, &analysis);
    if (r == CSM3D_OK) {
        print_analysis(&analysis);
    }

    print_prop_info(&g_doc, CSM3D_PROP_LEFT);
    print_prop_info(&g_doc, CSM3D_PROP_MIDDLE);
    print_prop_info(&g_doc, CSM3D_PROP_RIGHT);

    return 0;
}
