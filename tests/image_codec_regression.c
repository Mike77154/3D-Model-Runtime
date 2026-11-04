#include "../include/model_runtime89.h"
#include <stdio.h>
#include <string.h>

#define ENC_CAP (1024u * 1024u)
#define PIX_CAP (1024u * 1024u)
static unsigned char g_enc[ENC_CAP];
static unsigned char g_pix[PIX_CAP];
static unsigned char g_model_ws[16u * 1024u * 1024u];

static int join(char *dst, size_t cap, const char *dir, const char *name)
{
    size_t a;
    if (dst == 0 || cap == 0u || dir == 0 || name == 0) return 0;
    a = strlen(dir);
    if (a + 1u + strlen(name) + 1u > cap) return 0;
    strcpy(dst, dir);
    if (a != 0u && dst[a - 1u] != '/' && dst[a - 1u] != '\\') strcat(dst, "/");
    strcat(dst, name);
    return 1;
}

static int check_file(modelrt_runtime *rt, const char *dir, const char *name, modelrt_u32 bit)
{
    char path[768];
    modelrt_image_info ii;
    int rc;
    if (!join(path, sizeof(path), dir, name)) return 0;
    memset(&ii, 0, sizeof(ii));
    rc = modelrt_image_decode_file(rt, path, g_enc, sizeof(g_enc), g_pix, sizeof(g_pix), &ii);
    if (rc != MODELRT_OK || ii.decoder != bit || ii.width == 0u || ii.height == 0u || ii.rgba_bytes == 0u) {
        printf("FAIL %s rc=%d decoder=%08x %ux%u bytes=%u\n",
               name, rc, (unsigned int)ii.decoder, ii.width, ii.height, ii.rgba_bytes);
        return 0;
    }
    printf("PASS %-22s %-7s %ux%u bytes=%u\n", name, ii.decoder_name,
           ii.width, ii.height, ii.rgba_bytes);
    return 1;
}

int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_u32 w;
    modelrt_u32 hh;
    size_t used;
    int tc;
    int ok;
    if (argc != 2) return 2;
    modelrt_runtime_init(&rt);
    ok = 1;
    ok &= check_file(&rt, argv[1], "sample.png", MODELRT_IMAGE_PNG);
    ok &= check_file(&rt, argv[1], "sample.bmp", MODELRT_IMAGE_BMP);
    ok &= check_file(&rt, argv[1], "sample.dds", MODELRT_IMAGE_DDS);
    ok &= check_file(&rt, argv[1], "sample.tga", MODELRT_IMAGE_TGA);
    ok &= check_file(&rt, argv[1], "sample.jpg", MODELRT_IMAGE_JPG);
    ok &= check_file(&rt, argv[1], "sample.tim", MODELRT_IMAGE_TIM);
    ok &= check_file(&rt, argv[1], "sample.tiff", MODELRT_IMAGE_TIFF);
    ok &= check_file(&rt, argv[1], "sample_packbits.tiff", MODELRT_IMAGE_TIFF);
    ok &= check_file(&rt, argv[1], "sample_lzw.tiff", MODELRT_IMAGE_TIFF);
    ok &= check_file(&rt, argv[1], "sample_deflate.tiff", MODELRT_IMAGE_TIFF);

    modelrt_runtime_set_image_decoder_mask(&rt, MODELRT_IMAGE_ALL & ~MODELRT_IMAGE_JPG);
    {
        char path[768];
        modelrt_image_info ii;
        join(path, sizeof(path), argv[1], "sample.jpg");
        if (modelrt_image_decode_file(&rt, path, g_enc, sizeof(g_enc), g_pix, sizeof(g_pix), &ii) != MODELRT_ERR_UNSUPPORTED) {
            printf("FAIL decoder mask\n"); ok = 0;
        } else printf("PASS runtime optional-mask disable JPG\n");
    }
    modelrt_runtime_set_image_decoder_mask(&rt, MODELRT_IMAGE_ALL);

    modelrt_register_builtin_providers(&rt);
    modelrt_handle_init(&h, g_model_ws, sizeof(g_model_ws));
    if (modelrt_open_file(&rt, &h, "tests/directx_x_runtime_test.x") != MODELRT_OK) {
        printf("FAIL open x for external texture: %s\n", modelrt_last_error(&rt));
        ok = 0;
    } else {
        tc = modelrt_texture_count(&h);
        w = hh = 0u; used = 0u;
        if (tc < 1 || modelrt_texture_decode_rgba(&h, 0, g_pix, sizeof(g_pix), &w, &hh, &used) != MODELRT_OK) {
            printf("FAIL runtime external texture fallback count=%d\n", tc); ok = 0;
        } else {
            printf("PASS model texture fallback -> built-in JPG89 %ux%u\n", w, hh);
        }
        modelrt_close(&h);
    }
    return ok ? 0 : 1;
}
