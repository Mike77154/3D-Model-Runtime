#include "gltf3d.h"
#include "gltf3d_validate.h"
#include "gltf3d_mini_texture_decode.h"
#include <stdio.h>
#include <string.h>

#define DEMO_MAX_GLTF (64UL * 1024UL)
#define DEMO_MAX_BIN  (64UL * 1024UL)
#define DEMO_MAX_IMG  (256UL * 1024UL)
#define DEMO_MAX_RGBA (1024UL * 1024UL)
#define DEMO_MAX_WORK (512UL * 1024UL)

static unsigned char g_gltf[DEMO_MAX_GLTF];
static unsigned char g_bin[DEMO_MAX_BIN];
static unsigned char g_img[DEMO_MAX_IMG];
static unsigned char g_rgba[DEMO_MAX_RGBA];
static unsigned char g_work[DEMO_MAX_WORK];
static unsigned long g_bin_size;
static unsigned long g_img_size;

static int read_file(const char *path, unsigned char *dst, unsigned long cap, unsigned long *size)
{
    FILE *f;
    unsigned long n;
    f = fopen(path, "rb");
    if (!f) return -1;
    n = (unsigned long)fread(dst, 1, cap, f);
    if (!feof(f)) { fclose(f); return -2; }
    fclose(f);
    *size = n;
    return 0;
}

static int io_load_uri(void *user, const char *uri, const unsigned char **data, unsigned long *size)
{
    (void)user;
    if (strcmp(uri, "tiny_triangle.bin") == 0) {
        if (g_bin_size == 0UL) if (read_file("tests/tiny_triangle.bin", g_bin, DEMO_MAX_BIN, &g_bin_size) != 0) return -1;
        *data = g_bin; *size = g_bin_size; return 0;
    }
    if (strcmp(uri, "tiny_rgba.png") == 0) {
        if (g_img_size == 0UL) if (read_file("tests/tiny_rgba.png", g_img, DEMO_MAX_IMG, &g_img_size) != 0) return -1;
        *data = g_img; *size = g_img_size; return 0;
    }
    return -1;
}

int main(void)
{
    gltf3d_asset asset;
    gltf3d_io io;
    gltf3d_mini_texture_callbacks cb;
    gltf3d_mini_texture_result tr;
    unsigned long gltf_size;
    int r;
    if (read_file("tests/tiny_textured.gltf", g_gltf, DEMO_MAX_GLTF, &gltf_size) != 0) {
        puts("cannot read tests/tiny_textured.gltf");
        return 1;
    }
    io.load_uri = io_load_uri;
    io.user = 0;
    r = gltf3d_parse_memory(&asset, g_gltf, gltf_size, &io);
    if (r < 0) { printf("parse failed: %s\n", gltf3d_error_string(r)); return 2; }
    r = gltf3d_validate_basic(&asset);
    if (r < 0) { printf("validate failed: %s\n", gltf3d_error_string(r)); return 3; }
    cb.load_uri = io_load_uri;
    cb.basisu_transcode_rgba8 = 0;
    cb.user = 0;
    r = gltf3d_mini_decode_texture_rgba8(&asset, 0, g_rgba, DEMO_MAX_RGBA, g_work, DEMO_MAX_WORK, &tr, &cb);
    if (r != GLTF3D_MINI_OK) { printf("texture decode failed: %s\n", gltf3d_mini_error_string(r)); return 4; }
    printf("texture ok: %lux%lu fmt=%d src=%d first RGBA=%u,%u,%u,%u\n", tr.width, tr.height, tr.format, tr.source_kind, (unsigned)g_rgba[0], (unsigned)g_rgba[1], (unsigned)g_rgba[2], (unsigned)g_rgba[3]);
    return 0;
}
