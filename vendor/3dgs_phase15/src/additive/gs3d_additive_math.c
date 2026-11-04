#include "../gs3d_internal.h"
/* Reference-relative animation, avoiding matrix lerp/shear.
   q_delta = inverse(q_reference) * q_add; out = base * nlerp(I,delta,w).
   Translation is base + (add-ref)*w; scale = base * lerp(1,add/ref,w).
   Saturating arithmetic ensures defined 32-bit signed-long operation. */
static long neg(long x){return x==(-2147483647L-1L)?2147483647L:-x;}
static long sub(long a,long b){return gs_addsat(a,neg(b));}
static GS3D_Quat product(const GS3D_Quat *a,const GS3D_Quat *b){
    GS3D_Quat q;
    q.x=gs_addsat(gs_addsat(gs_qmul(a->w,b->x),gs_qmul(a->x,b->w)),
        sub(gs_qmul(a->y,b->z),gs_qmul(a->z,b->y)));
    q.y=gs_addsat(gs_addsat(gs_qmul(a->w,b->y),gs_qmul(a->y,b->w)),
        sub(gs_qmul(a->z,b->x),gs_qmul(a->x,b->z)));
    q.z=gs_addsat(gs_addsat(gs_qmul(a->w,b->z),gs_qmul(a->z,b->w)),
        sub(gs_qmul(a->x,b->y),gs_qmul(a->y,b->x)));
    q.w=sub(sub(gs_qmul(a->w,b->w),gs_qmul(a->x,b->x)),
        gs_addsat(gs_qmul(a->y,b->y),gs_qmul(a->z,b->z)));
    return q;
}
int gs3d_additive_matrix(const GS3D_Mat *b,const GS3D_Mat *a,
    const GS3D_Mat *r,long w,GS3D_Mat *out){
    GS3D_TRS base,add,ref,dst;
    GS3D_Quat inv,delta,weighted;
    unsigned int i;long ratio,factor,scaled;int status;
    if(!b||!a||!r||!out||w<0L||w>65536L)return GS3D_ERR_RANGE;
    if(!w){*out=*b;return GS3D_OK;}
    status=gs3d_trs_read(b,&base);if(status)return status;
    status=gs3d_trs_read(a,&add);if(status)return status;
    status=gs3d_trs_read(r,&ref);if(status)return status;
    inv=ref.q;inv.x=neg(inv.x);inv.y=neg(inv.y);inv.z=neg(inv.z);
    delta=product(&inv,&add.q);
    /* q and -q are the same rotation; pick shortest arc from identity. */
    if(delta.w<0L){delta.x=neg(delta.x);delta.y=neg(delta.y);
        delta.z=neg(delta.z);delta.w=neg(delta.w);}
    weighted.x=gs_qmul(delta.x,w);weighted.y=gs_qmul(delta.y,w);
    weighted.z=gs_qmul(delta.z,w);
    weighted.w=gs_addsat(gs_qmul(65536L,65536L-w),gs_qmul(delta.w,w));
    status=gs3d_trs_normalize(&weighted);if(status)return status;
    dst.q=product(&base.q,&weighted);
    status=gs3d_trs_normalize(&dst.q);if(status)return status;
    for(i=0U;i<3U;i++){
        if(ref.scale[i]<=0L)return GS3D_ERR_UNSUPPORTED;
        status=gs3d_div_q16(add.scale[i],ref.scale[i],&ratio);
        if(status)return status;
        scaled=gs_qmul(sub(ratio,65536L),w);
        factor=gs_addsat(65536L,scaled);
        dst.scale[i]=gs_qmul(base.scale[i],factor);
        if(dst.scale[i]<=0L)return GS3D_ERR_UNSUPPORTED;
        dst.translation[i]=gs_addsat(base.translation[i],
            gs_qmul(sub(add.translation[i],ref.translation[i]),w));
    }
    gs3d_trs_write(&dst,out);
    return GS3D_OK;
}
