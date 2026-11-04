#include "../src/gs3d_internal.h"
#include <stdio.h>
#include <limits.h>
#define CK(x) do {if(!(x)){printf("PHASE09 ROBUST failure line %d\n",__LINE__);return 1;}} while(0)
static unsigned long rng=0x4D474D31UL;
static unsigned long nextval(void){
    rng=rng*1664525UL+1013904223UL;
    return rng;
}
static int walk(const char *file,unsigned int vendor,unsigned int track,unsigned int loops){
    GS3D_Clip a,b;GS3D_Crossfade f;GS3D_BlendSample s;
    unsigned int i;unsigned long duration,elapsed,time_a,time_b;
    CK(gs3d_open(file)==0);
    CK(gs3d_clip_create(&a,vendor,0U,0U,1U,91UL,GS3D_CLIP_LOOP,track)==0);
    CK(gs3d_clip_create(&b,vendor,0U,1U,1U,123UL,GS3D_CLIP_LOOP,track)==0);
    CK(gs3d_crossfade_init(&f,&a,&b,12UL,17UL,251UL)==0);
    for(i=0U;i<loops;i++){
        duration=(nextval()%10000UL)+1UL;
        elapsed=nextval()%(duration+2UL);
        time_a=nextval()%100000UL;
        time_b=nextval()%100000UL;
        f.fade_ticks=duration;
        CK(gs3d_crossfade_seek(&f,time_a,time_b,elapsed)==0);
        CK(gs3d_crossfade_sample(&f,&s)==0);
        CK(s.weight_q16>=0L&&s.weight_q16<=65536L);
        CK(gs3d_crossfade_apply(&f,&s)==0);
        CK(s.weight_q16>=0L&&s.weight_q16<=65536L);
        CK(gs3d_crossfade_apply(&f,0)==0); /* nonaccumulating */
    }
    CK(gs3d_crossfade_seek(&f,ULONG_MAX,ULONG_MAX,0UL)==0);
    CK(gs3d_crossfade_advance(&f,1UL)==GS3D_ERR_RANGE);
    CK(f.from_ticks==ULONG_MAX&&f.to_ticks==ULONG_MAX&&f.fade_elapsed==0UL);
    CK(gs3d_crossfade_advance(&f,0UL)==0);
    f.playing=0U;
    CK(gs3d_crossfade_advance(&f,1UL)==0 && f.from_ticks==ULONG_MAX);
    gs3d_close();return 0;
}
int main(void){
    CK(walk("tests/fixtures/mdl3_morph_fraction.mdl",4U,1U,2000U)==0);
    CK(walk("tests/fixtures/mdl7_skeletal_sequence.mdl",7U,2U,500U)==0);
    puts("PHASE09 ROBUST PASS: 2500 randomized clock/pose samples, repeated evaluations, ULONG_MAX");
    return 0;
}
