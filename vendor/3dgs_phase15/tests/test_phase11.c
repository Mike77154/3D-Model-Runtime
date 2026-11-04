#include "../src/gs3d_internal.h"
#include <stdio.h>
#include <limits.h>
#define CHECK(x) do{if(!(x)){printf("PHASE11 FAIL line %d\n",__LINE__);return 1;}}while(0)
static long delta(long x,long y){return x>y?x-y:y-x;}
static int mtx_tests(void){
    GS3D_Mat b,a,r,o;
    gs_mat_identity(&b);gs_mat_identity(&a);gs_mat_identity(&r);
    b.m[12]=65536L;b.m[0]=196608L;
    a.m[12]=262144L;a.m[0]=262144L;
    r.m[12]=131072L;r.m[0]=131072L;
    CHECK(gs3d_additive_matrix(&b,&a,&r,32768L,&o)==0);
    CHECK(delta(o.m[12],131072L)<8L); /* base+0.5*(4-2) = 2 */
    CHECK(delta(o.m[0],294912L)<8L); /* 3 * (1+0.5*(4/2-1))=4.5 */
    CHECK(gs3d_additive_matrix(&b,&a,&r,0L,&o)==0);
    CHECK(memcmp(&b,&o,sizeof(b))==0);
    CHECK(gs3d_additive_matrix(&b,&a,&r,-1L,&o)==GS3D_ERR_RANGE);
    CHECK(gs3d_additive_matrix(&b,&a,&r,65537L,&o)==GS3D_ERR_RANGE);
    a.m[4]=32768L; /* shear */
    CHECK(gs3d_additive_matrix(&b,&a,&r,32768L,&o)==GS3D_ERR_UNSUPPORTED);
    return 0;
}
static int clip_test(void){
    GS3D_Clip base,add,ref,one;GS3D_AdditiveRequest req;
    GS3D_BoneMask mask;GS3D_Vec mid,end,zero;unsigned int gen;
    CHECK(gs3d_open("tests/fixtures/mdl7_skeletal_sequence.mdl")==0);
    CHECK(gs3d_mask_init(&mask,0L)==0);
    CHECK(gs3d_mask_set(&mask,1U,65536L)==0);
    CHECK(gs3d_clip_create(&base,7U,0U,0U,1U,100UL,0U,2U)==0);
    CHECK(gs3d_clip_create(&add,7U,0U,0U,2U,100UL,0U,2U)==0);
    CHECK(gs3d_clip_create(&ref,7U,0U,0U,1U,100UL,0U,2U)==0);
    memset(&req,0,sizeof(req));req.base=base;req.additive=add;req.reference=ref;
    req.strength_q16=65536L;req.additive_ticks=100UL;
    CHECK(gs3d_vendor_additive_apply(7U,&req,&mask)==0);
    end=gs3d_model.v[1];
    CHECK(delta(end.x,98304L)<280L&&delta(end.y,32768L)<280L);
    CHECK(gs3d_model.v[2].y==65536L); /* root bone excluded */
    req.strength_q16=32768L;
    CHECK(gs3d_additive_apply(&req,&mask)==0);
    mid=gs3d_model.v[1];
    CHECK(delta(mid.x,121480L)<350L&&delta(mid.y,23170L)<350L);
    CHECK(gs3d_additive_apply(&req,&mask)==0);
    CHECK(gs3d_model.v[1].x==mid.x&&gs3d_model.v[1].y==mid.y);
    req.strength_q16=0L;
    CHECK(gs3d_additive_apply(&req,&mask)==0);
    zero=gs3d_model.v[1];
    CHECK(delta(zero.x,131072L)<30L && delta(zero.y,0L)<30L);
    /* The reference is equal to the additive sample: neutral delta. */
    req.reference=add;req.reference_ticks=100UL;
    req.strength_q16=65536L;
    CHECK(gs3d_additive_apply(&req,&mask)==0);
    CHECK(delta(gs3d_model.v[1].x,zero.x)<40L);
    CHECK(delta(gs3d_model.v[1].y,zero.y)<40L);
    /* Reverse delta: from frame1 back to frame0 over base frame1. */
    req.base=add;req.base_ticks=100UL;
    req.additive=ref;req.additive_ticks=0UL;
    req.reference=add;req.reference_ticks=100UL;
    CHECK(gs3d_additive_apply(&req,&mask)==0);
    CHECK(delta(gs3d_model.v[1].x,393216L)<350L);
    CHECK(delta(gs3d_model.v[1].y,0L)<350L);
    req.strength_q16=65537L;
    CHECK(gs3d_additive_apply(&req,&mask)==GS3D_ERR_RANGE);
    req.strength_q16=65536L;
    CHECK(gs3d_mask_set(&mask,1U,-1L)==GS3D_ERR_RANGE);
    CHECK(gs3d_clip_create(&one,7U,0U,1U,1U,100UL,0U,2U)==0);
    req.reference=one;req.reference_ticks=0UL;
    CHECK(gs3d_additive_apply(&req,&mask)==0);
    for(gen=6U;gen<=8U;gen++){
        CHECK(gs3d_clip_create(&base,gen,0U,0U,1U,100UL,0U,2U)==0);
        CHECK(gs3d_clip_create(&add,gen,0U,0U,2U,100UL,0U,2U)==0);
        CHECK(gs3d_clip_create(&ref,gen,0U,0U,1U,100UL,0U,2U)==0);
        req.base=base;req.additive=add;req.reference=ref;
        req.base_ticks=0UL;req.additive_ticks=50UL;req.reference_ticks=0UL;
        req.strength_q16=32768L;
        CHECK(gs3d_vendor_additive_apply(gen,&req,&mask)==0);
        CHECK(gs3d_vendor_additive_apply(gen==8U?7U:8U,&req,&mask)!=0);
    }
    CHECK(gs3d_additive_apply_all(&req,&mask)==0);
    gs3d_close();
    CHECK(gs3d_open("tests/fixtures/a4_mdl3.mdl")==0);
    CHECK(gs3d_vendor_additive_apply(4U,&req,&mask)==GS3D_ERR_RANGE ||
          gs3d_vendor_additive_apply(4U,&req,&mask)==GS3D_ERR_UNSUPPORTED);
    gs3d_close();
    return 0;
}
static int legacy_additive_test(void){
    GS3D_AdditiveRequest req;GS3D_Clip base,add,ref;GS3D_BoneMask dummy;
    const char *paths[3]={"tests/fixtures/mdl3_morph_fraction.mdl",
        "tests/fixtures/a5_mdl4.mdl","tests/fixtures/a6_mdl5.mdl"};
    unsigned int gen[3]={4U,5U,6U},k;
    long initial_x,first;
    for(k=0U;k<3U;k++){
        CHECK(gs3d_open(paths[k])==GS3D_OK);
        CHECK(gs3d_clip_create(&base,gen[k],0U,0U,1U,100UL,
                               GS3D_CLIP_CLAMP,GS3D_CLIP_TRACK_MORPH)==0);
        CHECK(gs3d_clip_create(&add,gen[k],0U,1U,1U,100UL,
                               GS3D_CLIP_CLAMP,GS3D_CLIP_TRACK_MORPH)==0);
        CHECK(gs3d_clip_create(&ref,gen[k],0U,0U,1U,100UL,
                               GS3D_CLIP_CLAMP,GS3D_CLIP_TRACK_MORPH)==0);
        memset(&req,0,sizeof(req));req.base=base;req.additive=add;
        req.reference=ref;req.strength_q16=32768L;
        CHECK(gs3d_vendor_additive_apply(gen[k],&req,(const GS3D_BoneMask *)0)==0);
        first=gs3d_model.v[1].x;
        if(k==0U)CHECK(first==327680L);
        CHECK(gs3d_vendor_additive_apply(gen[k],&req,(const GS3D_BoneMask *)0)==0);
        CHECK(gs3d_model.v[1].x==first);
        CHECK(gs3d_vendor_additive_apply(gen[k],&req,&dummy)==GS3D_ERR_UNSUPPORTED);
        req.strength_q16=0L;
        CHECK(gs3d_vendor_additive_apply(gen[k],&req,(const GS3D_BoneMask *)0)==0);
        initial_x=gs3d_model.v[1].x;
        CHECK(initial_x==65536L);
        gs3d_close();
    }
    return 0;
}
static int sync_test(void){
    GS3D_Clip a,b;GS3D_ClipSample samp;long phase;
    memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
    a.count=2U;a.mode=GS3D_CLIP_LOOP;a.ticks_per_frame=100UL;
    b.count=4U;b.mode=GS3D_CLIP_LOOP;b.ticks_per_frame=500UL;
    CHECK(gs3d_sync_phase(&a,75UL,&phase)==0);
    CHECK(delta(phase,24576L)<2L);
    CHECK(gs3d_sync_sample(&a,75UL,&b,0L,&samp)==0);
    CHECK(samp.frame_a==1U&&samp.frame_b==2U);
    CHECK(delta(samp.alpha_q16,32768L)<5L);
    CHECK(gs3d_sync_sample(&a,75UL,&b,16384L,&samp)==0);
    CHECK(samp.frame_a==2U&&samp.frame_b==3U);
    CHECK(gs3d_sync_sample(&a,150UL,&b,0L,&samp)==0);
    CHECK(samp.frame_a==3U&&samp.frame_b==0U);
    CHECK(gs3d_sync_sample(&a,200UL,&b,0L,&samp)==0);
    CHECK(samp.frame_a==0U&&samp.alpha_q16==0L);
    CHECK(gs3d_sync_phase(&a,ULONG_MAX,&phase)==0);
    CHECK(phase>=0L&&phase<65536L);
    b.mode=GS3D_CLIP_CLAMP;
    CHECK(gs3d_sync_sample(&a,75UL,&b,0L,&samp)==GS3D_ERR_RANGE);
    b.mode=GS3D_CLIP_LOOP;
    CHECK(gs3d_sync_sample(&a,75UL,&b,65536L,&samp)==GS3D_ERR_RANGE);
    a.count=1U;
    CHECK(gs3d_sync_phase(&a,75UL,&phase)==GS3D_ERR_RANGE);
    return 0;
}
int main(void){
    CHECK(mtx_tests()==0);
    CHECK(sync_test()==0);
    CHECK(legacy_additive_test()==0);
    CHECK(clip_test()==0);
    puts("PHASE11 PASS: additive reference-local TRS, vendor routing, loop sync and idempotence");
    return 0;
}
