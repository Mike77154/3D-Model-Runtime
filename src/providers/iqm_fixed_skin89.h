#ifndef MODELRT_IQM_FIXED_SKIN89_H
#define MODELRT_IQM_FIXED_SKIN89_H
/* IQM 2 skeletal transformation primitives. ISO C89; signed Q16.16,
 * no heap, floating point, or 64-bit integer arithmetic. Matrices 3x4
 * row-major, points are column vectors. Saturation avoids signed overflow.
 * These helpers operate on deliberately bounded geometry / transforms. */

typedef struct iqmrt_transform_s {
    modelrt_q16 t[3];
    modelrt_q16 q[4];
    modelrt_q16 s[3];
} iqmrt_transform;
#define IQMRT_ONE ((modelrt_q16)65536L)
#define IQMRT_MAX ((modelrt_q16)2147483647L)
#define IQMRT_MIN ((modelrt_q16)(-2147483647L-1L))
static modelrt_u32 iqmrt_magnitude(modelrt_q16 a)
{return a<0?(modelrt_u32)(-(a+1))+1u:(modelrt_u32)a;}
/* Q16 multiplication from four 16-bit limbs, with saturation. */
static modelrt_q16 iqmrt_fmul(modelrt_q16 a,modelrt_q16 b)
{
    modelrt_u32 aa=iqmrt_magnitude(a),bb=iqmrt_magnitude(b),ah=aa>>16,bh=bb>>16;
    modelrt_u32 p1=ah*bh,p2=ah*(bb&65535u),p3=bh*(aa&65535u);
    modelrt_u32 low=(aa&65535u)*(bb&65535u),limit,acc;
    int neg=(a<0)!=(b<0);
    limit=neg?2147483648u:2147483647u;
    if(p1>limit/65536u)return neg?IQMRT_MIN:IQMRT_MAX;
    acc=p1*65536u;
    if(p2>limit-acc)return neg?IQMRT_MIN:IQMRT_MAX;
    acc+=p2;
    if(p3>limit-acc)return neg?IQMRT_MIN:IQMRT_MAX;
    acc+=p3;
    if((low>>16)>limit-acc)return neg?IQMRT_MIN:IQMRT_MAX;
    acc+=low>>16;
    if(neg){if(acc==2147483648u)return IQMRT_MIN;return -(modelrt_q16)acc;}
    return (modelrt_q16)acc;
}
/* All divisions used here have a divisor of small transform magnitude; the
 * 16 fractional bits are obtained with safe binary long division. */
static int iqmrt_fdiv(modelrt_q16 a,modelrt_q16 b,modelrt_q16*out)
{
    modelrt_u32 aa,bb,integer,rem,result;unsigned int i;int neg;
    if(!b)return MODELRT_ERR_FORMAT;
    neg=(a<0)!=(b<0);aa=iqmrt_magnitude(a);bb=iqmrt_magnitude(b);
    integer=aa/bb;rem=aa%bb;
    if(integer>=32768u || bb>1073741823u)return MODELRT_ERR_LIMIT;
    /* Divide only the fractional remainder, without overflow or 64 bits. */
    result=0u;
    for(i=0u;i<16u;i++){
        rem<<=1;result<<=1;
        if(rem>=bb){rem-=bb;result|=1u;}
    }
    if(integer>32767u || ((integer<<16)+result)>(neg?2147483648u:2147483647u))return MODELRT_ERR_LIMIT;
    result+=(integer<<16);
    *out=neg?(result==2147483648u?IQMRT_MIN:-(modelrt_q16)result):(modelrt_q16)result;
    return MODELRT_OK;
}
static modelrt_q16 iqmrt_fsqr_root(modelrt_q16 sum)
{
    modelrt_u32 lo=0u,hi=2097152u,mid; /* sqrt(32 Q16 units) max */
    if(sum<=0)return 0;
    while(lo+1u<hi){mid=lo+(hi-lo)/2u;
        if(iqmrt_fmul((modelrt_q16)mid,(modelrt_q16)mid)<=sum)lo=mid;
        else hi=mid;
    }
    return (modelrt_q16)lo;
}
static int iqmrt_normalize_quat(modelrt_q16 q[4])
{
    modelrt_q16 sum=0,len;int i;
    for(i=0;i<4;i++){
        if(q[i] > 262144L || q[i] < -262144L)return MODELRT_ERR_FORMAT;
        sum=iqmrt_sat_add(sum,iqmrt_fmul(q[i],q[i]));
    }
    if(sum<128)return MODELRT_ERR_FORMAT;
    len=iqmrt_fsqr_root(sum);if(len<1)return MODELRT_ERR_FORMAT;
    for(i=0;i<4;i++)if(iqmrt_fdiv(q[i],len,&q[i]))return MODELRT_ERR_FORMAT;
    return MODELRT_OK;
}
/* Rotation matrix from unit quaternion; scale applies per local column. */
static int iqmrt_matrix_from_trs(iqmrt_matrix*out,const iqmrt_transform*in,int inverse)
{
    modelrt_q16 q[4],r[9],xx,xy,xz,yy,yz,zz,wx,wy,wz,tw=2*IQMRT_ONE,sc[3],val;
    int i,j,rc;
    for(i=0;i<4;i++)q[i]=in->q[i];
    rc=iqmrt_normalize_quat(q);if(rc)return rc;
    xx=iqmrt_fmul(q[0],q[0]);yy=iqmrt_fmul(q[1],q[1]);zz=iqmrt_fmul(q[2],q[2]);
    xy=iqmrt_fmul(q[0],q[1]);xz=iqmrt_fmul(q[0],q[2]);yz=iqmrt_fmul(q[1],q[2]);
    wx=iqmrt_fmul(q[3],q[0]);wy=iqmrt_fmul(q[3],q[1]);wz=iqmrt_fmul(q[3],q[2]);
    r[0]=iqmrt_sat_add(IQMRT_ONE,-iqmrt_fmul(tw,iqmrt_sat_add(yy,zz)));
    r[1]=iqmrt_fmul(tw,iqmrt_sat_add(xy,-wz));
    r[2]=iqmrt_fmul(tw,iqmrt_sat_add(xz,wy));
    r[3]=iqmrt_fmul(tw,iqmrt_sat_add(xy,wz));
    r[4]=iqmrt_sat_add(IQMRT_ONE,-iqmrt_fmul(tw,iqmrt_sat_add(xx,zz)));
    r[5]=iqmrt_fmul(tw,iqmrt_sat_add(yz,-wx));
    r[6]=iqmrt_fmul(tw,iqmrt_sat_add(xz,-wy));
    r[7]=iqmrt_fmul(tw,iqmrt_sat_add(yz,wx));
    r[8]=iqmrt_sat_add(IQMRT_ONE,-iqmrt_fmul(tw,iqmrt_sat_add(xx,yy)));
    for(i=0;i<3;i++){
        if(in->s[i]<16 || in->s[i]>16777216L)return MODELRT_ERR_FORMAT;
        if(inverse){rc=iqmrt_fdiv(IQMRT_ONE,in->s[i],&sc[i]);if(rc)return rc;}
        else sc[i]=in->s[i];
    }
    for(i=0;i<3;i++)for(j=0;j<3;j++){
        val=inverse?r[j*3+i]:r[i*3+j];
        out->m[4*i+j]=iqmrt_fmul(val,sc[inverse?i:j]);
    }
    for(i=0;i<3;i++){
        if(!inverse)out->m[i*4+3]=in->t[i];
        else{
            modelrt_q16 p=0;
            for(j=0;j<3;j++)p=iqmrt_sat_add(p,iqmrt_fmul(out->m[i*4+j],in->t[j]));
            out->m[i*4+3]=-p;
        }
    }
    return MODELRT_OK;
}
static void iqmrt_matrix_mul(iqmrt_matrix*out,const iqmrt_matrix*a,const iqmrt_matrix*b)
{
    iqmrt_matrix t;int i,j,k;modelrt_q16 sum;
    for(i=0;i<3;i++)for(j=0;j<4;j++){
        sum=(j==3)?a->m[i*4+3]:0;
        for(k=0;k<3;k++)sum=iqmrt_sat_add(sum,iqmrt_fmul(a->m[i*4+k],b->m[k*4+j]));
        t.m[i*4+j]=sum;
    }
    *out=t;
}
static void iqmrt_matrix_point(const iqmrt_matrix*m,const modelrt_vec3*src,modelrt_vec3*dst,int normal)
{
    modelrt_q16 v[3],r[3];int i,j;
    v[0]=src->x;v[1]=src->y;v[2]=src->z;
    for(i=0;i<3;i++){
        r[i]=normal?0:m->m[i*4+3];
        for(j=0;j<3;j++)r[i]=iqmrt_sat_add(r[i],iqmrt_fmul(m->m[i*4+j],v[j]));
    }
    dst->x=r[0];dst->y=r[1];dst->z=r[2];
}
#endif
