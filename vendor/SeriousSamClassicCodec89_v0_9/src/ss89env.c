#include "ss89env.h"
/* Derivation from classic Serious Engine's documented reflection-vector
   and sphere-map convention; independent integer implementation. */
static unsigned ss89_isqrt(unsigned n)
{
    unsigned r=0U,bit=1U<<30;
    while(bit>n)bit>>=2;
    while(bit){if(n>=r+bit){n-=r+bit;r=(r>>1)+bit;}else r>>=1;bit>>=2;}
    return r;
}
static int bound(int x){return x<0?0:x>10000?10000:x;}
static int dot(int x,int y,int z,int a,int b,int c)
{
    /* Input vectors are normalized Q10000; 3*10000^2 fits signed 32 only
       when 3D clamp is applied. Perform each multiply /10000 first. */
    return (x*a)/10000+(y*b)/10000+(z*c)/10000;
}
int ss89_env_light(int nx,int ny,int nz,int lx,int ly,int lz)
{
    int d=dot(nx,ny,nz,lx,ly,lz);
    return d<0?0:d>10000?10000:d;
}
/* SE1 reflection: reflected viewer vector, X/Z sphere plane; -Y pole. */
int ss89_env_sphere_reflection(int nx,int ny,int nz,int vx,int vy,int vz,int *u,int *v)
{
    int d,rx,ry,rz,den;
    unsigned div;
    if(!u||!v)return 0;
    d=dot(nx,ny,nz,vx,vy,vz);
    rx=vx-2*(nx*d/10000);ry=vy-2*(ny*d/10000);rz=vz-2*(nz*d/10000);
    if(ry< -10000)ry=-10000;
    if(ry>10000)ry=10000;
    den=20000+2*ry;
    if(den<=1){*u=5000;*v=5000;return 1;}
    div=ss89_isqrt((unsigned)den*10000U);
    if(div<1U)div=1U;
    *u=bound(5000+(rx*5000)/(int)div);
    *v=bound(5000+(rz*5000)/(int)div);
    return 1;
}
/* SE1 specular: reflected light direction in view-space, X/Y sphere plane;
   -Z pole. This intentionally uses the engine's inverted reflect convention. */
int ss89_env_sphere_specular(int nx,int ny,int nz,int lx,int ly,int lz,int *u,int *v)
{
    int d,rx,ry,rz,den;
    unsigned div;
    if(!u||!v)return 0;
    d=dot(nx,ny,nz,lx,ly,lz);
    rx=lx-2*(nx*d/10000);ry=ly-2*(ny*d/10000);rz=lz-2*(nz*d/10000);
    if(rz< -10000)rz=-10000;
    if(rz>10000)rz=10000;
    den=20000+2*rz;
    if(den<=1){*u=5000;*v=5000;return 1;}
    div=ss89_isqrt((unsigned)den*10000U);
    if(div<1U)div=1U;
    *u=bound(5000+(rx*5000)/(int)div);
    *v=bound(5000+(ry*5000)/(int)div);
    return 1;
}
