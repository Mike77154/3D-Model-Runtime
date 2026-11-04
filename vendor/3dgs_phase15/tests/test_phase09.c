#include "../src/gs3d_internal.h"
#include <stdio.h>
#include <limits.h>
#define CHECK(x) do { if(!(x)){printf("PHASE09 FAILED at line %d\n",__LINE__);return 1;} } while(0)
static long absdiff(long a,long b){return a>b?a-b:b-a;}
static int test_morph(void){
    GS3D_Clip a,b,bad; GS3D_Crossfade x;GS3D_BlendSample s;
    GS3D_Vec av,bv,result;unsigned int vendor;int r;
    CHECK(gs3d_open("tests/fixtures/mdl3_morph_fraction.mdl")==GS3D_OK);
    CHECK(gs3d_clip_create(&a,4U,0U,0U,1U,100UL,GS3D_CLIP_CLAMP,GS3D_CLIP_TRACK_MORPH)==0);
    CHECK(gs3d_clip_create(&b,4U,0U,1U,1U,100UL,GS3D_CLIP_CLAMP,GS3D_CLIP_TRACK_MORPH)==0);
    CHECK(gs3d_clip_apply(&a,0UL,0)==0);av=gs3d_model.v[1];
    CHECK(gs3d_clip_apply(&b,0UL,0)==0);bv=gs3d_model.v[1];
    CHECK(gs3d_crossfade_init(&x,&a,&b,0UL,0UL,100UL)==0);
    CHECK(gs3d_vendor_crossfade_apply(4U,&x,&s)==0);
    CHECK(s.weight_q16==0L && gs3d_model.v[1].x==av.x);
    CHECK(gs3d_crossfade_advance(&x,50UL)==0);
    CHECK(gs3d_crossfade_apply(&x,&s)==0);
    result=gs3d_model.v[1];
    CHECK(s.weight_q16==32768L);
    CHECK(absdiff(result.x,gs_qmul(av.x,32768L)+gs_qmul(bv.x,32768L))<=2L);
    CHECK(gs3d_crossfade_apply(&x,0)==0 && gs3d_model.v[1].x==result.x);
    CHECK(gs3d_crossfade_advance(&x,50UL)==0);
    CHECK(gs3d_crossfade_apply(&x,&s)==0 && s.weight_q16==65536L);
    CHECK(gs3d_model.v[1].x==bv.x);
    x.playing=0U;CHECK(gs3d_crossfade_advance(&x,100UL)==0);
    CHECK(x.fade_elapsed==100UL);
    CHECK(gs3d_crossfade_seek(&x,0UL,0UL,25UL)==0);
    CHECK(gs3d_crossfade_apply(&x,&s)==0);
    CHECK(s.weight_q16==16384L);
    CHECK(gs3d_crossfade_seek(&x,0UL,0UL,9999UL)==0);
    CHECK(x.fade_elapsed==100UL);
    CHECK(gs3d_crossfade_seek(&x,ULONG_MAX,0UL,0UL)==0);
    x.playing=1U;r=gs3d_crossfade_advance(&x,1UL);
    CHECK(r==GS3D_ERR_RANGE && x.to_ticks==0UL&&x.fade_elapsed==0UL);
    CHECK(gs3d_crossfade_init(&x,&a,&b,0UL,0UL,0UL)==GS3D_ERR_RANGE);
    CHECK(gs3d_crossfade_init(&x,&a,&b,0UL,0UL,100UL)==0);
    bad=b;bad.generation=7U;
    CHECK(gs3d_crossfade_init(&x,&a,&bad,0UL,0UL,100UL)!=0);
    bad=b;bad.track=GS3D_CLIP_TRACK_SKELETAL;
    CHECK(gs3d_crossfade_init(&x,&a,&bad,0UL,0UL,100UL)==GS3D_ERR_UNSUPPORTED);
    bad=b;bad.first=255U;
    CHECK(gs3d_crossfade_init(&x,&a,&bad,0UL,0UL,100UL)==GS3D_ERR_RANGE);
    for(vendor=5U;vendor<=8U;vendor++)
        CHECK(gs3d_vendor_crossfade_apply(vendor,&x,0)==GS3D_ERR_UNSUPPORTED);
    gs3d_close();return 0;
}
static int test_skeletal(void){
    GS3D_Clip a,b;GS3D_Crossfade x;GS3D_BlendSample s;
    GS3D_Vec av,bv,mid,expected;unsigned int vendor;
    CHECK(gs3d_open("tests/fixtures/mdl7_skeletal_sequence.mdl")==0);
    CHECK(gs3d_clip_create(&a,7U,0U,0U,1U,100UL,GS3D_CLIP_CLAMP,GS3D_CLIP_TRACK_SKELETAL)==0);
    CHECK(gs3d_clip_create(&b,7U,0U,1U,1U,100UL,GS3D_CLIP_CLAMP,GS3D_CLIP_TRACK_SKELETAL)==0);
    CHECK(gs3d_clip_apply(&a,0UL,0)==0);av=gs3d_model.v[1];
    CHECK(gs3d_clip_apply(&b,0UL,0)==0);bv=gs3d_model.v[1];
    CHECK(gs3d_sample_skeletal_q16(0U,0U,1U,32768L)==0);
    expected=gs3d_model.v[1];
    CHECK(gs3d_crossfade_init(&x,&a,&b,0UL,0UL,100UL)==0);
    CHECK(gs3d_crossfade_apply(&x,&s)==0 && gs3d_model.v[1].x==av.x);
    CHECK(gs3d_crossfade_advance(&x,50UL)==0);
    CHECK(gs3d_vendor_crossfade_apply(7U,&x,&s)==0 && s.weight_q16==32768L);
    mid=gs3d_model.v[1];
    CHECK(absdiff(mid.x,expected.x)<=5L && absdiff(mid.y,expected.y)<=5L);
    CHECK(gs3d_vendor_crossfade_apply(7U,&x,0)==0);
    CHECK(gs3d_model.v[1].x==mid.x && gs3d_model.v[1].y==mid.y);
    CHECK(gs3d_crossfade_advance(&x,50UL)==0);
    CHECK(gs3d_crossfade_apply(&x,&s)==0 && gs3d_model.v[1].x==bv.x);
    for(vendor=6U;vendor<=8U;vendor++){
        GS3D_Clip ca,cb;GS3D_Crossfade cy;
        CHECK(gs3d_clip_create(&ca,vendor,0U,0U,1U,100UL,0U,2U)==0);
        CHECK(gs3d_clip_create(&cb,vendor,0U,1U,1U,100UL,0U,2U)==0);
        CHECK(gs3d_crossfade_init(&cy,&ca,&cb,0UL,0UL,100UL)==0);
        CHECK(gs3d_crossfade_seek(&cy,0UL,0UL,50UL)==0);
        CHECK(gs3d_vendor_crossfade_apply(vendor,&cy,0)==0);
        CHECK(absdiff(gs3d_model.v[1].x,expected.x)<=5L);
    }
    gs3d_close();return 0;
}
static int test_multi(void){
    GS3D_Clip a,b;GS3D_Crossfade x;GS3D_BlendSample s;
    CHECK(gs3d_open("tests/fixtures/mdl7_two_weighted_groups.mdl")==0);
    CHECK(gs3d_model.groups==2U);
    CHECK(gs3d_clip_create(&a,7U,0U,0U,1U,100UL,0U,2U)==0);
    CHECK(gs3d_clip_create(&b,7U,0U,0U,1U,100UL,0U,2U)==0);
    CHECK(gs3d_crossfade_init(&x,&a,&b,0UL,0UL,100UL)==0);
    CHECK(gs3d_crossfade_seek(&x,0UL,0UL,50UL)==0);
    CHECK(gs3d_crossfade_apply_all(&x,&s)==0);
    CHECK(s.weight_q16==32768L);
    gs3d_close();return 0;
}
int main(void){
    CHECK(test_morph()==0);
    CHECK(test_skeletal()==0);
    CHECK(test_multi()==0);
    puts("PHASE09 PASS: independent clocks, C89 crossfade, morph, skeleton, all vendors");
    return 0;
}
