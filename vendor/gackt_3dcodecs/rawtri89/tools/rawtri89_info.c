#include <stdio.h>
#include <string.h>
#include "rawtri89.h"

#ifndef RW3D_TOOL_MAX_FILE
#define RW3D_TOOL_MAX_FILE (4 * 1024 * 1024)
#endif

#ifndef RW3D_TOOL_ARENA_SIZE
#define RW3D_TOOL_ARENA_SIZE (16 * 1024 * 1024)
#endif

#ifndef RW3D_TOOL_OUT_SIZE
#define RW3D_TOOL_OUT_SIZE (16 * 1024 * 1024)
#endif

static unsigned char g_file[RW3D_TOOL_MAX_FILE];
static char g_out[RW3D_TOOL_OUT_SIZE];
RW3D_ARENA_STORAGE(g_arena_mem, RW3D_TOOL_ARENA_SIZE);

static int read_file(const char *path, unsigned char *buf, size_t cap, size_t *len_out)
{
    FILE *f;
    size_t n;

    f = fopen(path, "rb");
    if (f == 0) {
        return 0;
    }
    n = fread(buf, 1, cap, f);
    if (!feof(f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    *len_out = n;
    return 1;
}

static int write_file(const char *path, const char *buf, size_t len)
{
    FILE *f;
    size_t n;

    f = fopen(path, "wb");
    if (f == 0) {
        return 0;
    }
    n = fwrite(buf, 1, len, f);
    fclose(f);
    return n == len;
}

static void print_fix(const char *name, rw3d_fix v)
{
    char s[40];
    rw3d_fix_to_decimal(v, s, sizeof(s), 6);
    printf("%s%s", name, s);
}

static void usage(void)
{
    printf("usage: rawtri89_info [options] file.raw\n");
    printf("options:\n");
    printf("  -repair          drop zero-area/degenerate triangles in-place before export\n");
    printf("  -unique          drop duplicate triangles in-place before export\n");
    printf("  -center          translate mesh center to origin before export\n");
    printf("  -flip            flip winding before export\n");
    printf("  -obj file.obj    export OBJ into static output buffer\n");
    printf("  -ply file.ply    export ASCII PLY into static output buffer\n");
    printf("  -raw file.raw    export triangulated RAW into static output buffer\n");
}

int main(int argc, char **argv)
{
    size_t len;
    rw3d_arena arena;
    rw3d_mesh mesh;
    rw3d_report report;
    rw3d_report parse_report;
    rw3d_mesh_stats stats;
    const char *in_path;
    const char *obj_path;
    const char *ply_path;
    const char *raw_path;
    int do_repair;
    int do_unique;
    int do_center;
    int do_flip;
    int st;
    int i;
    size_t estimate;

    in_path = 0;
    obj_path = 0;
    ply_path = 0;
    raw_path = 0;
    do_repair = 0;
    do_unique = 0;
    do_center = 0;
    do_flip = 0;

    if (argc < 2) {
        usage();
        return 2;
    }

    i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-repair") == 0) {
            do_repair = 1;
            i++;
        } else if (strcmp(argv[i], "-unique") == 0) {
            do_unique = 1;
            i++;
        } else if (strcmp(argv[i], "-center") == 0) {
            do_center = 1;
            i++;
        } else if (strcmp(argv[i], "-flip") == 0) {
            do_flip = 1;
            i++;
        } else if (strcmp(argv[i], "-obj") == 0 && i + 1 < argc) {
            obj_path = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "-ply") == 0 && i + 1 < argc) {
            ply_path = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "-raw") == 0 && i + 1 < argc) {
            raw_path = argv[i + 1];
            i += 2;
        } else if (argv[i][0] == '-') {
            usage();
            return 2;
        } else {
            in_path = argv[i];
            i++;
        }
    }

    if (in_path == 0) {
        usage();
        return 2;
    }

    if (!read_file(in_path, g_file, sizeof(g_file), &len)) {
        printf("could not read file or file exceeds static buffer (%lu bytes)\n", (unsigned long)sizeof(g_file));
        return 1;
    }

    rw3d_arena_init(&arena, g_arena_mem.data, sizeof(g_arena_mem.data));
    estimate = 0UL;
    st = rw3d_estimate_raw_arena((const char *)g_file, len, 0UL, &estimate, &report);
    if (st != RW3D_OK) {
        printf("estimate error: %s line=%lu message=%s\n", rw3d_status_name(st), report.first_error_line, report.message);
        return 1;
    }
    st = rw3d_parse_raw_memory((const char *)g_file, len, &arena, 0UL, &mesh, &parse_report);
    if (st != RW3D_OK) {
        printf("parse error: %s line=%lu message=%s\n", rw3d_status_name(st), parse_report.first_error_line, parse_report.message);
        printf("lines=%lu valid=%lu invalid=%lu\n", parse_report.line_count, parse_report.valid_lines, parse_report.invalid_lines);
        return 1;
    }

    if (do_repair) {
        st = rw3d_remove_degenerate_tris_in_place(&mesh, &report);
        if (st != RW3D_OK) {
            printf("repair error: %s message=%s\n", rw3d_status_name(st), report.message);
            return 1;
        }
        printf("repair: dropped %lu degenerate triangles\n", report.dropped_degenerate);
    }
    if (do_unique) {
        st = rw3d_remove_duplicate_tris_in_place(&mesh, &report);
        if (st != RW3D_OK) {
            printf("unique error: %s message=%s\n", rw3d_status_name(st), report.message);
            return 1;
        }
        printf("unique: dropped %lu duplicate triangles\n", report.dropped_duplicate_triangles);
    }
    if (do_center) {
        st = rw3d_center_to_origin(&mesh);
        if (st != RW3D_OK) {
            printf("center error: %s\n", rw3d_status_name(st));
            return 1;
        }
    }
    if (do_flip) {
        st = rw3d_flip_winding(&mesh);
        if (st != RW3D_OK) {
            printf("flip error: %s\n", rw3d_status_name(st));
            return 1;
        }
    }

    st = rw3d_analyze_mesh(&mesh, &stats);
    if (st != RW3D_OK) {
        printf("analyze error: %s\n", rw3d_status_name(st));
        return 1;
    }

    printf("RAWTRI89 OK\n");
    printf("lines:       %lu\n", parse_report.line_count);
    printf("tri lines:   %lu\n", parse_report.tri_lines);
    printf("quad lines:  %lu\n", parse_report.quad_lines);
    printf("vertices:    %lu\n", (unsigned long)mesh.vertex_count);
    printf("triangles:   %lu\n", (unsigned long)mesh.triangle_count);
    printf("dupes:       %lu\n", parse_report.duplicate_vertices);
    printf("arena:       %lu bytes used, %lu estimated worst-case\n", (unsigned long)arena.used, (unsigned long)estimate);
    printf("edges:       %lu unique, %lu boundary, %lu nonmanifold\n", stats.edges_unique, stats.boundary_edges, stats.nonmanifold_edges);
    printf("tri issues:  %lu duplicate, %lu degenerate, %lu zero-area\n", stats.duplicate_triangles, stats.degenerate_triangles, stats.zero_area_triangles);
    printf("isolated:    %lu vertices\n", stats.isolated_vertices);
    if (mesh.has_bounds) {
        print_fix("bounds min:  ", mesh.bounds_min.x);
        print_fix(", ", mesh.bounds_min.y);
        print_fix(", ", mesh.bounds_min.z);
        printf("\n");
        print_fix("bounds max:  ", mesh.bounds_max.x);
        print_fix(", ", mesh.bounds_max.y);
        print_fix(", ", mesh.bounds_max.z);
        printf("\n");
    }

    if (obj_path != 0) {
        st = rw3d_write_obj_memory(&mesh, g_out, sizeof(g_out), RW3D_WRITE_F_SIX_DECIMALS, &report);
        if (st != RW3D_OK || !write_file(obj_path, g_out, report.bytes_written)) {
            printf("OBJ export failed: %s needed=%lu\n", rw3d_status_name(st), (unsigned long)report.bytes_needed);
            return 1;
        }
        printf("wrote OBJ:   %s (%lu bytes)\n", obj_path, (unsigned long)report.bytes_written);
    }
    if (ply_path != 0) {
        st = rw3d_write_ply_ascii_memory(&mesh, g_out, sizeof(g_out), RW3D_WRITE_F_SIX_DECIMALS, &report);
        if (st != RW3D_OK || !write_file(ply_path, g_out, report.bytes_written)) {
            printf("PLY export failed: %s needed=%lu\n", rw3d_status_name(st), (unsigned long)report.bytes_needed);
            return 1;
        }
        printf("wrote PLY:   %s (%lu bytes)\n", ply_path, (unsigned long)report.bytes_written);
    }
    if (raw_path != 0) {
        st = rw3d_write_raw_memory(&mesh, g_out, sizeof(g_out), RW3D_WRITE_F_SIX_DECIMALS, &report);
        if (st != RW3D_OK || !write_file(raw_path, g_out, report.bytes_written)) {
            printf("RAW export failed: %s needed=%lu\n", rw3d_status_name(st), (unsigned long)report.bytes_needed);
            return 1;
        }
        printf("wrote RAW:   %s (%lu bytes)\n", raw_path, (unsigned long)report.bytes_written);
    }

    return 0;
}
