#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
static hl2m_pose_matrices a,b,out,delta;
static hl2m_fixed weights[HL2M_MAX_BONES];
int main(void){int r;
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));memset(&delta,0,sizeof(delta));
 a.bone_count=b.bone_count=delta.bone_count=1;
 a.parent[0]=b.parent[0]=delta.parent[0]=-1;
 a.local[0].m[0][0]=a.local[0].m[1][1]=a.local[0].m[2][2]=HL2M_FIXED_ONE;
 b.local[0].m[0][0]=b.local[0].m[1][1]=b.local[0].m[2][2]=HL2M_FIXED_ONE;
 delta.local[0].m[0][0]=delta.local[0].m[1][1]=delta.local[0].m[2][2]=HL2M_FIXED_ONE;
 b.local[0].m[0][3]=8*HL2M_FIXED_ONE;
 r=hl2m_sequence_blend_local89(&a,&b,HL2M_FIXED_ONE/2,&out);
 if(r || out.local[0].m[0][3]!=4*HL2M_FIXED_ONE){printf("frame fractional FAIL %d %d\n",r,out.local[0].m[0][3]);return 1;}
 weights[0]=HL2M_FIXED_ONE;
 delta.local[0].m[1][3]=6*HL2M_FIXED_ONE;
 r=hl2m_master_gesture89(&out,&delta,weights,HL2M_FIXED_ONE/2,&out);
 if(r || out.local[0].m[1][3]!=3*HL2M_FIXED_ONE){printf("additive gesture FAIL %d\n",r);return 2;}
 r=hl2m_sequence_blend_local89(&a,&b,-1,&out);
 if(r!=HL2M_ERR_RANGE){printf("bad weight accepted %d\n",r);return 3;}
 puts("sequence116 synthetic interpolation, gesture and invalid weight PASS");return 0;}
