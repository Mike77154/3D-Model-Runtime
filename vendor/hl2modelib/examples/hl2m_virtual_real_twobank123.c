/* Controlled two-bank test: inject one additional include path into a COPY
   of a real Resident Evil MDL. Both banks reference the real supplied
   mbczom_anims bytes; rename a single parsed sequence in second metadata.
   Not an unmodified original Source pair. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LIMIT 4194304u
static hl2m_u8 b[2][LIMIT];
static hl2m_mdl m[3];
static hl2m_virtual_input123 inputs[3];
static hl2m_virtual_multi123 ctx;
static hl2m_pose_matrices pose;
static hl2m_sequence_layers_report120 report;
static hl2m_fixed ctrls[HL2M_VIRTUAL_POSE_CAP123];
static int load(const char *name,hl2m_u8 *buf,unsigned int *size)
{
    FILE *f=fopen(name,"rb");long l;
    if(!f)return 0;
    fseek(f,0,SEEK_END);l=ftell(f);fseek(f,0,SEEK_SET);
    if(l<=0||l>(long)(LIMIT-256u)){fclose(f);return 0;}
    if(fread(buf,1u,(size_t)l,f)!=(size_t)l){fclose(f);return 0;}
    fclose(f);*size=(unsigned int)l;return 1;
}
static void u32(hl2m_u8 *p,unsigned int v)
{p[0]=(hl2m_u8)v;p[1]=(hl2m_u8)(v>>8);p[2]=(hl2m_u8)(v>>16);p[3]=(hl2m_u8)(v>>24);}
int main(int argc,char **argv)
{
    unsigned int bs=0u,cs=0u,off;int r,i,k=-1;
    if(argc!=3||!load(argv[1],b[0],&bs)||!load(argv[2],b[1],&cs))return 2;
    if(hl2m_decode_mdl(b[0],bs,&m[0])||hl2m_decode_mdl(b[1],cs,&m[1]))return 2;
    m[2]=m[1];strcpy(m[2].sequences[0].label,"CloneIdle123");
    off=bs;
    u32(b[0]+336,2u);u32(b[0]+340,off);
    u32(b[0]+off+4u,16u);u32(b[0]+off+12u,96u);
    strcpy((char *)b[0]+off+16u,"models/mbczombies/mbczom_anims.mdl");
    strcpy((char *)b[0]+off+104u,"models/clone_bank123.mdl");
    inputs[0].mdl_bytes=b[0];inputs[0].mdl_size=bs+160u;
    inputs[0].ani_bytes=NULL;inputs[0].ani_size=0u;
    inputs[0].mdl=&m[0];inputs[0].include_path=NULL;
    for(i=1;i<=2;i++){
        inputs[i].mdl_bytes=b[1];inputs[i].mdl_size=cs;
        inputs[i].mdl=&m[i];inputs[i].ani_bytes=NULL;
        inputs[i].ani_size=0u;
        inputs[i].include_path=i==1?"models/mbczombies/mbczom_anims.mdl":
            "models/clone_bank123.mdl";
    }
    r=hl2m_virtual_build_multi123(inputs,3u,&ctx);
    if(r){printf("BUILD ERROR %d\n",r);return 1;}
    for(i=0;i<ctx.seq_count;i++)
        if(ctx.sequences[i].group==2){k=(int)i;break;}
    for(i=0;i<ctx.pose_count;i++)ctrls[i]=HL2M_FIXED_ONE/2;
    if(k<0)return 1;
    r=hl2m_master_virtual_sequence_multi123(&ctx,k,HL2M_FIXED_ONE/4,
       ctrls,(unsigned int)ctx.pose_count,0,&pose,&report);
    printf("CONTROLLED_REAL123: 3 groups, %d seq, group2 virtual %d, "
        "sample %d, bones %d\n",ctx.seq_count,k,r,pose.bone_count);
    return r?1:0;
}
