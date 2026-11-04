/* Source 2007/2013 mstudioanim_t RLE sampler. C89, caller-owned data.
   Embedded MDL animation chunks, ANIMPOS/ANIMROT, raw Vector48.
   Unsupported quaternion raw channels and external blocks fail explicitly. */
#include "hl2modelib.h"
#include <string.h>
#include <limits.h>

#define A89_RAWPOS 1
#define A89_RAWROT 2
#define A89_ANIMPOS 4
#define A89_ANIMROT 8
#define A89_DELTA 16
#define A89_RAWROT2 32

static int a89_has(unsigned int off, unsigned int need, unsigned int n)
{ return off <= n && need <= n-off; }
static int a89_s16(const hl2m_u8 *p)
{ unsigned int u = (unsigned int)p[0] | ((unsigned int)p[1]<<8); return u>=32768u ? (int)u-65536 : (int)u; }
static int a89_i32(const hl2m_u8 *p)
{ unsigned int u=(unsigned int)p[0]|((unsigned int)p[1]<<8)|((unsigned int)p[2]<<16)|((unsigned int)p[3]<<24); return (int)u; }
/* 16.16 multiply with 16-bit halves, no int64 and no double. Operand
   magnitudes are bounded in animation channels / their scale metadata. */
static hl2m_fixed a89_mul(hl2m_fixed a, hl2m_fixed b)
{
    int neg=0; unsigned int aa,bb,hi,lo;
    if (a<0) {neg=!neg;aa=(unsigned int)(-(a+1))+1u;} else aa=(unsigned int)a;
    if (b<0) {neg=!neg;bb=(unsigned int)(-(b+1))+1u;} else bb=(unsigned int)b;
    hi=(aa>>16)*(bb>>16)*65536u+(aa>>16)*(bb&65535u)+(bb>>16)*(aa&65535u);
    lo=((aa&65535u)*(bb&65535u))>>16;
    return neg ? -(hl2m_fixed)(hi+lo) : (hl2m_fixed)(hi+lo);
}
/* IEEE binary16 to signed 16.16 without floating point, saturates extremes. */
/* Saturating addition prevents signed overflow when an asset stores absurd
   translation scales.  This does not reinterpret the asset as valid. */
static hl2m_fixed a89_add_sat(hl2m_fixed a,hl2m_fixed b)
{
    if(b>0 && a>INT_MAX-b)return INT_MAX;
    if(b<0 && a<INT_MIN-b)return INT_MIN;
    return a+b;
}
static int a89_outlier(hl2m_fixed v,hl2m_fixed limit)
{
    return v>limit || v< -limit;
}
static hl2m_fixed a89_half(hl2m_u8 a, hl2m_u8 b)
{
    unsigned int bits=(unsigned int)a|((unsigned int)b<<8), mant=bits&1023u;
    int exponent=(int)((bits>>10)&31u), sh; unsigned int v;
    if(exponent==0) { if(!mant) return 0; sh=-8; }
    else {if(exponent==31) return (bits&32768u)? -2147483647 : 2147483647; mant+=1024u;sh=exponent-9;}
    if(sh>=0) {if(sh>20 || mant>2147483647u>>sh) v=2147483647u;else v=mant<<sh;}
    else {sh=-sh;v=sh>=32?0u:mant>>sh;}
    return (bits&32768u)?-(hl2m_fixed)v:(hl2m_fixed)v;
}
/* mstudioanimvalue_t: [valid,total] followed by `valid` signed shorts.
   A run holds its last sample for (total-valid) remaining frames. */
static int a89_rle(const hl2m_u8 *data,unsigned int size,unsigned int value_off,int frame,int *out)
{
    unsigned int p=value_off; int steps=0, valid,total;
    if(frame<0)return HL2M_ERR_RANGE;
    while(steps++<65536) {
        if(!a89_has(p,2u,size))return HL2M_ERR_RANGE;
        valid=(int)data[p];total=(int)data[p+1u];
        if(valid<1 || total<valid || total<1)return HL2M_ERR_PARSE;
        if(!a89_has(p,2u+2u*(unsigned int)valid,size))return HL2M_ERR_RANGE;
        if(frame<total) {
            if(frame>=valid)frame=valid-1;
            *out=a89_s16(data+p+2u+2u*(unsigned int)frame); return HL2M_OK;
        }
        frame-=total;p+=2u+2u*(unsigned int)valid;
    }
    return HL2M_ERR_LIMIT;
}
static int a89_axis(const hl2m_u8 *data,unsigned int size,unsigned int valueptr,int axis,int frame,int *out)
{
    int rel;
    if(!a89_has(valueptr,6u,size))return HL2M_ERR_RANGE;
    rel=a89_s16(data+valueptr+2u*(unsigned int)axis);
    if(rel==0){*out=0;return HL2M_OK;}
    if(rel<0 || !a89_has(valueptr,(unsigned int)rel,size))return HL2M_ERR_RANGE;
    return a89_rle(data,size,valueptr+(unsigned int)rel,frame,out);
}
/* Source SDK studio.cpp mstudioanimdesc_t::pAnim: resolve the
   correct per-section channel chain and LOCAL section frame.
   Supports embedded block 0; external .ani blocks require a separate VFS. */
/* Resolve Source section (studio.cpp) and optional external ANI block.
   All offsets are bounds-checked; no global VFS or heap is required. */
static int a89_resolve_section(const hl2m_u8 *data,unsigned int size,
  const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
  int anim,int frame,const hl2m_u8 **src,unsigned int *src_size,
  unsigned int *off,int *local_frame)
{
    const hl2m_mdl_animdesc *d=&mdl->anims[anim];
    int anim_table,desc_off,section,block,index,sec_off;
    unsigned int base,start;
    if(frame<0 || frame>=d->numframes)return HL2M_ERR_RANGE;
    /* Legacy callers may supply only a channel buffer with pre-resolved
       first_anim_offset (not an entire MDL). Keep that C89 API compatible. */
    if(d->sectionframes==0 && d->animblock==0 && d->first_anim_offset>=0 &&
       a89_has((unsigned int)d->first_anim_offset,4u,size)){
        *src=data;*src_size=size;*off=(unsigned int)d->first_anim_offset;
        *local_frame=frame;return HL2M_OK;
    }
    if(!a89_has(184u,4u,size))return HL2M_ERR_RANGE;
    anim_table=a89_i32(data+184u);
    if(anim_table<=0 || (unsigned int)anim_table>size ||
       (unsigned int)anim>(size-(unsigned int)anim_table)/100u)return HL2M_ERR_RANGE;
    desc_off=anim_table+anim*100;
    block=d->animblock;index=d->animindex;
    *local_frame=frame;
    if(d->sectionframes!=0){
        if(d->sectionframes<0 || d->sectionindex<=0)return HL2M_ERR_PARSE;
        if(d->numframes>d->sectionframes && frame==d->numframes-1){
            section=d->numframes/d->sectionframes+1;
            *local_frame=0;
        }else{
            section=frame/d->sectionframes;
            *local_frame=frame-section*d->sectionframes;
        }
        if(section<0 || section>65536)return HL2M_ERR_LIMIT;
        if((unsigned int)desc_off>size ||
           !a89_has((unsigned int)desc_off,(unsigned int)d->sectionindex,size))return HL2M_ERR_RANGE;
        base=(unsigned int)desc_off+(unsigned int)d->sectionindex;
        if(!a89_has(base,(unsigned int)(section+1)*8u,size))return HL2M_ERR_RANGE;
        sec_off=(int)base+section*8;
        block=a89_i32(data+(unsigned int)sec_off);
        index=a89_i32(data+(unsigned int)sec_off+4u);
    }
    if(block==0){
        if(index<=0 || (unsigned int)desc_off>size ||
            (unsigned int)index>=size-(unsigned int)desc_off)return HL2M_ERR_RANGE;
        *src=data;*src_size=size;
        *off=(unsigned int)desc_off+(unsigned int)index;
    }else{
        if(!ani)return HL2M_ERR_UNSUPPORTED;
        if(block<1 || block>=mdl->animblock_count || index<0)return HL2M_ERR_RANGE;
        if(mdl->animblocks[block].datastart<0)return HL2M_ERR_RANGE;
        start=(unsigned int)mdl->animblocks[block].datastart;
        if(start>ani_size || (unsigned int)index>ani_size-start)return HL2M_ERR_RANGE;
        *src=ani;*src_size=ani_size;
        *off=start+(unsigned int)index;
    }
    if(!a89_has(*off,4u,*src_size))return HL2M_ERR_RANGE;
    return HL2M_OK;
}

static hl2m_fixed *a89_component(hl2m_vec3 *p,int axis)
{ return axis==0?&p->x:(axis==1?&p->y:&p->z); }

static hl2m_fixed a89_read_comp(const hl2m_vec3 *p,int axis)
{ return axis==0?p->x:(axis==1?p->y:p->z); }

/* Compute a signed RLE sample multiplied by its ORIGINAL binary32 scale,
   converted to signed Q16.16.  No float/double, int64 or allocations.

   For a normal IEEE float, scale = mantissa * 2^(exp-150).
   Therefore sample*scale*65536 = (abs(sample)*mantissa) >> (134-exp).
   The 40-bit product is represented as two unsigned 32-bit words.
   This avoids rounding scales smaller than 1/65536 down to zero. */
static hl2m_fixed a89_scaled_ieee32(hl2m_u32 bits,int sample)
{
    unsigned int exponent=(bits>>23)&255u, mant=bits&8388607u;
    unsigned int mag,lo,hi,part,carry,shift,rhi,rlo,add;
    int negative, count;
    if(sample==0 || (bits&2147483647u)==0u)return 0;
    if(exponent==255u)return 0; /* malformed NaN/Infinity scale */
    negative=((bits>>31)&1u)!=0u;
    if(sample<0){negative=!negative;mag=(unsigned int)(-(sample+1))+1u;}
    else mag=(unsigned int)sample;
    if(exponent)mant|=8388608u;
    else exponent=1u; /* subnormal exponent: 1 - bias */
    /* The RLE input is signed 16-bit, so max 32768. */
    if(mag>32768u)return 0;
    lo=mag*(mant&65535u);
    part=mag*(mant>>16);
    hi=(part>>16);
    add=part<<16;
    carry=(lo+add)<lo ? 1u : 0u;
    lo+=add;hi+=carry;
    count=134-(int)exponent;
    if(count<0){
        /* Magnitudes that still fit Q16.16 must not be saturated. */
        unsigned int n=(unsigned int)(-count);
        if(n>30u || hi || lo>(2147483647u>>n))
            return negative ? (-2147483647-1) : 2147483647;
        rlo=lo<<n;
        return negative ? -(hl2m_fixed)rlo : (hl2m_fixed)rlo;
    }
    shift=(unsigned int)count;
    if(shift>63u)return 0;
    /* round product before right shift */
    if(shift>0u){
        if(shift<=32u){
            add=1u<<(shift-1u);
            carry=(lo+add)<lo ? 1u : 0u;
            lo+=add;hi+=carry;
        }else{hi+=1u<<(shift-33u);}
    }
    if(shift==0u){rhi=hi;rlo=lo;}
    else if(shift<32u){rhi=hi>>shift;rlo=(lo>>shift)|(hi<<(32u-shift));}
    else if(shift==32u){rhi=0u;rlo=hi;}
    else{rhi=0u;rlo=hi>>(shift-32u);}
    if(rhi || rlo>2147483647u){
        return negative ? (-2147483647-1) : 2147483647;
    }
    return negative ? -(hl2m_fixed)rlo : (hl2m_fixed)rlo;
}

/* Public, deterministic helper for testing fixed-point animation precision. */
hl2m_fixed hl2m_native_scale_rle89(hl2m_u32 ieee32_scale,int sample)
{ return a89_scaled_ieee32(ieee32_scale,sample); }

int hl2m_native_bind_pose89(const hl2m_mdl *mdl,hl2m_ik_pose *out)
{
    int i;
    if(!mdl || !out)return HL2M_ERR_NULL;
    if(mdl->bone_count<0 || mdl->bone_count>HL2M_MAX_BONES)return HL2M_ERR_LIMIT;
    memset(out,0,sizeof(*out));out->bone_count=mdl->bone_count;
    for(i=0;i<out->bone_count;i++) {
        strncpy(out->bone_names[i],mdl->bones[i].name,HL2M_NAME_MAX-1u);
        out->parent[i]=mdl->bones[i].parent;
        out->pos[i]=mdl->bones[i].pos;out->rot[i]=mdl->bones[i].rot;
    }
    return HL2M_OK;
}

static int a89_sample(const hl2m_u8 *mdl_data,unsigned int mdl_size,
                                    const hl2m_u8 *ani,unsigned int ani_size,
                                    const hl2m_mdl *mdl,int animation,int frame,hl2m_ik_pose *out,
                                    hl2m_native_anim_report89 *report,int allow_raw,
                                    hl2m_fixed guard_limit,hl2m_anim_safety_report89 *safety)
{
    const hl2m_mdl_animdesc *desc;
    unsigned int off,channel_size;
    const hl2m_u8 *channel_data;
    int i, r, flags, bone, next, value, axis, steps=0;
    int pos_base, rot_base,local_frame;
    hl2m_fixed *dst;
    if(!mdl_data || !mdl || !out)return HL2M_ERR_NULL;
    if(report)memset(report,0,sizeof(*report));
    if(safety){memset(safety,0,sizeof(*safety));safety->first_bone=-1;}
    if(animation<0 || animation>=mdl->anim_count)return HL2M_ERR_RANGE;
    desc=&mdl->anims[animation];
    if(frame<0 || frame>=desc->numframes)return HL2M_ERR_RANGE;
    r=a89_resolve_section(mdl_data,mdl_size,ani,ani_size,mdl,animation,frame,
        &channel_data,&channel_size,&off,&local_frame);
    if(r!=HL2M_OK)return r;
    r=hl2m_native_bind_pose89(mdl,out);if(r!=HL2M_OK)return r;
    /* Source CalcAnimation: an animation descriptor carrying STUDIO_DELTA
       initializes every unanimated bone to identity/zero, not to bind pose.
       This is separate from the per-channel STUDIO_ANIM_DELTA flag. */
    if(desc->flags & 0x0004){
        for(i=0;i<out->bone_count;i++){
            out->pos[i].x=out->pos[i].y=out->pos[i].z=0;
            out->rot[i].x=out->rot[i].y=out->rot[i].z=0;
        }
    }
    while(a89_has(off,4u,channel_size) && steps++<HL2M_MAX_BONES) {
        bone=(int)channel_data[off];flags=(int)channel_data[off+1u];next=a89_s16(channel_data+off+2u);
        if(bone==255)break;
        if(bone<0 || bone>=mdl->bone_count)return HL2M_ERR_RANGE;
        if((flags&(A89_RAWROT|A89_RAWROT2)) && !allow_raw)return HL2M_ERR_UNSUPPORTED;
        if((flags&A89_ANIMPOS) && (flags&A89_RAWPOS))return HL2M_ERR_UNSUPPORTED;
        if((flags&A89_ANIMROT) && (flags&(A89_RAWROT|A89_RAWROT2)))return HL2M_ERR_UNSUPPORTED;
        /* Valve bone_setup.cpp: STUDIO_ANIM_DELTA without a position/rotation
           channel means zero translation / identity rotation, not bind pose. */
        if(flags&A89_DELTA) {
            out->pos[bone].x=out->pos[bone].y=out->pos[bone].z=0;
            out->rot[bone].x=out->rot[bone].y=out->rot[bone].z=0;
        } else if(desc->flags&0x0004) {
            /* Source CalcBonePosition/Quaternion add the reference bone
               transform to non-DELTA channels, even inside a DELTA animdesc. */
            out->pos[bone]=mdl->bones[bone].pos;
            out->rot[bone]=mdl->bones[bone].rot;
        }
        rot_base=(int)off+4;
        pos_base=rot_base+((flags&A89_ANIMROT)?6:0);
        if(flags&A89_ANIMROT) {
            for(axis=0;axis<3;axis++) {
                r=a89_axis(channel_data,channel_size,(unsigned int)rot_base,axis,local_frame,&value);
                if(r!=HL2M_OK)return r;
                dst=a89_component(&out->rot[bone],axis);
                *dst=a89_add_sat(*dst,(mdl->bones[bone].rotscale_ieee32[axis]!=0u)
                    ? a89_scaled_ieee32(mdl->bones[bone].rotscale_ieee32[axis],value)
                    : a89_mul(value*HL2M_FIXED_ONE,a89_read_comp(&mdl->bones[bone].rotscale,axis)));
                /* RLE values are delta angles scaled by bone's rotscale. */
            }
        }
        if(flags&A89_ANIMPOS) {
            for(axis=0;axis<3;axis++) {
                r=a89_axis(channel_data,channel_size,(unsigned int)pos_base,axis,local_frame,&value);
                if(r!=HL2M_OK)return r;
                dst=a89_component(&out->pos[bone],axis);
                *dst=a89_add_sat(*dst,(mdl->bones[bone].posscale_ieee32[axis]!=0u)
                    ? a89_scaled_ieee32(mdl->bones[bone].posscale_ieee32[axis],value)
                    : a89_mul(value*HL2M_FIXED_ONE,a89_read_comp(&mdl->bones[bone].posscale,axis)));
            }
        } else if(flags&A89_RAWPOS) {
            unsigned int at=off+4u+((flags&A89_RAWROT2)?8u:((flags&A89_RAWROT)?6u:0u));
            if(!a89_has(at,6u,channel_size))return HL2M_ERR_RANGE;
            for(axis=0;axis<3;axis++) {
                dst=a89_component(&out->pos[bone],axis);
                *dst=a89_half(channel_data[at+2u*(unsigned int)axis],channel_data[at+2u*(unsigned int)axis+1u]);
            }
        }
        /* Optional model-space local position guard.  Source uses the original
           raw channel; the guard is a separately requested PREVIEW fallback.
           Only the bad components are restored to the reference local pose. */
        if(guard_limit>0 && safety){
            int bad=0;
            for(axis=0;axis<3;axis++){
                hl2m_fixed *v=a89_component(&out->pos[bone],axis);
                hl2m_fixed original=a89_read_comp(&mdl->bones[bone].pos,axis);
                hl2m_fixed delta;
                if(original>0 && *v<INT_MIN+original)delta=INT_MIN;
                else if(original<0 && *v>INT_MAX+original)delta=INT_MAX;
                else delta=*v-original;
                if(a89_outlier(delta,guard_limit)){
                    *v=(flags&A89_DELTA)?0:original;
                    safety->replaced_axes++;
                    bad=1;
                }
            }
            if(bad){
                safety->replaced_bones++;
                if(safety->first_bone<0)safety->first_bone=bone;
            }
        }
        if(report) {report->channels++;if(flags&A89_ANIMROT)report->rotation_channels++;if(flags&A89_ANIMPOS)report->position_channels++;if(flags&(A89_RAWROT|A89_RAWROT2))report->raw_quaternion_channels++;}
        if(next==0)break;
        if(next<4 || !a89_has(off,(unsigned int)next,channel_size))return HL2M_ERR_RANGE;
        off+=(unsigned int)next;
    }
    if(steps>=HL2M_MAX_BONES && a89_has(off,4u,channel_size) && channel_data[off]!=255)return HL2M_ERR_LIMIT;
    if(report){report->animation_index=animation;report->frame=frame;report->frame_count=desc->numframes;report->fps=desc->fps;}
    for(i=0;i<out->bone_count;i++)if(out->parent[i]>=i || out->parent[i]<-1)return HL2M_ERR_PARSE;
    return HL2M_OK;
}


int hl2m_native_sample_anim_frame89(const hl2m_u8 *data,unsigned int size,
    const hl2m_mdl *mdl,int anim,int frame,hl2m_ik_pose *pose,hl2m_native_anim_report89 *rep)
{ return a89_sample(data,size,NULL,0u,mdl,anim,frame,pose,rep,0,0,NULL); }

static hl2m_fixed a89_sqrt_unit(hl2m_fixed v)
{
    int lo=0,hi=HL2M_FIXED_ONE,mid;
    if(v<=0)return 0;
    if(v>=HL2M_FIXED_ONE)return HL2M_FIXED_ONE;
    while(lo+1<hi){mid=lo+(hi-lo)/2;if(a89_mul(mid,mid)<=v)lo=mid;else hi=mid;}
    return lo;
}
static void a89_quat_decode(const hl2m_u8 *p,int is64,hl2m_vec4 *q)
{
    unsigned int x,y,z;hl2m_fixed sum;
    if(is64){
        x=(((unsigned int)p[0])|((unsigned int)p[1]<<8)|((unsigned int)p[2]<<16))&2097151u;
        y=(((unsigned int)p[2]>>5)|((unsigned int)p[3]<<3)|((unsigned int)p[4]<<11)|((unsigned int)p[5]<<19))&2097151u;
        z=(((unsigned int)p[5]>>2)|((unsigned int)p[6]<<6)|((unsigned int)p[7]<<14))&2097151u;
        q->x=((int)x-1048576)/16;q->y=((int)y-1048576)/16;q->z=((int)z-1048576)/16;
    }else{
        x=(unsigned int)p[0]|((unsigned int)p[1]<<8);
        y=(unsigned int)p[2]|((unsigned int)p[3]<<8);
        z=((unsigned int)p[4]|((unsigned int)p[5]<<8))&32767u;
        q->x=((int)x-32768)*2;q->y=((int)y-32768)*2;q->z=((int)z-16384)*4;
    }
    sum=a89_mul(q->x,q->x)+a89_mul(q->y,q->y)+a89_mul(q->z,q->z);
    if(sum<0)sum=0;
    if(sum>HL2M_FIXED_ONE)sum=HL2M_FIXED_ONE;
    q->w=a89_sqrt_unit(HL2M_FIXED_ONE-sum);
    if((is64 && (p[7]&128u)) || (!is64 && (p[5]&128u)))q->w=-q->w;
}
static void a89_quat_mat(const hl2m_vec4 *q,hl2m_mat3x4 *m)
{
    hl2m_fixed x=q->x,y=q->y,z=q->z,w=q->w;
    hl2m_fixed xx=a89_mul(x,x),yy=a89_mul(y,y),zz=a89_mul(z,z);
    hl2m_fixed xy=a89_mul(x,y),xz=a89_mul(x,z),yz=a89_mul(y,z);
    hl2m_fixed wx=a89_mul(w,x),wy=a89_mul(w,y),wz=a89_mul(w,z);
    m->m[0][0]=HL2M_FIXED_ONE-2*(yy+zz);
    m->m[0][1]=2*(xy-wz);m->m[0][2]=2*(xz+wy);
    m->m[1][0]=2*(xy+wz);m->m[1][1]=HL2M_FIXED_ONE-2*(xx+zz);
    m->m[1][2]=2*(yz-wx);m->m[2][0]=2*(xz-wy);
    m->m[2][1]=2*(yz+wx);m->m[2][2]=HL2M_FIXED_ONE-2*(xx+yy);
}
static void a89_matrix_mul(const hl2m_mat3x4 *a,const hl2m_mat3x4 *b,hl2m_mat3x4 *out)
{
    int r,c;hl2m_mat3x4 t;
    for(r=0;r<3;r++)for(c=0;c<4;c++){
        t.m[r][c]=a89_mul(a->m[r][0],b->m[0][c])+a89_mul(a->m[r][1],b->m[1][c])+a89_mul(a->m[r][2],b->m[2][c]);
        if(c==3)t.m[r][c]+=a->m[r][3];
    }
    *out=t;
}

/* Original Source bind transforms.  MDL bones store the authoritative
   poseToBone matrices.  Re-deriving an inverse from approximate Euler angles
   causes severe limb spikes even if the animation channels are correct. */
static hl2m_fixed a89_ieee32(const hl2m_u8 *p)
{
    unsigned int bits=(unsigned int)p[0]|((unsigned int)p[1]<<8)|
        ((unsigned int)p[2]<<16)|((unsigned int)p[3]<<24);
    unsigned int mant=bits&8388607u,mag;int e=(int)((bits>>23)&255u),shift;
    int neg=(bits&2147483648u)!=0;
    if(e==255)return neg?-2147483647:2147483647;
    if(e==0){if(mant==0)return 0;e=-126;}
    else{mant|=8388608u;e-=127;}
    shift=e-7;
    if(shift>=0){
        if(shift>7 || mant>(2147483647u>>shift))mag=2147483647u;
        else mag=mant<<shift;
    }else{shift=-shift;mag=shift>=32?0u:mant>>shift;}
    return neg?-(hl2m_fixed)mag:(hl2m_fixed)mag;
}
int hl2m_native_bind_matrices89(const hl2m_mdl *mdl,hl2m_pose_matrices *out)
{
    int i,r;
    if(!mdl || !out)return HL2M_ERR_NULL;
    memset(out,0,sizeof(*out));out->bone_count=mdl->bone_count;
    for(i=0;i<mdl->bone_count;i++){
        const hl2m_mdl_bone *b=&mdl->bones[i];int parent=b->parent;
        if(parent>=i || parent< -1)return HL2M_ERR_PARSE;
        strncpy(out->bone_names[i],b->name,HL2M_NAME_MAX-1u);
        out->parent[i]=parent;
        a89_quat_mat(&b->quat,&out->local[i]);
        out->local[i].m[0][3]=b->pos.x;
        out->local[i].m[1][3]=b->pos.y;
        out->local[i].m[2][3]=b->pos.z;
        if(parent>=0) a89_matrix_mul(&out->world[parent],&out->local[i],&out->world[i]);
        else out->world[i]=out->local[i];
    }
    r=HL2M_OK;return r;
}
int hl2m_native_inverse_bind89(const hl2m_u8 *data,unsigned int size,
    const hl2m_mdl *mdl,hl2m_pose_matrices *out)
{
    int boneindex,i,r,c;unsigned int off;
    if(!data || !mdl || !out)return HL2M_ERR_NULL;
    if(!a89_has(160u,4u,size))return HL2M_ERR_RANGE;
    boneindex=a89_i32(data+160u);
    if(boneindex<0)return HL2M_ERR_RANGE;
    memset(out,0,sizeof(*out));out->bone_count=mdl->bone_count;
    for(i=0;i<mdl->bone_count;i++){
        off=(unsigned int)boneindex+(unsigned int)i*216u+96u;
        if(!a89_has(off,48u,size))return HL2M_ERR_RANGE;
        strncpy(out->bone_names[i],mdl->bones[i].name,HL2M_NAME_MAX-1u);
        out->parent[i]=mdl->bones[i].parent;
        for(r=0;r<3;r++)for(c=0;c<4;c++){
            out->world[i].m[r][c]=a89_ieee32(data+off+4u*(unsigned int)(r*4+c));
            out->local[i].m[r][c]=out->world[i].m[r][c];
        }
    }
    return HL2M_OK;
}

/* Full native matrices: animated Euler channels plus raw quaternion 48/64.
   Matrix hierarchy is recomposed after overriding raw quaternion bones. */
static int a89_matrices_internal(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,
    const hl2m_mdl *mdl,int anim,int frame,
    hl2m_pose_matrices *mat,hl2m_native_anim_report89 *report,
    hl2m_fixed guard_limit,hl2m_anim_safety_report89 *safety)
{
    hl2m_ik_pose pose;unsigned int off,channel_size;
    const hl2m_u8 *channel_data;int r,steps=0,raw[HL2M_MAX_BONES],local_frame;
    hl2m_vec4 quats[HL2M_MAX_BONES];int i;
    if(!data || !mdl || !mat)return HL2M_ERR_NULL;
    r=a89_sample(data,size,ani,ani_size,mdl,anim,frame,&pose,report,1,guard_limit,safety);if(r)return r;
    memset(raw,0,sizeof(raw));
    /* CalcAnimation: unanimated bones of an STUDIO_DELTA animation are
       identity quaternions.  The old raw-quat fallback erroneously restored
       the bind quaternions *after* a89_sample initialized them correctly,
       rotating the whole skeleton when a 4-bone eye gesture was applied. */
    for(i=0;i<mdl->bone_count;i++){
        raw[i]=1;
        if(mdl->anims[anim].flags&0x0004){
            quats[i].x=quats[i].y=quats[i].z=0;
            quats[i].w=HL2M_FIXED_ONE;
        }else quats[i]=mdl->bones[i].quat;
    }
    r=a89_resolve_section(data,size,ani,ani_size,mdl,anim,frame,
        &channel_data,&channel_size,&off,&local_frame);
    if(r!=HL2M_OK)return r;
    (void)local_frame;
    while(a89_has(off,4u,channel_size) && steps++<HL2M_MAX_BONES){
        int bone=(int)channel_data[off],flags=(int)channel_data[off+1u],next=a89_s16(channel_data+off+2u);
        if(bone==255)break;
        if(bone<0 || bone>=mdl->bone_count)return HL2M_ERR_RANGE;
        if(flags&A89_ANIMROT)raw[bone]=0;
        /* In Valve's CalcBoneQuaternion, a DELTA channel without rotation
           uses the identity quaternion, not the reference quaternion. */
        if((flags&A89_DELTA) && !(flags&(A89_RAWROT|A89_RAWROT2|A89_ANIMROT))){
            quats[bone].x=quats[bone].y=quats[bone].z=0;
            quats[bone].w=HL2M_FIXED_ONE; raw[bone]=1;
        }
        if(flags&(A89_RAWROT|A89_RAWROT2)){
            unsigned int len=(flags&A89_RAWROT2)?8u:6u;
            if(!a89_has(off+4u,len,channel_size))return HL2M_ERR_RANGE;
            a89_quat_decode(channel_data+off+4u,(flags&A89_RAWROT2)!=0,&quats[bone]);raw[bone]=1;
        }
        if(next==0)break;
        if(next<4 || !a89_has(off,(unsigned int)next,channel_size))return HL2M_ERR_RANGE;
        off+=(unsigned int)next;
    }
    r=hl2m_pose_matrices_from_pose(&pose,1,mat);if(r)return r;
    for(i=0;i<mat->bone_count;i++){
        int par=mat->parent[i];
        if(raw[i]){
            a89_quat_mat(&quats[i],&mat->local[i]);
            mat->local[i].m[0][3]=pose.pos[i].x;
            mat->local[i].m[1][3]=pose.pos[i].y;
            mat->local[i].m[2][3]=pose.pos[i].z;
        }
        if(par>=0 && par<i)a89_matrix_mul(&mat->world[par],&mat->local[i],&mat->world[i]);
        else mat->world[i]=mat->local[i];
    }
    return HL2M_OK;
}

int hl2m_native_sample_anim_matrices89(const hl2m_u8 *data,unsigned int size,
    const hl2m_mdl *mdl,int anim,int frame,
    hl2m_pose_matrices *mat,hl2m_native_anim_report89 *report)
{ return a89_matrices_internal(data,size,NULL,0u,mdl,anim,frame,mat,report,0,NULL); }

int hl2m_native_sample_anim_matrices_external89(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,
    const hl2m_mdl *mdl,int anim,int frame,
    hl2m_pose_matrices *mat,hl2m_native_anim_report89 *report)
{ return a89_matrices_internal(data,size,ani,ani_size,mdl,anim,frame,mat,report,0,NULL); }

/* Explicit non-native visual safety mode: never enable silently.
   Native Source decoding remains the default; this returns a report of every
   replaced local translation component for user-facing validation. */
int hl2m_native_sample_anim_matrices_guard89(const hl2m_u8 *data,
    unsigned int size,const hl2m_u8 *ani,unsigned int ani_size,
    const hl2m_mdl *mdl,int anim,int frame,hl2m_fixed max_delta,
    hl2m_pose_matrices *mat,hl2m_native_anim_report89 *report,
    hl2m_anim_safety_report89 *safety)
{
    if(!safety || max_delta<=0 || max_delta>16384*HL2M_FIXED_ONE)
        return HL2M_ERR_RANGE;
    return a89_matrices_internal(data,size,ani,ani_size,mdl,anim,frame,
        mat,report,max_delta,safety);
}

/* Select one native sequence blend cell, not the full 2D pose-parameter system.
   Both frame and blend coordinates are explicit; no misleading 9-way blending. */
int hl2m_native_sequence_animation89(const hl2m_u8 *data,unsigned int size,const hl2m_mdl *mdl,
                                     int sequence,int blend_x,int blend_y,int *out_anim)
{
    int idx,rel,base,seqindex,off,s;
    if(!data || !mdl || !out_anim)return HL2M_ERR_NULL;
    if(sequence<0 || sequence>=mdl->sequence_count)return HL2M_ERR_RANGE;
    if(blend_x<0 || blend_y<0 || blend_x>=mdl->sequences[sequence].groupsize[0] ||
       blend_y>=mdl->sequences[sequence].groupsize[1])return HL2M_ERR_RANGE;
    if(!a89_has(192u,4u,size))return HL2M_ERR_RANGE;
    seqindex=a89_i32(data+192u);
    if(seqindex<=0)return HL2M_ERR_RANGE;
    if((unsigned int)sequence>(size-(unsigned int)seqindex)/212u)return HL2M_ERR_RANGE;
    base=seqindex+212*sequence;rel=mdl->sequences[sequence].animindexindex;
    idx=blend_y*mdl->sequences[sequence].groupsize[0]+blend_x;
    if(rel<=0 || idx<0 || !a89_has((unsigned int)base,(unsigned int)rel+2u*(unsigned int)(idx+1),size))return HL2M_ERR_RANGE;
    off=base+rel+2*idx;s=a89_s16(data+(unsigned int)off);
    if(s<0 || s>=mdl->anim_count)return HL2M_ERR_RANGE;
    *out_anim=s;return HL2M_OK;
}

/* Source SDK bone_setup.cpp Studio_AnimPosition(), single-cycle equivalent.
   Root movement belongs to the entity transform, not the skinned vertices.
   Sampling frame is integer; external loops / turning remain caller-owned. */
int hl2m_native_root_motion89(const hl2m_u8 *data,unsigned int size,
                              const hl2m_mdl *mdl,int anim,int frame,
                              hl2m_vec3 *out_pos,hl2m_fixed *out_yaw)
{
    int desc_index,base,i,endframe,prevframe,rel,off;
    hl2m_vec3 previous;
    hl2m_fixed old_yaw;
    if(!data || !mdl || !out_pos || !out_yaw)return HL2M_ERR_NULL;
    out_pos->x=out_pos->y=out_pos->z=0;*out_yaw=0;
    if(anim<0 || anim>=mdl->anim_count)return HL2M_ERR_RANGE;
    if(frame<0 || frame>=mdl->anims[anim].numframes)return HL2M_ERR_RANGE;
    if(mdl->anims[anim].nummovements==0)return HL2M_ERR_UNSUPPORTED;
    if(mdl->anims[anim].nummovements<0 || mdl->anims[anim].nummovements>256)return HL2M_ERR_LIMIT;
    if(!a89_has(184u,4u,size))return HL2M_ERR_RANGE;
    desc_index=a89_i32(data+184u);
    if(desc_index<=0 || !a89_has((unsigned int)desc_index,100u,size) ||
       (unsigned int)anim>(size-(unsigned int)desc_index)/100u)return HL2M_ERR_RANGE;
    base=desc_index+100*anim;
    rel=mdl->anims[anim].movementindex;
    if(rel<=0 || !a89_has((unsigned int)base,(unsigned int)rel,size))return HL2M_ERR_RANGE;
    previous.x=previous.y=previous.z=0; old_yaw=0;prevframe=0;
    for(i=0;i<mdl->anims[anim].nummovements;i++){
        hl2m_fixed f,delta,v0,v1,angle;
        hl2m_vec3 vec,endpoint;
        unsigned int uoff=(unsigned int)base+(unsigned int)rel+44u*(unsigned int)i;
        if(!a89_has(uoff,44u,size))return HL2M_ERR_RANGE;
        off=(int)uoff;
        endframe=a89_i32(data+off);
        if(endframe<=prevframe)return HL2M_ERR_PARSE;
        v0=a89_ieee32(data+off+8);v1=a89_ieee32(data+off+12);
        angle=a89_ieee32(data+off+16);
        vec.x=a89_ieee32(data+off+20);vec.y=a89_ieee32(data+off+24);vec.z=a89_ieee32(data+off+28);
        endpoint.x=a89_ieee32(data+off+32);endpoint.y=a89_ieee32(data+off+36);endpoint.z=a89_ieee32(data+off+40);
        if(frame<=endframe){
            f=(hl2m_fixed)(((unsigned int)(frame-prevframe)*65536u)/(unsigned int)(endframe-prevframe));
            delta=a89_mul(v0,f)+a89_mul(a89_mul(v1-v0,f),f)/2;
            out_pos->x=previous.x+a89_mul(delta,vec.x);
            out_pos->y=previous.y+a89_mul(delta,vec.y);
            out_pos->z=previous.z+a89_mul(delta,vec.z);
            *out_yaw=a89_mul(old_yaw,HL2M_FIXED_ONE-f)+a89_mul(angle,f);
            return HL2M_OK;
        }
        previous=endpoint;old_yaw=angle;prevframe=endframe;
    }
    return HL2M_ERR_RANGE;
}
