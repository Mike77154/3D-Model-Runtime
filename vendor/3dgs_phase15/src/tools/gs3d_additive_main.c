#include "../../include/gs3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int number(const char *p,unsigned long *out){
    char *end;unsigned long v;
    if(!p||!*p||*p=='+'||*p=='-')return 0;
    v=strtoul(p,&end,10);
    if(p==end||*end!='\0')return 0;
    *out=v;return 1;
}
int main(int argc,char **argv){
    unsigned long v[7];unsigned int k;int r;
    GS3D_AdditiveRequest req;GS3D_BoneMask mask;
    unsigned int track;const GS3D_BoneMask *chosen_mask;
    if(argc!=10){
        puts("Usage: gs3d_additive model.mdl outprefix generation group base_frame add_frame ref_frame weight_q16 mask_subtree_root");
        puts("Example: gs3d_additive tests/fixtures/mdl7_skeletal_sequence.mdl pose 7 0 0 1 0 32768 1");
        puts("For MDL3/4/5 mask_subtree_root is ignored (morphs have no bones).");
        return 2;
    }
    if(strlen(argv[2])>380U)return 2;
    for(k=0U;k<7U;k++)if(!number(argv[3U+k],&v[k]))return 2;
    if(v[0]<4UL||v[0]>8UL||v[1]>65535UL||
       v[2]>65535UL||v[3]>65535UL||v[4]>65535UL||
       v[5]>65536UL||v[6]>65535UL)return 2;
    r=gs3d_open(argv[1]);if(r)return 1;
    track=gs3d_model.format==7 ? GS3D_CLIP_TRACK_SKELETAL : GS3D_CLIP_TRACK_MORPH;
    chosen_mask=0;
    memset(&req,0,sizeof(req));
    r=gs3d_clip_create(&req.base,(unsigned int)v[0],(unsigned int)v[1],
        (unsigned int)v[2],1U,1UL,GS3D_CLIP_CLAMP,track);
    if(!r)r=gs3d_clip_create(&req.additive,(unsigned int)v[0],(unsigned int)v[1],
        (unsigned int)v[3],1U,1UL,GS3D_CLIP_CLAMP,track);
    if(!r)r=gs3d_clip_create(&req.reference,(unsigned int)v[0],(unsigned int)v[1],
        (unsigned int)v[4],1U,1UL,GS3D_CLIP_CLAMP,track);
    if(!r && gs3d_model.format==7){
        r=gs3d_mask_init(&mask,0L);
        if(!r)r=gs3d_mask_subtree(&mask,(unsigned int)v[6],65536L);
        chosen_mask=&mask;
    }
    req.strength_q16=(long)v[5];
    if(!r)r=gs3d_vendor_additive_apply((unsigned int)v[0],&req,chosen_mask);
    if(!r)r=gs3d_export_obj(argv[2]);
    if(r)printf("gs3d_additive: %s (%d)\n",gs3d_error_name(r),r);
    else printf("Wrote %s.obj and .mtl\n",argv[2]);
    gs3d_close();return r?1:0;
}
