#include "../src/gs3d_internal.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c) do {if(!(c)){printf("PHASE07 ERROR at line %d\n",__LINE__);return 1;}}while(0)
static long absdiff(long a,long b){return a>b?a-b:b-a;}
static int near(long a,long b,long epsilon){return absdiff(a,b)<=epsilon;}
static int math_tests(void){
    GS3D_Mat a,b,out;long xx,yy;
    gs_mat_identity(&a);gs_mat_identity(&b);
    b.m[0]=0L;b.m[1]=65536L;b.m[4]=-65536L;b.m[5]=0L;
    CHECK(gs3d_trs_interpolate(&a,&b,32768L,&out)==GS3D_OK);
    CHECK(near(out.m[0],46341L,100L));CHECK(near(out.m[1],46341L,100L));
    CHECK(near(out.m[4],-46341L,100L));CHECK(near(out.m[5],46341L,100L));
    xx=out.m[0];yy=out.m[1];
    CHECK(near(gs_addsat(gs_qmul(xx,xx),gs_qmul(yy,yy)),65536L,200L));
    CHECK(gs3d_trs_interpolate(&a,&b,0L,&out)==GS3D_OK);
    CHECK(out.m[0]==65536L && out.m[1]==0L);
    CHECK(gs3d_trs_interpolate(&a,&b,65536L,&out)==GS3D_OK);
    CHECK(out.m[0]==0L && out.m[1]==65536L);
    CHECK(gs3d_trs_interpolate(&a,&b,-1L,&out)==GS3D_ERR_RANGE);
    CHECK(gs3d_trs_interpolate(&a,&b,65537L,&out)==GS3D_ERR_RANGE);
    gs_mat_identity(&b);b.m[0]=-65536L;
    CHECK(gs3d_trs_interpolate(&a,&b,32768L,&out)==GS3D_ERR_UNSUPPORTED);
    b.m[4]=32768L; b.m[0]=65536L; b.m[1]=0L;b.m[5]=65536L;
    CHECK(gs3d_trs_interpolate(&a,&b,32768L,&out)==GS3D_ERR_UNSUPPORTED);
    /* Nonuniform scale: 1->2 on X; translation 0->4 on X. */
    gs_mat_identity(&b);b.m[0]=131072L;b.m[12]=262144L;
    CHECK(gs3d_trs_interpolate(&a,&b,32768L,&out)==GS3D_OK);
    CHECK(near(out.m[0],98304L,20L) && out.m[12]==131072L);
    /* Opposite 180 degree orientations are valid rotations, not shear. */
    gs_mat_identity(&b);b.m[0]=-65536L;b.m[5]=-65536L;
    CHECK(gs3d_trs_interpolate(&a,&b,32768L,&out)==GS3D_OK);
    CHECK(near(out.m[0],0L,150L) && near(out.m[1],65536L,150L));
    return 0;
}
static int weighted_sequence(void){
    GS3D_WeightSet w;GS3D_Vec v;unsigned int i;
    CHECK(gs3d_open("tests/fixtures/mdl7_skeletal_sequence.mdl")==GS3D_OK);
    CHECK(gs3d_model.bones==2U && gs3d_model.group[0].frame_count==2U);
    CHECK(gs3d_find_frame(0U,"start")==0 && gs3d_find_frame(0U,"end")==1);
    CHECK(gs3d_get_vertex_weights(1UL,&w)==GS3D_OK && w.count==2U);
    CHECK(gs3d_vendor_sample_skeletal(4U,0U,0U,1U,32768L)==GS3D_ERR_UNSUPPORTED);
    CHECK(gs3d_vendor_sample_skeletal(5U,0U,0U,1U,32768L)==GS3D_ERR_UNSUPPORTED);
    for(i=6U;i<=8U;i++){
        CHECK(gs3d_vendor_sample_skeletal(i,0U,0U,1U,0L)==GS3D_OK);
        v=gs3d_model.v[1];CHECK(near(v.x,131072L,1L)&&near(v.y,0L,1L));
        CHECK(gs3d_vendor_sample_skeletal(i,0U,0U,1U,65536L)==GS3D_OK);
        v=gs3d_model.v[1];CHECK(near(v.x,360448L,3L)&&near(v.y,32768L,3L));
        CHECK(gs3d_vendor_sample_skeletal(i,0U,0U,1U,32768L)==GS3D_OK);
        v=gs3d_model.v[1];CHECK(near(v.x,252543L,260L)&&near(v.y,23170L,260L));
        CHECK(gs3d_vendor_sample_skeletal(i,0U,0U,1U,32768L)==GS3D_OK);
        v=gs3d_model.v[1];CHECK(near(v.x,252543L,260L));
    }
    CHECK(gs3d_sample_skeletal_q16(0U,0U,2U,0L)==GS3D_ERR_RANGE);
    CHECK(gs3d_sample_skeletal_q16(0U,0U,1U,65537L)==GS3D_ERR_RANGE);
    CHECK(gs3d_sample_morph_q16(0U,0U,1U,32768L)==GS3D_ERR_UNSUPPORTED);
    gs3d_close();return 0;
}
int main(void){
    CHECK(math_tests()==0);CHECK(weighted_sequence()==0);
    puts("PHASE07 PASS: quaternion NLERP, TRS, weighted MDL7 2-frame sequence, vendor routing, idempotence");
    return 0;
}
