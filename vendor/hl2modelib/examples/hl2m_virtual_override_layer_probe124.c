/* Controlled Source corpus regression: deliberately insert a forward
   declaration and a cross-bank layer IN MEMORY. Originals remain unchanged.
   Requires reo_hunter.mdl anim_hunter.mdl anim_hunter.ani. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BASE_CAP (1024u*1024u)
#define BANK_CAP (2u*1024u*1024u)
static hl2m_u8 base_data[BASE_CAP],bank_data[BANK_CAP],ani_data[BANK_CAP];
static hl2m_mdl base_mdl,bank_mdl;
static hl2m_virtual_input123 inputs[2];
static hl2m_virtual_multi123 cat;
static hl2m_virtual_graph124 graph;
static hl2m_pose_matrices original,layered;
static hl2m_sequence_layers_report120 report;
static hl2m_fixed controls[HL2M_VIRTUAL_POSE_CAP123];
static void w32(hl2m_u8 *p,unsigned int off,unsigned int v)
{p[off]=(hl2m_u8)v;p[off+1]=(hl2m_u8)(v>>8);p[off+2]=(hl2m_u8)(v>>16);p[off+3]=(hl2m_u8)(v>>24);}
static void w16(hl2m_u8 *p,unsigned int off,unsigned int v)
{p[off]=(hl2m_u8)v;p[off+1]=(hl2m_u8)(v>>8);}
static unsigned int r32(hl2m_u8 *p,unsigned int off)
{return (unsigned int)p[off]|((unsigned int)p[off+1]<<8)|
    ((unsigned int)p[off+2]<<16)|((unsigned int)p[off+3]<<24);}
static int load(const char *file,hl2m_u8 *dst,unsigned int cap,unsigned int *size)
{
 FILE *f=fopen(file,"rb");long n;
 if(!f)return 0;
 if(fseek(f,0,SEEK_END)|| (n=ftell(f))<0||(unsigned long)n>cap || fseek(f,0,SEEK_SET))
 {fclose(f);return 0;}
 if(fread(dst,1u,(size_t)n,f)!=(size_t)n){fclose(f);return 0;}
 fclose(f);*size=(unsigned int)n;return 1;
}
int main(int argc,char **argv)
{
 unsigned int nb,nc,na,seqbase,record,layer;
 int r,v,owner,seq;
 if(argc!=4){fprintf(stderr,"usage: %s reo_hunter.mdl anim_hunter.mdl anim_hunter.ani\n",argv[0]);return 2;}
 if(!load(argv[1],base_data,BASE_CAP-64u,&nb)||
    !load(argv[2],bank_data,BANK_CAP,&nc)||
    !load(argv[3],ani_data,BANK_CAP,&na))return 2;
 if(hl2m_decode_mdl(base_data,nb,&base_mdl)||
    hl2m_decode_mdl(bank_data,nc,&bank_mdl))return 2;
 if(base_mdl.sequence_count<2 || bank_mdl.sequence_count<63)return 2;
 /* Declare bank's Pounce_01 at local index 0 using Source override rule. */
 strcpy(base_mdl.sequences[0].label,"Pounce_01");
 base_mdl.sequences[0].flags|=0x0800;
 /* Add a synthetic autolayer to real base ragdoll. No original MDL edited. */
 seqbase=r32(base_data,192);record=seqbase+212u;
 layer=nb+4u;
 w32(base_data,record+148u,1u);
 w32(base_data,record+152u,layer-record);
 w16(base_data,layer,0u);w16(base_data,layer+2u,0u);
 w32(base_data,layer+4u,0u); /* full-cycle layer: neutral 0 points */
 memset(base_data+layer+8u,0,16u);nb=layer+24u;
 inputs[0].include_path=NULL;
 inputs[0].mdl_bytes=base_data;inputs[0].mdl_size=nb;inputs[0].mdl=&base_mdl;
 inputs[1].include_path="models/infected/anim_hunter.mdl";
 inputs[1].mdl_bytes=bank_data;inputs[1].mdl_size=nc;
 inputs[1].ani_bytes=ani_data;inputs[1].ani_size=na;inputs[1].mdl=&bank_mdl;
 r=hl2m_virtual_build_recursive124(inputs,2u,&cat,&graph);if(r){printf("build=%d\n",r);return 3;}
 seq=-1;
 for(v=0;v<cat.seq_count;v++){
  owner=cat.sequences[v].group;
  if(owner==0&&cat.sequences[v].local==1){seq=v;break;}
 }
 if(seq<0)return 3;
 for(v=0;v<cat.pose_count;v++)controls[v]=HL2M_FIXED_ONE/2;
 r=hl2m_master_virtual_sequence_multi123(&cat,seq,HL2M_FIXED_ONE/4,
    controls,(unsigned int)cat.pose_count,0,&original,&report);
 if(r){printf("base=%d\n",r);return 4;}
 r=hl2m_master_virtual_sequence_layers124(&cat,seq,HL2M_FIXED_ONE/4,
    controls,(unsigned int)cat.pose_count,&layered,&report);
 if(r){printf("layer=%d\n",r);return 5;}
 if(report.declared<1||report.applied<1||original.bone_count!=layered.bone_count)
     return 6;
 printf("OVERRIDE124 PASS: bank override, real Pounce_01 as cross-group layer, base_bones=%d applied=%d changed=%s\n",
    layered.bone_count,report.applied,
    memcmp(&original,&layered,sizeof(layered))?"YES":"NO");
 return memcmp(&original,&layered,sizeof(layered))?0:7;
}
