#include "../../include/gs3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static int numeric(const char *s,unsigned long *v){
    char *end=0;
    if(!s||!*s||*s=='+'||*s=='-')return 0;
    *v=strtoul(s,&end,10);
    return s!=end && *end=='\0';
}
static int fail(int r){printf("error: %s (%d)\n",gs3d_error_name(r),r);return 1;}
int main(int argc,char **argv){
    unsigned long p[10];unsigned long i,ticks,period,step;
    unsigned int k;GS3D_Clip a,b;GS3D_Crossfade fade;
    GS3D_BlendSample sample;char name[512];int r,all=0;
    if(argc!=13 && argc!=14){
        puts("Usage: gs3d_blend model.mdl prefix gen group from_first from_count to_first to_count ticks_per_frame fade_ticks samples track [all]");
        puts("track: 0=auto, 1=morph, 2=skeletal; both clips use loop mode");
        return 2;
    }
    if(strlen(argv[2])>380U){puts("prefix too long");return 2;}
    if(argc==14){if(strcmp(argv[13],"all")!=0)return 2;all=1;}
    for(k=0U;k<10U;k++)if(!numeric(argv[3U+k],&p[k])){
        puts("Invalid numeric input");return 2;
    }
    if(p[0]>8UL||p[0]<4UL||p[1]>UINT_MAX||p[2]>UINT_MAX||p[3]>UINT_MAX||
       p[4]>UINT_MAX||p[5]>UINT_MAX||!p[6]||!p[7]||p[8]<2UL||p[8]>1001UL||p[9]>2UL){
        puts("Arguments outside supported range");return 2;
    }
    r=gs3d_open(argv[1]);if(r)return fail(r);
    r=gs3d_clip_create(&a,(unsigned int)p[0],(unsigned int)p[1],
            (unsigned int)p[2],(unsigned int)p[3],p[6],GS3D_CLIP_LOOP,(unsigned int)p[9]);
    if(r){gs3d_close();return fail(r);}
    r=gs3d_clip_create(&b,(unsigned int)p[0],(unsigned int)p[1],
            (unsigned int)p[4],(unsigned int)p[5],p[6],GS3D_CLIP_LOOP,(unsigned int)p[9]);
    if(r){gs3d_close();return fail(r);}
    r=gs3d_crossfade_init(&fade,&a,&b,0UL,0UL,p[7]);
    if(r){gs3d_close();return fail(r);}
    period=p[8]-1UL;
    for(i=0UL;i<p[8];i++){
        /* Avoid fade_ticks*i overflow; use quotient/remainder decomposition. */
        step=p[7]/period;
        ticks=step*i;
        if(p[7]%period && i>(ULONG_MAX-ticks)/(p[7]%period)){
            gs3d_close();return 2;
        }
        ticks+=((p[7]%period)*i)/period;
        r=gs3d_crossfade_seek(&fade,ticks,ticks,ticks);if(r)break;
        r=all?gs3d_crossfade_apply_all(&fade,&sample):
            gs3d_vendor_crossfade_apply((unsigned int)p[0],&fade,&sample);
        if(r)break;
        sprintf(name,"%s_%04lu",argv[2],i);
        r=gs3d_export_obj(name);if(r)break;
        printf("%s: t=%lu weight=%ld/65536 from=%u,%u to=%u,%u\n",
            name,ticks,sample.weight_q16,sample.from.frame_a,
            sample.from.frame_b,sample.to.frame_a,sample.to.frame_b);
    }
    gs3d_close();
    return r?fail(r):0;
}
