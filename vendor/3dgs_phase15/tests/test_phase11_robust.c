#include "../src/gs3d_internal.h"
#include <stdio.h>
#include <limits.h>
#define TEST(x) do {if(!(x)){printf("P11 ROBUST FAIL line %d\n",__LINE__);return 1;}}while(0)
static unsigned long seed=0x571AB123UL;
static unsigned long rand32(void){seed=seed*1664525UL+1013904223UL;return seed;}
static unsigned char buffer[65536UL];
static int mod_and_fuzz(void){
    FILE *fp;long n;unsigned long i,ix;unsigned char before;int rc;
    GS3D_BoneMask mask;GS3D_Clip c;GS3D_AdditiveRequest req;
    fp=fopen("tests/fixtures/mdl7_skeletal_sequence.mdl","rb");TEST(fp!=0);
    TEST(fseek(fp,0L,SEEK_END)==0);n=ftell(fp);
    TEST(n>0L && (unsigned long)n<sizeof(buffer));rewind(fp);
    TEST(fread(buffer,1U,(size_t)n,fp)==(size_t)n);fclose(fp);
    for(i=0UL;i<500UL;i++){
        ix=rand32()%(unsigned long)n;before=buffer[ix];
        buffer[ix]=(unsigned char)(before ^ (1U<<(unsigned int)(rand32()%8UL)));
        rc=gs3d_open_memory(buffer,(unsigned long)n);
        if(rc==0){
            if(gs3d_model.format==7&&gs3d_model.bones && gs3d_model.group[0].frame_count){
                if(gs3d_mask_init(&mask,0L)==0 &&
                   gs3d_clip_create(&c,7U,0U,0U,1U,100UL,0U,2U)==0){
                    memset(&req,0,sizeof(req));req.base=req.additive=req.reference=c;
                    req.strength_q16=(long)(rand32()%65537UL);
                    (void)gs3d_additive_apply(&req,&mask);
                }
            }
        }
        gs3d_close();buffer[ix]=before;
    }
    return 0;
}
static int sync_fuzz(void){
    GS3D_Clip a,b;GS3D_ClipSample out;unsigned long i;int rc;
    long phase;
    memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
    a.mode=b.mode=GS3D_CLIP_LOOP;
    for(i=0UL;i<9000UL;i++){
        a.first=0U;b.first=0U;
        a.count=(unsigned short)(2UL+rand32()%GS3D_MAX_FRAMES);
        b.count=(unsigned short)(2UL+rand32()%GS3D_MAX_FRAMES);
        if(a.count>GS3D_MAX_FRAMES)a.count=GS3D_MAX_FRAMES;
        if(b.count>GS3D_MAX_FRAMES)b.count=GS3D_MAX_FRAMES;
        a.ticks_per_frame=1UL+(rand32()%65535UL);
        b.ticks_per_frame=1UL+(rand32()%65535UL);
        rc=gs3d_sync_phase(&a,rand32(),&phase);
        TEST(rc==GS3D_OK && phase>=0L&&phase<65536L);
        rc=gs3d_sync_sample(&a,rand32(),&b,0L,&out);
        TEST(rc==GS3D_OK);
        TEST(out.frame_a<b.count && out.frame_b<b.count);
        TEST(out.alpha_q16>=0L&&out.alpha_q16<=65536L);
    }
    a.count=2U;b.count=2U;
    b.first=65535U;
    TEST(gs3d_sync_sample(&a,0UL,&b,0L,&out)==GS3D_ERR_RANGE);
    return 0;
}
int main(void){
    TEST(mod_and_fuzz()==0);
    TEST(sync_fuzz()==0);
    puts("PHASE11 ROBUST PASS: 500 mutated model inputs + 9000 phase/tick cases");
    return 0;
}
