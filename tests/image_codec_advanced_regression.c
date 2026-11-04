#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

static modelrt_u8 g_encoded[2u * 1024u * 1024u];
static modelrt_u8 g_rgba[16u * 1024u * 1024u];
static char g_path[1024];

static size_t load_file(const char *dir, const char *name)
{
    FILE *f;
    long n;
    size_t got;
    sprintf(g_path, "%s/%s", dir, name);
    f = fopen(g_path, "rb");
    if (f == 0) return 0u;
    if (fseek(f, 0L, SEEK_END) != 0) { fclose(f); return 0u; }
    n = ftell(f);
    if (n <= 0L || (size_t)n > sizeof(g_encoded)) { fclose(f); return 0u; }
    if (fseek(f, 0L, SEEK_SET) != 0) { fclose(f); return 0u; }
    got = fread(g_encoded, 1u, (size_t)n, f);
    fclose(f);
    return got == (size_t)n ? got : 0u;
}

static int decode0(modelrt_runtime *rt, const char *dir, const char *name,
                   modelrt_image_info *ii)
{
    size_t n;
    n = load_file(dir, name);
    if (n == 0u) return MODELRT_ERR_IO;
    return modelrt_image_decode_memory(rt, g_encoded, n, g_rgba, sizeof(g_rgba), ii);
}

int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_image_info ii;
    modelrt_image_subresource_info si;
    size_t n;
    int rc;
    const char *dir;
    if (argc != 2) return 2;
    dir = argv[1];
    modelrt_runtime_init(&rt);

    rc = decode0(&rt, dir, "sample_adam7.png", &ii);
    if (rc != MODELRT_OK || ii.width != 17u || ii.height != 13u ||
        ii.decoder != MODELRT_IMAGE_PNG ||
        (ii.flags & MODELRT_IMAGE_FLAG_ADAM7) == 0u) return 10;
    printf("PASS Adam7 PNG %ux%u flags=0x%x\n", ii.width, ii.height, ii.flags);

    rc = decode0(&rt, dir, "sample_progressive.jpg", &ii);
    if (rc != MODELRT_OK || ii.width != 17u || ii.height != 13u ||
        ii.decoder != MODELRT_IMAGE_JPG ||
        (ii.flags & MODELRT_IMAGE_FLAG_PROGRESSIVE) == 0u) return 20;
    printf("PASS progressive JPEG %ux%u\n", ii.width, ii.height);

    n = load_file(dir, "sample_cube_mips.dds");
    if (n == 0u) return 30;
    rc = modelrt_image_decode_memory(&rt, g_encoded, n, g_rgba, sizeof(g_rgba), &ii);
    if (rc != MODELRT_OK || ii.mip_count != 4u || ii.face_count != 6u ||
        ii.subresource_count != 24u || (ii.flags & MODELRT_IMAGE_FLAG_CUBEMAP) == 0u)
        return 31;
    rc = modelrt_image_subresource_info_memory(&rt, g_encoded, n, 14u, &si);
    if (rc != MODELRT_OK || si.kind != MODELRT_IMAGE_SUBRESOURCE_DDS_SURFACE ||
        si.face_index != 3u || si.mip_level != 2u || si.width != 2u || si.height != 2u)
        return 32;
    rc = modelrt_image_decode_subresource_memory(&rt, g_encoded, n, 14u,
                                                  g_rgba, sizeof(g_rgba), &ii, &si);
    if (rc != MODELRT_OK || ii.width != 2u || ii.height != 2u ||
        g_rgba[0] != 123u || g_rgba[1] != 134u) return 33;
    printf("PASS DDS cubemap/mips faces=%u mips=%u subs=%u sub14=face%u/mip%u/%ux%u\n",
           ii.face_count, ii.mip_count, ii.subresource_count,
           si.face_index, si.mip_level, si.width, si.height);

    n = load_file(dir, "sample_multi_palette.tim");
    if (n == 0u) return 40;
    rc = modelrt_image_decode_memory(&rt, g_encoded, n, g_rgba, sizeof(g_rgba), &ii);
    if (rc != MODELRT_OK || ii.palette_count != 2u || ii.subresource_count != 2u ||
        (ii.flags & MODELRT_IMAGE_FLAG_INDEXED) == 0u) return 41;
    rc = modelrt_image_decode_subresource_memory(&rt, g_encoded, n, 1u,
                                                  g_rgba, sizeof(g_rgba), &ii, &si);
    if (rc != MODELRT_OK || si.palette_index != 1u || g_rgba[4] != 0u ||
        g_rgba[5] != 16u || g_rgba[6] != 0u) return 42;
    printf("PASS TIM palettes=%u selected=%u\n", ii.palette_count, si.palette_index);

    n = load_file(dir, "sample_tiled.tiff");
    if (n == 0u) return 50;
    rc = modelrt_image_decode_memory(&rt, g_encoded, n, g_rgba, sizeof(g_rgba), &ii);
    if (rc != MODELRT_OK || (ii.flags & MODELRT_IMAGE_FLAG_TILED) == 0u ||
        ii.width != 32u || ii.height != 32u || g_rgba[2] != 123u) return 51;
    printf("PASS TIFF tiled %ux%u\n", ii.width, ii.height);

    n = load_file(dir, "sample_planar.tiff");
    if (n == 0u) return 60;
    rc = modelrt_image_decode_memory(&rt, g_encoded, n, g_rgba, sizeof(g_rgba), &ii);
    if (rc != MODELRT_OK || (ii.flags & MODELRT_IMAGE_FLAG_PLANAR) == 0u ||
        g_rgba[0] != 11u || g_rgba[1] != 77u || g_rgba[2] != 199u) return 61;
    printf("PASS TIFF planar-separate %ux%u\n", ii.width, ii.height);

    n = load_file(dir, "sample_multipage.tiff");
    if (n == 0u) return 70;
    rc = modelrt_image_decode_memory(&rt, g_encoded, n, g_rgba, sizeof(g_rgba), &ii);
    if (rc != MODELRT_OK || ii.page_count != 2u || ii.subresource_count != 2u ||
        (ii.flags & MODELRT_IMAGE_FLAG_MULTIPAGE) == 0u) return 71;
    rc = modelrt_image_decode_subresource_memory(&rt, g_encoded, n, 1u,
                                                  g_rgba, sizeof(g_rgba), &ii, &si);
    if (rc != MODELRT_OK || si.page_index != 1u || ii.width != 8u || ii.height != 5u ||
        g_rgba[0] != 210u || g_rgba[1] != 120u || g_rgba[2] != 40u) return 72;
    printf("PASS TIFF multipage pages=%u page1=%ux%u\n", ii.page_count, ii.width, ii.height);

    puts("Advanced built-in image decoder regression OK");
    return 0;
}
