#include <stdio.h>
#include <string.h>
#include "../src/nff89.h"
#include "../src/nff89_cad.h"

#define DEMO_LIGHTS 16
#define DEMO_SPHERES 64
#define DEMO_CONES 64
#define DEMO_POLYS 256
#define DEMO_BEZIERS 16
#define DEMO_NURBS 16
#define DEMO_OBJECTS 32
#define DEMO_SCRATCH_VERTS 512
#define DEMO_CAD_GRID 4096
#define DEMO_TRIM_POINTS 1024
#define DEMO_TRIM_LOOPS 16

static nff89_light g_lights[DEMO_LIGHTS];
static nff89_sphere g_spheres[DEMO_SPHERES];
static nff89_cone g_cones[DEMO_CONES];
static nff89_poly g_polys[DEMO_POLYS];
static nff89_bezier g_beziers[DEMO_BEZIERS];
static nff89_nurbs g_nurbs[DEMO_NURBS];
static nff89_object g_objects[DEMO_OBJECTS];
static nff89_vertex g_scratch_vertices[DEMO_SCRATCH_VERTS];
static nff89_cad_vertex g_cad_grid[DEMO_CAD_GRID];
static nff89_cad_uv g_trim_points[DEMO_TRIM_POINTS];
static nff89_cad_trim_poly g_trim_loops[DEMO_TRIM_LOOPS];

struct tri_count_s { unsigned count; };

static int cad_count_tri(void *user, const nff89_cad_vertex *a, const nff89_cad_vertex *b, const nff89_cad_vertex *c, const nff89_nurbs *surface)
{
    struct tri_count_s *tc;
    (void)a; (void)b; (void)c; (void)surface;
    tc = (struct tri_count_s *)user;
    tc->count++;
    return 0;
}

static int cad_count_bez_tri(void *user, const nff89_cad_vertex *a, const nff89_cad_vertex *b, const nff89_cad_vertex *c, const nff89_bezier *patch)
{
    struct tri_count_s *tc;
    (void)a; (void)b; (void)c; (void)patch;
    tc = (struct tri_count_s *)user;
    tc->count++;
    return 0;
}

static void print_fix(const char *name, nff89_fix v)
{
    char buf[32];
    nff89_fix_to_string(v, buf, sizeof(buf), 4U);
    printf("%s%s", name, buf);
}

int main(int argc, char **argv)
{
    FILE *f;
    long n;
    long size;
    static char text[262144];
    nff89_scene scene;
    int r;

    if (argc < 2) {
        printf("usage: nff89_demo file.nff|file.enff\n");
        return 2;
    }

    f = fopen(argv[1], "rb");
    if (!f) {
        printf("could not open %s\n", argv[1]);
        return 2;
    }
    size = 0L;
    while (size < (long)sizeof(text)) {
        n = (long)fread(text + size, 1U, (size_t)(sizeof(text) - (unsigned long)size), f);
        if (n <= 0L) break;
        size += n;
    }
    fclose(f);

    nff89_init_scene(&scene);
    nff89_set_buffers(&scene,
                      g_lights, DEMO_LIGHTS,
                      g_spheres, DEMO_SPHERES,
                      g_cones, DEMO_CONES,
                      g_polys, DEMO_POLYS,
                      g_beziers, DEMO_BEZIERS,
                      g_nurbs, DEMO_NURBS,
                      g_objects, DEMO_OBJECTS);

    if (size >= 3L && (text[0] == 'n' || text[0] == 'N') &&
        (text[1] == 'f' || text[1] == 'F') &&
        (text[2] == 'f' || text[2] == 'F')) {
        r = nff89_parse_sense8(&scene, text, size, g_scratch_vertices, DEMO_SCRATCH_VERTS);
    } else {
        r = nff89_parse(&scene, text, size);
    }

    printf("status: %s (%d)\n", nff89_error_name(r), r);
    printf("format: %s\n", nff89_format_name(scene.format));
    printf("lights=%u spheres=%u cones=%u polys=%u triangles=%u beziers=%u nurbs=%u objects=%u\n",
           scene.lights_count, scene.spheres_count, scene.cones_count,
           scene.polys_count, nff89_tri_count(&scene), scene.beziers_count,
           scene.nurbs_count, scene.objects_count);
    if (scene.has_background) {
        print_fix("background r=", scene.background.r);
        print_fix(" g=", scene.background.g);
        print_fix(" b=", scene.background.b);
        printf("\n");
    }
    if (r != NFF89_OK && r != NFF89_ERR_CAPACITY) {
        printf("line %ld token '%s'\n", scene.diag.line, scene.diag.last_token);
    }
    if (scene.nurbs_count > 0U) {
        nff89_vec3 p0;
        struct tri_count_s tc;
        int cr;
        cr = nff89_cad_eval_nurbs_surface(&scene.nurbs[0], NFF89_FIX_ONE / 2L, NFF89_FIX_ONE / 2L, &p0);
        printf("cad nurbs eval: %s (%d)", nff89_cad_error_name(cr), cr);
        if (cr == NFF89_CAD_OK) {
            print_fix(" x=", p0.x); print_fix(" y=", p0.y); print_fix(" z=", p0.z);
        }
        printf("\n");
        tc.count = 0U;
        cr = nff89_cad_tessellate_nurbs(&scene.nurbs[0], 4U, 4U, g_cad_grid, DEMO_CAD_GRID, cad_count_tri, &tc);
        printf("cad nurbs tess: %s (%d) tris=%u\n", nff89_cad_error_name(cr), cr, tc.count);
        if (scene.nurbs[0].trim_count > 0U) {
            tc.count = 0U;
            cr = nff89_cad_tessellate_nurbs_trimmed(&scene.nurbs[0], 4U, 4U, 4U, 24U,
                                                    g_cad_grid, DEMO_CAD_GRID,
                                                    g_trim_points, DEMO_TRIM_POINTS,
                                                    g_trim_loops, DEMO_TRIM_LOOPS,
                                                    cad_count_tri, &tc);
            printf("cad nurbs trimmed tess: %s (%d) tris=%u trims=%u\n",
                   nff89_cad_error_name(cr), cr, tc.count, scene.nurbs[0].trim_count);
        }
    }
    if (scene.beziers_count > 0U) {
        struct tri_count_s tc;
        int cr;
        tc.count = 0U;
        cr = nff89_cad_tessellate_bezier(&scene.beziers[0], 4U, 4U, g_cad_grid, DEMO_CAD_GRID, cad_count_bez_tri, &tc);
        printf("cad bezier tess: %s (%d) tris=%u\n", nff89_cad_error_name(cr), cr, tc.count);
    }
    if (scene.diag.overflow_count || scene.diag.skipped_unknown || scene.diag.truncated_polygons || scene.diag.truncated_curves) {
        printf("diag: overflow=%u unknown=%u trunc_poly=%u trunc_curve=%u\n",
               scene.diag.overflow_count, scene.diag.skipped_unknown,
               scene.diag.truncated_polygons, scene.diag.truncated_curves);
    }
    return r == NFF89_OK || r == NFF89_ERR_CAPACITY ? 0 : 1;
}
