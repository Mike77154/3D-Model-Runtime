#include "../../include/gs3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static int numeric(const char *s,unsigned long *v){
    char *end=0;
    if(!s||!*s||*s=='+'||*s=='-')return 0;
    *v=strtoul(s,&end,10);
    return s!=end&&*end=='\0';
}
int main(int argc,char **argv){
    GS3D_BoneMask mask;GS3D_LayerRequest req;GS3D_RootDelta delta;
    unsigned long v[12];unsigned int i;int r;
    if(argc!=15){
        puts("Usage: gs3d_layer model.mdl prefix gen group base_first base_count overlay_first overlay_count base_ticks overlay_ticks frame_ticks mask_bone layer_weight_q16 consume_root");
        puts("Root is bone 0. Both clips CLAMP; mask applies to subtree; exports one OBJ.");
        return 2;
    }
    if(strlen(argv[2])>380U)return 2;
    for(i=0U;i<12U;i++)if(!numeric(argv[i+3U],&v[i]))return 2;
    if(v[0]<4UL||v[0]>8UL||v[1]>65535UL||v[2]>65535UL||
       v[3]>65535UL||v[4]>65535UL||v[5]>65535UL||
       v[8]==0UL||v[9]>65535UL||v[10]>65536UL||v[11]>1UL)
       return 2;
    r=gs3d_open(argv[1]);if(r)return 1;
    memset(&req,0,sizeof(req));
    r=gs3d_clip_create(&req.base,(unsigned int)v[0],(unsigned int)v[1],
      (unsigned int)v[2],(unsigned int)v[3],v[8],GS3D_CLIP_CLAMP,GS3D_CLIP_TRACK_SKELETAL);
    if(r){gs3d_close();return 1;}
    r=gs3d_clip_create(&req.overlay,(unsigned int)v[0],(unsigned int)v[1],
      (unsigned int)v[4],(unsigned int)v[5],v[8],GS3D_CLIP_CLAMP,GS3D_CLIP_TRACK_SKELETAL);
    if(r){gs3d_close();return 1;}
    req.base_ticks=v[6];req.overlay_ticks=v[7];
    req.previous_base_ticks=0UL;
    req.overlay_weight_q16=(long)v[10];req.consume_root_motion=(unsigned char)v[11];
    req.root_bone=0U;
    r=gs3d_mask_init(&mask,0L);
    if(!r)r=gs3d_mask_subtree(&mask,(unsigned int)v[9],65536L);
    if(!r)r=gs3d_vendor_layer_apply((unsigned int)v[0],&req,&mask,&delta);
    if(!r)r=gs3d_export_obj(argv[2]);
    if(!r)printf("Root delta Q16.16=(%ld,%ld,%ld)\n",delta.delta_q16[0],delta.delta_q16[1],delta.delta_q16[2]);
    if(r)printf("gs3d_layer: %s (%d)\n",gs3d_error_name(r),r);
    gs3d_close();return r?1:0;
}
