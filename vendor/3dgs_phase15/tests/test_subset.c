#include "../include/gs3d.h"
#include <stdio.h>
int main(void){
    int r;
#if GS3D_WITH_LEGACY
    r=gs3d_open("tests/fixtures/a4_mdl3.mdl");
    if(r!=GS3D_OK)return 1;
    gs3d_close();
#else
    r=gs3d_open("tests/fixtures/a4_mdl3.mdl");
    if(r!=GS3D_ERR_UNSUPPORTED)return 2;
    gs3d_close();
#endif
#if GS3D_WITH_MDL7
    r=gs3d_open("tests/fixtures/a7_a8_mdl7.mdl");
    if(r!=GS3D_OK)return 3;
    gs3d_close();
#else
    r=gs3d_open("tests/fixtures/a7_a8_mdl7.mdl");
    if(r!=GS3D_ERR_UNSUPPORTED)return 4;
    gs3d_close();
#endif
    puts("SINGLE-PARSER BUILD PASS");return 0;
}
