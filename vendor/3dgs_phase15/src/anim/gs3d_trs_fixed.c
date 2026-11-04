#include "../gs3d_internal.h"
/* Entirely fixed-point Q16.16 TRS decomposition + shortest-arc quaternion NLERP.
   Uses no floating point or dynamic allocation, and does not depend on sizeof(long)==8. */
static unsigned long magnitude(long v){
    return v<0L ? (unsigned long)(-(v+1L))+1UL : (unsigned long)v;
}
/* Restoring binary division for (numerator/denominator)*65536, overflow-safe on 32-bit long. */
static int fixed_div(long numerator,long denominator,long *result){
    unsigned long a,b,q,rem,integer;int bit,negative;
    if(!result||denominator==0L)return GS3D_ERR_CORRUPT;
    negative=(numerator<0L)!=(denominator<0L);
    a=magnitude(numerator);b=magnitude(denominator);
    integer=a/b;
    if(integer>32767UL)return GS3D_ERR_RANGE;
    q=integer<<16;rem=a%b;
    for(bit=15;bit>=0;bit--){
        /* rem*2>=b, without evaluating an overflowing product. */
        if(rem>=b-rem){rem-=b-rem;q|=1UL<<(unsigned int)bit;}
        else rem+=rem;
    }
    if(negative) {
        if(q>=2147483648UL)*result=(-2147483647L-1L);
        else *result=-(long)q;
    } else *result=(long)q;
    return GS3D_OK;
}
static long fixed_sqrt(long input){
    unsigned long lo=0UL,hi=16777216UL,mid;
    if(input<=0L)return 0L;
    while(hi-lo>1UL){
        mid=lo+(hi-lo)/2UL;
        if(gs_qmul((long)mid,(long)mid)<=input)lo=mid;
        else hi=mid;
    }
    return (long)lo;
}
static long tw(long v){return gs_addsat(v,v);}
static long sub(long a,long b){
    if(b==(-2147483647L-1L))return a>=0L?2147483647L:gs_addsat(a,2147483647L);
    return gs_addsat(a,-b);
}
static int normalize(GS3D_Quat *q){
    long l=0L,s;int r;
    l=gs_addsat(l,gs_qmul(q->x,q->x));
    l=gs_addsat(l,gs_qmul(q->y,q->y));
    l=gs_addsat(l,gs_qmul(q->z,q->z));
    l=gs_addsat(l,gs_qmul(q->w,q->w));
    s=fixed_sqrt(l);if(s<128L)return GS3D_ERR_CORRUPT;
    r=fixed_div(q->x,s,&q->x);if(r)return r;
    r=fixed_div(q->y,s,&q->y);if(r)return r;
    r=fixed_div(q->z,s,&q->z);if(r)return r;
    return fixed_div(q->w,s,&q->w);
}
static int read_trs(const GS3D_Mat *m,GS3D_TRS *o){
    long rot[9],dot,len,trace,t,den,det,cross[3];unsigned int i,k,j;int r,branch;
    const unsigned int idx[9]={0U,1U,2U,4U,5U,6U,8U,9U,10U};
    if(m->m[3]!=0L||m->m[7]!=0L||m->m[11]!=0L||m->m[15]!=65536L)
        return GS3D_ERR_UNSUPPORTED;
    for(k=0U;k<3U;k++){
        dot=0L;
        for(i=0U;i<3U;i++){
            t=m->m[idx[k*3U+i]];
            dot=gs_addsat(dot,gs_qmul(t,t));
        }
        len=fixed_sqrt(dot);if(len<128L)return GS3D_ERR_UNSUPPORTED;
        o->scale[k]=len;
        for(i=0U;i<3U;i++){
            r=fixed_div(m->m[idx[k*3U+i]],len,&rot[k*3U+i]);
            if(r)return r;
        }
        o->translation[k]=m->m[12U+k];
    }
    /* Reject significant shear: a TRS rotation cannot represent it faithfully. */
    for(k=0U;k<3U;k++)for(i=k+1U;i<3U;i++){
        dot=0L;
        for(j=0U;j<3U;j++)dot=gs_addsat(dot,gs_qmul(rot[k*3U+j],rot[i*3U+j]));
        if(dot>4096L||dot< -4096L)return GS3D_ERR_UNSUPPORTED;
    }
    /* A reflected basis is NOT a proper quaternion rotation.
       Reject it rather than silently losing mirrored geometry. */
    cross[0]=sub(gs_qmul(rot[1],rot[5]),gs_qmul(rot[2],rot[4]));
    cross[1]=sub(gs_qmul(rot[2],rot[3]),gs_qmul(rot[0],rot[5]));
    cross[2]=sub(gs_qmul(rot[0],rot[4]),gs_qmul(rot[1],rot[3]));
    det=gs_addsat(gs_addsat(gs_qmul(cross[0],rot[6]),
                 gs_qmul(cross[1],rot[7])),gs_qmul(cross[2],rot[8]));
    if(det<59000L||det>72000L)return GS3D_ERR_UNSUPPORTED;
    /* Standard matrix-to-quaternion branches. rot is column-major. */
    trace=gs_addsat(gs_addsat(rot[0],rot[4]),rot[8]);
    branch=trace>0L?0:(rot[0]>rot[4] && rot[0]>rot[8] ? 1:(rot[4]>rot[8]?2:3));
    switch(branch){
    case 0:
        t=gs_addsat(65536L,trace);den=tw(fixed_sqrt(t));
        if(den<128L)return GS3D_ERR_UNSUPPORTED;
        o->q.w=den/4L;
        if(fixed_div(sub(rot[5],rot[7]),den,&o->q.x) ||
           fixed_div(sub(rot[6],rot[2]),den,&o->q.y) ||
           fixed_div(sub(rot[1],rot[3]),den,&o->q.z))return GS3D_ERR_RANGE;
        break;
    case 1:
        t=sub(sub(gs_addsat(65536L,rot[0]),rot[4]),rot[8]);den=tw(fixed_sqrt(t));
        if(den<128L)return GS3D_ERR_UNSUPPORTED;
        o->q.x=den/4L;
        if(fixed_div(sub(rot[5],rot[7]),den,&o->q.w) ||
           fixed_div(gs_addsat(rot[3],rot[1]),den,&o->q.y) ||
           fixed_div(gs_addsat(rot[6],rot[2]),den,&o->q.z))return GS3D_ERR_RANGE;
        break;
    case 2:
        t=sub(sub(gs_addsat(65536L,rot[4]),rot[0]),rot[8]);den=tw(fixed_sqrt(t));
        if(den<128L)return GS3D_ERR_UNSUPPORTED;
        o->q.y=den/4L;
        if(fixed_div(sub(rot[6],rot[2]),den,&o->q.w) ||
           fixed_div(gs_addsat(rot[3],rot[1]),den,&o->q.x) ||
           fixed_div(gs_addsat(rot[7],rot[5]),den,&o->q.z))return GS3D_ERR_RANGE;
        break;
    default:
        t=sub(sub(gs_addsat(65536L,rot[8]),rot[0]),rot[4]);den=tw(fixed_sqrt(t));
        if(den<128L)return GS3D_ERR_UNSUPPORTED;
        o->q.z=den/4L;
        if(fixed_div(sub(rot[1],rot[3]),den,&o->q.w) ||
           fixed_div(gs_addsat(rot[6],rot[2]),den,&o->q.x) ||
           fixed_div(gs_addsat(rot[7],rot[5]),den,&o->q.y))return GS3D_ERR_RANGE;
        break;
    }
    return normalize(&o->q);
}
static void write_trs(const GS3D_TRS *t,GS3D_Mat *m){
    GS3D_Quat q=t->q;long xx,yy,zz,xy,xz,yz,xw,yw,zw;
    xx=gs_qmul(q.x,q.x);yy=gs_qmul(q.y,q.y);zz=gs_qmul(q.z,q.z);
    xy=gs_qmul(q.x,q.y);xz=gs_qmul(q.x,q.z);yz=gs_qmul(q.y,q.z);
    xw=gs_qmul(q.x,q.w);yw=gs_qmul(q.y,q.w);zw=gs_qmul(q.z,q.w);
    gs_mat_identity(m);
    m->m[0]=sub(65536L,tw(gs_addsat(yy,zz)));
    m->m[1]=tw(gs_addsat(xy,zw));
    m->m[2]=tw(sub(xz,yw));
    m->m[4]=tw(sub(xy,zw));
    m->m[5]=sub(65536L,tw(gs_addsat(xx,zz)));
    m->m[6]=tw(gs_addsat(yz,xw));
    m->m[8]=tw(gs_addsat(xz,yw));
    m->m[9]=tw(sub(yz,xw));
    m->m[10]=sub(65536L,tw(gs_addsat(xx,yy)));
    m->m[0]=gs_qmul(m->m[0],t->scale[0]);
    m->m[1]=gs_qmul(m->m[1],t->scale[0]);
    m->m[2]=gs_qmul(m->m[2],t->scale[0]);
    m->m[4]=gs_qmul(m->m[4],t->scale[1]);
    m->m[5]=gs_qmul(m->m[5],t->scale[1]);
    m->m[6]=gs_qmul(m->m[6],t->scale[1]);
    m->m[8]=gs_qmul(m->m[8],t->scale[2]);
    m->m[9]=gs_qmul(m->m[9],t->scale[2]);
    m->m[10]=gs_qmul(m->m[10],t->scale[2]);
    m->m[12]=t->translation[0];m->m[13]=t->translation[1];m->m[14]=t->translation[2];
}
static long mix(long a,long b,long alpha){
    return gs_addsat(gs_qmul(a,65536L-alpha),gs_qmul(b,alpha));
}
int gs3d_trs_interpolate(const GS3D_Mat *a,const GS3D_Mat *b,long alpha,GS3D_Mat *out){
    GS3D_TRS left,right,mixed;long dot=0L;int r;unsigned int i;
    if(!a||!b||!out||alpha<0L||alpha>65536L)return GS3D_ERR_RANGE;
    if(alpha==0L){*out=*a;return GS3D_OK;}
    if(alpha==65536L){*out=*b;return GS3D_OK;}
    r=read_trs(a,&left);if(r)return r;
    r=read_trs(b,&right);if(r)return r;
    dot=gs_addsat(dot,gs_qmul(left.q.x,right.q.x));
    dot=gs_addsat(dot,gs_qmul(left.q.y,right.q.y));
    dot=gs_addsat(dot,gs_qmul(left.q.z,right.q.z));
    dot=gs_addsat(dot,gs_qmul(left.q.w,right.q.w));
    if(dot<0L){
        right.q.x=-right.q.x;right.q.y=-right.q.y;
        right.q.z=-right.q.z;right.q.w=-right.q.w;
    }
    mixed.q.x=mix(left.q.x,right.q.x,alpha);
    mixed.q.y=mix(left.q.y,right.q.y,alpha);
    mixed.q.z=mix(left.q.z,right.q.z,alpha);
    mixed.q.w=mix(left.q.w,right.q.w,alpha);
    r=normalize(&mixed.q);if(r)return r;
    for(i=0U;i<3U;i++){
        mixed.scale[i]=mix(left.scale[i],right.scale[i],alpha);
        mixed.translation[i]=mix(left.translation[i],right.translation[i],alpha);
    }
    write_trs(&mixed,out);return GS3D_OK;
}

/* Shared C89 TRS decomposition for additive pose overlays. */
int gs3d_trs_read(const GS3D_Mat *m,GS3D_TRS *out){
    if(!m||!out)return GS3D_ERR_RANGE;
    return read_trs(m,out);
}
void gs3d_trs_write(const GS3D_TRS *in,GS3D_Mat *out){
    if(in&&out)write_trs(in,out);
}
int gs3d_trs_normalize(GS3D_Quat *q){
    if(!q)return GS3D_ERR_RANGE;
    return normalize(q);
}
int gs3d_div_q16(long a,long b,long *out){return fixed_div(a,b,out);}
