/* Source virtualmodel multi-bank console probe (C89).
   usage: probe base.mdl [bank1.mdl bank2.mdl bank3.mdl]
   Bank files must follow direct $includemodel declaration order. No files
   are discovered implicitly. For external ANI playback pass data through
   the library's API; this diagnostic tool only loads MDLs. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
#define MAX_FILE123 4194304u
static hl2m_u8 bytes[HL2M_VIRTUAL_GROUP_CAP123][MAX_FILE123];
static hl2m_mdl models[HL2M_VIRTUAL_GROUP_CAP123];
static unsigned int lengths[HL2M_VIRTUAL_GROUP_CAP123];
static hl2m_virtual_input123 inputs[HL2M_VIRTUAL_GROUP_CAP123];
static hl2m_virtual_multi123 catalog;
static hl2m_pose_matrices pose;
static hl2m_sequence_layers_report120 report;
static hl2m_fixed ctl[HL2M_VIRTUAL_POSE_CAP123];
static char paths[HL2M_VIRTUAL_INCLUDE_CAP122][HL2M_PATH_MAX];
static int read_file(const char *path,hl2m_u8 *buffer,unsigned int *length)
{
    FILE *f;long n;size_t actual;
    f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)){fclose(f);return 0;}
    n=ftell(f);
    if(n<0||n>(long)MAX_FILE123){fclose(f);return 0;}
    if(fseek(f,0,SEEK_SET)){fclose(f);return 0;}
    actual=fread(buffer,1u,(size_t)n,f);fclose(f);
    if(actual!=(size_t)n)return 0;
    *length=(unsigned int)n;return 1;
}
int main(int argc,char **argv)
{
    unsigned int i,expected=0u,sz=0u;int r,k,seen;
    if(argc<2||argc>5){
        fprintf(stderr,"usage: %s base.mdl [bank1.mdl bank2.mdl bank3.mdl]\n",argv[0]);
        return 2;
    }
    if(!read_file(argv[1],bytes[0],&lengths[0])){
        fprintf(stderr,"cannot open base %s\n",argv[1]);return 2;}
    r=hl2m_virtual_includes122(bytes[0],lengths[0],paths,
        HL2M_VIRTUAL_INCLUDE_CAP122,&expected);
    if(r){printf("base include header failed: %d\n",r);return 2;}
    printf("Declared $includemodel (%u):\n",expected);
    for(i=0u;i<expected;i++)printf("  [%u] %s\n",i+1u,paths[i]);
    if(expected!=(unsigned int)(argc-2)){
        printf("To test all banks, supply %u bank MDL file(s) in that order.\n",expected);
        return expected?3:0;
    }
    for(i=0u;i<(unsigned int)argc-1u;i++){
        if(i>0u&&!read_file(argv[i+1],bytes[i],&lengths[i])){
            fprintf(stderr,"cannot open bank %s\n",argv[i+1]);return 2;}
        sz=lengths[i];
        inputs[i].mdl_bytes=bytes[i];inputs[i].mdl_size=sz;
        inputs[i].ani_bytes=NULL;inputs[i].ani_size=0u;
        inputs[i].mdl=&models[i];
        inputs[i].include_path=i?paths[i-1u]:NULL;
        r=hl2m_decode_mdl(bytes[i],sz,&models[i]);
        if(r){printf("decode group %u failed: %d\n",i,r);return 2;}
    }
    r=hl2m_virtual_build_multi123(inputs,(unsigned int)(argc-1),&catalog);
    if(r){printf("multi-bank build error: %d\n",r);return 2;}
    printf("GROUPS %d SEQUENCES %d ANIMATIONS %d CONTROLS %d\n",
        catalog.group_count,catalog.seq_count,catalog.anim_count,catalog.pose_count);
    for(i=0u;i<(unsigned int)catalog.group_count;i++){
        printf("GROUP %u name=%s mapped=%d mismatched_parents=%d\n",i,
            models[i].name,catalog.groups[i].mapped_bones,
            catalog.groups[i].parent_conflicts);
    }
    for(i=0u;i<(unsigned int)catalog.pose_count;i++)ctl[i]=HL2M_FIXED_ONE/2;
    for(i=1u;i<(unsigned int)catalog.group_count;i++){
        seen=0;
        for(k=0;k<catalog.seq_count;k++){
            if(catalog.sequences[k].group!=(int)i)continue;
            if(seen++>=4)break;
            r=hl2m_master_virtual_sequence_multi123(&catalog,k,
                HL2M_FIXED_ONE/4,ctl,(unsigned int)catalog.pose_count,
                0,&pose,&report);
            printf("  group=%u virtual=%d '%s' sample=%d%s\n",i,k,
                models[i].sequences[catalog.sequences[k].local].label,r,
                r==0?" [POSE OK]":"");
        }
    }
    return 0;
}
