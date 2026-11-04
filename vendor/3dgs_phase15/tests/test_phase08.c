#include "../src/gs3d_internal.h"
#include <stdio.h>
#include <limits.h>
#define CHECK(x) do {if(!(x)){printf("PHASE08 failed line %d\n",__LINE__);return 1;}}while(0)
static long absdif(long x,long y){return x>y?x-y:y-x;}
static int clk(const GS3D_Clip *c,unsigned long t,unsigned int a,
               unsigned int b,long alpha,unsigned int backwards,unsigned int end){
    GS3D_ClipSample v;
    if(gs3d_clip_resolve(c,t,&v))return 0;
    return v.frame_a==a && v.frame_b==b && absdif(v.alpha_q16,alpha)<=1L
        && v.direction==backwards && v.at_end==end;
}
static int legacy_clip(void){
    GS3D_Clip c;GS3D_ClipSample s;GS3D_ClipPlayer p;
    CHECK(gs3d_open("tests/fixtures/mdl3_morph_fraction.mdl")==GS3D_OK);
    CHECK(gs3d_clip_from_names(&c,4,0,"stand","walk",100UL,GS3D_CLIP_CLAMP,
             GS3D_CLIP_TRACK_AUTO)==GS3D_OK);
    CHECK(c.count==2U && c.generation==4U);
    CHECK(clk(&c,0UL,0,1,0L,0,0));
    CHECK(clk(&c,50UL,0,1,32768L,0,0));
    CHECK(clk(&c,100UL,1,1,0L,0,1));
    CHECK(clk(&c,ULONG_MAX,1,1,0L,0,1));
    CHECK(gs3d_clip_apply(&c,50UL,&s)==GS3D_OK);
    CHECK(gs3d_model.v[1].x==327680L);
    CHECK(gs3d_vendor_clip_apply(4,&c,50UL,0)==GS3D_OK);
    CHECK(gs3d_vendor_clip_apply(5,&c,50UL,0)==GS3D_ERR_UNSUPPORTED);
    CHECK(gs3d_clip_from_prefix(&c,4,0,"sta",11UL,GS3D_CLIP_LOOP,
             GS3D_CLIP_TRACK_MORPH)==GS3D_OK);
    CHECK(c.count==1U && clk(&c,123UL,0,0,0L,0,1));
    CHECK(gs3d_clip_from_prefix(&c,4,0,"not_a_clip",11UL,GS3D_CLIP_LOOP,
             GS3D_CLIP_TRACK_MORPH)==GS3D_ERR_RANGE);
    CHECK(gs3d_clip_create(&c,4,0,0,2,100,GS3D_CLIP_LOOP,
             GS3D_CLIP_TRACK_MORPH)==GS3D_OK);
    CHECK(clk(&c,100UL,1,0,0L,0,0));
    CHECK(clk(&c,150UL,1,0,32768L,0,0));
    CHECK(clk(&c,200UL,0,1,0L,0,0));
    CHECK(gs3d_clip_create(&c,4,0,0,2,100,GS3D_CLIP_PINGPONG,
             GS3D_CLIP_TRACK_AUTO)==GS3D_OK);
    CHECK(clk(&c,100UL,1,0,0L,1,0));
    CHECK(clk(&c,150UL,1,0,32768L,1,0));
    CHECK(clk(&c,200UL,0,1,0L,0,0));
    CHECK(gs3d_player_init(&p,&c)==GS3D_OK);
    CHECK(gs3d_player_advance(&p,50UL)==GS3D_OK);
    CHECK(gs3d_player_sample(&p,&s)==GS3D_OK && s.alpha_q16==32768L);
    p.playing=0U;
    CHECK(gs3d_player_advance(&p,50UL)==GS3D_OK);
    CHECK(p.elapsed_ticks==50UL);
    CHECK(gs3d_player_apply(&p,&s)==GS3D_OK);
    CHECK(gs3d_model.v[1].x==327680L);
    p.playing=1U;
    CHECK(gs3d_player_seek(&p,ULONG_MAX)==GS3D_OK);
    CHECK(gs3d_player_advance(&p,1UL)==GS3D_ERR_RANGE);
    CHECK(p.elapsed_ticks==ULONG_MAX);
    CHECK(gs3d_clip_create(&c,4,0,0,2,0UL,0,0)==GS3D_ERR_RANGE);
    CHECK(gs3d_clip_create(&c,4,0,1,2,1UL,0,0)==GS3D_ERR_RANGE);
    CHECK(gs3d_clip_create(&c,4,0,0,2,1UL,7U,0U)==GS3D_ERR_RANGE);
    CHECK(gs3d_clip_create(&c,4,0,0,2,1UL,0,GS3D_CLIP_TRACK_SKELETAL)==GS3D_ERR_UNSUPPORTED);
    gs3d_close();return 0;
}
static int skeletal_clip(void){
    GS3D_Clip c;GS3D_ClipSample s;
    CHECK(gs3d_open("tests/fixtures/mdl7_skeletal_sequence.mdl")==GS3D_OK);
    CHECK(gs3d_clip_from_names(&c,7U,0U,"start","end",100UL,
            GS3D_CLIP_LOOP,GS3D_CLIP_TRACK_AUTO)==GS3D_OK);
    CHECK(clk(&c,50UL,0U,1U,32768L,0U,0U));
    CHECK(gs3d_clip_apply(&c,50UL,&s)==GS3D_OK);
    CHECK(absdif(gs3d_model.v[1].x,252543L)<260L);
    CHECK(absdif(gs3d_model.v[1].y,23170L)<260L);
    CHECK(gs3d_clip_apply(&c,150UL,&s)==GS3D_OK);
    CHECK(s.frame_a==1U && s.frame_b==0U);
    CHECK(gs3d_clip_apply(&c,150UL,&s)==GS3D_OK); /* non-accumulative */
    CHECK(gs3d_clip_create(&c,4U,0U,0U,2U,100UL,0U,0U)==GS3D_ERR_RANGE);
    CHECK(gs3d_clip_create(&c,7U,0U,0U,2U,100UL,0U,
            GS3D_CLIP_TRACK_SKELETAL)==GS3D_OK);
    CHECK(gs3d_clip_apply(&c,1000UL,&s)==GS3D_OK);
    CHECK(s.at_end==1U&&s.frame_a==1U&&s.frame_b==1U);
    gs3d_close();return 0;
}
static int multigroup(void){
    GS3D_Clip c;GS3D_ClipSample s;
    CHECK(gs3d_open("tests/fixtures/a7_a8_mdl7.mdl")==GS3D_OK);
    CHECK(gs3d_model.groups==2U);
    CHECK(gs3d_clip_create(&c,7U,0U,0U,1U,100UL,GS3D_CLIP_LOOP,
                  GS3D_CLIP_TRACK_MORPH)==GS3D_OK);
    CHECK(gs3d_clip_apply_all(&c,1234UL,&s)==GS3D_OK);
    CHECK(s.frame_a==0U);
    gs3d_close();return 0;
}
int main(void){
    CHECK(legacy_clip()==0);CHECK(skeletal_clip()==0);CHECK(multigroup()==0);
    puts("PHASE08 PASS: named clips, explicit timeline, clamp/loop/pingpong, player, A4/A7, multigroup");
    return 0;
}
