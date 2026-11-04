#include "hl2m_master89.h"
#include <string.h>
#include <stdio.h>
static hl2m_pose_matrices a,b,out;
static hl2m_fixed weights[HL2M_MAX_BONES];
static void id(hl2m_mat3x4 *m,int x)
{memset(m,0,sizeof(*m));m->m[0][0]=m->m[1][1]=m->m[2][2]=HL2M_FIXED_ONE;m->m[0][3]=x*HL2M_FIXED_ONE;}
int main(void)
{
 int r,i;hl2m_seq_layer89 l;hl2m_fixed cy,w;hl2m_mdl_seqdesc seq;hl2m_seq_metadata89 meta;
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));memset(&out,0,sizeof(out));
 a.bone_count=b.bone_count=2;
 for(i=0;i<2;i++){
    a.parent[i]=b.parent[i]=i-1;
    id(&a.local[i],0);id(&b.local[i],10);
    id(&a.world[i],0);id(&b.world[i],10);
    weights[i]=i==0?0:HL2M_FIXED_ONE;
 }
 r=hl2m_master_layer_mask89(&a,&b,weights,HL2M_FIXED_ONE/2,0,&out);
 if(r || out.local[0].m[0][3]!=0 || out.local[1].m[0][3]!=5*HL2M_FIXED_ONE){puts("FAIL masked blend");return 1;}
 id(&b.local[1],2);
 r=hl2m_master_layer_mask89(&a,&b,weights,HL2M_FIXED_ONE/2,HL2M_SEQ_DELTA89|HL2M_SEQ_POST89,&out);
 if(r || out.local[1].m[0][3]!=HL2M_FIXED_ONE){puts("FAIL delta post");return 2;}
 memset(&l,0,sizeof(l));l.start=0;l.peak=HL2M_FIXED_ONE/4;l.tail=3*HL2M_FIXED_ONE/4;l.end=HL2M_FIXED_ONE;
 r=hl2m_sequence_layer_envelope89(&l,HL2M_FIXED_ONE/8,HL2M_FIXED_ONE,&cy,&w);
 if(r || cy!=HL2M_FIXED_ONE/8 || w<HL2M_FIXED_ONE/2-3){puts("FAIL layer ramp");return 3;}
 l.flags=HL2M_SEQ_LAYER_POSE89;
 r=hl2m_sequence_layer_envelope89(&l,0,HL2M_FIXED_ONE,&cy,&w);
 if(r!=HL2M_ERR_UNSUPPORTED){puts("FAIL unbound pose layer accepted");return 4;}
 memset(&seq,0,sizeof(seq));memset(&meta,0,sizeof(meta));
 seq.groupsize[0]=3;seq.paramstart[0]=-HL2M_FIXED_ONE;seq.paramend[0]=HL2M_FIXED_ONE;
 r=hl2m_sequence_pose_axis89(&seq,&meta,0,0,&w);
 if(r || w<HL2M_FIXED_ONE/2-2 || w>HL2M_FIXED_ONE/2+2){puts("FAIL physical pose parameter");return 5;}
 meta.has_posekeys=1;meta.posekeys[0][0]=-HL2M_FIXED_ONE;
 meta.posekeys[0][1]=0;meta.posekeys[0][2]=2*HL2M_FIXED_ONE;
 r=hl2m_sequence_pose_axis89(&seq,&meta,0,HL2M_FIXED_ONE,&w);
 if(r || w<49150 || w>49155){printf("FAIL pose keys %d\n",w);return 6;}
 puts("sequence119: per-bone mask, delta post, layer envelope, physical pose and posekeys PASS");return 0;
}
