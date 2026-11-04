#include "gif_ms3d_codec.h"

#include <stdio.h>

static unsigned char tiny_ms3d[] = {
    'M','S','3','D','0','0','0','0','0','0',
    4,0,0,0,
    3,0,
    0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,0,
    0, 0,0,128,63, 0,0,0,0, 0,0,0,0, 255,0,
    0, 0,0,0,0, 0,0,128,63, 0,0,0,0, 255,0,
    1,0,
    0,0, 0,0, 1,0, 2,0,
    0,0,0,0, 0,0,0,0, 0,0,128,63,
    0,0,0,0, 0,0,0,0, 0,0,128,63,
    0,0,0,0, 0,0,0,0, 0,0,128,63,
    0,0,0,0, 0,0,128,63, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,128,63,
    1,0,
    1,0,
    0,
    't','r','i','a','n','g','l','e',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,0, 0,0, 255,
    0,0,
    0,0,192,65, 0,0,0,0, 1,0,0,0,
    0,0
};

static ms3d_model model;

int main(void)
{
    int rc;
    rc = ms3d_parse(&model, tiny_ms3d, sizeof(tiny_ms3d));
    if (rc != MS3D_OK) {
        printf("FAIL parse %s %d\n", ms3d_error_string(rc), rc);
        return 1;
    }
    if (model.num_vertices != 3U || model.num_triangles != 1U || model.num_groups != 1U) {
        printf("FAIL counts\n");
        return 1;
    }
    if (model.vertices[1].position[0] != MS3D_FX_ONE) {
        printf("FAIL fixed conversion\n");
        return 1;
    }
    printf("PASS ms3d tiny parse\n");
    return 0;
}
