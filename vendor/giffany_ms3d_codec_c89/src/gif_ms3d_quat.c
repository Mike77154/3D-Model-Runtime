#include "gif_ms3d_quat.h"

#define Q_PI 205887L
#define Q_HALF_PI 102944L
#define Q_TWO_PI 411775L
static const long q_atan[16]={51472L,30386L,16055L,8149L,4091L,
    2047L,1024L,512L,256L,128L,64L,32L,16L,8L,4L,2L};
static void q_sincos(ms3d_fx angle, ms3d_fx *s, ms3d_fx *c)
{
    long x,y,z,dx,dy,denom;
    int i,neg;
    z=angle%Q_TWO_PI;
    if(z>Q_PI)z-=Q_TWO_PI;
    if(z< -Q_PI)z+=Q_TWO_PI;
    neg=0;
    if(z>Q_HALF_PI){z-=Q_PI;neg=1;}
    else if(z< -Q_HALF_PI){z+=Q_PI;neg=1;}
    x=39797L;y=0L;denom=1L;
    for(i=0;i<16;++i){
        dx=x/denom;dy=y/denom;
        if(z>=0L){x-=dy;y+=dx;z-=q_atan[i];}
        else{x+=dy;y-=dx;z+=q_atan[i];}
        denom*=2L;
    }
    *s=neg?-y:y;*c=neg?-x:x;
}
static unsigned long q_isqrt(unsigned long a)
{
    unsigned long bit=1UL<<30,root=0UL;
    while(bit>a)bit>>=2;
    while(bit){
        if(a>=root+bit){a-=root+bit;root=(root>>1)+bit;}
        else root>>=1;
        bit>>=2;
    }
    return root;
}
static void q_normalize(ms3d_quat *q)
{
    long v[4],absmax=0L,t;
    unsigned long sum=0UL,root;
    int i;
    v[0]=q->x;v[1]=q->y;v[2]=q->z;v[3]=q->w;
    for(i=0;i<4;++i){
        t=v[i]<0L?-v[i]:v[i];
        if(t>absmax)absmax=t;
    }
    if(absmax==0L){q->x=q->y=q->z=0L;q->w=MS3D_FX_ONE;return;}
    /* Divide by 256 before squaring: max input Q16.16 quaternion
       is <= 1.0, so 4 * 256^2 fits safely in 32 bits. */
    for(i=0;i<4;++i){v[i]/=256L;sum+=(unsigned long)(v[i]*v[i]);}
    root=q_isqrt(sum);
    if(root==0UL){q->x=q->y=q->z=0L;q->w=MS3D_FX_ONE;return;}
    /* Correct integer-quantization by calculating output from the source
       components, scaled by 256 / norm (norm is in Q8.8 units). */
    q->x=(ms3d_fx)((q->x*256L)/(long)root);
    q->y=(ms3d_fx)((q->y*256L)/(long)root);
    q->z=(ms3d_fx)((q->z*256L)/(long)root);
    q->w=(ms3d_fx)((q->w*256L)/(long)root);
}
void ms3d_quat_from_euler(const ms3d_fx r[3],ms3d_quat *q)
{
    ms3d_fx sx,cx,sy,cy,sz,cz;
    ms3d_fx a,b,c,d,e,f;
    if(!r||!q)return;
    q_sincos(r[0]/2L,&sx,&cx);
    q_sincos(r[1]/2L,&sy,&cy);
    q_sincos(r[2]/2L,&sz,&cz);
    /* q = qz*qy*qx */
    a=ms3d_fx_mul(cz,cy);b=ms3d_fx_mul(sz,sy);
    c=ms3d_fx_mul(sz,cy);d=ms3d_fx_mul(cz,sy);
    e=ms3d_fx_mul(a,cx);f=ms3d_fx_mul(b,sx);
    q->w=ms3d_fx_add_sat(e,f);
    q->x=ms3d_fx_sub_sat(ms3d_fx_mul(a,sx),ms3d_fx_mul(b,cx));
    q->y=ms3d_fx_add_sat(ms3d_fx_mul(d,cx),ms3d_fx_mul(c,sx));
    q->z=ms3d_fx_sub_sat(ms3d_fx_mul(c,cx),ms3d_fx_mul(d,sx));
    q_normalize(q);
}
void ms3d_quat_nlerp(const ms3d_quat *a,const ms3d_quat *b,ms3d_fx alpha,ms3d_quat *out)
{
    ms3d_fx dot;
    ms3d_quat rhs;
    if(!a||!b||!out)return;
    if(alpha<0L)alpha=0L;
    if(alpha>MS3D_FX_ONE)alpha=MS3D_FX_ONE;
    dot=ms3d_fx_add_sat(ms3d_fx_add_sat(ms3d_fx_mul(a->x,b->x),ms3d_fx_mul(a->y,b->y)),
        ms3d_fx_add_sat(ms3d_fx_mul(a->z,b->z),ms3d_fx_mul(a->w,b->w)));
    rhs=*b;
    if(dot<0L){rhs.x=-rhs.x;rhs.y=-rhs.y;rhs.z=-rhs.z;rhs.w=-rhs.w;}
    out->x=ms3d_fx_lerp(a->x,rhs.x,alpha);
    out->y=ms3d_fx_lerp(a->y,rhs.y,alpha);
    out->z=ms3d_fx_lerp(a->z,rhs.z,alpha);
    out->w=ms3d_fx_lerp(a->w,rhs.w,alpha);
    q_normalize(out);
}
/* Q16.16 inverse cosine for [0, 1] via monotonic binary search.
 * Shortest-hemisphere SLERP therefore needs only [0,pi/2]. */
static ms3d_fx q_acos_nonnegative(ms3d_fx cosine)
{
    long lo=0L,hi=Q_HALF_PI,mid,ss,cc;
    int i;
    if(cosine>=MS3D_FX_ONE)return 0L;
    if(cosine<=0L)return Q_HALF_PI;
    for(i=0;i<18;++i){
        mid=lo+(hi-lo)/2L;
        q_sincos(mid,&ss,&cc);
        if(cc>cosine)lo=mid;
        else hi=mid;
        if(hi-lo<=1L)break;
    }
    return lo+(hi-lo)/2L;
}
void ms3d_quat_slerp(const ms3d_quat *a,const ms3d_quat *b,ms3d_fx alpha,ms3d_quat *out)
{
    ms3d_fx dot,theta,si,co,w0,w1;
    ms3d_quat rhs;
    if(!a||!b||!out)return;
    if(alpha<=0L){*out=*a;return;}
    if(alpha>=MS3D_FX_ONE){*out=*b;return;}
    dot=ms3d_fx_add_sat(ms3d_fx_add_sat(ms3d_fx_mul(a->x,b->x),
        ms3d_fx_mul(a->y,b->y)),ms3d_fx_add_sat(ms3d_fx_mul(a->z,b->z),
        ms3d_fx_mul(a->w,b->w)));
    rhs=*b;
    if(dot<0L){
        dot=-dot;
        rhs.x=-rhs.x;rhs.y=-rhs.y;rhs.z=-rhs.z;rhs.w=-rhs.w;
    }
    if(dot>MS3D_FX_ONE)dot=MS3D_FX_ONE;
    /* Trig division near angle 0 is numerically unstable in fixed point. */
    if(dot>=65400L){ms3d_quat_nlerp(a,&rhs,alpha,out);return;}
    theta=q_acos_nonnegative(dot);
    q_sincos(theta,&si,&co);
    (void)co;
    if(si<256L){ms3d_quat_nlerp(a,&rhs,alpha,out);return;}
    q_sincos(ms3d_fx_mul(theta,MS3D_FX_ONE-alpha),&w0,&co);
    q_sincos(ms3d_fx_mul(theta,alpha),&w1,&co);
    w0=ms3d_fx_div(w0,si);w1=ms3d_fx_div(w1,si);
    out->x=ms3d_fx_add_sat(ms3d_fx_mul(a->x,w0),ms3d_fx_mul(rhs.x,w1));
    out->y=ms3d_fx_add_sat(ms3d_fx_mul(a->y,w0),ms3d_fx_mul(rhs.y,w1));
    out->z=ms3d_fx_add_sat(ms3d_fx_mul(a->z,w0),ms3d_fx_mul(rhs.z,w1));
    out->w=ms3d_fx_add_sat(ms3d_fx_mul(a->w,w0),ms3d_fx_mul(rhs.w,w1));
    q_normalize(out);
}
void ms3d_quat_to_affine(const ms3d_quat *q,ms3d_affine *out)
{
    ms3d_fx xx,yy,zz,xy,xz,yz,wx,wy,wz;
    if(!q||!out)return;
    ms3d_affine_identity(out);
    xx=ms3d_fx_mul(q->x,q->x);yy=ms3d_fx_mul(q->y,q->y);
    zz=ms3d_fx_mul(q->z,q->z);xy=ms3d_fx_mul(q->x,q->y);
    xz=ms3d_fx_mul(q->x,q->z);yz=ms3d_fx_mul(q->y,q->z);
    wx=ms3d_fx_mul(q->w,q->x);wy=ms3d_fx_mul(q->w,q->y);
    wz=ms3d_fx_mul(q->w,q->z);
    out->m[0][0]=MS3D_FX_ONE-2L*(yy+zz);
    out->m[0][1]=2L*(xy-wz);
    out->m[0][2]=2L*(xz+wy);
    out->m[1][0]=2L*(xy+wz);
    out->m[1][1]=MS3D_FX_ONE-2L*(xx+zz);
    out->m[1][2]=2L*(yz-wx);
    out->m[2][0]=2L*(xz-wy);
    out->m[2][1]=2L*(yz+wx);
    out->m[2][2]=MS3D_FX_ONE-2L*(xx+yy);
}
