#include "../include/gs3d.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){printf("DEFORM FAIL line %d\n",__LINE__);return 1;} }while(0)
static int check_legacy(const char *path,unsigned int generation){
    GS3D_WeightSet w;GS3D_DeformerInfo info;GS3D_Vec v;
    CHECK(gs3d_open(path)==GS3D_OK);
    CHECK(gs3d_vendor_deformer_info(generation,0U,&info)==GS3D_OK);
    CHECK(info.kind==GS3D_DEFORM_VERTEX_FRAME);
    CHECK(gs3d_vendor_weights(generation,1UL,&w)==GS3D_OK);
    CHECK(w.kind==GS3D_DEFORM_VERTEX_FRAME&&w.count==1U);
    CHECK(w.item[0].bone==65535U&&w.item[0].weight_q16==65536L);
    CHECK(gs3d_vendor_deform(generation,0U,1U)==GS3D_OK);
    v=gs3d_model.v[1];
    CHECK(v.x==65536L);
    gs3d_close();return 0;
}
static int weighted(void){
    GS3D_WeightSet w;GS3D_DeformerInfo di;GS3D_Vec v;
    CHECK(gs3d_open("tests/fixtures/mdl7_weighted_valid.mdl")==GS3D_OK);
    CHECK(gs3d_model.format==7 && gs3d_model.bones==2U);
    CHECK(gs3d_vendor_deformer_info(7U,0U,&di)==GS3D_OK);
    CHECK(di.kind==GS3D_DEFORM_WEIGHTED_BONE);
    CHECK(di.declared_deformers==1U && di.parsed_deformers==1U);
    CHECK(di.explicit_influences==2UL && di.weighted_vertices==1UL);
    CHECK(gs3d_vendor_weights(7U,1UL,&w)==GS3D_OK);
    CHECK(w.count==2U && w.kind==GS3D_DEFORM_WEIGHTED_BONE);
    CHECK(w.item[0].weight_q16==32768L&&w.item[1].weight_q16==32768L);
    CHECK(w.item[0].bone==0U && w.item[1].bone==1U);
    CHECK(gs3d_vendor_deform(7U,0U,0U)==GS3D_OK);
    v=gs3d_model.v[1];
    CHECK(v.x==294912L && v.y==294912L && v.z==327680L);
    CHECK(gs3d_vendor_deform(6U,0U,0U)==GS3D_OK);
    CHECK(gs3d_vendor_deform(8U,0U,0U)==GS3D_OK);
    v=gs3d_model.v[1];CHECK(v.x==294912L && v.y==294912L);
    CHECK(gs3d_a4_open("tests/fixtures/mdl7_weighted_valid.mdl")==GS3D_ERR_UNSUPPORTED);
    return 0;
}
static int invalid_weights(void){
    static unsigned char bytes[2048];unsigned long n;FILE *f;
    unsigned int offset;int r;
    f=fopen("tests/fixtures/mdl7_weighted_valid.mdl","rb");CHECK(f!=NULL);
    n=(unsigned long)fread(bytes,1,sizeof(bytes),f);fclose(f);CHECK(n>300UL&&n<2048UL);
    /* There are two trailing weights: turn the last into a negative f32. */
    bytes[n-1UL]=0xBFU; r=gs3d_open_memory(bytes,n);CHECK(r==GS3D_ERR_CORRUPT);
    /* One deformer element now references an impossible bone. */
    f=fopen("tests/fixtures/mdl7_weighted_valid.mdl","rb");CHECK(f!=NULL);
    n=(unsigned long)fread(bytes,1,sizeof(bytes),f);fclose(f);
    offset=(unsigned int)(n-72UL); /* first bone index in trailing 72-byte elements */
    CHECK(offset<n);
    bytes[offset]=200U;r=gs3d_open_memory(bytes,n);CHECK(r==GS3D_ERR_CORRUPT);
    /* Entire structure truncated must be rejected by the reader. */
    r=gs3d_open_memory(bytes,n-1UL);CHECK(r!=GS3D_OK);
    return 0;
}
int main(void){
    CHECK(check_legacy("tests/fixtures/a4_mdl3.mdl",4U)==0);
    CHECK(check_legacy("tests/fixtures/a5_mdl4.mdl",5U)==0);
    CHECK(check_legacy("tests/fixtures/a6_mdl5.mdl",6U)==0);
    CHECK(check_legacy("tests/fixtures/a5_mdl4.mdl",7U)==0);
    CHECK(check_legacy("tests/fixtures/a6_mdl5.mdl",8U)==0);
    CHECK(weighted()==0);
    CHECK(invalid_weights()==0);
    puts("DEFORM WEIGHTS TESTS PASS: A4-A8 profile dispatch, MDL3/4/5 frame morphology, MDL7 two bone blend, malformed and truncation");
    return 0;
}
