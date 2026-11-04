#include "hlfp_frustum89.h"
#include <limits.h>
#include <string.h>

static int f89_add(hlfp_fixed a,hlfp_fixed b,hlfp_fixed *r){
    if ((b>0 && a>INT_MAX-b)||(b<0 && a<INT_MIN-b)) return HLFP_ERR_OVERFLOW;
    *r=a+b;return HLFP_OK;
}
static int f89_sub(hlfp_fixed a,hlfp_fixed b,hlfp_fixed *r){
    if ((b<0 && a>INT_MAX+b)||(b>0 && a<INT_MIN+b))return HLFP_ERR_OVERFLOW;
    *r=a-b;return HLFP_OK;
}
static int f89_slope(int px,hlfp_fixed focal,hlfp_fixed scale,hlfp_fixed *s){
    hlfp_fixed pixel_units,projected;
    if(px<-16383||px>16383||focal<=0||scale<=0)return HLFP_ERR_BAD_ARGUMENT;
    pixel_units=(hlfp_fixed)(px*HLFP_FIXED_ONE);
    if(hlfp_fixed_div(pixel_units,scale,&projected)!=HLFP_OK)return HLFP_ERR_OVERFLOW;
    return hlfp_fixed_div(projected,focal,s);
}
int hlfp_f89_from_viewport(const hlfp_p89_camera *p,
                           const hlfp_soft_camera *s,int w,int h,
                           hlfp_fixed far_z,hlfp_f89_frustum *f){
    hlfp_fixed y0,y1,tmp;
    int low,high,rc;
    if(!p||!s||!f||w<1||h<1||p->near_z<=0||p->focal<=0||
       s->scale<=0||far_z<=p->near_z)return HLFP_ERR_BAD_ARGUMENT;
    if(s->center_x < -16383||s->center_x>16383||
       s->center_y < -16383||s->center_y>16383||w>8192||h>8192)
        return HLFP_ERR_OUT_OF_RANGE;
    memset(f,0,sizeof(*f));f->near_z=p->near_z;f->far_z=far_z;
    low=-s->center_x;high=w-1-s->center_x;
    rc=f89_slope(low,p->focal,s->scale,&f->left_slope);
    if(rc!=HLFP_OK)return rc;
    rc=f89_slope(high,p->focal,s->scale,&f->right_slope);
    if(rc!=HLFP_OK)return rc;
    low=-s->center_y;high=h-1-s->center_y;
    if(s->flip_y){tmp=(hlfp_fixed)low;low=-high;high=-(int)tmp;}
    rc=f89_slope(low,p->focal,s->scale,&y0);
    if(rc!=HLFP_OK)return rc;
    rc=f89_slope(high,p->focal,s->scale,&y1);
    if(rc!=HLFP_OK)return rc;
    f->bottom_slope=y0;f->top_slope=y1;
    return HLFP_OK;
}
/* signed distance; >= 0 means inside. Negative to positive edges
 * use magnitudes to avoid signed subtraction overflow. */
static int f89_distance(const hlfp_f89_frustum *f,int plane,
                        const hlfp_p89_vertex *v,hlfp_fixed *d){
    hlfp_fixed k;
    switch(plane){
    case 0:return f89_sub(v->camera.z,f->near_z,d);
    case 1:return f89_sub(f->far_z,v->camera.z,d);
    case 2:
        if(hlfp_fixed_mul(f->left_slope,v->camera.z,&k)!=HLFP_OK)return HLFP_ERR_OVERFLOW;
        return f89_sub(v->camera.x,k,d);
    case 3:
        if(hlfp_fixed_mul(f->right_slope,v->camera.z,&k)!=HLFP_OK)return HLFP_ERR_OVERFLOW;
        return f89_sub(k,v->camera.x,d);
    case 4:
        if(hlfp_fixed_mul(f->bottom_slope,v->camera.z,&k)!=HLFP_OK)return HLFP_ERR_OVERFLOW;
        return f89_sub(v->camera.y,k,d);
    default:
        if(hlfp_fixed_mul(f->top_slope,v->camera.z,&k)!=HLFP_OK)return HLFP_ERR_OVERFLOW;
        return f89_sub(k,v->camera.y,d);
    }
}
static unsigned long f89_magnitude(hlfp_fixed x){
    return x<0 ? (unsigned long)(-(x+1))+1UL : (unsigned long)x;
}
static hlfp_fixed f89_fraction(hlfp_fixed a,hlfp_fixed b){
    unsigned long da=f89_magnitude(a),db=f89_magnitude(b);
    /* Distances have opposite signs. Reduce at 32-bit overflow threshold
     * without 64-bit integers (errors bounded by < 1/65536). */
    while(db>2147483647UL || da>2147483647UL-db){da=(da+1UL)/2UL;db=(db+1UL)/2UL;}
    if(da+db==0UL)return 0;
    return hlfp_p89_ratio(da,da+db);
}
static int f89_lerp(hlfp_fixed a,hlfp_fixed b,hlfp_fixed t,hlfp_fixed *dst){
    hlfp_fixed diff,part;
    if(f89_sub(b,a,&diff)!=HLFP_OK)return HLFP_ERR_OVERFLOW;
    if(hlfp_fixed_mul(diff,t,&part)!=HLFP_OK)return HLFP_ERR_OVERFLOW;
    return f89_add(a,part,dst);
}
static int f89_intersect(const hlfp_f89_frustum *f,int plane,
          const hlfp_p89_vertex *a,const hlfp_p89_vertex *b,
          hlfp_fixed d0,hlfp_fixed d1,hlfp_p89_vertex *o){
    hlfp_fixed t=f89_fraction(d0,d1),k;
    int is,it;
    *o=*a;
#define F89_LERP(field) if(f89_lerp(a->field,b->field,t,&o->field)!=HLFP_OK)return HLFP_ERR_OVERFLOW
    F89_LERP(camera.x);F89_LERP(camera.y);F89_LERP(camera.z);
    F89_LERP(vertex.pos.x);F89_LERP(vertex.pos.y);F89_LERP(vertex.pos.z);
    F89_LERP(vertex.normal.x);F89_LERP(vertex.normal.y);F89_LERP(vertex.normal.z);
    F89_LERP(vertex.u);F89_LERP(vertex.v);
#undef F89_LERP
    /* Use 8.8 intermediate products: signed 16-bit UV times 16-bit ratio
     * remain bounded to signed int32. */
    is=((int)a->vertex.s*((int)(HLFP_FIXED_ONE-t)/256) +
        (int)b->vertex.s*((int)t/256))/256;
    it=((int)a->vertex.t*((int)(HLFP_FIXED_ONE-t)/256) +
        (int)b->vertex.t*((int)t/256))/256;
    if(is<SHRT_MIN||is>SHRT_MAX||it<SHRT_MIN||it>SHRT_MAX)
        return HLFP_ERR_OVERFLOW;
    o->vertex.s=(hlfp_i16)is;o->vertex.t=(hlfp_i16)it;
    if(t>HLFP_FIXED_ONE/2){o->vertex.bone=b->vertex.bone;
                           o->vertex.normal_bone=b->vertex.normal_bone;}
    /* Snap intersection to its plane: prevents repeated clipping from
     * drifting outside due to 16.16 quantization. */
    switch(plane){
      case 0:o->camera.z=f->near_z;break;
      case 1:o->camera.z=f->far_z;break;
      case 2:case 3:
        if(hlfp_fixed_mul(plane==2?f->left_slope:f->right_slope,
            o->camera.z,&k)!=HLFP_OK)return HLFP_ERR_OVERFLOW;
        o->camera.x=k;break;
      default:
        if(hlfp_fixed_mul(plane==4?f->bottom_slope:f->top_slope,
            o->camera.z,&k)!=HLFP_OK)return HLFP_ERR_OVERFLOW;
        o->camera.y=k;break;
    }
    return HLFP_OK;
}
static int f89_equal_pos(const hlfp_p89_vertex *a,const hlfp_p89_vertex *b){
    return a->camera.x==b->camera.x && a->camera.y==b->camera.y &&
           a->camera.z==b->camera.z;
}
static int f89_push(hlfp_p89_vertex *dst,int *count,
                    const hlfp_p89_vertex *v){
    if(*count && f89_equal_pos(&dst[*count-1],v))return HLFP_OK;
    if(*count>=HLFP_F89_MAX_VERTICES)return HLFP_ERR_CAPACITY;
    dst[(*count)++]=*v;return HLFP_OK;
}
int hlfp_f89_clip_triangle(const hlfp_f89_frustum *f,
                           const hlfp_p89_vertex in[3],
                           hlfp_p89_vertex out[HLFP_F89_MAX_VERTICES],int *count){
    hlfp_p89_vertex a[HLFP_F89_MAX_VERTICES],b[HLFP_F89_MAX_VERTICES];
    int n=3,plane,i,m,rc;
    if(!f||!in||!out||!count||f->near_z<=0||f->far_z<=f->near_z||
       f->left_slope>f->right_slope||f->bottom_slope>f->top_slope)
       return HLFP_ERR_BAD_ARGUMENT;
    *count=0;
    for(i=0;i<3;++i)a[i]=in[i];
    for(plane=0;plane<HLFP_F89_PLANES && n;plane++){
        m=0;
        for(i=0;i<n;i++){
            const hlfp_p89_vertex *p=&a[i],*q=&a[(i+1)%n];
            hlfp_fixed dp,dq;
            int pin,qin;
            rc=f89_distance(f,plane,p,&dp);if(rc!=HLFP_OK)return rc;
            rc=f89_distance(f,plane,q,&dq);if(rc!=HLFP_OK)return rc;
            pin=dp>=0;qin=dq>=0;
            if(pin){rc=f89_push(b,&m,p);if(rc!=HLFP_OK)return rc;}
            if(pin!=qin){
                hlfp_p89_vertex cross;
                rc=f89_intersect(f,plane,p,q,dp,dq,&cross);
                if(rc!=HLFP_OK)return rc;
                rc=f89_push(b,&m,&cross);if(rc!=HLFP_OK)return rc;
            }
        }
        if(m>1 && f89_equal_pos(&b[0],&b[m-1]))--m;
        n=m;for(i=0;i<n;++i)a[i]=b[i];
    }
    for(i=0;i<n;++i)out[i]=a[i];
    *count=n;
    return HLFP_OK;
}
int hlfp_f89_clip_segment(const hlfp_f89_frustum *f,
                          hlfp_p89_vertex *a,hlfp_p89_vertex *b,int *visible){
    int plane,rc;
    if(!f||!a||!b||!visible)return HLFP_ERR_BAD_ARGUMENT;
    *visible=0;
    for(plane=0;plane<HLFP_F89_PLANES;++plane){
        hlfp_fixed da,db;int ia,ib;
        rc=f89_distance(f,plane,a,&da);if(rc!=HLFP_OK)return rc;
        rc=f89_distance(f,plane,b,&db);if(rc!=HLFP_OK)return rc;
        ia=da>=0;ib=db>=0;
        if(!ia&&!ib)return HLFP_OK;
        if(ia!=ib){
            hlfp_p89_vertex cut;
            rc=f89_intersect(f,plane,a,b,da,db,&cut);
            if(rc!=HLFP_OK)return rc;
            if(ia)*b=cut;else *a=cut;
        }
    }
    *visible=1;return HLFP_OK;
}
