#include <stdio.h>
#include <stdlib.h>
#include "../mini_codecs/gltf3d_mini_pngdec.h"

int main(int argc, char **argv)
{
    FILE *f;
    unsigned char *filebuf;
    unsigned char *work;
    unsigned char *rgba;
    long sz;
    unsigned long wb;
    unsigned long db;
    unsigned long w;
    unsigned long h;
    int r;
    if (argc < 2) { printf("usage: gltf3d_png_decode_demo image.png\n"); return 0; }
    f = fopen(argv[1], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    filebuf = (unsigned char*)malloc((size_t)sz);
    if (!filebuf) { fclose(f); return 3; }
    if (fread(filebuf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(filebuf); return 4; }
    fclose(f);
    r = gltf3d_mini_png_required_rgba8_work(filebuf, (unsigned long)sz, &wb, &db, &w, &h);
    if (r != GLTF3D_MINI_OK) { printf("png required failed: %s\n", gltf3d_mini_error_string(r)); free(filebuf); return 5; }
    work = (unsigned char*)malloc((size_t)wb);
    rgba = (unsigned char*)malloc((size_t)db);
    if (!work || !rgba) { free(filebuf); if(work)free(work); if(rgba)free(rgba); return 6; }
    r = gltf3d_mini_png_decode_rgba8_work(filebuf, (unsigned long)sz, rgba, db, work, wb, &w, &h);
    if (r != GLTF3D_MINI_OK) { printf("png decode failed: %s\n", gltf3d_mini_error_string(r)); }
    else { printf("png decode ok: %lux%lu first RGBA=%u,%u,%u,%u\n", w, h, (unsigned)rgba[0], (unsigned)rgba[1], (unsigned)rgba[2], (unsigned)rgba[3]); }
    free(filebuf); free(work); free(rgba);
    return r == GLTF3D_MINI_OK ? 0 : 7;
}
