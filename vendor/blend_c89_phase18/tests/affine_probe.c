/* C89-only affine inverse test harness, 16 Q16.16 integer matrix entries/line. */
#include "bc_rig_q16.h"
#include <stdio.h>
int main(void)
{
  BC_RigMat m,inv,product;
  unsigned int i;
  int status;
  for(;;){
    for(i=0U;i<16U;i++){
      status=scanf("%d",&m.m[i]);
      if(status==EOF && i==0U)return 0;
      if(status!=1)return 2;
    }
    if(bc_rig_inverse_affine(&m,&inv)!=BC_RIG_OK ||
       bc_rig_mat_mul(&m,&inv,&product)!=BC_RIG_OK)return 3;
    for(i=0U;i<16U;i++){
      if(printf("%d%s",product.m[i],i==15U?"\n":" ")<0)return 4;
    }
  }
}
