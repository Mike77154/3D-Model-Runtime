#include "hlfp_perspective89.h"
#include <limits.h>
#include <string.h>
static int p89_add(hlfp_fixed a,hlfp_fixed b,hlfp_fixed *o){
    if((b>0 && a>INT_MAX-b)||(b<0 && a<INT_MIN-b)) return HLFP_ERR_OVERFLOW;
    *o=a+b;return HLFP_OK;
}
static int p89_sub(hlfp_fixed a,hlfp_fixed b,hlfp_fixed *o){
    if((b<0 && a>INT_MAX+b)||(b>0 && a<INT_MIN+b)) return HLFP_ERR_OVERFLOW;
    *o=a-b;return HLFP_OK;
}
void hlfp_p89_camera_init(hlfp_p89_camera *c){
    if(!c)return;
    memset(c,0,sizeof(*c));
    c->right.x=HLFP_FIXED_ONE;
    c->up.y=HLFP_FIXED_ONE;
    c->forward.z=HLFP_FIXED_ONE;
    c->near_z=HLFP_FIXED_ONE;
    c->focal=128*HLFP_FIXED_ONE;
}
static int p89_dot(const hlfp_vec3 *v,const hlfp_vec3 *axis,hlfp_fixed *out){
    hlfp_fixed a,b,c,r;
    if(hlfp_fixed_mul(v->x,axis->x,&a)!=HLFP_OK ||
       hlfp_fixed_mul(v->y,axis->y,&b)!=HLFP_OK ||
       hlfp_fixed_mul(v->z,axis->z,&c)!=HLFP_OK) return HLFP_ERR_OVERFLOW;
    if(p89_add(a,b,&r)!=HLFP_OK || p89_add(r,c,out)!=HLFP_OK)
        return HLFP_ERR_OVERFLOW;
    return HLFP_OK;
}
int hlfp_p89_transform(const hlfp_p89_camera *cam,const hlfp_vec3 *pos,hlfp_vec3 *out){
    hlfp_vec3 delta;
    if(!cam||!pos||!out)return HLFP_ERR_BAD_ARGUMENT;
    if(p89_sub(pos->x,cam->eye.x,&delta.x)!=HLFP_OK ||
       p89_sub(pos->y,cam->eye.y,&delta.y)!=HLFP_OK ||
       p89_sub(pos->z,cam->eye.z,&delta.z)!=HLFP_OK) return HLFP_ERR_OVERFLOW;
    if(p89_dot(&delta,&cam->right,&out->x)!=HLFP_OK ||
       p89_dot(&delta,&cam->up,&out->y)!=HLFP_OK ||
       p89_dot(&delta,&cam->forward,&out->z)!=HLFP_OK) return HLFP_ERR_OVERFLOW;
    return HLFP_OK;
}
int hlfp_p89_project(const hlfp_p89_camera *cam,const hlfp_vec3 *v,
                     hlfp_fixed scale,hlfp_fixed *sx,hlfp_fixed *sy,hlfp_fixed *q){
    hlfp_fixed a,b,r;
    if(!cam||!v||!sx||!sy||!q||cam->near_z<=0||cam->focal<=0)
        return HLFP_ERR_BAD_ARGUMENT;
    if(v->z<cam->near_z) return HLFP_ERR_OUT_OF_RANGE;
    if(hlfp_fixed_div(cam->focal,v->z,q)!=HLFP_OK)return HLFP_ERR_OVERFLOW;
    if(hlfp_fixed_mul(v->x,*q,&a)!=HLFP_OK ||
       hlfp_fixed_mul(v->y,*q,&b)!=HLFP_OK)return HLFP_ERR_OVERFLOW;
    if(hlfp_fixed_mul(a,scale,&r)!=HLFP_OK)return HLFP_ERR_OVERFLOW;
    *sx=r;
    return hlfp_fixed_mul(b,scale,sy);
}
/* Interpolate a whole expanded vertex including source normal and UV.
   Bone ownership is discrete, so choose nearest endpoint. */
static int p89_lerp_fixed(hlfp_fixed a,hlfp_fixed b,hlfp_fixed t,hlfp_fixed *o){
    hlfp_fixed d,p;
    if(p89_sub(b,a,&d)!=HLFP_OK || hlfp_fixed_mul(d,t,&p)!=HLFP_OK)
        return HLFP_ERR_OVERFLOW;
    return p89_add(a,p,o);
}
static int p89_cut(const hlfp_p89_camera *cam,const hlfp_p89_vertex *a,
                   const hlfp_p89_vertex *b,hlfp_p89_vertex *o){
    hlfp_fixed num,den,t;
    int s,tt;
    if(p89_sub(cam->near_z,a->camera.z,&num)!=HLFP_OK ||
       p89_sub(b->camera.z,a->camera.z,&den)!=HLFP_OK || den==0)
        return HLFP_ERR_OVERFLOW;
    if(hlfp_fixed_div(num,den,&t)!=HLFP_OK)return HLFP_ERR_OVERFLOW;
    if(t<0)t=0;
    if(t>HLFP_FIXED_ONE)t=HLFP_FIXED_ONE;
    *o=*a;
#define P89_LERP_FIELD(field) if(p89_lerp_fixed(a->field,b->field,t,&o->field)!=HLFP_OK) return HLFP_ERR_OVERFLOW
    P89_LERP_FIELD(camera.x);P89_LERP_FIELD(camera.y);
    o->camera.z=cam->near_z;
    P89_LERP_FIELD(vertex.pos.x);P89_LERP_FIELD(vertex.pos.y);P89_LERP_FIELD(vertex.pos.z);
    P89_LERP_FIELD(vertex.normal.x);P89_LERP_FIELD(vertex.normal.y);P89_LERP_FIELD(vertex.normal.z);
    P89_LERP_FIELD(vertex.u);P89_LERP_FIELD(vertex.v);
#undef P89_LERP_FIELD
    s=((int)a->vertex.s*(int)(HLFP_FIXED_ONE-t)/256+(int)b->vertex.s*(int)t/256)/256;
    tt=((int)a->vertex.t*(int)(HLFP_FIXED_ONE-t)/256+(int)b->vertex.t*(int)t/256)/256;
    if(s<SHRT_MIN||s>SHRT_MAX||tt<SHRT_MIN||tt>SHRT_MAX)return HLFP_ERR_OVERFLOW;
    o->vertex.s=(hlfp_i16)s;o->vertex.t=(hlfp_i16)tt;
    if(t>HLFP_FIXED_ONE/2){o->vertex.bone=b->vertex.bone;o->vertex.normal_bone=b->vertex.normal_bone;}
    return HLFP_OK;
}
int hlfp_p89_clip_near(const hlfp_p89_camera *cam,const hlfp_p89_vertex in[3],
                      hlfp_p89_vertex out[4],int *count){
    int i,n=0;
    if(!cam||!in||!out||!count||cam->near_z<=0)return HLFP_ERR_BAD_ARGUMENT;
    for(i=0;i<3;++i){
        const hlfp_p89_vertex *a=&in[i],*b=&in[(i+1)%3];
        int ai=a->camera.z>=cam->near_z,bi=b->camera.z>=cam->near_z;
        if(ai){if(n>=4)return HLFP_ERR_OVERFLOW;out[n++]=*a;}
        if(ai!=bi){if(n>=4)return HLFP_ERR_OVERFLOW;
            if(p89_cut(cam,a,b,&out[n])!=HLFP_OK)return HLFP_ERR_OVERFLOW;
            ++n;
        }
    }
    *count=n;return HLFP_OK;
}
/* Unsigned binary fractional division, never forms weight<<16; safe on 32-bit. */
hlfp_fixed hlfp_p89_ratio(unsigned long w,unsigned long total){
    unsigned int bit;unsigned long r,answer=0UL;
    if(total==0UL)return 0;
    if(w>=total)return HLFP_FIXED_ONE;
    r=w;
    for(bit=0U;bit<16U;++bit){
        answer*=2UL;
        if(r>=total-r){r=r-(total-r);answer+=1UL;}
        else r+=r;
    }
    return (hlfp_fixed)answer;
}
int hlfp_p89_correct_weights(long b0,long b1,long area,
        hlfp_fixed q0,hlfp_fixed q1,hlfp_fixed q2,hlfp_fixed weights[3]){
    hlfp_fixed a[3],v[3],sum,r;
    unsigned long w0,w1,tot;
    if(!weights||area==0||q0<=0||q1<=0||q2<=0)return HLFP_ERR_BAD_ARGUMENT;
    /* Edge signs must agree with signed area for points inside the triangle. */
    if(area==LONG_MIN||b0==LONG_MIN||b1==LONG_MIN)return HLFP_ERR_OVERFLOW;
    tot=(unsigned long)(area<0?-area:area);
    w0=(unsigned long)(b0<0?-b0:b0);
    w1=(unsigned long)(b1<0?-b1:b1);
    if(w0>tot||w1>tot||w1>tot-w0)return HLFP_ERR_BAD_ARGUMENT;
    a[0]=hlfp_p89_ratio(w0,tot);a[1]=hlfp_p89_ratio(w1,tot);
    a[2]=HLFP_FIXED_ONE-a[0]-a[1];
    if(a[2]<0)a[2]=0;
    if(hlfp_fixed_mul(a[0],q0,&v[0])!=HLFP_OK ||
       hlfp_fixed_mul(a[1],q1,&v[1])!=HLFP_OK ||
       hlfp_fixed_mul(a[2],q2,&v[2])!=HLFP_OK)return HLFP_ERR_OVERFLOW;
    if(p89_add(v[0],v[1],&r)!=HLFP_OK||p89_add(r,v[2],&sum)!=HLFP_OK||sum<=0)
        return HLFP_ERR_OVERFLOW;
    if(hlfp_fixed_div(v[0],sum,&weights[0])!=HLFP_OK ||
       hlfp_fixed_div(v[1],sum,&weights[1])!=HLFP_OK)return HLFP_ERR_OVERFLOW;
    weights[2]=HLFP_FIXED_ONE-weights[0]-weights[1];
    if(weights[2]<0)weights[2]=0;
    return HLFP_OK;
}
