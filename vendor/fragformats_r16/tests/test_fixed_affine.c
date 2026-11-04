#include "../vendor_common/fixed_matrix.h"
#include <stdio.h>
#define CHECK(v) do{if(!(v)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#v);return 1;}}while(0)
static long absl(long a){return a<0L?-a:a;}
static int near(long a,long b,long epsilon){return absl(fx_sub(a,b))<=epsilon;}
static int matrix_ident(const long m[16]){
 int i;for(i=0;i<16;i++)if(!near(m[i],i%5==0?FX_ONE:0L,256L))return 0;
 return 1;
}
int main(void){
 long m[16],inv[16],old[16],p[16],v[3],r[3],back[3],q;
 int i;
 CHECK(fx_div_q16(65536L,131072L,&q) && q==32768L);
 CHECK(fx_div_q16(-65536L,131072L,&q) && q==-32768L);
 CHECK(fx_div_q16(98304L,65536L,&q) && q==98304L);
 CHECK(!fx_div_q16(65536L,0L,&q));
 CHECK(!fx_div_q16(2147483647L,1L,&q));
 CHECK(fx_div_q16(65536L,2147483647L,&q) && q==2L);
 fx_identity(m);
 m[0]=131072L;m[5]=32768L;m[10]=98304L;
 m[1]=16384L; m[4]=8192L; m[9]=-16384L; /* scale+shear */
 m[12]=196608L;m[13]=-131072L;m[14]=32768L;
 CHECK(fx_inverse_affine(m,inv));
 fx_matrix_mul(m,inv,p);CHECK(matrix_ident(p));
 fx_matrix_mul(inv,m,p);CHECK(matrix_ident(p));
 v[0]=65536L;v[1]=196608L;v[2]=-65536L;
 fx_transform_point(m,v,r);fx_transform_point(inv,r,back);
 for(i=0;i<3;i++)CHECK(near(back[i],v[i],256L));
 fx_identity(m);m[0]=0L;m[1]=65536L;m[4]=-65536L;m[5]=0L;
 CHECK(fx_inverse_affine(m,inv));
 CHECK(fx_inverse_rigid(m,old));
 for(i=0;i<16;i++)CHECK(near(inv[i],old[i],256L));
 fx_identity(m);m[0]=0L;
 CHECK(!fx_inverse_affine(m,inv));
 fx_identity(m);m[0]=16L;
 CHECK(!fx_inverse_affine(m,inv));
 fx_identity(m);m[3]=1L;
 CHECK(!fx_inverse_affine(m,inv));
 fx_identity(m);m[0]=-65536L; /* reflection valid for general affine inversion */
 CHECK(fx_inverse_affine(m,inv));
 CHECK(!fx_inverse_rigid(m,old));
 puts("R16 PASS: 32-bit Q16 affine inverse, shear/scale/reflection, two-sided identity, round trip, singular rejection");
 return 0;
}
