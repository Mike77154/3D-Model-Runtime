#include "../include/gs3d_vtk_bridge.h"
#include <string.h>
#include <stdio.h>
#define CHECK(X) do {if(!(X)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#X);return 1;}}while(0)
static int fake_decode(void *user,const char *name,unsigned char *rgba,unsigned long cap,
                       unsigned long *w,unsigned long *h){
    unsigned long i;(void)user;
    if(strcmp(name,"fake_external.tga")!=0 || cap<4UL)return GS3D_ERR_UNSUPPORTED;
    for(i=0UL;i<4UL;i++){
        rgba[4UL*i]=0U;rgba[4UL*i+1UL]=255U;
        rgba[4UL*i+2UL]=0U;rgba[4UL*i+3UL]=255U;
    }
    *w=2UL;*h=2UL;return GS3D_OK;
}
int main(void){
    unsigned long w,h;unsigned int i,ag,as;GS3D_TextureInfo ti;
    const unsigned char *data;unsigned int ns=0U;unsigned long nv,nf,nu;unsigned int frames;
    CHECK(gs3d_open("tests/fixtures/mdl7_all_dds_and_reference.mdl")==GS3D_OK);
    CHECK(gs3d_vtk_group(0U,&nv,&nf,&nu,&ns,&frames)==GS3D_OK);
    CHECK(ns==5U && nv==3UL && nf==1UL);
    for(i=0U;i<5U;i++){
        CHECK(gs3d_texture_info(0U,i,&ti)==GS3D_OK);
        CHECK(ti.slot==i && ti.group==0U);
        CHECK(gs3d_vtk_skin_info(0U,i,&ti)==GS3D_OK);
        CHECK(gs3d_vtk_skin(0U,i,&w,&h,&data)==GS3D_OK);
        CHECK(w==4UL && h==4UL);
        CHECK(data[0]>=248U && data[1]==0U && data[2]==0U && data[3]==255U);
    }
    CHECK(gs3d_texture_info(0U,4U,&ti)==GS3D_OK);
    CHECK(ti.storage==GS3D_TEXTURE_REFERENCE && ti.resolved_slot==0U);
    CHECK(gs3d_texture_resolve(0U,4U,&ag,&as)==GS3D_OK);
    CHECK(ag==0U&&as==0U);
    CHECK(gs3d_texture_info(0U,5U,&ti)==GS3D_ERR_RANGE);
    gs3d_close();
    CHECK(gs3d_open("tests/fixtures/mdl7_dds_large_blob.mdl")==GS3D_OK);
    CHECK(gs3d_texture_info(0U,0U,&ti)==GS3D_OK);
    CHECK(ti.encoded_bytes>65535UL);
    CHECK(gs3d_decode_skin(0U,0U,&w,&h)==GS3D_OK);
    CHECK(w==512UL && h==256UL);
    CHECK(gs3d_rgba[0]>=248U && gs3d_rgba[1]==0U);
    gs3d_close();
    CHECK(gs3d_open("tests/fixtures/mdl7_dds_bad_magic.mdl")==GS3D_OK);
    CHECK(gs3d_decode_skin(0U,0U,&w,&h)==GS3D_ERR_CORRUPT);
    gs3d_close();
    CHECK(gs3d_open("tests/fixtures/mdl7_dds_truncated.mdl")!=GS3D_OK);
    gs3d_close();
    CHECK(gs3d_open("tests/fixtures/mdl7_external_callback.mdl")==GS3D_OK);
    CHECK(gs3d_decode_skin(0U,0U,&w,&h)==GS3D_ERR_UNSUPPORTED);
    gs3d_set_external_texture_decoder(fake_decode,(void *)0);
    CHECK(gs3d_decode_skin(0U,0U,&w,&h)==GS3D_OK);
    CHECK(w==2UL && h==2UL && gs3d_rgba[1]==255U);
    gs3d_set_external_texture_decoder((GS3D_ExternalTextureDecode)0,(void *)0);
    gs3d_close();
    puts("PASS phase14 DDS BC1/BC2/BC3/RGBA, aliases, bounds, external decoder callbacks");
    return 0;
}
