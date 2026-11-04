/* Source Studio seqdesc weights, posekeys and layer curves. Independent C89
 * vendor. No heap, float, 64-bit integers or global mutable state.
 * Compare source-sdk-2013: bone_setup.cpp Studio_LocalPoseParameter,
 * AddSequenceLayers, BlendBones/SlerpBones; public/studio.h seqdesc layout.
 */
#include "hl2modelib.h"
#include <string.h>
#include <limits.h>
#define L89_ONE HL2M_FIXED_ONE
#define L89_SEQSIZE 212u

static int l89_has(unsigned int o,unsigned int n,unsigned int size)
{ return o<=size && n<=size-o; }
static int l89_read32(const hl2m_u8 *s,unsigned int size,unsigned int off,int *r)
{
    unsigned int v;
    if(!l89_has(off,4u,size))return HL2M_ERR_RANGE;
    v=(unsigned int)s[off]|((unsigned int)s[off+1u]<<8)|
        ((unsigned int)s[off+2u]<<16)|((unsigned int)s[off+3u]<<24);
    *r=(int)v;return HL2M_OK;
}
static int l89_read16(const hl2m_u8 *s,unsigned int size,unsigned int off,int *r)
{
    unsigned int v;if(!l89_has(off,2u,size))return HL2M_ERR_RANGE;
    v=(unsigned int)s[off]|((unsigned int)s[off+1u]<<8);
    *r=(v&32768u)?(int)v-65536:(int)v;return HL2M_OK;
}
/* float bit pattern -> signed Q16.16, exact bit-level integer conversion. */
static hl2m_fixed l89_float_q(int raw)
{
    unsigned int x=(unsigned int)raw,exp=(x>>23)&255u,man=x&8388607u;
    unsigned int mag;int shift,neg=(x>>31)!=0;
    if(exp==255u)return neg?INT_MIN:INT_MAX;
    if(exp==0u){if(!man)return 0;shift=-133;}
    else{man|=8388608u;shift=(int)exp-134;}
    if(shift>=0){
        if(shift>=32 || man>(2147483647u>>(unsigned int)shift))return neg?INT_MIN:INT_MAX;
        mag=man<<(unsigned int)shift;
    }else{shift=-shift;mag=shift>=32?0u:man>>(unsigned int)shift;}
    return neg?-(hl2m_fixed)mag:(hl2m_fixed)mag;
}
static int l89_fixed(const hl2m_u8 *d,unsigned int size,unsigned int off,hl2m_fixed *result)
{int bits,r=l89_read32(d,size,off,&bits);if(r)return r;*result=l89_float_q(bits);return HL2M_OK;}
static hl2m_fixed l89_mul(hl2m_fixed a,hl2m_fixed b)
{
    unsigned int u,v,h,l;int neg=0;
    if(a<0){u=(unsigned int)(-(a+1))+1u;neg=!neg;}else u=(unsigned int)a;
    if(b<0){v=(unsigned int)(-(b+1))+1u;neg=!neg;}else v=(unsigned int)b;
    h=(u>>16)*(v>>16)*65536u+(u>>16)*(v&65535u)+(v>>16)*(u&65535u);
    l=((u&65535u)*(v&65535u))>>16;
    return neg?-(hl2m_fixed)(h+l):(hl2m_fixed)(h+l);
}
static hl2m_fixed l89_div(hl2m_fixed a,hl2m_fixed b)
{
    unsigned int aa,bb,quo,rem,frac=0u;int i,neg=0;
    if(!b)return 0;
    if(a<0){aa=(unsigned int)(-(a+1))+1u;neg=!neg;}else aa=(unsigned int)a;
    if(b<0){bb=(unsigned int)(-(b+1))+1u;neg=!neg;}else bb=(unsigned int)b;
    quo=aa/bb;rem=aa%bb;
    if(quo>=32768u)return neg?INT_MIN:INT_MAX;
    for(i=0;i<16;i++){
        /* avoid overflow even when divisor approaches INT_MAX */
        frac<<=1;
        if(rem>=bb-rem){rem=rem-(bb-rem);frac|=1u;}
        else rem+=rem;
    }
    return neg?-(hl2m_fixed)((quo<<16)|frac):(hl2m_fixed)((quo<<16)|frac);
}
static hl2m_fixed l89_clamp(hl2m_fixed x)
{ return x<0?0:(x>L89_ONE?L89_ONE:x); }

int hl2m_sequence_metadata89(const hl2m_u8 *data,unsigned int size,
    const hl2m_mdl *mdl,int sequence,hl2m_seq_metadata89 *out)
{
    int base,seqindex,n,rel,i,r,j,flags;
    unsigned int off,start;
    if(!data || !mdl || !out)return HL2M_ERR_NULL;
    if(sequence<0 || sequence>=mdl->sequence_count || mdl->bone_count<0 ||
       mdl->bone_count>HL2M_MAX_BONES)return HL2M_ERR_RANGE;
    r=l89_read32(data,size,192u,&seqindex);if(r)return r;
    if(seqindex<0 || (unsigned int)seqindex>size ||
       (unsigned int)sequence>(size-(unsigned int)seqindex)/L89_SEQSIZE)return HL2M_ERR_RANGE;
    base=seqindex+sequence*(int)L89_SEQSIZE;
    if(!l89_has((unsigned int)base,L89_SEQSIZE,size))return HL2M_ERR_RANGE;
    memset(out,0,sizeof(*out));out->sequence=sequence;out->bone_count=mdl->bone_count;
    r=l89_read32(data,size,(unsigned int)base+12u,&out->flags);if(r)return r;
    for(i=0;i<out->bone_count;i++)out->bone_weights[i]=L89_ONE;
    r=l89_read32(data,size,(unsigned int)base+156u,&rel);if(r)return r;
    if(rel>0){
        start=(unsigned int)base+(unsigned int)rel;
        if(start<(unsigned int)base || !l89_has(start,(unsigned int)mdl->bone_count*4u,size))return HL2M_ERR_RANGE;
        out->has_weightlist=1;
        for(i=0;i<out->bone_count;i++){
            r=l89_fixed(data,size,start+(unsigned int)i*4u,&out->bone_weights[i]);if(r)return r;
            out->bone_weights[i]=l89_clamp(out->bone_weights[i]);
        }
    }
    r=l89_read32(data,size,(unsigned int)base+148u,&n);if(r)return r;
    r=l89_read32(data,size,(unsigned int)base+152u,&rel);if(r)return r;
    if(n<0 || n>HL2M_SEQ_MAX_LAYERS89)return HL2M_ERR_LIMIT;
    if(n){
        if(rel<=0)return HL2M_ERR_RANGE;
        start=(unsigned int)base+(unsigned int)rel;
        if(start<(unsigned int)base || !l89_has(start,(unsigned int)n*24u,size))return HL2M_ERR_RANGE;
        for(i=0;i<n;i++){
            hl2m_seq_layer89 *layer=&out->layers[i];
            off=start+(unsigned int)i*24u;
            r=l89_read16(data,size,off,&layer->sequence);if(r)return r;
            r=l89_read16(data,size,off+2u,&layer->pose_index);if(r)return r;
            r=l89_read32(data,size,off+4u,&flags);if(r)return r;layer->flags=flags;
            r=l89_fixed(data,size,off+8u,&layer->start);if(r)return r;
            r=l89_fixed(data,size,off+12u,&layer->peak);if(r)return r;
            r=l89_fixed(data,size,off+16u,&layer->tail);if(r)return r;
            r=l89_fixed(data,size,off+20u,&layer->end);if(r)return r;
        }
    }
    out->layer_count=n;
    r=l89_read32(data,size,(unsigned int)base+160u,&rel);if(r)return r;
    if(rel>0){
        const hl2m_mdl_seqdesc *sq=&mdl->sequences[sequence];
        int stride=sq->groupsize[0], count1=sq->groupsize[1];
        if(stride>0 && stride<=64 && count1>0 && count1<=64){
            /* studio.h: poseKey(axis, cell) = float[axis*groupsize[0]+cell].
               The second axis has its own count, but shares the first stride. */
            start=(unsigned int)base+(unsigned int)rel;
            if(start<(unsigned int)base ||
               !l89_has(start,(unsigned int)(stride+count1)*4u,size))
                return HL2M_ERR_RANGE;
            out->has_posekeys=1;
            for(i=0;i<2;i++){
                int nkeys=i?count1:stride;
                for(j=0;j<nkeys;j++){
                    r=l89_fixed(data,size,start+(unsigned int)(i*stride+j)*4u,
                        &out->posekeys[i][j]);
                    if(r)return r;
                }
            }
        }
    }
    return HL2M_OK;
}

int hl2m_sequence_pose_axis89(const hl2m_mdl_seqdesc *seq,
    const hl2m_seq_metadata89 *meta,int axis,hl2m_fixed physical,
    hl2m_fixed *normalized)
{
    int count,j;hl2m_fixed begin,end,t;
    if(!seq || !meta || !normalized)return HL2M_ERR_NULL;
    if(axis<0 || axis>1)return HL2M_ERR_RANGE;
    count=seq->groupsize[axis];
    if(count<=1){*normalized=0;return HL2M_OK;}
    if(count>64)return HL2M_ERR_LIMIT;
    if(meta->has_posekeys){
        /* Valve compares physical pose settings to pose keys per grid axis. */
        for(j=0;j<count-2;j++)if(physical<meta->posekeys[axis][j+1])break;
        begin=meta->posekeys[axis][j];end=meta->posekeys[axis][j+1];
        t=(end!=begin)?l89_clamp(l89_div(physical-begin,end-begin)):0;
    }else{
        begin=seq->paramstart[axis];end=seq->paramend[axis];
        t=(end!=begin)?l89_clamp(l89_div(physical-begin,end-begin)):0;
        /* A non-keyed parameter is a normalized continuum over all cells. */
        *normalized=t;return HL2M_OK;
    }
    /* Avoid wide multiply (j+t)*1/count; count <=64. */
    *normalized=l89_div(((hl2m_fixed)j*L89_ONE)+t,(count-1)*L89_ONE);
    return HL2M_OK;
}

int hl2m_sequence_layer_envelope89(const hl2m_seq_layer89 *layer,
    hl2m_fixed base_cycle,hl2m_fixed base_weight,
    hl2m_fixed *layer_cycle,hl2m_fixed *layer_weight)
{
    hl2m_fixed s=L89_ONE,c=base_cycle,x;
    if(!layer || !layer_cycle || !layer_weight)return HL2M_ERR_NULL;
    if(base_weight<0 || base_weight>L89_ONE)return HL2M_ERR_RANGE;
    /* Layer driven by pose requires explicit pose parameter; reject rather
       than apply cycle erroneously. Caller may supply that pose elsewhere. */
    if(layer->flags&HL2M_SEQ_LAYER_POSE89)return HL2M_ERR_UNSUPPORTED;
    if(layer->start!=layer->end){
        if(base_cycle<layer->start || base_cycle>=layer->end){
            *layer_cycle=0;*layer_weight=0;return HL2M_OK;
        }
        if(base_cycle<layer->peak && layer->peak!=layer->start)
            s=l89_clamp(l89_div(base_cycle-layer->start,layer->peak-layer->start));
        else if(base_cycle>layer->tail && layer->end!=layer->tail)
            s=l89_clamp(l89_div(layer->end-base_cycle,layer->end-layer->tail));
        if(layer->flags&HL2M_SEQ_LAYER_SPLINE89){
            x=l89_mul(s,s);s=l89_mul(x,3*L89_ONE-2*s);
        }
        c=l89_div(base_cycle-layer->start,layer->end-layer->start);
    }
    if((layer->flags&HL2M_SEQ_LAYER_XFADE89) && base_cycle>layer->tail){
        hl2m_fixed w=l89_mul(s,base_weight),den=L89_ONE-base_weight+w;
        *layer_weight=den?l89_clamp(l89_div(w,den)):0;
    }else if(layer->flags&HL2M_SEQ_LAYER_NOBLEND89)
        *layer_weight=s;
    else *layer_weight=l89_mul(base_weight,s);
    *layer_cycle=l89_clamp(c);return HL2M_OK;
}

/* v1.0.20: Source AddSequenceLayers() uses the physical pose parameter
   instead of the parent's animation cycle for STUDIO_AL_POSE envelopes.
   A pose-driven layer still inherits its parent's cycle for animation time. */
int hl2m_sequence_layer_envelope_control120(const hl2m_seq_layer89 *layer,
    hl2m_fixed base_cycle,hl2m_fixed base_weight,hl2m_fixed pose_physical,
    hl2m_fixed *layer_cycle,hl2m_fixed *layer_weight)
{
    hl2m_seq_layer89 temporary;
    hl2m_fixed index,ignored_cycle;
    int r;
    if(!layer || !layer_cycle || !layer_weight)return HL2M_ERR_NULL;
    if(!(layer->flags&HL2M_SEQ_LAYER_POSE89))
        return hl2m_sequence_layer_envelope89(layer,base_cycle,base_weight,
            layer_cycle,layer_weight);
    /* Evaluate the envelope at the physical parameter, but retain cycle. */
    temporary=*layer;
    temporary.flags&=~HL2M_SEQ_LAYER_POSE89;
    index=pose_physical;
    r=hl2m_sequence_layer_envelope89(&temporary,index,base_weight,
        &ignored_cycle,layer_weight);
    if(r)return r;
    *layer_cycle=base_cycle;
    return HL2M_OK;
}

/* Matrix operations are strictly limited to rigid local transforms. Delta
   input must be identity-relative: neutral translation=0 and rotation=I. */
static int l89_compatible(const hl2m_pose_matrices *a,const hl2m_pose_matrices *b)
{
    int i;if(a->bone_count!=b->bone_count || a->bone_count<0 || a->bone_count>HL2M_MAX_BONES)return 0;
    for(i=0;i<a->bone_count;i++)if(a->parent[i]!=b->parent[i])return 0;
    return 1;
}
static void l89_compose(const hl2m_mat3x4 *a,const hl2m_mat3x4 *b,hl2m_mat3x4 *out)
{
    hl2m_mat3x4 tmp;int i,j;
    for(i=0;i<3;i++)for(j=0;j<4;j++){
        tmp.m[i][j]=l89_mul(a->m[i][0],b->m[0][j])+
            l89_mul(a->m[i][1],b->m[1][j])+
            l89_mul(a->m[i][2],b->m[2][j]);
        if(j==3)tmp.m[i][j]+=a->m[i][3];
    }
    *out=tmp;
}
static int l89_rebuild(hl2m_pose_matrices *p)
{
    int i,par;for(i=0;i<p->bone_count;i++){
        par=p->parent[i];if(par>=i || par< -1)return HL2M_ERR_PARSE;
        if(par>=0)l89_compose(&p->world[par],&p->local[i],&p->world[i]);
        else p->world[i]=p->local[i];
    }
    return HL2M_OK;
}
int hl2m_sequence_blend_mask89(const hl2m_pose_matrices *base,
    const hl2m_pose_matrices *overlay,const hl2m_fixed *bone_weights,
    hl2m_fixed strength,int flags,hl2m_pose_matrices *out)
{
    hl2m_pose_matrices result;hl2m_mat3x4 neutral,step;
    int i,j,r;hl2m_fixed w;
    if(!base || !overlay || !out)return HL2M_ERR_NULL;
    if(!l89_compatible(base,overlay) || strength<0 || strength>L89_ONE)return HL2M_ERR_RANGE;
    result=*base;memset(&neutral,0,sizeof(neutral));
    for(i=0;i<3;i++)neutral.m[i][i]=L89_ONE;
    for(i=0;i<base->bone_count;i++){
        w=l89_mul(strength,bone_weights?l89_clamp(bone_weights[i]):L89_ONE);
        if(w==0)continue;
        if(flags&HL2M_SEQ_DELTA89){
            r=hl2m_quaternion_blend_matrix89(&neutral,&overlay->local[i],w,&step);
            if(r)return r;
            for(j=0;j<3;j++)step.m[j][3]=l89_mul(overlay->local[i].m[j][3],w);
            if(flags&HL2M_SEQ_POST89)l89_compose(&result.local[i],&step,&result.local[i]);
            else l89_compose(&step,&result.local[i],&result.local[i]);
            /* Source adds translations in the parent frame rather than
               rotating the existing offset by delta: preserve base pos. */
            for(j=0;j<3;j++)result.local[i].m[j][3]=base->local[i].m[j][3]+step.m[j][3];
        }else{
            r=hl2m_quaternion_blend_matrix89(&base->local[i],&overlay->local[i],w,&result.local[i]);
            if(r)return r;
        }
    }
    r=l89_rebuild(&result);if(r)return r;
    *out=result;return HL2M_OK;
}
