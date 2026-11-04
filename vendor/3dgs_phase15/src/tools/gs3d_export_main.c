#include "../../include/gs3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
int main(int argc,char **argv){
    int r;unsigned int i;unsigned long frame,fa,fb,alpha;char *end;
    if(argc<3){puts("usage: gs3d_export input.mdl output_prefix [frame_number]\n"
                    "  or: gs3d_export input.mdl output_prefix skeletal frameA frameB alpha_q16");return 2;}
    for(i=0;i<256U;i++) gs3d_palette[i*3U]=gs3d_palette[i*3U+1U]=
                              gs3d_palette[i*3U+2U]=(unsigned char)i;
    r=gs3d_open(argv[1]);if(r){printf("read error: %s\n",gs3d_error_name(r));return 1;}
    if(argc==7 && strcmp(argv[3],"skeletal")==0){
        fa=strtoul(argv[4],&end,10);
        if(!argv[4][0]||*end||fa>(unsigned long)UINT_MAX){puts("invalid frameA");return 2;}
        fb=strtoul(argv[5],&end,10);
        if(!argv[5][0]||*end||fb>(unsigned long)UINT_MAX){puts("invalid frameB");return 2;}
        alpha=strtoul(argv[6],&end,10);
        if(!argv[6][0]||*end||alpha>65536UL){puts("invalid Q16 alpha");return 2;}
        for(i=0U;i<gs3d_model.groups;i++){
            if(!gs3d_model.group[i].frame_count)continue;
            r=gs3d_sample_skeletal_q16(i,(unsigned int)fa,(unsigned int)fb,(long)alpha);
            if(r){printf("skeleton error group %u: %s\n",i,gs3d_error_name(r));gs3d_close();return 1;}
        }
    } else if(argc>3){
        frame=strtoul(argv[3],&end,10);
        if(!argv[3][0] || *end || frame>(unsigned long)UINT_MAX){
            puts("invalid frame number");gs3d_close();return 2;
        }
        for(i=0U;i<gs3d_model.groups;i++){
            r=gs3d_apply_deformer(i,(unsigned int)frame);
            if(r){printf("pose error in group %u: %s\n",i,gs3d_error_name(r));gs3d_close();return 1;}
        }
    }
    r=gs3d_export_obj(argv[2]);if(r){printf("export error: %s\n",gs3d_error_name(r));return 1;}
    puts("OBJ/MTL exported (TGA only for raster textures)");gs3d_close();return 0;
}
