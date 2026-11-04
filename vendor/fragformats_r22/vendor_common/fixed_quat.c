#include "fixed_quat.h"
#include "fixed_matrix.h"
#define QMAX 2147483647L
#define QEPS 2048L
static long qabs(long a){return a<0L?-a:a;}
static long qdot3(const long m[16],int a,int b){
 int k;long result=0L;
 for(k=0;k<3;k++)result=fx_add(result,fx_mul(m[a*4+k],m[b*4+k]));
 return result;
}
static long qdet(const long m[16]){
 long x=fx_sub(fx_mul(m[5],m[10]),fx_mul(m[6],m[9]));
 long y=fx_sub(fx_mul(m[4],m[10]),fx_mul(m[6],m[8]));
 long z=fx_sub(fx_mul(m[4],m[9]),fx_mul(m[5],m[8]));
 return fx_add(fx_sub(fx_mul(m[0],x),fx_mul(m[1],y)),fx_mul(m[2],z));
}
static int qrigid(const long m[16]){
 int i,j;
 if(!fx_affine(m))return 0;
 for(i=0;i<3;i++)for(j=i;j<3;j++){
  long v=qdot3(m,i,j),want=i==j?FX_ONE:0L;
  if(qabs(fx_sub(v,want))>QEPS)return 0;
 }
 return qabs(fx_sub(qdet(m),FX_ONE))<=4096L;
}
/* Binary-search sqrt(value in Q16.16); use bounded fx_mul, not 64-bit. */
static long qsqrt(long x){
 long lo=0L,hi=262144L,mid;
 if(x<=0L)return 0L;
 while(lo<hi){mid=lo+(hi-lo+1L)/2L;
  if(fx_mul(mid,mid)<=x)lo=mid;else hi=mid-1L;
 }
 return lo;
}
/* Signed Q16.16 divide; input magnitudes must be <= 2^20.
 * Fractional 16-bit long division avoids intermediate 64-bit products. */
static long qdiv(long a,long b){
 unsigned long x,y,whole,rem,frac;int i,neg;
 if(!b)return 0L;
 neg=(a<0L)!=(b<0L);
 x=(unsigned long)qabs(a);y=(unsigned long)qabs(b);
 if(x>1048576UL||y>1048576UL)return neg?-QMAX:QMAX;
 whole=x/y;
 if(whole>32767UL)return neg?-QMAX:QMAX;
 rem=x%y;frac=0UL;
 for(i=0;i<16;i++){
  rem<<=1;frac<<=1;
  if(rem>=y){rem-=y;frac|=1UL;}
 }
 {long ans=(long)(whole*65536UL+frac);return neg?-ans:ans;}
}
static int qfrom(const long m[16],long q[4]){
 long t,s;
 if(!qrigid(m))return 0;
 t=fx_add(fx_add(m[0],m[5]),m[10]);
 if(t>0L){
  s=fx_add(qsqrt(fx_add(FX_ONE,t)),qsqrt(fx_add(FX_ONE,t)));
  if(s<1L)return 0;
  q[3]=s/4L;
  q[0]=qdiv(fx_sub(m[6],m[9]),s);
  q[1]=qdiv(fx_sub(m[8],m[2]),s);
  q[2]=qdiv(fx_sub(m[1],m[4]),s);
 }else if(m[0]>m[5]&&m[0]>m[10]){
  t=fx_sub(fx_sub(fx_add(FX_ONE,m[0]),m[5]),m[10]);
  s=2L*qsqrt(t);if(s<1L)return 0;
  q[0]=s/4L;
  q[3]=qdiv(fx_sub(m[6],m[9]),s);
  q[1]=qdiv(fx_add(m[1],m[4]),s);
  q[2]=qdiv(fx_add(m[2],m[8]),s);
 }else if(m[5]>m[10]){
  t=fx_sub(fx_sub(fx_add(FX_ONE,m[5]),m[0]),m[10]);
  s=2L*qsqrt(t);if(s<1L)return 0;
  q[1]=s/4L;
  q[3]=qdiv(fx_sub(m[8],m[2]),s);
  q[0]=qdiv(fx_add(m[1],m[4]),s);
  q[2]=qdiv(fx_add(m[6],m[9]),s);
 }else{
  t=fx_sub(fx_sub(fx_add(FX_ONE,m[10]),m[0]),m[5]);
  s=2L*qsqrt(t);if(s<1L)return 0;
  q[2]=s/4L;
  q[3]=qdiv(fx_sub(m[1],m[4]),s);
  q[0]=qdiv(fx_add(m[2],m[8]),s);
  q[1]=qdiv(fx_add(m[6],m[9]),s);
 }
 return 1;
}
static void qmatrix(const long q[4],long m[16]){
 long x=q[0],y=q[1],z=q[2],w=q[3];long xx,yy,zz,xy,xz,yz,xw,yw,zw;
 xx=fx_mul(x,x);yy=fx_mul(y,y);zz=fx_mul(z,z);
 xy=fx_mul(x,y);xz=fx_mul(x,z);yz=fx_mul(y,z);
 xw=fx_mul(x,w);yw=fx_mul(y,w);zw=fx_mul(z,w);
 fx_identity(m);
 m[0]=fx_sub(FX_ONE,fx_add(fx_add(yy,yy),fx_add(zz,zz)));
 m[1]=fx_add(fx_add(xy,xy),fx_add(zw,zw));
 m[2]=fx_sub(fx_add(xz,xz),fx_add(yw,yw));
 m[4]=fx_sub(fx_add(xy,xy),fx_add(zw,zw));
 m[5]=fx_sub(FX_ONE,fx_add(fx_add(xx,xx),fx_add(zz,zz)));
 m[6]=fx_add(fx_add(yz,yz),fx_add(xw,xw));
 m[8]=fx_add(fx_add(xz,xz),fx_add(yw,yw));
 m[9]=fx_sub(fx_add(yz,yz),fx_add(xw,xw));
 m[10]=fx_sub(FX_ONE,fx_add(fx_add(xx,xx),fx_add(yy,yy)));
}
int fxq_blend_rigid(const long a[16],const long b[16],long t,long out[16]){
 long qa[4],qb[4],q[4],len,dot=0L,tmp[16];int i;
 if(t<0L||t>FX_ONE)return 0;
 if(!qfrom(a,qa)||!qfrom(b,qb))return 0;
 if(t==0L||t==FX_ONE){
  const long *src=t==0L?a:b;
  for(i=0;i<16;i++)out[i]=src[i];
  return 1;
 }
 for(i=0;i<4;i++)dot=fx_add(dot,fx_mul(qa[i],qb[i]));
 if(dot<0L)for(i=0;i<4;i++)qb[i]=-qb[i];
 for(i=0;i<4;i++)q[i]=fx_lerp(qa[i],qb[i],t);
 len=0L;for(i=0;i<4;i++)len=fx_add(len,fx_mul(q[i],q[i]));
 len=qsqrt(len);if(len<16384L)return 0;
 for(i=0;i<4;i++)q[i]=qdiv(q[i],len);
 qmatrix(q,tmp);
 for(i=0;i<3;i++)tmp[12+i]=fx_lerp(a[12+i],b[12+i],t);
 for(i=0;i<16;i++)out[i]=tmp[i];
 return 1;
}

int fxq_face_normal(const long a[3],const long b[3],const long c[3],long out[3]){
 long u[3],v[3],n[3],l;int i;
 for(i=0;i<3;i++){u[i]=fx_sub(b[i],a[i]);v[i]=fx_sub(c[i],a[i]);}
 n[0]=fx_sub(fx_mul(u[1],v[2]),fx_mul(u[2],v[1]));
 n[1]=fx_sub(fx_mul(u[2],v[0]),fx_mul(u[0],v[2]));
 n[2]=fx_sub(fx_mul(u[0],v[1]),fx_mul(u[1],v[0]));
 /* Avoid Q16.16 normalization overflow by uniform power-of-two scaling. */
 while(qabs(n[0])>FX_ONE || qabs(n[1])>FX_ONE || qabs(n[2])>FX_ONE){
  for(i=0;i<3;i++)n[i]/=2L;
 }
 l=fx_add(fx_add(fx_mul(n[0],n[0]),fx_mul(n[1],n[1])),fx_mul(n[2],n[2]));
 l=qsqrt(l);if(l<1L)return 0;
 for(i=0;i<3;i++)out[i]=qdiv(n[i],l);
 return 1;
}

/* R16: normalize a signed Q16.16 vector with bounded 32-bit intermediates.
 * Repeated power-of-two scaling preserves direction and avoids overflow. */
int fxq_normalize_vec3(const long input[3],long out[3]){
 long n[3],len;int i;
 for(i=0;i<3;i++)n[i]=input[i];
 while(qabs(n[0])>FX_ONE || qabs(n[1])>FX_ONE || qabs(n[2])>FX_ONE){
  for(i=0;i<3;i++)n[i]/=2L;
 }
 len=fx_add(fx_add(fx_mul(n[0],n[0]),fx_mul(n[1],n[1])),fx_mul(n[2],n[2]));
 len=qsqrt(len);if(len<1L)return 0;
 for(i=0;i<3;i++)out[i]=qdiv(n[i],len);
 return 1;
}
