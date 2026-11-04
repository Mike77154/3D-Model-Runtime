/* v1.0.18 deterministic C89 tests for Source-like shortest path rotation. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static hl2m_pose_matrices pa,pb,po,pd;
static hl2m_fixed weights[HL2M_MAX_BONES];
static int close89(int a,int b,int tolerance){return abs(a-b)<=tolerance;}
static void ident(hl2m_mat3x4 *m){int i;memset(m,0,sizeof(*m));for(i=0;i<3;i++)m->m[i][i]=HL2M_FIXED_ONE;}
static int valid(const hl2m_mat3x4 *m){
 int a,b,c;long sum;
 for(a=0;a<3;a++){
   sum=0;
   for(b=0;b<3;b++){c=m->m[b][a]/128;sum+=c*c;}
   if(labs(sum-262144L)>14000L)return 0;
 }
 return 1;
}
int main(void){int r;
 hl2m_mat3x4 a,b,o,tmp;
 ident(&a);ident(&b);
 b.m[0][0]=-HL2M_FIXED_ONE;b.m[1][1]=-HL2M_FIXED_ONE;
 r=hl2m_quaternion_blend_matrix89(&a,&b,HL2M_FIXED_ONE/2,&o);
 if(r || !close89(o.m[0][0],0,700)|| !close89(o.m[1][1],0,700)||!valid(&o)){
   printf("180 degree midpoint FAIL %d x=%d y=%d\n",r,o.m[0][0],o.m[1][1]);return 1;
 }
 /* From +170 degrees to -170 degrees: shortest path passes 180, not 0. */
 ident(&a);ident(&b);
 a.m[0][0]=-64540;a.m[1][1]=-64540;
 a.m[0][1]=-11380;a.m[1][0]=11380;
 b.m[0][0]=-64540;b.m[1][1]=-64540;
 b.m[0][1]=11380;b.m[1][0]=-11380;
 r=hl2m_quaternion_blend_matrix89(&a,&b,HL2M_FIXED_ONE/2,&o);
 if(r || o.m[0][0]>-63000 || o.m[1][1]>-63000 || !valid(&o)){
   printf("short arc FAIL %d x=%d y=%d\n",r,o.m[0][0],o.m[1][1]);return 2;
 }
 r=hl2m_quaternion_blend_matrix89(&a,&b,0,&tmp);
 if(r||memcmp(&tmp,&a,sizeof(a))){puts("endpoint 0 FAIL");return 3;}
 r=hl2m_quaternion_blend_matrix89(&a,&b,HL2M_FIXED_ONE,&tmp);
 if(r||memcmp(&tmp,&b,sizeof(b))){puts("endpoint 1 FAIL");return 4;}
 ident(&a);ident(&b);b.m[0][0]=-65536;b.m[1][1]=-65536;
 b.m[0][3]=8*HL2M_FIXED_ONE;
 memset(&pa,0,sizeof(pa));memset(&pb,0,sizeof(pb));memset(&pd,0,sizeof(pd));
 pa.bone_count=pb.bone_count=pd.bone_count=1;
 pa.parent[0]=pb.parent[0]=pd.parent[0]=-1;
 pa.local[0]=a;pb.local[0]=b;ident(&pd.local[0]);
 r=hl2m_sequence_blend_local89(&pa,&pb,HL2M_FIXED_ONE/2,&po);
 if(r || !close89(po.local[0].m[0][3],4*HL2M_FIXED_ONE,8) || !valid(&po.local[0])){
   puts("sequence position/rotation FAIL");return 5;
 }
 pd.local[0]=b;pd.local[0].m[0][3]=0;
 weights[0]=HL2M_FIXED_ONE;
 r=hl2m_sequence_add_delta89(&pa,&pd,weights,HL2M_FIXED_ONE/2,&po);
 if(r || !valid(&po.local[0])||!close89(po.local[0].m[0][0],0,700)){
   puts("gesture quaternion blend FAIL");return 6;
 }
 r=hl2m_quaternion_blend_matrix89(&a,&b,-1,&o);
 if(r!=HL2M_ERR_RANGE){puts("invalid weight FAIL");return 7;}
 puts("quaternion118 shortest-path / endpoints / rigid / sequence / gesture PASS");return 0;
}
