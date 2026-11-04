/* Source 2013-inspired shortest-arc QuaternionSlerp / BlendBones.
 * Independent fixed-point C89 vendor. No float, double, heap, or int64.
 * Uses a 16-step CORDIC sine/cosine and quaternion-to-matrix conversion.
 * This is a numerical approximation to Valve's floating-point implementation.
 */
#include "hl2modelib.h"
#include <limits.h>

#define Q89_ONE HL2M_FIXED_ONE
#define Q89_HALF (HL2M_FIXED_ONE/2)
#define Q89_PI2 102944

typedef struct q89_quat_s { hl2m_fixed x,y,z,w; } q89_quat;

static hl2m_fixed q89_satadd(hl2m_fixed a,hl2m_fixed b){
 if(b>0 && a>INT_MAX-b)return INT_MAX;
 if(b<0 && a<INT_MIN-b)return INT_MIN;
 return a+b;
}
/* Q16 product, decomposed into 16-bit partial products; inputs bounded to ±4. */
static hl2m_fixed q89_mul(hl2m_fixed a,hl2m_fixed b){
 unsigned int u,v,hi,lo;int neg=0;
 if(a<0){u=(unsigned int)(-(a+1))+1u;neg=!neg;}else u=(unsigned int)a;
 if(b<0){v=(unsigned int)(-(b+1))+1u;neg=!neg;}else v=(unsigned int)b;
 hi=(u>>16)*(v>>16)*65536u+(u>>16)*(v&65535u)+(v>>16)*(u&65535u);
 lo=((u&65535u)*(v&65535u))>>16;
 return neg?-(hl2m_fixed)(hi+lo):(hl2m_fixed)(hi+lo);
}
static hl2m_fixed q89_div(hl2m_fixed a,hl2m_fixed b){
 unsigned int ua,ub,part,rem,frac=0;int i,neg=0;
 if(!b)return 0;
 if(a<0){ua=(unsigned int)(-(a+1))+1u;neg=!neg;}else ua=(unsigned int)a;
 if(b<0){ub=(unsigned int)(-(b+1))+1u;neg=!neg;}else ub=(unsigned int)b;
 part=ua/ub;rem=ua%ub;
 if(part>32767u)return neg?INT_MIN:INT_MAX;
 for(i=0;i<16;i++){
   /* remainder never exceeds Q16.16 denominator in these callers */
   rem=rem*2u;frac=frac*2u;
   if(rem>=ub){rem-=ub;frac|=1u;}
 }
 return neg?-(hl2m_fixed)((part<<16)|frac):(hl2m_fixed)((part<<16)|frac);
}
static hl2m_fixed q89_sqrt(hl2m_fixed x){
 hl2m_fixed l=0,h=2*Q89_ONE,m;
 if(x<=0)return 0;
 while(h-l>1){m=l+(h-l)/2;if(q89_mul(m,m)<=x)l=m;else h=m;}
 return l;
}
static hl2m_fixed q89_dot(q89_quat a,q89_quat b){
 return q89_satadd(q89_satadd(q89_mul(a.x,b.x),q89_mul(a.y,b.y)),
 q89_satadd(q89_mul(a.z,b.z),q89_mul(a.w,b.w)));
}
static q89_quat q89_normalize(q89_quat q){
 hl2m_fixed norm=q89_sqrt(q89_dot(q,q));
 if(norm<=32){q.x=q.y=q.z=0;q.w=Q89_ONE;return q;}
 q.x=q89_div(q.x,norm);q.y=q89_div(q.y,norm);
 q.z=q89_div(q.z,norm);q.w=q89_div(q.w,norm);return q;
}
/* CORDIC lookup: atan(2^-i) in Q16.16 radians. */
static const int q89_atan[16]={51472,30386,16055,8150,4091,2047,1024,512,
 256,128,64,32,16,8,4,2};
static void q89_sincos(int radians,hl2m_fixed *co,hl2m_fixed *si){
 int x=39797,y=0,z=radians,nx,ny,i;
 if(radians<=0){*co=Q89_ONE;*si=0;return;}
 if(radians>=Q89_PI2){*co=0;*si=Q89_ONE;return;}
 for(i=0;i<16;i++){
   if(z>=0){nx=x-y/(1<<i);ny=y+x/(1<<i);z-=q89_atan[i];}
   else {nx=x+y/(1<<i);ny=y-x/(1<<i);z+=q89_atan[i];}
   x=nx;y=ny;
 }
 *co=x;*si=y;
}
static int q89_acos(hl2m_fixed dot){
 int l=0,h=Q89_PI2,m,i;hl2m_fixed c,s;
 if(dot>=Q89_ONE)return 0;
 if(dot<=0)return Q89_PI2;
 for(i=0;i<17;i++){
   m=(l+h)/2;q89_sincos(m,&c,&s);
   if(c>dot)l=m;else h=m;
 }
 return (l+h)/2;
}
static q89_quat q89_from_matrix(const hl2m_mat3x4 *m){
 hl2m_fixed trace,s,a,b,c; q89_quat q;
 trace=m->m[0][0]+m->m[1][1]+m->m[2][2];
 if(trace>0){
   s=q89_sqrt(trace+Q89_ONE)*2;
   if(s<32){q.x=q.y=q.z=0;q.w=Q89_ONE;return q;}
   q.w=s/4;
   q.x=q89_div(m->m[2][1]-m->m[1][2],s);
   q.y=q89_div(m->m[0][2]-m->m[2][0],s);
   q.z=q89_div(m->m[1][0]-m->m[0][1],s);
 }else if(m->m[0][0]>=m->m[1][1] && m->m[0][0]>=m->m[2][2]){
   a=Q89_ONE+m->m[0][0]-m->m[1][1]-m->m[2][2];
   s=2*q89_sqrt(a);
   if(s<32){q.x=q.y=q.z=0;q.w=Q89_ONE;return q;}
   q.w=q89_div(m->m[2][1]-m->m[1][2],s);
   q.x=s/4;
   q.y=q89_div(m->m[0][1]+m->m[1][0],s);
   q.z=q89_div(m->m[0][2]+m->m[2][0],s);
 }else if(m->m[1][1]>=m->m[2][2]){
   b=Q89_ONE+m->m[1][1]-m->m[0][0]-m->m[2][2];
   s=2*q89_sqrt(b);
   if(s<32){q.x=q.y=q.z=0;q.w=Q89_ONE;return q;}
   q.w=q89_div(m->m[0][2]-m->m[2][0],s);
   q.x=q89_div(m->m[0][1]+m->m[1][0],s);
   q.y=s/4;
   q.z=q89_div(m->m[1][2]+m->m[2][1],s);
 }else{
   c=Q89_ONE+m->m[2][2]-m->m[0][0]-m->m[1][1];
   s=2*q89_sqrt(c);
   if(s<32){q.x=q.y=q.z=0;q.w=Q89_ONE;return q;}
   q.w=q89_div(m->m[1][0]-m->m[0][1],s);
   q.x=q89_div(m->m[0][2]+m->m[2][0],s);
   q.y=q89_div(m->m[1][2]+m->m[2][1],s);
   q.z=s/4;
 }
 return q89_normalize(q);
}
static void q89_to_matrix(q89_quat q,hl2m_mat3x4 *m){
 hl2m_fixed xx,yy,zz,xy,xz,yz,wx,wy,wz;
 q=q89_normalize(q);
 xx=q89_mul(q.x,q.x);yy=q89_mul(q.y,q.y);zz=q89_mul(q.z,q.z);
 xy=q89_mul(q.x,q.y);xz=q89_mul(q.x,q.z);yz=q89_mul(q.y,q.z);
 wx=q89_mul(q.w,q.x);wy=q89_mul(q.w,q.y);wz=q89_mul(q.w,q.z);
 m->m[0][0]=Q89_ONE-2*(yy+zz);m->m[0][1]=2*(xy-wz);m->m[0][2]=2*(xz+wy);
 m->m[1][0]=2*(xy+wz);m->m[1][1]=Q89_ONE-2*(xx+zz);m->m[1][2]=2*(yz-wx);
 m->m[2][0]=2*(xz-wy);m->m[2][1]=2*(yz+wx);m->m[2][2]=Q89_ONE-2*(xx+yy);
}
static q89_quat q89_slerp(q89_quat a,q89_quat b,hl2m_fixed t){
 hl2m_fixed dot,k0,k1,sin_theta,cos_theta,sin0,sin1,tmp;
 int theta;
 q89_quat out;
 a=q89_normalize(a);b=q89_normalize(b);
 dot=q89_dot(a,b);
 if(dot<0){b.x=-b.x;b.y=-b.y;b.z=-b.z;b.w=-b.w;dot=-dot;}
 if(dot>Q89_ONE)dot=Q89_ONE;
 if(dot>=65490){k0=Q89_ONE-t;k1=t;}
 else{
   theta=q89_acos(dot);
   q89_sincos(theta,&cos_theta,&sin_theta);
   if(sin_theta<128){k0=Q89_ONE-t;k1=t;}
   else{
     q89_sincos(q89_mul(theta,Q89_ONE-t),&tmp,&sin0);
     q89_sincos(q89_mul(theta,t),&tmp,&sin1);
     k0=q89_div(sin0,sin_theta);k1=q89_div(sin1,sin_theta);
   }
 }
 out.x=q89_satadd(q89_mul(a.x,k0),q89_mul(b.x,k1));
 out.y=q89_satadd(q89_mul(a.y,k0),q89_mul(b.y,k1));
 out.z=q89_satadd(q89_mul(a.z,k0),q89_mul(b.z,k1));
 out.w=q89_satadd(q89_mul(a.w,k0),q89_mul(b.w,k1));
 return q89_normalize(out);
}
/* Interpolates LOCAL rotation only. Translation remains linear. 
 * Matrices are assumed to be rigid input transforms (Source bone poses). */
int hl2m_quaternion_blend_matrix89(const hl2m_mat3x4 *a,
    const hl2m_mat3x4 *b,hl2m_fixed t,hl2m_mat3x4 *out){
 q89_quat qa,qb,qr;int i;
 if(!a||!b||!out)return HL2M_ERR_NULL;
 if(t<0||t>Q89_ONE)return HL2M_ERR_RANGE;
 if(t==0){*out=*a;return HL2M_OK;}
 if(t==Q89_ONE){*out=*b;return HL2M_OK;}
 qa=q89_from_matrix(a);qb=q89_from_matrix(b);
 qr=q89_slerp(qa,qb,t);
 q89_to_matrix(qr,out);
 for(i=0;i<3;i++)out->m[i][3]=q89_satadd(q89_mul(a->m[i][3],Q89_ONE-t),q89_mul(b->m[i][3],t));
 return HL2M_OK;
}
