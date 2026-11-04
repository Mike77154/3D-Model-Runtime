#include "../vendor_common/fixed_quat.h"
#include "../vendor_common/fixed_matrix.h"
#include <stdio.h>
static long absq(long x){return x<0L?-x:x;}
static int near(long x,long y,long t){return absq(x-y)<=t;}
int main(void){
 long a[16],b[16],out[16],v[3],p[3];
 fx_identity(a);fx_identity(b);
 /* Row-vector Z rotation +90 degrees. */
 b[0]=0;b[1]=FX_ONE;b[4]=-FX_ONE;b[5]=0;
 if(!fxq_blend_rigid(a,b,32768L,out))return 1;
 v[0]=FX_ONE;v[1]=0;v[2]=0;
 fx_transform_point(out,v,p);
 if(!near(p[0],46341L,32L)||!near(p[1],46341L,32L)||p[2]!=0)return 2;
 if(!near(fx_add(fx_mul(p[0],p[0]),fx_mul(p[1],p[1])),FX_ONE,80L))return 3;
 if(!fxq_blend_rigid(a,b,0L,out)||!near(out[0],FX_ONE,30L))return 4;
 if(!fxq_blend_rigid(a,b,FX_ONE,out)||!near(out[1],FX_ONE,30L))return 5;
 /* A reflection, non-uniform scale and shear are not rotations. */
 b[0]=-FX_ONE;if(fxq_blend_rigid(a,b,32768L,out))return 6;
 b[0]=2L*FX_ONE;if(fxq_blend_rigid(a,b,32768L,out))return 7;
 fx_identity(b);b[1]=FX_ONE/3L;if(fxq_blend_rigid(a,b,32768L,out))return 8;
 fx_identity(b);b[3]=1L;if(fxq_blend_rigid(a,b,32768L,out))return 9;
 puts("R14 PASS: quaternion NLERP produces a unit-length 45 degree rotation; invalid matrices rejected");
 return 0;
}
