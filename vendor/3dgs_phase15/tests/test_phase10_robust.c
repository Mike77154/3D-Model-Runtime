#include "../src/gs3d_internal.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){printf("PHASE10 robustness failed: %d\n",__LINE__);return 1;}} while(0)
static unsigned char payload[8192],tmp[8192];
static unsigned long seed=0x1357UL;
static unsigned long rand32(void){seed=seed*1664525UL+1013904223UL;return seed;}
static void exercise(void){
    GS3D_BoneMask mask;GS3D_Clip clip;GS3D_LayerRequest l;
    GS3D_RootDelta root;
    if(gs3d_model.format!=7||!gs3d_model.bones||!gs3d_model.groups)return;
    if(gs3d_mask_init(&mask,0L))return;
    (void)gs3d_mask_subtree(&mask,0U,GS3D_Q16_ONE);
    if(gs3d_model.group[0].frame_count<2U)return;
    if(gs3d_clip_create(&clip,7U,0U,0U,2U,100UL,0U,2U))return;
    (void)gs3d_root_motion_delta(&clip,0UL,50UL,0U,&root);
    memset(&l,0,sizeof(l));l.base=l.overlay=clip;
    l.overlay_weight_q16=GS3D_Q16_ONE;
    (void)gs3d_layer_apply(&l,&mask,0);
}
int main(void){
    FILE *f;unsigned long n,i,idx;int c;
    f=fopen("tests/fixtures/mdl7_skeletal_sequence.mdl","rb");
    CHECK(f!=0);n=0UL;
    while((c=fgetc(f))!=EOF && n<sizeof(payload))payload[n++]=(unsigned char)c;
    fclose(f);CHECK(n>100UL&&n<sizeof(payload));
    for(i=0UL;i<=n;i++){
        if(!gs3d_open_memory(payload,i))exercise();
        gs3d_close();
    }
    for(i=0UL;i<900UL;i++){
        memcpy(tmp,payload,(size_t)n);
        idx=rand32()%n;
        tmp[idx]^=(unsigned char)(1U<<(unsigned int)(rand32()%8UL));
        if(!gs3d_open_memory(tmp,n))exercise();
        gs3d_close();
    }
    puts("PHASE10 PASS: 661 truncations + 900 one-bit corruption cases");
    return 0;
}
