/* Source 2013 studiohdr_t local pose parameter descriptors (v1.0.21).
   Based on public/studio.h mstudioposeparamdesc_t and bone_setup.cpp
   Studio_LocalPoseParameter / Studio_SetPoseParameter.
   Fixed point Q16.16, 32-bit only, no allocation or float expressions. */
#include "hl2modelib.h"
#include <string.h>
#include <limits.h>

#define P121_ONE HL2M_FIXED_ONE
#define P121_HEADER_COUNT 300u
#define P121_HEADER_OFFSET 304u
#define P121_DESC_SIZE 20u

static int p121_span(unsigned int o,unsigned int n,unsigned int size)
{ return o<=size && n<=size-o; }
static unsigned int p121_u32(const hl2m_u8 *d,unsigned int o)
{ return (unsigned int)d[o]|((unsigned int)d[o+1]<<8)|
    ((unsigned int)d[o+2]<<16)|((unsigned int)d[o+3]<<24); }
static int p121_i32(const hl2m_u8 *d,unsigned int o)
{ return (int)p121_u32(d,o); }
static hl2m_fixed p121_from_float(int raw)
{
    unsigned int bits=(unsigned int)raw,exp=(bits>>23)&255u;
    unsigned int mant=bits&8388607u,mag;int shift,neg=(bits>>31)!=0;
    if(exp==255u)return neg?INT_MIN:INT_MAX;
    if(exp==0u){if(mant==0u)return 0;shift=-133;}
    else{mant|=8388608u;shift=(int)exp-134;}
    if(shift>=0){
        if(shift>=32 || mant>(2147483647u>>(unsigned int)shift))
            return neg?INT_MIN:INT_MAX;
        mag=mant<<(unsigned int)shift;
    }else{
        shift=-shift;mag=shift>=32?0u:mant>>(unsigned int)shift;
    }
    return neg?-(hl2m_fixed)mag:(hl2m_fixed)mag;
}
static unsigned int p121_abs(int x)
{ return x<0?0u-(unsigned int)x:(unsigned int)x; }
/* a * normalized, 0 <= normalized <= ONE, saturating signed Q16.16. */
static hl2m_fixed p121_mul_unit(hl2m_fixed a,hl2m_fixed normalized)
{
    unsigned int m=p121_abs(a),n=(unsigned int)normalized,h,l,lim;
    int negative=a<0;
    h=(m>>16)*n;
    l=((m&65535u)*n)>>16;
    lim=negative?2147483648u:2147483647u;
    if(h>lim || l>lim-h)return negative?INT_MIN:INT_MAX;
    if(negative && h+l==2147483648u)return INT_MIN;
    return negative?-(hl2m_fixed)(h+l):(hl2m_fixed)(h+l);
}
/* Compute a/b in Q16.16 using restoring division; no 64-bit intermediate. */
static hl2m_fixed p121_div(hl2m_fixed a,hl2m_fixed b)
{
    unsigned int aa,bb,quot,rem,frac=0u,lim;int i,neg;
    if(b==0)return 0;
    neg=(a<0)!=(b<0);aa=p121_abs(a);bb=p121_abs(b);
    quot=aa/bb;rem=aa%bb;
    lim=neg?32768u:32767u;
    if(quot>lim)return neg?INT_MIN:INT_MAX;
    for(i=0;i<16;i++){
        frac<<=1;
        if(rem>=bb-rem){rem-=bb-rem;frac|=1u;}
        else rem+=rem;
    }
    if(quot==32768u)return INT_MIN;
    return neg?-(hl2m_fixed)((quot<<16)|frac):(hl2m_fixed)((quot<<16)|frac);
}
/* Studio_SetPoseParameter wraps into a centered interval before clamping. */
static int p121_wrap(hl2m_fixed value,const hl2m_poseparam121 *desc,
    hl2m_fixed *wrapped)
{
    hl2m_fixed shift,center,loop=desc->loop;int sum,adjusted,rem;
    if(loop<=0){*wrapped=value;return HL2M_OK;}
    /* center = ((start+end)/2 + loop/2), protect additions. */
    if((desc->start>0 && desc->end>INT_MAX-desc->start) ||
       (desc->start<0 && desc->end<INT_MIN-desc->start))return HL2M_ERR_RANGE;
    sum=desc->start+desc->end;
    center=sum/2+loop/2;
    shift=loop-center;
    if((shift>0 && value>INT_MAX-shift) ||
       (shift<0 && value<INT_MIN-shift))return HL2M_ERR_RANGE;
    adjusted=value+shift;
    rem=adjusted%loop;
    if(rem<0)rem+=loop;
    if((shift>0 && rem<INT_MIN+shift) ||
       (shift<0 && rem>INT_MAX+shift))return HL2M_ERR_RANGE;
    *wrapped=rem-shift;
    return HL2M_OK;
}
int hl2m_poseparams_read121(const hl2m_u8 *bytes,unsigned int size,
    hl2m_poseparam121 *out,unsigned int capacity,unsigned int *count)
{
    unsigned int i,off,n,index,name_offset,k,base;
    if(!bytes || !count)return HL2M_ERR_NULL;
    if(!p121_span(0u,308u,size) || memcmp(bytes,"IDST",4)!=0)
        return HL2M_ERR_MAGIC;
    n=p121_u32(bytes,P121_HEADER_COUNT);
    index=p121_u32(bytes,P121_HEADER_OFFSET);
    if(n>HL2M_POSEPARAM_CAP121 || n>capacity)return HL2M_ERR_LIMIT;
    if(n && (!out || !p121_span(index,n*P121_DESC_SIZE,size)))
        return HL2M_ERR_RANGE;
    for(i=0;i<n;i++){
        hl2m_poseparam121 *p=&out[i];
        base=index+i*P121_DESC_SIZE;
        memset(p,0,sizeof(*p));
        p->flags=p121_i32(bytes,base+4u);
        p->start=p121_from_float(p121_i32(bytes,base+8u));
        p->end=p121_from_float(p121_i32(bytes,base+12u));
        p->loop=p121_from_float(p121_i32(bytes,base+16u));
        off=p121_u32(bytes,base);
        if(off && off<size-base){
            name_offset=base+off;
            for(k=0;k<HL2M_NAME_MAX-1 && name_offset+k<size;k++){
                char c=(char)bytes[name_offset+k];
                if(c==0)break;
                p->name[k]=c;
            }
            p->name[k]=0;
        }
    }
    *count=n;
    return HL2M_OK;
}
int hl2m_pose_control_from_physical121(const hl2m_poseparam121 *desc,
    hl2m_fixed physical,hl2m_fixed *normalized)
{
    hl2m_fixed wrapped,v,range;int r;
    if(!desc || !normalized)return HL2M_ERR_NULL;
    r=p121_wrap(physical,desc,&wrapped);if(r)return r;
    if(desc->end<desc->start)return HL2M_ERR_RANGE;
    if(desc->start<0 && desc->end>INT_MAX+desc->start)
        return HL2M_ERR_RANGE;
    range=desc->end-desc->start;
    if(range==0){*normalized=0;return HL2M_OK;}
    if(wrapped<=desc->start){*normalized=0;return HL2M_OK;}
    if(wrapped>=desc->end){*normalized=P121_ONE;return HL2M_OK;}
    v=p121_div(wrapped-desc->start,range);
    *normalized=v<0?0:(v>P121_ONE?P121_ONE:v);
    return HL2M_OK;
}
int hl2m_pose_physical_from_control121(const hl2m_poseparam121 *desc,
    hl2m_fixed normalized,hl2m_fixed *physical)
{
    hl2m_fixed range,value;
    if(!desc || !physical)return HL2M_ERR_NULL;
    if(normalized<0 || normalized>P121_ONE || desc->end<desc->start)
        return HL2M_ERR_RANGE;
    if(desc->start<0 && desc->end>INT_MAX+desc->start)
        return HL2M_ERR_RANGE;
    range=desc->end-desc->start;
    value=p121_mul_unit(range,normalized);
    if((desc->start>0 && value>INT_MAX-desc->start) ||
       (desc->start<0 && value<INT_MIN-desc->start))return HL2M_ERR_RANGE;
    *physical=desc->start+value;
    return HL2M_OK;
}
/* Determine a sequence's two LOCAL axis values from the GLOBAL normalized
   controller vector. Crucially, different sequences can name different
   paramindex[] entries. An unused axis selects cell zero. */
int hl2m_pose_sequence_axes121(const hl2m_mdl *mdl,int sequence,
    const hl2m_poseparam121 *descs,unsigned int desc_count,
    const hl2m_fixed *normalized,unsigned int normalized_count,
    hl2m_fixed *physical_x,hl2m_fixed *physical_y)
{
    const hl2m_mdl_seqdesc *seq;int j,idx,r;
    hl2m_fixed *dest[2];
    if(!mdl || !physical_x || !physical_y)return HL2M_ERR_NULL;
    if(sequence<0 || sequence>=mdl->sequence_count)return HL2M_ERR_RANGE;
    seq=&mdl->sequences[sequence];dest[0]=physical_x;dest[1]=physical_y;
    for(j=0;j<2;j++){
        idx=seq->paramindex[j];
        if(idx<0 || seq->groupsize[j]<=1){*dest[j]=seq->paramstart[j];continue;}
        if(!descs || !normalized || (unsigned int)idx>=desc_count ||
           (unsigned int)idx>=normalized_count)return HL2M_ERR_RANGE;
        r=hl2m_pose_physical_from_control121(&descs[idx],normalized[idx],dest[j]);
        if(r)return r;
    }
    return HL2M_OK;
}
