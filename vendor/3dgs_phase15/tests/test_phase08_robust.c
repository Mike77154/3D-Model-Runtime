#include "../src/gs3d_internal.h"
#include <stdio.h>
#include <limits.h>
#define CHECK(x) do {if(!(x)){printf("ROBUST08 FAIL line %d\n",__LINE__);return 1;}}while(0)
static unsigned long rng=234567UL;
static unsigned long next_rand(void){rng=rng*1664525UL+1013904223UL;return rng;}
int main(void){
    GS3D_Clip c;GS3D_ClipSample s;unsigned int i;
    CHECK(gs3d_open("tests/fixtures/mdl7_skeletal_sequence.mdl")==0);
    CHECK(gs3d_clip_create(&c,7U,0U,0U,2U,1UL,GS3D_CLIP_LOOP,
                           GS3D_CLIP_TRACK_AUTO)==GS3D_OK);
    for(i=0U;i<8000U;i++){
        unsigned long t=next_rand(),rate=(next_rand()%65535UL)+1UL;
        c.ticks_per_frame=rate;
        c.mode=(unsigned char)(next_rand()%3UL);
        CHECK(gs3d_clip_resolve(&c,t,&s)==GS3D_OK);
        CHECK(s.frame_a<2U && s.frame_b<2U);
        CHECK(s.alpha_q16>=0L && s.alpha_q16<65536L);
        if(c.mode==GS3D_CLIP_CLAMP && t>=rate){
            CHECK(s.frame_a==1U && s.frame_b==1U && s.at_end==1U);
        }
    }
    c.ticks_per_frame=ULONG_MAX;
    CHECK(gs3d_clip_resolve(&c,ULONG_MAX-1UL,&s)==GS3D_OK);
    CHECK(s.alpha_q16<=65535L);
    c.count=0U;CHECK(gs3d_clip_resolve(&c,1UL,&s)==GS3D_ERR_RANGE);
    c.count=2U;c.ticks_per_frame=0UL;
    CHECK(gs3d_clip_resolve(&c,1UL,&s)==GS3D_ERR_RANGE);
    c.ticks_per_frame=1UL;c.first=65535U;
    CHECK(gs3d_clip_resolve(&c,1UL,&s)==GS3D_ERR_RANGE);
    gs3d_close();puts("PHASE08 ROBUST PASS: 8000 randomized timeline cases and ULONG_MAX edges");return 0;
}
