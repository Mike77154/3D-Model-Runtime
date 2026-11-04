/* Self-contained numeric regression harness. Compile as strict C89. */
#include "bc_rig_q16.h"
#include <stdio.h>
#include <limits.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
  int out=0,p[3]={65536,0,0},v[3],loc[3]={131072,0,0};
  int scale[3]={65536,65536,65536},quat[4]={0,0,0,65536};
  BC_RigMat m,rest,pose,deform,world[2],local[2],palette[2];
  int par[2]={-1,0};
  BC_RigInfluence inf[2];
  CHECK(bc_qmul_checked(98304,-131072,&out)==0 && out==-196608);
  CHECK(bc_qmul_checked(INT_MIN,-65536,&out)!=0);
  CHECK(bc_qmul_checked(INT_MIN,65536,&out)==0 && out==INT_MIN);
  CHECK(bc_qdiv_checked(65536,65536,&out)==0 && out==65536);
  CHECK(bc_qdiv_checked(-98304,196608,&out)==0 && out==-32768);
  CHECK(bc_qdiv_checked(INT_MIN,-65536,&out)!=0);
  CHECK(bc_qdiv_checked(INT_MIN,65536,&out)==0 && out==INT_MIN);
  CHECK(bc_qdiv_checked(3,0,&out)!=0);
  CHECK(bc_rig_mat_from_trs(loc,quat,scale,&m)==0);
  CHECK(bc_rig_mat_point(&m,p,v)==0 && v[0]==65536 && v[1]==0 && v[2]==0);
  bc_rig_mat_identity(&rest);
  rest.m[3]=131072;
  bc_rig_mat_identity(&pose);
  pose.m[3]=196608;
  CHECK(bc_rig_deform_matrix(&pose,&rest,&deform)==0);
  CHECK(deform.m[3]==65536);
  CHECK(bc_rig_mat_point(&deform,p,v)==0 && v[0]==131072);
  bc_rig_mat_identity(&local[0]);local[0].m[3]=65536;
  bc_rig_mat_identity(&local[1]);local[1].m[7]=131072;
  CHECK(bc_rig_world(local,par,2U,world)==0);
  CHECK(world[1].m[3]==65536 && world[1].m[7]==131072);
  par[1]=1;
  CHECK(bc_rig_world(local,par,2U,world)==BC_RIG_UNSUPPORTED);
  par[1]=0;
  bc_rig_mat_identity(&palette[0]);palette[0].m[3]=262144;
  bc_rig_mat_identity(&palette[1]);
  inf[0].bone=0;inf[0].weight=16384;
  inf[1].bone=1;inf[1].weight=49152;
  CHECK(bc_rig_skin_point(palette,2U,p,inf,2U,v)==0);
  CHECK(v[0]==131072 && v[1]==0);
  inf[0].weight=65536;inf[1].weight=0;
  CHECK(bc_rig_skin_point(palette,2U,p,inf,2U,v)==0);
  CHECK(v[0]==327680);
  inf[1].bone=2;
  CHECK(bc_rig_skin_point(palette,2U,p,inf,2U,v)!=0);
  rest.m[0]=131072;
  CHECK(bc_rig_inverse_rigid(&rest,&m)==BC_RIG_UNSUPPORTED);
  puts("PASS: C89 Q16 arithmetic, quaternion TRS, inverse-rest, hierarchy, weighted skin, validation");
  return 0;
}
