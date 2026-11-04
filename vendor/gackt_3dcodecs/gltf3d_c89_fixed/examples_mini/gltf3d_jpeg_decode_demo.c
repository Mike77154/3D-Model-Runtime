#include "gltf3d_mini_jpegdec.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned char file_buf[1024 * 1024];
static unsigned char rgba_buf[1024 * 1024 * 4];
static unsigned char work_buf[8192];

int main(int argc, char **argv)
{
    FILE *f;
    long n;
    gltf3d_mini_jpegdec_result res;
    int r;
    if (argc < 2) {
        printf("usage: %s image.jpg\n", argv[0]);
        return 1;
    }
    f = fopen(argv[1], "rb");
    if (!f) { printf("open failed\n"); return 1; }
    n = (long)fread(file_buf, 1, sizeof(file_buf), f);
    fclose(f);
    r = gltf3d_mini_jpeg_decode_rgba8(file_buf, (unsigned long)n, rgba_buf, sizeof(rgba_buf), work_buf, sizeof(work_buf), &res);
    if (r != GLTF3D_MINI_OK) {
        printf("jpeg decode failed: %s (%d)\n", gltf3d_mini_error_string(r), r);
        return 2;
    }
    printf("jpeg decode ok: %lux%lu comps=%d first RGBA=%u,%u,%u,%u\n", res.width, res.height, res.component_count, (unsigned)rgba_buf[0], (unsigned)rgba_buf[1], (unsigned)rgba_buf[2], (unsigned)rgba_buf[3]);
    return 0;
}
