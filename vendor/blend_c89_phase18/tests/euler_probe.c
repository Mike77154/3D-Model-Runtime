#include "bc_rig_q16.h"
#include <stdio.h>
int main(void)
{
  unsigned int i;
  int order,angles[3],q[4];
  for(i=0U;i<1000U;i++){
    if(scanf("%d %d %d %d",&order,&angles[0],&angles[1],&angles[2])!=4)break;
    if(bc_rig_euler_quat(angles,(unsigned int)order,q)!=0)return 2;
    printf("%d %d %d %d\n",q[0],q[1],q[2],q[3]);
  }
  return 0;
}
