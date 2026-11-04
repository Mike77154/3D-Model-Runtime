#include "../gs3d_internal.h"
/* Saturating Q16.16 multiplication with only ISO C89 unsigned long
   operations. Also valid when sizeof(long)==4 (Win32). */
static unsigned long mag(long n){
    return n<0L ? (unsigned long)(-(n+1L))+1UL : (unsigned long)n;
}
static long signed_result(unsigned long v,int negative){
    if(negative){
        if(v>=2147483648UL)return (-2147483647L-1L);
        return -(long)v;
    }
    if(v>=2147483647UL)return 2147483647L;
    return (long)v;
}
long gs_qmul(long a,long b){
    unsigned long x=mag(a),y=mag(b),ah=x>>16,bh=y>>16,al=x&65535UL,bl=y&65535UL;
    unsigned long limit=((a<0L)!=(b<0L))?2147483648UL:2147483647UL;
    unsigned long out=0UL,term;
    int neg=((a<0L)!=(b<0L));
    if(ah && bh > (limit>>16)/ah)return signed_result(limit,neg);
    out=(ah*bh)<<16;
    if(ah && bl>(limit-out)/ah)return signed_result(limit,neg);
    out+=ah*bl;
    if(bh && al>(limit-out)/bh)return signed_result(limit,neg);
    out+=bh*al;
    term=(al*bl)>>16; /* 16x16 product fits unsigned long32 */
    if(term>limit-out)return signed_result(limit,neg);
    return signed_result(out+term,neg);
}
void gs_mat_identity(GS3D_Mat *dst){
    unsigned int i;
    for(i=0U;i<16U;i++)dst->m[i]=0L;
    dst->m[0]=dst->m[5]=dst->m[10]=dst->m[15]=65536L;
}
void gs_mat_translate(GS3D_Mat *out,long x,long y,long z){
    gs_mat_identity(out);out->m[12]=x;out->m[13]=y;out->m[14]=z;
}
/* Affine only. Column-major 4x4, canonical final row. */
void gs_mat_mul(GS3D_Mat *dst,const GS3D_Mat *a,const GS3D_Mat *b){
    GS3D_Mat t;unsigned int row,col,k;long s;
    gs_mat_identity(&t);
    for(col=0U;col<4U;col++){
        for(row=0U;row<3U;row++){
            s=0L;
            for(k=0U;k<3U;k++)s=gs_addsat(s,gs_qmul(a->m[row+4U*k],b->m[k+4U*col]));
            if(col==3U)s=gs_addsat(s,a->m[row+12U]);
            t.m[row+4U*col]=s;
        }
    }
    *dst=t;
}
void gs_mat_point(const GS3D_Mat *m,const GS3D_Vec *v,GS3D_Vec *out){
    GS3D_Vec t=*v;
    t.x=gs_addsat(gs_addsat(gs_addsat(gs_qmul(m->m[0],v->x),gs_qmul(m->m[4],v->y)),gs_qmul(m->m[8],v->z)),m->m[12]);
    t.y=gs_addsat(gs_addsat(gs_addsat(gs_qmul(m->m[1],v->x),gs_qmul(m->m[5],v->y)),gs_qmul(m->m[9],v->z)),m->m[13]);
    t.z=gs_addsat(gs_addsat(gs_addsat(gs_qmul(m->m[2],v->x),gs_qmul(m->m[6],v->y)),gs_qmul(m->m[10],v->z)),m->m[14]);
    *out=t;
}
