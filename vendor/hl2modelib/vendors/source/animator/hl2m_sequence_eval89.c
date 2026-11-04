/* Source-inspired sequence evaluator: fractional cycles, 2D blend grid and
   explicit additive gesture overlays. C89, 32-bit fixed only, no heap.
   SDK reference: bone_setup.cpp CalcAnimation / CalcPoseSingle / BlendBones.
   This is NOT a full Valve Studio renderer or exact QuaternionSlerp. */
#include "hl2modelib.h"
#include <string.h>
#include <limits.h>

#define SE89_UNIT HL2M_FIXED_ONE
#define SE89_LOOPING 0x0001
#define SE89_DELTA 0x0004

/* multiply Q16.16 bounded components, using 16-bit halves only */
static hl2m_fixed se89_mul(hl2m_fixed a,hl2m_fixed b)
{
    unsigned int u,v,top,low;int neg=0;
    if(a<0){u=(unsigned int)(-(a+1))+1u;neg=!neg;}else u=(unsigned int)a;
    if(b<0){v=(unsigned int)(-(b+1))+1u;neg=!neg;}else v=(unsigned int)b;
    top=(u>>16)*(v>>16)*65536u+(u>>16)*(v&65535u)+(v>>16)*(u&65535u);
    low=((u&65535u)*(v&65535u))>>16;
    return neg?-(hl2m_fixed)(top+low):(hl2m_fixed)(top+low);
}
static hl2m_fixed se89_add_sat(hl2m_fixed a,hl2m_fixed b)
{
    if(b>0 && a>INT_MAX-b)return INT_MAX;
    if(b<0 && a<INT_MIN-b)return INT_MIN;
    return a+b;
}
/* Quaternion interpolation lives in its own vendor, following Source's
   BlendBones rotation path. This wrapper preserves the legacy API. */
static void se89_rotate_interp(const hl2m_mat3x4 *a,const hl2m_mat3x4 *b,
    hl2m_fixed t,hl2m_mat3x4 *o)
{
    (void)hl2m_quaternion_blend_matrix89(a,b,t,o);
}
static void se89_matmul(const hl2m_mat3x4 *a,const hl2m_mat3x4 *b,hl2m_mat3x4 *out)
{
    hl2m_mat3x4 t;int r,c;
    for(r=0;r<3;r++)for(c=0;c<4;c++){
        t.m[r][c]=se89_add_sat(se89_add_sat(se89_mul(a->m[r][0],b->m[0][c]),
          se89_mul(a->m[r][1],b->m[1][c])),se89_mul(a->m[r][2],b->m[2][c]));
        if(c==3)t.m[r][c]=se89_add_sat(t.m[r][c],a->m[r][3]);
    }
    *out=t;
}
static int se89_rebuild(hl2m_pose_matrices *out)
{
    int i,p;
    for(i=0;i<out->bone_count;i++){
        p=out->parent[i];if(p>=i || p< -1)return HL2M_ERR_PARSE;
        if(p>=0)se89_matmul(&out->world[p],&out->local[i],&out->world[i]);
        else out->world[i]=out->local[i];
    }
    return HL2M_OK;
}
static int se89_compatible(const hl2m_pose_matrices *a,const hl2m_pose_matrices *b)
{
    int i;if(a->bone_count!=b->bone_count)return 0;
    for(i=0;i<a->bone_count;i++)if(a->parent[i]!=b->parent[i])return 0;
    return 1;
}
/* Interpolate LOCAL matrices, then rebuild world transforms.
   Orthogonalized matrix interpolation is an explicit approximation to Source
   shortest-path quaternion slerp (not a replacement for exact parity). */
int hl2m_sequence_blend_local89(const hl2m_pose_matrices *a,
    const hl2m_pose_matrices *b,hl2m_fixed t,hl2m_pose_matrices *out)
{
    int i;
    if(!a || !b || !out)return HL2M_ERR_NULL;
    if(!se89_compatible(a,b))return HL2M_ERR_RANGE;
    if(t<0 || t>SE89_UNIT)return HL2M_ERR_RANGE;
    if(t==0){*out=*a;return HL2M_OK;}
    if(t==SE89_UNIT){*out=*b;return HL2M_OK;}
    out->bone_count=a->bone_count;
    for(i=0;i<a->bone_count;i++){
        out->parent[i]=a->parent[i];
        memcpy(out->bone_names[i],a->bone_names[i],HL2M_NAME_MAX);
        se89_rotate_interp(&a->local[i],&b->local[i],t,&out->local[i]);
    }
    return se89_rebuild(out);
}
int hl2m_sequence_sample_cycle89(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int animation,hl2m_fixed cycle,int looping,hl2m_pose_matrices *out)
{
    hl2m_pose_matrices a,b;unsigned int scaled;int n,f0,f1,r;
    hl2m_fixed fraction;
    if(!data || !mdl || !out)return HL2M_ERR_NULL;
    if(animation<0 || animation>=mdl->anim_count)return HL2M_ERR_RANGE;
    n=mdl->anims[animation].numframes;
    if(n<=0 || n>65536)return HL2M_ERR_RANGE;
    if(looping){cycle%=SE89_UNIT;if(cycle<0)cycle+=SE89_UNIT;}
    else{if(cycle<0)cycle=0;if(cycle>SE89_UNIT)cycle=SE89_UNIT;}
    scaled=(unsigned int)(n-1)*(unsigned int)cycle;
    f0=(int)(scaled>>16);fraction=(hl2m_fixed)(scaled&65535u);
    f1=f0+1;if(f1>=n)f1=f0;
    r=hl2m_native_sample_anim_matrices_external89(data,size,ani,ani_size,mdl,animation,f0,&a,NULL);
    if(r!=HL2M_OK)return r;
    if(f1==f0 || fraction==0){*out=a;return HL2M_OK;}
    r=hl2m_native_sample_anim_matrices_external89(data,size,ani,ani_size,mdl,animation,f1,&b,NULL);
    if(r!=HL2M_OK)return r;
    return hl2m_sequence_blend_local89(&a,&b,fraction,out);
}
static void se89_grid_coord(hl2m_fixed parameter,int count,int *i,hl2m_fixed *weight)
{
    unsigned int scaled;
    if(count<=1){*i=0;*weight=0;return;}
    if(parameter<0)parameter=0;
    if(parameter>SE89_UNIT)parameter=SE89_UNIT;
    scaled=(unsigned int)(count-1)*(unsigned int)parameter;
    *i=(int)(scaled>>16);*weight=(hl2m_fixed)(scaled&65535u);
    if(*i>=count-1){*i=count-2;*weight=SE89_UNIT;}
}
int hl2m_sequence_sample_blend89(const hl2m_u8 *data,unsigned int size,
    const hl2m_u8 *ani,unsigned int ani_size,const hl2m_mdl *mdl,
    int sequence,hl2m_fixed cycle,hl2m_fixed param_x,hl2m_fixed param_y,
    hl2m_pose_matrices *out, int sampled_animations[4])
{
    const hl2m_mdl_seqdesc *s;
    hl2m_pose_matrices p00,p01,p10,p11,row0,row1;
    hl2m_fixed tx,ty;int ix,iy,a00,a01,a10,a11,r,loop;
    if(!data || !mdl || !out)return HL2M_ERR_NULL;
    if(sequence<0 || sequence>=mdl->sequence_count)return HL2M_ERR_RANGE;
    s=&mdl->sequences[sequence];
    if(s->groupsize[0]<1 || s->groupsize[1]<1 || s->groupsize[0]>64 || s->groupsize[1]>64)return HL2M_ERR_RANGE;
    se89_grid_coord(param_x,s->groupsize[0],&ix,&tx);
    se89_grid_coord(param_y,s->groupsize[1],&iy,&ty);
    r=hl2m_native_sequence_animation89(data,size,mdl,sequence,ix,iy,&a00);if(r)return r;
    r=hl2m_native_sequence_animation89(data,size,mdl,sequence,
       ix+(s->groupsize[0]>1),iy,&a10);if(r)return r;
    r=hl2m_native_sequence_animation89(data,size,mdl,sequence,
       ix,iy+(s->groupsize[1]>1),&a01);if(r)return r;
    r=hl2m_native_sequence_animation89(data,size,mdl,sequence,
       ix+(s->groupsize[0]>1),iy+(s->groupsize[1]>1),&a11);if(r)return r;
    if(sampled_animations){sampled_animations[0]=a00;sampled_animations[1]=a10;
      sampled_animations[2]=a01;sampled_animations[3]=a11;}
    loop=(s->flags&SE89_LOOPING)!=0;
    r=hl2m_sequence_sample_cycle89(data,size,ani,ani_size,mdl,a00,cycle,loop,&p00);if(r)return r;
    if(a10==a00 || tx==0)row0=p00;
    else{
        r=hl2m_sequence_sample_cycle89(data,size,ani,ani_size,mdl,a10,cycle,loop,&p10);if(r)return r;
        r=hl2m_sequence_blend_local89(&p00,&p10,tx,&row0);if(r)return r;
    }
    if(s->groupsize[1]==1 || ty==0){*out=row0;return HL2M_OK;}
    r=hl2m_sequence_sample_cycle89(data,size,ani,ani_size,mdl,a01,cycle,loop,&p01);if(r)return r;
    if(a11==a01 || tx==0)row1=p01;
    else{
        r=hl2m_sequence_sample_cycle89(data,size,ani,ani_size,mdl,a11,cycle,loop,&p11);if(r)return r;
        r=hl2m_sequence_blend_local89(&p01,&p11,tx,&row1);if(r)return r;
    }
    return hl2m_sequence_blend_local89(&row0,&row1,ty,out);
}
/* Optional additive gesture layer. Caller supplies a DELTA animation pose,
   an explicit per-bone Q16 mask and a global weight. Does not silently
   replace a reference base pose, and is NOT full Source layer ordering. */
int hl2m_sequence_add_delta89(const hl2m_pose_matrices *base,
    const hl2m_pose_matrices *delta,const hl2m_fixed *bone_weights,
    hl2m_fixed strength,hl2m_pose_matrices *out)
{
    int i,r;hl2m_mat3x4 neutral,step;
    if(!base || !delta || !bone_weights || !out)return HL2M_ERR_NULL;
    if(!se89_compatible(base,delta) || strength<0 || strength>SE89_UNIT)return HL2M_ERR_RANGE;
    *out=*base;
    for(i=0;i<base->bone_count;i++){
        hl2m_fixed w=se89_mul(bone_weights[i],strength);
        if(w<0 || w>SE89_UNIT)return HL2M_ERR_RANGE;
        if(w==0)continue;
        memset(&neutral,0,sizeof(neutral));
        for(r=0;r<3;r++)neutral.m[r][r]=SE89_UNIT;
        se89_rotate_interp(&neutral,&delta->local[i],w,&step);
        /* additive translation is not an absolute root translation */
        for(r=0;r<3;r++)step.m[r][3]=se89_mul(delta->local[i].m[r][3],w);
        se89_matmul(&base->local[i],&step,&out->local[i]);
    }
    return se89_rebuild(out);
}
