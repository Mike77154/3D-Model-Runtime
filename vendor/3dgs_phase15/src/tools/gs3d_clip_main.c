#include "../../include/gs3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static int num(const char *s,unsigned long *out){
    char *end=0;
    if(!s||!*s||*s=='-'||*s=='+')return 0;
    *out=strtoul(s,&end,10);
    return end!=s && *end=='\0';
}
static void usage(void){
    puts("Usage: gs3d_clip model.mdl list [group]");
    puts("   or: gs3d_clip model.mdl export prefix gen group first count ticks_per_frame step samples mode track [all]");
    puts("  mode: 0=clamp, 1=loop, 2=pingpong; track: 0=auto,1=morph,2=skeletal");
    puts("  ticks and tick steps are caller-chosen units, NOT inferred model FPS.");
}
int main(int argc,char **argv){
    unsigned long x[9],ticks=0UL,j;unsigned int group,i;
    GS3D_Clip c;GS3D_ClipSample sample;char prefix[512];int r,all=0;
    if(argc<3){usage();return 2;}
    r=gs3d_open(argv[1]);if(r){printf("Error: %s\n",gs3d_error_name(r));return 1;}
    if(strcmp(argv[2],"list")==0){
        if(argc>4){usage();gs3d_close();return 2;}
        if(argc==4){if(!num(argv[3],&x[0])||x[0]>=gs3d_model.groups){usage();gs3d_close();return 2;}
             group=(unsigned int)x[0];}
        else group=0U;
        printf("3DGS MDL%d: %u group(s)\n",gs3d_model.format,gs3d_model.groups);
        for(i=0U;i<gs3d_model.group[group].frame_count;i++)
             printf("%u %s\n",i,gs3d_model.group[group].frames[i].name);
        gs3d_close();return 0;
    }
    if(strcmp(argv[2],"export")!=0||(argc!=13&&argc!=14)){
        usage();gs3d_close();return 2;
    }
    if(strlen(argv[3])>380U){puts("prefix too long");gs3d_close();return 2;}
    for(i=0U;i<9U;i++)if(!num(argv[i+4U],&x[i])){
        puts("invalid numeric argument");gs3d_close();return 2;
    }
    if(x[0]>UINT_MAX||x[1]>UINT_MAX||x[2]>UINT_MAX||x[3]>UINT_MAX||
       x[5]==0UL||x[6]==0UL||x[6]>1000UL||x[7]>2UL||x[8]>2UL){
        puts("numeric value out of allowed range");gs3d_close();return 2;
    }
    if(argc==14){if(strcmp(argv[13],"all")!=0){usage();gs3d_close();return 2;}all=1;}
    r=gs3d_clip_create(&c,(unsigned int)x[0],(unsigned int)x[1],
        (unsigned int)x[2],(unsigned int)x[3],x[4],(unsigned int)x[7],
        (unsigned int)x[8]);
    if(r){printf("clip create error: %s\n",gs3d_error_name(r));gs3d_close();return 1;}
    for(j=0UL;j<x[6];j++){
        r=all?gs3d_clip_apply_all(&c,ticks,&sample):gs3d_clip_apply(&c,ticks,&sample);
        if(r){printf("clip sample error at %lu: %s\n",j,gs3d_error_name(r));gs3d_close();return 1;}
        sprintf(prefix,"%s_%04lu",argv[3],j);
        r=gs3d_export_obj(prefix);
        if(r){printf("export error at %lu: %s\n",j,gs3d_error_name(r));gs3d_close();return 1;}
        printf("%s: t=%lu frames=%u,%u alpha=%ld\n",prefix,ticks,
                     sample.frame_a,sample.frame_b,sample.alpha_q16);
        if(j+1UL<x[6]){
            if(x[5]>ULONG_MAX-ticks){puts("tick overflow");gs3d_close();return 2;}
            ticks+=x[5];
        }
    }
    gs3d_close();return 0;
}
