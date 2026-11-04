#include "../vendor_common/fixed_matrix.h"
#include <stdio.h>
static long diff(long a,long b){return a>b?a-b:b-a;}
static int near_identity(const long m[16]){
 int i;for(i=0;i<16;i++){long target=i%5==0?FX_ONE:0L;if(diff(m[i],target)>9L)return 0;}return 1;
}
int main(void){
 long m[16],inv[16],prod[16],v[3],u[3],round[3];int i;
 fx_identity(m);m[12]=3L*FX_ONE;m[13]=-2L*FX_ONE;m[14]=7L*FX_ONE;
 if(!fx_inverse_rigid(m,inv))return 1;
 fx_matrix_mul(m,inv,prod);if(!near_identity(prod))return 2;
 fx_identity(m);m[0]=0;m[1]=FX_ONE;m[4]=-FX_ONE;m[5]=0;
 m[12]=3L*FX_ONE;m[13]=-2L*FX_ONE;m[14]=FX_ONE;
 if(!fx_inverse_rigid(m,inv))return 3;
 fx_matrix_mul(m,inv,prod);if(!near_identity(prod))return 4;
 fx_matrix_mul(inv,m,prod);if(!near_identity(prod))return 5;
 v[0]=4L*FX_ONE;v[1]=5L*FX_ONE;v[2]=2L*FX_ONE;
 fx_transform_point(m,v,u);fx_transform_point(inv,u,round);
 for(i=0;i<3;i++)if(diff(v[i],round[i])>3L)return 6;
 m[0]=2L*FX_ONE;if(fx_inverse_rigid(m,inv))return 7;/* scale */
 m[0]=FX_ONE;m[1]=FX_ONE/2L;if(fx_inverse_rigid(m,inv))return 8;/* shear */
 m[1]=0;m[4]=FX_ONE;if(fx_inverse_rigid(m,inv))return 9;/* reflection */
 m[4]=-FX_ONE;m[3]=1;if(fx_inverse_rigid(m,inv))return 10;/* perspective */
 puts("R15 PASS: Q16 inverse rigid; products, roundtrip, invalid matrices");return 0;
}
