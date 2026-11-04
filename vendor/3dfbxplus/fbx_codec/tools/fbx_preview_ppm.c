#include <stdio.h>
#include <string.h>

#include "3dfbx.h"

#ifndef FBX_PREVIEW_W
#define FBX_PREVIEW_W 256
#endif
#ifndef FBX_PREVIEW_H
#define FBX_PREVIEW_H 256
#endif

static unsigned char g_img[FBX_PREVIEW_W * FBX_PREVIEW_H * 3];

static void img_clear(void)
{
    int i;
    for (i = 0; i < FBX_PREVIEW_W * FBX_PREVIEW_H * 3; ++i) g_img[i] = 255u;
}

static void img_plot_rgb(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
    int idx;
    if (x < 0 || y < 0 || x >= FBX_PREVIEW_W || y >= FBX_PREVIEW_H) return;
    idx = (y * FBX_PREVIEW_W + x) * 3;
    g_img[idx + 0] = r;
    g_img[idx + 1] = g;
    g_img[idx + 2] = b;
}

static void img_plot(int x, int y)
{
    img_plot_rgb(x, y, 0u, 0u, 0u);
}

static int iabs2(int v)
{
    return v < 0 ? -v : v;
}

static void img_line(int x0, int y0, int x1, int y1)
{
    int dx;
    int sx;
    int dy;
    int sy;
    int err;
    int e2;

    dx = iabs2(x1 - x0);
    sx = x0 < x1 ? 1 : -1;
    dy = -iabs2(y1 - y0);
    sy = y0 < y1 ? 1 : -1;
    err = dx + dy;

    while (1) {
        img_plot(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static int project_x(fbx_real x)
{
    fbx_real v;
    v = fbx_real_mul(x, fbx_real_from_int(60));
    return FBX_PREVIEW_W / 2 + fbx_real_to_int_round(v);
}

static int project_y(fbx_real z)
{
    fbx_real v;
    v = fbx_real_mul(z, fbx_real_from_int(60));
    return FBX_PREVIEW_H / 2 - fbx_real_to_int_round(v);
}

static int clamp_i(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int edge2(int ax, int ay, int bx, int by, int px, int py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static unsigned char real_to_u8(fbx_real v)
{
    int i;
    if (v < FBX_REAL_ZERO) v = FBX_REAL_ZERO;
    if (v > FBX_REAL_ONE) v = FBX_REAL_ONE;
    i = fbx_real_to_int_round(fbx_real_mul(v, fbx_real_from_int(255)));
    if (i < 0) i = 0;
    if (i > 255) i = 255;
    return (unsigned char)i;
}

static const FbxMaterial *find_material_by_id(const FbxScene *scene, fbx_i64 id)
{
    int i;
    if (!scene) return NULL;
    for (i = 0; i < scene->material_count; ++i) {
        if (scene->materials[i] && scene->materials[i]->fbx_id == id) return scene->materials[i];
    }
    return NULL;
}

static void material_rgb(const FbxScene *scene, const FbxMesh *mesh, fbx_u32 tri_index,
                         unsigned char *out_r, unsigned char *out_g, unsigned char *out_b)
{
    const FbxMaterial *mat;
    fbx_i64 mat_id;
    int mat_slot;
    fbx_u32 s;
    mat = NULL;
    mat_id = 0;
    mat_slot = 0;
    if (mesh && mesh->submeshes && mesh->submesh_count > 0u) {
        fbx_u32 first_index;
        first_index = tri_index * 3u;
        for (s = 0u; s < mesh->submesh_count; ++s) {
            if (first_index >= mesh->submeshes[s].first_index &&
                first_index < mesh->submeshes[s].first_index + mesh->submeshes[s].index_count) {
                mat_slot = mesh->submeshes[s].material_index;
                break;
            }
        }
    }
    if (mesh && mesh->material_ids && mat_slot >= 0 && mat_slot < mesh->material_slot_count) {
        mat_id = mesh->material_ids[mat_slot];
        mat = find_material_by_id(scene, mat_id);
    }
    if (mat) {
        *out_r = real_to_u8(mat->diffuse_color[0]);
        *out_g = real_to_u8(mat->diffuse_color[1]);
        *out_b = real_to_u8(mat->diffuse_color[2]);
    } else {
        *out_r = 190u;
        *out_g = 190u;
        *out_b = 190u;
    }
}

static void img_fill_tri(int ax, int ay, int bx, int by, int cx, int cy,
                         unsigned char r, unsigned char g, unsigned char b)
{
    int minx;
    int maxx;
    int miny;
    int maxy;
    int area;
    int x;
    int y;
    minx = clamp_i(ax < bx ? (ax < cx ? ax : cx) : (bx < cx ? bx : cx), 0, FBX_PREVIEW_W - 1);
    maxx = clamp_i(ax > bx ? (ax > cx ? ax : cx) : (bx > cx ? bx : cx), 0, FBX_PREVIEW_W - 1);
    miny = clamp_i(ay < by ? (ay < cy ? ay : cy) : (by < cy ? by : cy), 0, FBX_PREVIEW_H - 1);
    maxy = clamp_i(ay > by ? (ay > cy ? ay : cy) : (by > cy ? by : cy), 0, FBX_PREVIEW_H - 1);
    area = edge2(ax, ay, bx, by, cx, cy);
    if (area == 0) return;
    for (y = miny; y <= maxy; ++y) {
        for (x = minx; x <= maxx; ++x) {
            int w0;
            int w1;
            int w2;
            w0 = edge2(bx, by, cx, cy, x, y);
            w1 = edge2(cx, cy, ax, ay, x, y);
            w2 = edge2(ax, ay, bx, by, x, y);
            if ((area > 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                img_plot_rgb(x, y, r, g, b);
            }
        }
    }
}

static void render_mesh_solid(const FbxScene *scene, const FbxMesh *mesh)
{
    fbx_u32 i;
    fbx_u32 tri;
    if (!mesh || !mesh->positions || !mesh->indices) return;
    tri = 0u;
    for (i = 0u; i + 2u < mesh->index_count; i += 3u) {
        fbx_u32 ia;
        fbx_u32 ib;
        fbx_u32 ic;
        int ax, ay, bx, by, cx, cy;
        unsigned char r, g, b;
        ia = mesh->indices[i + 0u];
        ib = mesh->indices[i + 1u];
        ic = mesh->indices[i + 2u];
        if (ia >= mesh->vertex_count || ib >= mesh->vertex_count || ic >= mesh->vertex_count) continue;
        ax = project_x(mesh->positions[ia * 3u + 0u]);
        ay = project_y(mesh->positions[ia * 3u + 2u]);
        bx = project_x(mesh->positions[ib * 3u + 0u]);
        by = project_y(mesh->positions[ib * 3u + 2u]);
        cx = project_x(mesh->positions[ic * 3u + 0u]);
        cy = project_y(mesh->positions[ic * 3u + 2u]);
        material_rgb(scene, mesh, tri, &r, &g, &b);
        img_fill_tri(ax, ay, bx, by, cx, cy, r, g, b);
        tri += 1u;
    }
}

static void render_mesh_wire(const FbxMesh *mesh)
{
    fbx_u32 i;
    if (!mesh || !mesh->positions || !mesh->indices) return;
    for (i = 0u; i + 2u < mesh->index_count; i += 3u) {
        fbx_u32 ia;
        fbx_u32 ib;
        fbx_u32 ic;
        int ax, ay, bx, by, cx, cy;
        ia = mesh->indices[i + 0u];
        ib = mesh->indices[i + 1u];
        ic = mesh->indices[i + 2u];
        if (ia >= mesh->vertex_count || ib >= mesh->vertex_count || ic >= mesh->vertex_count) continue;
        ax = project_x(mesh->positions[ia * 3u + 0u]);
        ay = project_y(mesh->positions[ia * 3u + 2u]);
        bx = project_x(mesh->positions[ib * 3u + 0u]);
        by = project_y(mesh->positions[ib * 3u + 2u]);
        cx = project_x(mesh->positions[ic * 3u + 0u]);
        cy = project_y(mesh->positions[ic * 3u + 2u]);
        img_line(ax, ay, bx, by);
        img_line(bx, by, cx, cy);
        img_line(cx, cy, ax, ay);
    }
}

static void render_scene(const FbxScene *scene)
{
    const FbxModel *m;
    if (!scene) return;
    m = scene->models;
    while (m) {
        if (m->mesh) {
            render_mesh_solid(scene, m->mesh);
            render_mesh_wire(m->mesh);
        }
        m = m->next;
    }
}

int main(int argc, char **argv)
{
    const char *in_path;
    const char *out_path;
    FbxContext *ctx;
    FbxNode *root;
    FbxScene *scene;
    FILE *fp;

    if (argc < 3) {
        printf("Usage: %s <input.fbx> <out.ppm>\n", argv[0]);
        return 1;
    }

    in_path = argv[1];
    out_path = argv[2];
    ctx = fbx_open(in_path);
    if (!ctx) {
        printf("fbx_open failed: %s\n", in_path);
        return 2;
    }
    root = fbx_read_raw_tree(ctx);
    if (!root) {
        printf("read failed: %s\n", fbx_last_error(ctx));
        fbx_close(ctx, NULL);
        return 3;
    }
    scene = fbx_scene_build(root);
    if (!scene) {
        printf("scene build failed\n");
        fbx_close(ctx, root);
        return 4;
    }

    img_clear();
    render_scene(scene);

    fp = fopen(out_path, "wb");
    if (!fp) {
        printf("open output failed: %s\n", out_path);
        fbx_scene_destroy(scene);
        fbx_close(ctx, root);
        return 5;
    }
    fprintf(fp, "P6\n%d %d\n255\n", FBX_PREVIEW_W, FBX_PREVIEW_H);
    fwrite(g_img, 1u, sizeof(g_img), fp);
    fclose(fp);

    fbx_scene_destroy(scene);
    fbx_close(ctx, root);
    printf("preview ppm OK: %s\n", out_path);
    return 0;
}
