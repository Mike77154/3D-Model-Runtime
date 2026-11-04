/* Integration test for real Source MDL: compare v119 and v120,
 * synthetic STUDIO_AL_POSE trigger, and circular autolayer prevention.
 */
#include "hl2m_master89.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CAP120 (24u*1024u*1024u)
static hl2m_u8 blob[CAP120];
static hl2m_mdl mdl;
static hl2m_pose_matrices before,legacy,modern,backup;
static hl2m_sequence_layers_report89 rep119;
static hl2m_sequence_layers_report120 rep120;
static void wr16(unsigned int at,unsigned int val)
{blob[at]=(hl2m_u8)val;blob[at+1]=(hl2m_u8)(val>>8);}
static void wr32(unsigned int at,unsigned int val)
{blob[at]=(hl2m_u8)val;blob[at+1]=(hl2m_u8)(val>>8);blob[at+2]=(hl2m_u8)(val>>16);blob[at+3]=(hl2m_u8)(val>>24);}
static unsigned int rd32(unsigned int at)
{return (unsigned int)blob[at]|((unsigned int)blob[at+1]<<8)|((unsigned int)blob[at+2]<<16)|((unsigned int)blob[at+3]<<24);}
static int same(const hl2m_pose_matrices *a,const hl2m_pose_matrices *b)
{
 int i,j,k;if(a->bone_count!=b->bone_count)return 0;
 for(i=0;i<a->bone_count;i++)for(j=0;j<3;j++)for(k=0;k<4;k++)
 if(a->local[i].m[j][k]!=b->local[i].m[j][k])return 0;
 return 1;
}
int main(int argc,char **argv)
{
 FILE *f;long sz;unsigned int seqtable,parent,child,layer,after;
 int seq,child_seq,r,i;hl2m_fixed control[1],cy=HL2M_FIXED_ONE/2;
 if(argc!=3){fprintf(stderr,"usage: %s model.mdl sequence-name\n",argv[0]);return 2;}
 f=fopen(argv[1],"rb");if(!f)return 3;
 if(fseek(f,0,SEEK_END)!=0 || (sz=ftell(f))<=0 ||
   (unsigned long)sz>CAP120-32u){fclose(f);return 4;}
 rewind(f);if(fread(blob,1,(size_t)sz,f)!=(size_t)sz){fclose(f);return 5;}fclose(f);
 r=hl2m_decode_mdl(blob,(unsigned int)sz,&mdl);if(r)return 6;
 seq=-1;
 for(i=0;i<mdl.sequence_count;i++)
    if(!strcmp(mdl.sequences[i].label,argv[2])){seq=i;break;}
 if(seq<0){puts("sequence not found");return 7;}
 seqtable=rd32(192);parent=seqtable+(unsigned int)seq*212u;
 if(rd32(parent+148u)<1u){puts("no layers in chosen sequence");return 8;}
 layer=parent+rd32(parent+152u);
 child_seq=(int)(short)(blob[layer]|((unsigned int)blob[layer+1]<<8));
 if(child_seq<0 || child_seq>=mdl.sequence_count)return 9;
 child=seqtable+(unsigned int)child_seq*212u;
 r=hl2m_master_sequence_pose89(blob,(unsigned int)sz,NULL,0,&mdl,seq,cy,0,0,&before,NULL);
 if(r)return 10;
 r=hl2m_master_sequence_layered89(blob,(unsigned int)sz,NULL,0,&mdl,seq,cy,0,0,&legacy,&rep119);
 if(r)return 11;
 r=hl2m_master_sequence_layers120(blob,(unsigned int)sz,NULL,0,&mdl,seq,cy,0,0,
     NULL,0,&modern,&rep120);
 if(r || !same(&modern,&legacy)){
     printf("FAIL v119 parity %d applied %d\n",r,rep120.applied);return 12;
 }
 printf("REAL %s seq=%s bones=%d v119/v120 exact-matrix parity PASS layers=%d\n",
     argv[1],argv[2],modern.bone_count,rep120.applied);
 /* Simulate one POSE-driven autolayer in real Birkin: weight changes from
    zero at pose parameter 1.0 to full at 0.5, while cycle stays 0.5. */
 wr32(layer+4u,rd32(layer+4u)|HL2M_SEQ_LAYER_POSE89);
 wr16(layer+2u,0);
 wr32(layer+8u,0u);wr32(layer+12u,0x3f000000u);
 wr32(layer+16u,0x3f000000u);wr32(layer+20u,0x3f800000u);
 control[0]=HL2M_FIXED_ONE;
 r=hl2m_master_sequence_layers120(blob,(unsigned int)sz,NULL,0,&mdl,seq,cy,0,0,
     control,1,&modern,&rep120);
 if(r || rep120.applied!=0 || !same(&modern,&before)){
     printf("FAIL zero-weight physical pose r=%d applied=%d\n",r,rep120.applied);return 13;
 }
 control[0]=HL2M_FIXED_ONE/2;
 r=hl2m_master_sequence_layers120(blob,(unsigned int)sz,NULL,0,&mdl,seq,cy,0,0,
     control,1,&modern,&rep120);
 if(r || rep120.pose_driven!=1 || !same(&modern,&legacy)){
     printf("FAIL pose-driven at peak r=%d pose=%d\n",r,rep120.pose_driven);return 14;
 }
 puts("SYNTHETIC STUDIO_AL_POSE changes layer weight at constant cycle PASS");
 /* Restore parent layer; child acquires an autolayer pointing BACK to parent.
    We append 24 bytes of valid descriptor and assert bounded cycle detection. */
 wr32(layer+4u,rd32(layer+4u)&~(unsigned int)HL2M_SEQ_LAYER_POSE89);
 after=(unsigned int)sz;
 wr32(child+148u,1u);wr32(child+152u,after-child);
 memset(blob+after,0,24u);wr16(after,(unsigned int)seq);
 /* A healthy two-level tree: add_eyeball -> slow_idle. */
 wr16(after,1u);
 r=hl2m_master_sequence_layers120(blob,after+24u,NULL,0,&mdl,seq,cy,0,0,
     NULL,0,&modern,&rep120);
 if(r || rep120.applied!=2 || rep120.nested_applied!=1 || rep120.max_depth<2){
     printf("FAIL healthy nested sequence r=%d applied=%d nested=%d depth=%d\n",
         r,rep120.applied,rep120.nested_applied,rep120.max_depth);return 15;
 }
 puts("SYNTHETIC two-level Source autolayer tree PASS");
 wr16(after,(unsigned int)seq);
 backup=legacy;
 modern=backup;
 r=hl2m_master_sequence_layers120(blob,after+24u,NULL,0,&mdl,seq,cy,0,0,
     NULL,0,&modern,&rep120);
 if(r!=HL2M_ERR_UNSUPPORTED || !rep120.cycle_detected || !same(&modern,&backup)){
     printf("FAIL circular layer r=%d cycle=%d\n",r,rep120.cycle_detected);return 16;
 }
 (void)backup;
 puts("SYNTHETIC cyclic Source autolayer reference rejected PASS");
 return 0;
}
