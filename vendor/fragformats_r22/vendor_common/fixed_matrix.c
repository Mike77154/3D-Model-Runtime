#include "fixed_matrix.h"
#define FX_MAX 2147483647L
#define FX_MIN (-2147483647L)
long fx_add(long a,long b){
 if(b>0L && a>FX_MAX-b)return FX_MAX;
 if(b<0L && a<FX_MIN-b)return FX_MIN;
 return a+b;
}
long fx_sub(long a,long b){
 if(b>0L && a<FX_MIN+b)return FX_MIN;
 if(b<0L && a>FX_MAX+b)return FX_MAX;
 return a-b;
}
/* (abs(a)*abs(b)) >> 16, evaluated as four 16-bit limbs.
 * All intermediate products and sums are <= 2^31-1, including on Win32.
 */
long fx_mul(long a,long b){
 unsigned long x,y,ah,al,bh,bl,p,q,z;
 int neg;
 if(!a||!b)return 0L;
 neg=(a<0L)!=(b<0L);
 x=(unsigned long)(a<0L?-a:a);y=(unsigned long)(b<0L?-b:b);
 ah=x>>16;al=x&65535UL;bh=y>>16;bl=y&65535UL;
 p=ah*bh;
 if(p>32767UL)return neg?FX_MIN:FX_MAX;
 z=p*65536UL;
 q=ah*bl;
 if(q>2147483647UL-z)return neg?FX_MIN:FX_MAX;
 z+=q;
 q=bh*al;
 if(q>2147483647UL-z)return neg?FX_MIN:FX_MAX;
 z+=q;
 q=(al*bl)>>16;
 if(q>2147483647UL-z)return neg?FX_MIN:FX_MAX;
 z+=q;
 return neg?-(long)z:(long)z;
}
long fx_lerp(long a,long b,long t){
 if(t<=0L)return a;
 if(t>=FX_ONE)return b;
 return fx_add(fx_mul(a,FX_ONE-t),fx_mul(b,t));
}
void fx_identity(long out[16]){int i;for(i=0;i<16;i++)out[i]=(i%5==0)?FX_ONE:0L;}
void fx_matrix_mul(const long a[16],const long b[16],long out[16]){
 long tmp[16],v;int i,j,k;
 for(i=0;i<4;i++)for(j=0;j<4;j++){
  v=0L;
  for(k=0;k<4;k++)v=fx_add(v,fx_mul(a[i*4+k],b[k*4+j]));
  tmp[i*4+j]=v;
 }
 for(i=0;i<16;i++)out[i]=tmp[i];
}
void fx_transform_point(const long m[16],const long v[3],long out[3]){
 long tmp[3];int j,k;
 for(j=0;j<3;j++){
  tmp[j]=m[12+j];
  for(k=0;k<3;k++)tmp[j]=fx_add(tmp[j],fx_mul(v[k],m[k*4+j]));
 }
 for(j=0;j<3;j++)out[j]=tmp[j];
}
int fx_affine(const long m[16]){
 return m[3]==0L && m[7]==0L && m[11]==0L && m[15]==FX_ONE;
}

/* R15: inverse for a proper rigid row-vector 4x4 matrix.
 * For p' = p R + t, p = p' R^T - t R^T.
 * No division, heap, floating point or 64-bit integers. Check before using. */
static long fx_abs_safe(long v){return v<0L?-v:v;}
int fx_inverse_rigid(const long m[16],long out[16]){
 long tmp[16],d,cofactor[3],det;int i,j,k;
 if(!fx_affine(m))return 0;
 for(i=0;i<3;i++)for(j=i;j<3;j++){
  long v=0L,expected=i==j?FX_ONE:0L;
  for(k=0;k<3;k++)v=fx_add(v,fx_mul(m[i*4+k],m[j*4+k]));
  if(fx_abs_safe(fx_sub(v,expected))>2048L)return 0;
 }
 cofactor[0]=fx_sub(fx_mul(m[5],m[10]),fx_mul(m[6],m[9]));
 cofactor[1]=fx_sub(fx_mul(m[4],m[10]),fx_mul(m[6],m[8]));
 cofactor[2]=fx_sub(fx_mul(m[4],m[9]),fx_mul(m[5],m[8]));
 d=fx_sub(fx_mul(m[0],cofactor[0]),fx_mul(m[1],cofactor[1]));
 det=fx_add(d,fx_mul(m[2],cofactor[2]));
 if(fx_abs_safe(fx_sub(det,FX_ONE))>4096L)return 0;
 fx_identity(tmp);
 for(i=0;i<3;i++)for(j=0;j<3;j++)tmp[i*4+j]=m[j*4+i];
 for(j=0;j<3;j++){
  long t=0L;
  for(k=0;k<3;k++)t=fx_add(t,fx_mul(m[12+k],tmp[k*4+j]));
  tmp[12+j]=fx_sub(0L,t);
 }
 for(i=0;i<16;i++)out[i]=tmp[i];
 return 1;
}

/* R16: signed Q16.16 division without 64-bit intermediates.
 * The remainder is represented modulo 2^32 so that doubling a value near
 * 2^31 behaves identically on 32-bit and 64-bit unsigned long hosts.
 * Truncates toward zero; saturates when the result cannot fit Q16.16. */
int fx_div_q16(long a,long b,long *out){
 unsigned long n,d,whole,rem,frac,bit,carry,mag;
 int neg,i;
 if(!out || b==0L || a>FX_MAX || a<FX_MIN || b>FX_MAX || b<FX_MIN)return 0;
 neg=(a<0L)!=(b<0L);
 n=(unsigned long)(a<0L?-a:a);
 d=(unsigned long)(b<0L?-b:b);
 whole=n/d;
 if(whole>32767UL)return 0;
 rem=n%d;frac=0UL;
 for(i=0;i<16;i++){
  carry=rem & 0x80000000UL;
  rem=(rem<<1)&0xffffffffUL;
  bit=(carry!=0UL || rem>=d)?1UL:0UL;
  if(bit)rem=(rem-d)&0xffffffffUL;
  frac=(frac<<1)|bit;
 }
 mag=whole*65536UL+frac;
 if(mag>2147483647UL)return 0;
 *out=neg?-(long)mag:(long)mag;
 return 1;
}

/* General 3x3 affine inverse, ROW-vector convention [A,0; t,1].
 * Adjugate / determinant in signed Q16.16. Reject near-singular matrices,
 * overflow and ill-conditioned inverses by checking BOTH round trips.
 * This supplements fx_inverse_rigid; it does not prove native MDL7 layout. */
int fx_inverse_affine(const long m[16],long out[16]){
 long c[9],det,t,inv[16],product[16];
 int i,j,k,pass;
 if(!out || !fx_affine(m))return 0;
 c[0]=fx_sub(fx_mul(m[5],m[10]),fx_mul(m[6],m[9]));
 c[1]=fx_sub(fx_mul(m[2],m[9]),fx_mul(m[1],m[10]));
 c[2]=fx_sub(fx_mul(m[1],m[6]),fx_mul(m[2],m[5]));
 c[3]=fx_sub(fx_mul(m[6],m[8]),fx_mul(m[4],m[10]));
 c[4]=fx_sub(fx_mul(m[0],m[10]),fx_mul(m[2],m[8]));
 c[5]=fx_sub(fx_mul(m[2],m[4]),fx_mul(m[0],m[6]));
 c[6]=fx_sub(fx_mul(m[4],m[9]),fx_mul(m[5],m[8]));
 c[7]=fx_sub(fx_mul(m[1],m[8]),fx_mul(m[0],m[9]));
 c[8]=fx_sub(fx_mul(m[0],m[5]),fx_mul(m[1],m[4]));
 det=fx_add(fx_add(fx_mul(m[0],c[0]),fx_mul(m[1],c[3])),fx_mul(m[2],c[6]));
 if(det>-256L && det<256L)return 0;
 fx_identity(inv);
 for(i=0;i<3;i++)for(j=0;j<3;j++){
  if(!fx_div_q16(c[i*3+j],det,&inv[i*4+j]))return 0;
 }
 for(i=0;i<3;i++){
  t=0L;
  for(k=0;k<3;k++)t=fx_add(t,fx_mul(m[12+k],inv[k*4+i]));
  inv[12+i]=fx_sub(0L,t);
 }
 for(pass=0;pass<2;pass++){
  if(pass==0)fx_matrix_mul(m,inv,product);
  else fx_matrix_mul(inv,m,product);
  for(i=0;i<16;i++){
   long expected=(i%5==0)?FX_ONE:0L;
   long diff=fx_sub(product[i],expected);
   if(diff>256L || diff< -256L)return 0;
  }
 }
 for(i=0;i<16;i++)out[i]=inv[i];
 return 1;
}
