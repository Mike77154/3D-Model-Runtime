#include <stdio.h>
#include <stdlib.h>
#include "../mini_codecs/gltf3d_mini_images.h"
#include "../mini_codecs/gltf3d_mini_ktx2_basisu.h"

int main(int argc, char **argv)
{
    FILE *f;
    unsigned char *buf;
    long sz;
    gltf3d_mini_image_info ii;
    gltf3d_mini_ktx2_info ki;
    int r;
    if (argc < 2) {
        printf("usage: gltf3d_mini_probe image.png|image.jpg|texture.ktx2\n");
        return 0;
    }
    f = fopen(argv[1], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return 3; }
    buf = (unsigned char*)malloc((size_t)sz);
    if (!buf) { fclose(f); return 4; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return 5; }
    fclose(f);
    r = gltf3d_mini_image_probe(buf, (unsigned long)sz, &ii);
    if (r == GLTF3D_MINI_OK) {
        printf("image: %lux%lu channels=%d bits=%d png=%d jpeg=%d\n", ii.width, ii.height, ii.channels, ii.bits_per_channel, ii.is_png, ii.is_jpeg);
        free(buf);
        return 0;
    }
    r = gltf3d_mini_ktx2_probe(buf, (unsigned long)sz, &ki);
    if (r == GLTF3D_MINI_OK) {
        printf("ktx2: %lux%lu levels=%lu supercompression=%lu vkFormat=%lu\n", ki.pixel_width, ki.pixel_height, ki.level_count, ki.supercompression_scheme, ki.vk_format);
        free(buf);
        return 0;
    }
    printf("not recognized: %s\n", gltf3d_mini_error_string(r));
    free(buf);
    return 1;
}
