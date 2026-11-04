#include "giffy_recodec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void putpix(unsigned char *img, int w, int h, int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
    unsigned char *p;
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    p = img + ((size_t)y * (size_t)w + (size_t)x) * 3u;
    p[0] = r; p[1] = g; p[2] = b;
}

static void line2d(unsigned char *img, int w, int h, int x0, int y0, int x1, int y1, unsigned char r, unsigned char g, unsigned char b)
{
    int dx;
    int sx;
    int dy;
    int sy;
    int err;
    int e2;
    dx = abs(x1 - x0);
    sx = (x0 < x1) ? 1 : -1;
    dy = -abs(y1 - y0);
    sy = (y0 < y1) ? 1 : -1;
    err = dx + dy;
    while (1) {
        putpix(img, w, h, x0, y0, r, g, b);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static float get_axis_value(const grc_vec3 *v, int axis)
{
    if (axis == 0) return v->x;
    if (axis == 1) return v->y;
    return v->z;
}

static int project_vertex(const grc_model *m, int idx, grc_vec3 mn, grc_vec3 mx, int w, int h, int ax, int ay, int *x, int *y)
{
    float sx;
    float sy;
    float scale;
    float cx;
    float cy;
    float px;
    float py;
    if (idx < 0 || (grc_u32)idx >= m->vertex_count) return 0;
    sx = get_axis_value(&mx, ax) - get_axis_value(&mn, ax);
    sy = get_axis_value(&mx, ay) - get_axis_value(&mn, ay);
    scale = sx;
    if (sy > scale) scale = sy;
    if (scale <= 0.00001f) scale = 1.0f;
    cx = (get_axis_value(&mn, ax) + get_axis_value(&mx, ax)) * 0.5f;
    cy = (get_axis_value(&mn, ay) + get_axis_value(&mx, ay)) * 0.5f;
    px = (get_axis_value(&m->vertices[idx], ax) - cx) / scale;
    py = (get_axis_value(&m->vertices[idx], ay) - cy) / scale;
    *x = (int)((float)w * (0.5f + px * 0.85f));
    *y = (int)((float)h * (0.5f - py * 0.85f));
    return 1;
}

static int save_ppm(const char *path, const unsigned char *img, int w, int h)
{
    FILE *f;
    f = fopen(path, "wb");
    if (f == NULL) return GRC_ERR_IO;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(img, 1u, (size_t)w * (size_t)h * 3u, f);
    fclose(f);
    return GRC_OK;
}

static void draw_grid(unsigned char *img, int w, int h)
{
    int i;
    for (i = 0; i < w; i += 32) line2d(img, w, h, i, 0, i, h - 1, 20u, 20u, 20u);
    for (i = 0; i < h; i += 32) line2d(img, w, h, 0, i, w - 1, i, 20u, 20u, 20u);
    line2d(img, w, h, w / 2, 0, w / 2, h - 1, 45u, 45u, 45u);
    line2d(img, w, h, 0, h / 2, w - 1, h / 2, 45u, 45u, 45u);
}

static int render_wire(const grc_model *m, const char *out, int w, int h, const char *axis_name)
{
    unsigned char *img;
    size_t bytes;
    grc_u32 i;
    grc_vec3 mn;
    grc_vec3 mx;
    int ax;
    int ay;
    ax = 0;
    ay = 1;
    if (axis_name != NULL) {
        if (strcmp(axis_name, "xz") == 0) { ax = 0; ay = 2; }
        else if (strcmp(axis_name, "zy") == 0 || strcmp(axis_name, "yz") == 0) { ax = 2; ay = 1; }
        else if (strcmp(axis_name, "xy") == 0) { ax = 0; ay = 1; }
    }
    bytes = (size_t)w * (size_t)h * 3u;
    img = (unsigned char *)malloc(bytes);
    if (img == NULL) return GRC_ERR_MEMORY;
    memset(img, 0, bytes);
    draw_grid(img, w, h);
    grc_model_compute_bounds(m, &mn, &mx);
    for (i = 0u; i < m->face_count; ++i) {
        const grc_face *fa;
        grc_u32 e;
        unsigned char r;
        unsigned char g;
        unsigned char b;
        fa = &m->faces[i];
        r = fa->r;
        g = fa->g;
        b = fa->b;
        if (r < 32u && g < 32u && b < 32u) { r = 220u; g = 220u; b = 220u; }
        for (e = 0u; e < fa->vertex_count; ++e) {
            grc_u32 n;
            int x0;
            int y0;
            int x1;
            int y1;
            n = (e + 1u) % fa->vertex_count;
            if (project_vertex(m, fa->v[e], mn, mx, w, h, ax, ay, &x0, &y0) && project_vertex(m, fa->v[n], mn, mx, w, h, ax, ay, &x1, &y1)) {
                line2d(img, w, h, x0, y0, x1, y1, r, g, b);
                putpix(img, w, h, x0, y0, 255u, 255u, 255u);
            }
        }
    }
    i = (grc_u32)save_ppm(out, img, w, h);
    free(img);
    return (int)i;
}

int main(int argc, char **argv)
{
    grc_model m;
    grc_options opt;
    int w;
    int h;
    int rc;
    const char *axis_name;
    if (argc < 3) {
        printf("usage: giffy_re_view <model> <out.ppm> [w h] [xy|xz|yz]\n");
        return 1;
    }
    w = 512;
    h = 512;
    axis_name = "xy";
    if (argc >= 5) {
        w = atoi(argv[3]);
        h = atoi(argv[4]);
        if (w < 16) w = 512;
        if (h < 16) h = 512;
    }
    if (argc >= 6) axis_name = argv[5];
    grc_options_default(&opt);
    rc = grc_load_model(argv[1], &m, &opt);
    if (rc != GRC_OK) {
        fprintf(stderr, "load failed: %s\n", grc_result_string(rc));
        return 1;
    }
    rc = render_wire(&m, argv[2], w, h, axis_name);
    grc_model_free(&m);
    if (rc != GRC_OK) {
        fprintf(stderr, "render failed: %s\n", grc_result_string(rc));
        return 1;
    }
    printf("rendered: %s\n", argv[2]);
    return 0;
}
