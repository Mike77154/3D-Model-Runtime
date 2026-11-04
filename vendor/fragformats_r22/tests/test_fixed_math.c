/* C89 smoke + edge tests independent of parser, no dynamic allocations. */
#include "../vendor_common/fixed_matrix.h"
#include "../vendor_common/skin_fixed.h"
#include <stdio.h>
static int equal3(const long a[3],long x,long y,long z){return a[0]==x&&a[1]==y&&a[2]==z;}
int main(void){
 long parent[16],child[16],global[16],out[3],p[3]={65536L,65536L,65536L};
 static long mats[3][16];
 unsigned char ids[4]={0,1,0,0};
 long weights[4]={32768L,32768L,0L,0L};int k;
 fx_identity(parent);fx_identity(child);
 parent[12]=4L*FX_ONE;parent[13]=5L*FX_ONE;parent[14]=6L*FX_ONE;
 child[0]=2L*FX_ONE;child[5]=3L*FX_ONE;child[10]=4L*FX_ONE;
 child[12]=FX_ONE;child[13]=2L*FX_ONE;
 fx_matrix_mul(child,parent,global);fx_transform_point(global,p,out);
 if(!equal3(out,7L*FX_ONE,10L*FX_ONE,10L*FX_ONE))return 1;
 if(fx_mul(-32768L,65536L)!=-32768L || fx_mul(2147483647L,2147483647L)!=2147483647L)return 2;
 if(fx_lerp(FX_ONE,5L*FX_ONE,32768L)!=3L*FX_ONE)return 3;
 for(k=0;k<3;k++)fx_identity(mats[k]);
 mats[0][12]=2L*FX_ONE;mats[1][12]=4L*FX_ONE;
 if(!skin_blend_point(p,(const long (*)[16])mats,3UL,ids,weights,out))return 4;
 if(!equal3(out,4L*FX_ONE,FX_ONE,FX_ONE))return 5;
 ids[1]=15;
 if(skin_blend_point(p,(const long (*)[16])mats,3UL,ids,weights,out))return 6;
 weights[1]=32767L;ids[1]=1;
 if(!skin_blend_point(p,(const long (*)[16])mats,3UL,ids,weights,out))return 7;
 puts("PASS: Q16 matrix hierarchy, saturation, interpolation and 4-weight LBS sanity");
 return 0;
}
