#include "c4d89.h"
/* C89 strict, 32-bit-safe Q16.16 skinning. The caller must supply validated
   weights and bone skin matrices (animated_global * inverse_bind). This is
   NOT a decoder of native Cinema 4D Weight tags. */
#define QMAX 2147483647L
#define QMIN (-2147483647L-1L)
static int checked_add(long a,long b,long *out)
{
    if(a>QMAX||a<QMIN||b>QMAX||b<QMIN)return 0;
    if(b>0L && a>QMAX-b)return 0;
    if(b<0L && a<QMIN-b)return 0;
    *out=a+b;return 1;
}
static int add_unsigned(unsigned long *acc,unsigned long x,unsigned long limit)
{
    if(x>limit||*acc>limit-x)return 0;
    *acc+=x;return 1;
}
int c4d89_fixed_mul(long a,long b,long *out)
{
    unsigned long aa,bb,ah,al,bh,bl,res,lim,part;
    int neg;
    if(!out||a>QMAX||a<QMIN||b>QMAX||b<QMIN)return C4D89_ERR_LIMIT;
    neg=(a<0L)!=(b<0L);
    aa=(a<0L)?(unsigned long)(-(a+1L))+1ul:(unsigned long)a;
    bb=(b<0L)?(unsigned long)(-(b+1L))+1ul:(unsigned long)b;
    lim=neg?2147483648ul:2147483647ul;
    ah=aa>>16;al=aa&65535ul;bh=bb>>16;bl=bb&65535ul;
    res=0ul;
    if(ah&&bh) {
        if(ah>32768ul||bh>32768ul||ah*bh>lim/65536ul)return C4D89_ERR_LIMIT;
        part=ah*bh*65536ul;
        if(!add_unsigned(&res,part,lim))return C4D89_ERR_LIMIT;
    }
    if(ah&&bl){
        if(ah>lim/bl)return C4D89_ERR_LIMIT;
        if(!add_unsigned(&res,ah*bl,lim))return C4D89_ERR_LIMIT;
    }
    if(bh&&al){
        if(bh>lim/al)return C4D89_ERR_LIMIT;
        if(!add_unsigned(&res,bh*al,lim))return C4D89_ERR_LIMIT;
    }
    /* 16-bit halves each: product fits unsigned 32-bit even on MinGW32. */
    part=(al*bl)>>16;
    if(!add_unsigned(&res,part,lim))return C4D89_ERR_LIMIT;
    if(neg) {
        *out=res==2147483648ul?QMIN:-(long)res;
    } else *out=(long)res;
    return C4D89_OK;
}
static int affine_point(const long *m,const long *xyz,long out[3])
{
    unsigned int c,i;
    long t,v;
    for(c=0u;c<3u;c++) {
        v=m[c];
        for(i=0u;i<3u;i++) {
            if(c4d89_fixed_mul(m[3u+3u*i+c],xyz[i],&t)!=C4D89_OK ||
               !checked_add(v,t,&v))return C4D89_ERR_LIMIT;
        }
        out[c]=v;
    }
    return C4D89_OK;
}
int c4d89_skin_point(const long xyz[3],const C4D89_SkinVertex *weights,
                     const long *joint_matrices,unsigned int joint_count,
                     long result_xyz[3])
{
    long acc[3]={0L,0L,0L},position[3],term;
    unsigned int i,c;long sum=0L;
    if(!xyz||!weights||!joint_matrices||!result_xyz||
       joint_count==0u||weights->count==0u||
       weights->count>C4D89_SKIN_MAX_INFLUENCES)return C4D89_ERR_CORRUPT;
    for(i=0u;i<weights->count;i++) {
        const C4D89_Influence *inf=&weights->influences[i];
        if(inf->joint>=joint_count||inf->weight_q16<0L||
           inf->weight_q16>C4D89_QONE||
           !checked_add(sum,inf->weight_q16,&sum))return C4D89_ERR_CORRUPT;
    }
    /* Do not conceal missing influences by normalizing invalid weights. */
    if(sum!=C4D89_QONE)return C4D89_ERR_CORRUPT;
    for(i=0u;i<weights->count;i++) {
        const C4D89_Influence *inf=&weights->influences[i];
        const long *m=joint_matrices+(unsigned long)inf->joint*12ul;
        if(affine_point(m,xyz,position)!=C4D89_OK)return C4D89_ERR_LIMIT;
        for(c=0u;c<3u;c++) {
            if(c4d89_fixed_mul(position[c],inf->weight_q16,&term)!=C4D89_OK ||
               !checked_add(acc[c],term,&acc[c]))return C4D89_ERR_LIMIT;
        }
    }
    for(c=0u;c<3u;c++)result_xyz[c]=acc[c];
    return C4D89_OK;
}
