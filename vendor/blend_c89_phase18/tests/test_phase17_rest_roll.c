/* Direct C89 kernel test: original rest roll is carried into IK pose. */
#include "bc_ik2.h"
#include <stdio.h>

static int near(int x,int y,int epsilon)
{
  int difference;
  if(bc_qadd_checked(x,-y,&difference)!=0)return 0;
  return difference<=epsilon &&difference>=-epsilon;
}
static void frame(BC_RigMat *r,int roll,int child)
{
  bc_rig_mat_identity(r);
  if(!roll){
    r->m[0]=0;r->m[1]=-65536;r->m[2]=0;
    r->m[4]=65536;r->m[5]=0;r->m[6]=0;
    r->m[8]=0;r->m[9]=0;r->m[10]=65536;
  }else{
    r->m[0]=0;r->m[1]=-65536;r->m[2]=0;
    r->m[4]=0;r->m[5]=0;r->m[6]=65536;
    r->m[8]=-65536;r->m[9]=0;r->m[10]=0;
  }
  r->m[3]=child?-65536:0;
}
int main(void)
{
  BC_IK2Goal goal;
  BC_IK2Result a,b;
  BC_RigMat rest0[2],rest1[2];
  int head[6]={0,0,0,-65536,0,0};
  int tail[6]={-65536,0,0,-131072,0,0};
  int tip[3]={-131072,0,0},actual0[3],actual1[3];
  unsigned int k;
  for(k=0U;k<3U;k++){
    goal.root[k]=0;goal.target[k]=0;goal.pole[k]=0;
  }
  goal.root[0]=3*65536;
  goal.target[0]=4*65536;goal.target[1]=65536;
  goal.pole[0]=3*65536;goal.pole[1]=65536;goal.pole[2]=65536;
  goal.length[0]=65536;goal.length[1]=65536;
  frame(&rest0[0],0,0);frame(&rest0[1],0,1);
  frame(&rest1[0],1,0);frame(&rest1[1],1,1);
  if(bc_ik2_solve_rest(&goal,rest0,head,tail,&a)!=0 ||
     bc_ik2_solve_rest(&goal,rest1,head,tail,&b)!=0 ||
     bc_rig_mat_point(&a.deform[1],tip,actual0)!=0 ||
     bc_rig_mat_point(&b.deform[1],tip,actual1)!=0){
    fputs("FAIL solving rest roll\n",stderr);return 1;
  }
  for(k=0U;k<3U;k++){
    if(!near(actual0[k],goal.target[k],400) ||
       !near(actual1[k],goal.target[k],400)){
      fputs("FAIL endpoint\n",stderr);return 1;
    }
  }
  /* Swing must be the same physical deformation for either rest roll. */
  for(k=0U;k<16U;k++){
    if(!near(a.deform[1].m[k],b.deform[1].m[k],400)){
      fprintf(stderr,"FAIL roll contamination in deformation [%u]: %d vs %d\n",k,a.deform[1].m[k],b.deform[1].m[k]);return 1;
    }
  }
  /* But the posed local X axes themselves must keep their original roll. */
  if(near(a.pose[1].m[0],b.pose[1].m[0],1000) &&
     near(a.pose[1].m[4],b.pose[1].m[4],1000) &&
     near(a.pose[1].m[8],b.pose[1].m[8],1000)){
    fputs("FAIL roll lost\n",stderr);return 1;
  }
  puts("PASS: C89 local rest roll preserved in pose, rest-independent swing deformation");
  return 0;
}
