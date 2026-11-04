#include "bc_rig_q16.h"
#include <limits.h>
#include <string.h>

#if UINT_MAX != 4294967295U || INT_MAX != 2147483647
#error Q16.16 backend requires 32-bit int and unsigned int
#endif

static unsigned int bc_mag(int a)
{
  return (a<0)?(unsigned int)(-(a+1))+1U:(unsigned int)a;
}
static int bc_signed(unsigned int val,int negative,int *dst)
{
  unsigned int maxv=negative?2147483648U:2147483647U;
  if(val>maxv)return BC_RIG_BAD;
  if(!negative){*dst=(int)val;return BC_RIG_OK;}
  if(val==2147483648U){*dst=INT_MIN;return BC_RIG_OK;}
  *dst=-(int)val;
  return BC_RIG_OK;
}
int bc_qadd_checked(int a,int b,int *dst)
{
  if((b>0 && a>INT_MAX-b) || (b<0 && a<INT_MIN-b))return BC_RIG_BAD;
  *dst=a+b;
  return BC_RIG_OK;
}
/* 32-bit-only multiplication: split each magnitude into upper/lower 16 bits.
   Floor(|a*b|/65536), then restore sign; fail rather than overflow or clamp. */
int bc_qmul_checked(int a,int b,int *dst)
{
  unsigned int ua=bc_mag(a),ub=bc_mag(b),ah=ua>>16,bh=ub>>16;
  unsigned int al=ua&65535U,bl=ub&65535U;
  unsigned int limit=((a<0)!=(b<0))?2147483648U:2147483647U;
  unsigned int total=0U,part,hi;
  if(ah!=0U && bh!=0U){
    hi=ah*bh;
    if(hi>limit/65536U)return BC_RIG_BAD;
    total=hi*65536U;
  }
  part=ah*bl;
  if(part>limit-total)return BC_RIG_BAD;
  total+=part;
  part=bh*al;
  if(part>limit-total)return BC_RIG_BAD;
  total+=part;
  part=(al*bl)>>16;
  if(part>limit-total)return BC_RIG_BAD;
  total+=part;
  return bc_signed(total,(a<0)!=(b<0),dst);
}
/* Divide signed Q16 values using binary long division over a 48-bit numerator.
   The remainder is < divisor <= 2^31, so the 32-bit doubling is safe. */
int bc_qdiv_checked(int a,int b,int *dst)
{
  unsigned int num,den,rem=0U,quot=0U,lim,bit;
  int i,negative;
  if(b==0)return BC_RIG_BAD;
  negative=((a<0)!=(b<0));
  lim=negative?2147483648U:2147483647U;
  num=bc_mag(a);den=bc_mag(b);
  for(i=47;i>=0;i--){
    bit=(i>=16)?((num>>(unsigned int)(i-16))&1U):0U;
    rem=rem*2U+bit;
    bit=0U;
    if(rem>=den){rem-=den;bit=1U;}
    if(quot>lim/2U)return BC_RIG_BAD;
    quot=quot*2U+bit;
    if(quot>lim)return BC_RIG_BAD;
  }
  return bc_signed(quot,negative,dst);
}
void bc_rig_mat_identity(BC_RigMat *dst)
{
  unsigned int k;
  for(k=0U;k<16U;k++)dst->m[k]=0;
  for(k=0U;k<4U;k++)dst->m[k*4U+k]=BC_RIG_ONE;
}
int bc_rig_mat_mul(const BC_RigMat *a,const BC_RigMat *b,BC_RigMat *dst)
{
  BC_RigMat result;
  unsigned int i,j,k;int v,sum;
  for(i=0U;i<4U;i++)for(j=0U;j<4U;j++){
    sum=0;
    for(k=0U;k<4U;k++){
      if(bc_qmul_checked(a->m[i*4U+k],b->m[k*4U+j],&v)!=BC_RIG_OK ||
         bc_qadd_checked(sum,v,&sum)!=BC_RIG_OK)return BC_RIG_BAD;
    }
    result.m[i*4U+j]=sum;
  }
  *dst=result;
  return BC_RIG_OK;
}
int bc_rig_mat_point(const BC_RigMat *mat,const int point[3],int dst[3])
{
  unsigned int i,j;int r[3],term,sum;
  for(i=0U;i<3U;i++){
    sum=mat->m[i*4U+3U];
    for(j=0U;j<3U;j++){
      if(bc_qmul_checked(mat->m[i*4U+j],point[j],&term)!=BC_RIG_OK ||
         bc_qadd_checked(sum,term,&sum)!=BC_RIG_OK)return BC_RIG_BAD;
    }
    r[i]=sum;
  }
  for(i=0U;i<3U;i++)dst[i]=r[i];
  return BC_RIG_OK;
}
static int q2(int a,int *out) {return bc_qadd_checked(a,a,out);}
/* Quat input is unit length, w-x-y-z. Columns are scaled independently. */
int bc_rig_mat_from_trs(const int loc[3],const int quat[4],const int scale[3],BC_RigMat *dst)
{
  int xx,xy,xz,xw,yy,yz,yw,zz,zw,ww,norm,tmp,tmp2,r[9];
  int i,j;
  if(bc_qmul_checked(quat[1],quat[1],&xx)!=0 ||
     bc_qmul_checked(quat[1],quat[2],&xy)!=0 ||
     bc_qmul_checked(quat[1],quat[3],&xz)!=0 ||
     bc_qmul_checked(quat[1],quat[0],&xw)!=0 ||
     bc_qmul_checked(quat[2],quat[2],&yy)!=0 ||
     bc_qmul_checked(quat[2],quat[3],&yz)!=0 ||
     bc_qmul_checked(quat[2],quat[0],&yw)!=0 ||
     bc_qmul_checked(quat[3],quat[3],&zz)!=0 ||
     bc_qmul_checked(quat[3],quat[0],&zw)!=0 ||
     bc_qmul_checked(quat[0],quat[0],&ww)!=0)return BC_RIG_BAD;
  if(bc_qadd_checked(xx,yy,&norm)!=0 || bc_qadd_checked(norm,zz,&norm)!=0 ||
     bc_qadd_checked(norm,ww,&norm)!=0)return BC_RIG_BAD;
  if(norm<BC_RIG_ONE-256 || norm>BC_RIG_ONE+256)return BC_RIG_UNSUPPORTED;
#define RADD(a,b,o) if(bc_qadd_checked((a),(b),&(o))!=0)return BC_RIG_BAD
#define RSUB(a,b,o) if(bc_qadd_checked((a),-(b),&(o))!=0)return BC_RIG_BAD
#define RTWO(a,o) if(q2((a),&(o))!=0)return BC_RIG_BAD
  RADD(yy,zz,tmp);RTWO(tmp,tmp);RSUB(BC_RIG_ONE,tmp,r[0]);
  RSUB(xy,zw,tmp);RTWO(tmp,r[1]);
  RADD(xz,yw,tmp);RTWO(tmp,r[2]);
  RADD(xy,zw,tmp);RTWO(tmp,r[3]);
  RADD(xx,zz,tmp);RTWO(tmp,tmp);RSUB(BC_RIG_ONE,tmp,r[4]);
  RSUB(yz,xw,tmp);RTWO(tmp,r[5]);
  RSUB(xz,yw,tmp);RTWO(tmp,r[6]);
  RADD(yz,xw,tmp);RTWO(tmp,r[7]);
  RADD(xx,yy,tmp);RTWO(tmp,tmp2);RSUB(BC_RIG_ONE,tmp2,r[8]);
#undef RADD
#undef RSUB
#undef RTWO
  bc_rig_mat_identity(dst);
  for(i=0;i<3;i++){
    for(j=0;j<3;j++)if(bc_qmul_checked(r[i*3+j],scale[j],
                                     &dst->m[i*4+j])!=BC_RIG_OK)return BC_RIG_BAD;
    dst->m[i*4+3]=loc[i];
  }
  return BC_RIG_OK;
}
static int bc_near(int a,int b,int epsilon)
{
  return a>=b-epsilon && a<=b+epsilon;
}
/* Only inverse rigid orthonormal bases. Scaling/shearing require a later affine vendor. */
int bc_rig_inverse_rigid(const BC_RigMat *src,BC_RigMat *dst)
{
  unsigned int i,j,k;int dot,term,sum,minus;
  BC_RigMat r;
  for(j=0U;j<3U;j++)if(src->m[12U+j]!=0)return BC_RIG_UNSUPPORTED;
  if(src->m[15]!=BC_RIG_ONE)return BC_RIG_UNSUPPORTED;
  for(i=0U;i<3U;i++)for(j=0U;j<3U;j++){
    dot=0;
    for(k=0U;k<3U;k++){
      if(bc_qmul_checked(src->m[k*4U+i],src->m[k*4U+j],&term)!=0 ||
         bc_qadd_checked(dot,term,&dot)!=0)return BC_RIG_BAD;
    }
    if(!bc_near(dot,i==j?BC_RIG_ONE:0,256))return BC_RIG_UNSUPPORTED;
  }
  bc_rig_mat_identity(&r);
  for(i=0U;i<3U;i++)for(j=0U;j<3U;j++)r.m[i*4U+j]=src->m[j*4U+i];
  for(i=0U;i<3U;i++){
    sum=0;
    for(j=0U;j<3U;j++){
      if(bc_qmul_checked(r.m[i*4U+j],src->m[j*4U+3U],&term)!=0 ||
         bc_qadd_checked(sum,term,&sum)!=0)return BC_RIG_BAD;
    }
    if(sum==INT_MIN)return BC_RIG_BAD;
    minus=-sum;r.m[i*4U+3U]=minus;
  }
  *dst=r;
  return BC_RIG_OK;
}
int bc_rig_deform_matrix(const BC_RigMat *pose,const BC_RigMat *rest,BC_RigMat *dst)
{
  BC_RigMat inv;
  int r=bc_rig_inverse_rigid(rest,&inv);
  return r==BC_RIG_OK?bc_rig_mat_mul(pose,&inv,dst):r;
}
/* Parent indices must refer to already-computed bones (root parent=-1). */
int bc_rig_world(const BC_RigMat *local,const int *parent,unsigned int count,BC_RigMat *world)
{
  unsigned int i;
  if(count>BC_RIG_MAX_BONES)return BC_RIG_BAD;
  for(i=0U;i<count;i++){
    if(parent[i]<-1 || (parent[i]>=0 && (unsigned int)parent[i]>=i))return BC_RIG_UNSUPPORTED;
    if(parent[i]<0)world[i]=local[i];
    else if(bc_rig_mat_mul(&world[(unsigned int)parent[i]],&local[i],&world[i])!=0)
      return BC_RIG_BAD;
  }
  return BC_RIG_OK;
}
int bc_rig_skin_point(const BC_RigMat *deform,unsigned int nmat,const int source[3],
                      const BC_RigInfluence *weights,unsigned int count,int result[3])
{
  unsigned int i,j,wt=0U,remaining=65536U,part,last_positive=0U;
  int transformed[3],total[3]={0,0,0},factor,term;
  if(count>BC_RIG_MAX_WEIGHTS)return BC_RIG_UNSUPPORTED;
  if(count==0U){for(j=0U;j<3U;j++)result[j]=source[j];return BC_RIG_OK;}
  for(i=0U;i<count;i++){
    if(weights[i].bone>=nmat || weights[i].weight<0 || weights[i].weight>65536 ||
       wt>65536U*BC_RIG_MAX_WEIGHTS-(unsigned int)weights[i].weight)return BC_RIG_BAD;
    wt+=(unsigned int)weights[i].weight;
    if(weights[i].weight>0)last_positive=i;
  }
  if(wt==0U){for(j=0U;j<3U;j++)result[j]=source[j];return BC_RIG_OK;}
  for(i=0U;i<count;i++){
    if(i==last_positive)factor=(int)remaining;
    else {
      /* Each ratio is Q16, with remainder assigned to last influence. */
      if(bc_qdiv_checked(weights[i].weight,(int)wt,&factor)!=0)return BC_RIG_BAD;
      if(factor<0 || (unsigned int)factor>remaining)return BC_RIG_BAD;
    }
    part=(unsigned int)factor;remaining-=part;
    if(bc_rig_mat_point(&deform[weights[i].bone],source,transformed)!=0)return BC_RIG_BAD;
    for(j=0U;j<3U;j++){
      if(bc_qmul_checked(transformed[j],factor,&term)!=0 ||
         bc_qadd_checked(total[j],term,&total[j])!=0)return BC_RIG_BAD;
    }
  }
  for(j=0U;j<3U;j++)result[j]=total[j];
  return BC_RIG_OK;
}

/* Affine inverse with 3x3 adjugate/determinant in checked Q16.16 arithmetic.
   Reject singular/overflowing input and non-affine bottom rows. */
int bc_rig_inverse_affine(const BC_RigMat *src, BC_RigMat *dst)
{
  BC_RigMat result;
  int cof[9],det=0,x,y,z,prod,t;
  unsigned int i,j,k;
  const int *a=src->m;
  if(a[12]!=0 ||a[13]!=0 ||a[14]!=0 ||a[15]!=BC_RIG_ONE)
    return BC_RIG_UNSUPPORTED;
#define CQSUB(A,B,OUT) do { \
    if((B)==INT_MIN ||bc_qadd_checked((A),-(B),&(OUT))!=BC_RIG_OK) \
      return BC_RIG_BAD; \
  } while(0)
#define CQDET(I,J,K,L,OUT) do { \
    if(bc_qmul_checked(a[(I)],a[(J)],&x)!=BC_RIG_OK || \
       bc_qmul_checked(a[(K)],a[(L)],&y)!=BC_RIG_OK) \
      return BC_RIG_BAD; \
    CQSUB(x,y,(OUT)); \
  } while(0)
  CQDET(5,10,6,9,cof[0]);
  CQDET(6,8,4,10,cof[1]);
  CQDET(4,9,5,8,cof[2]);
  CQDET(2,9,1,10,cof[3]);
  CQDET(0,10,2,8,cof[4]);
  CQDET(1,8,0,9,cof[5]);
  CQDET(1,6,2,5,cof[6]);
  CQDET(2,4,0,6,cof[7]);
  CQDET(0,5,1,4,cof[8]);
#undef CQDET
#undef CQSUB
  for(j=0U;j<3U;j++){
    if(bc_qmul_checked(a[j],cof[j],&prod)!=BC_RIG_OK ||
       bc_qadd_checked(det,prod,&det)!=BC_RIG_OK)return BC_RIG_BAD;
  }
  if(det>-128 && det<128)return BC_RIG_UNSUPPORTED;
  bc_rig_mat_identity(&result);
  for(i=0U;i<3U;i++)for(j=0U;j<3U;j++){
    if(bc_qdiv_checked(cof[j*3U+i],det,&result.m[i*4U+j])!=BC_RIG_OK)
      return BC_RIG_BAD;
  }
  for(i=0U;i<3U;i++){
    z=0;
    for(k=0U;k<3U;k++){
      if(bc_qmul_checked(result.m[i*4U+k],a[k*4U+3U],&t)!=BC_RIG_OK ||
         bc_qadd_checked(z,t,&z)!=BC_RIG_OK)return BC_RIG_BAD;
    }
    if(z==INT_MIN)return BC_RIG_BAD;
    result.m[i*4U+3U]=-z;
  }
  *dst=result;
  return BC_RIG_OK;
}
