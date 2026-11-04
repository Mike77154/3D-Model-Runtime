/* IQM v2 native scene/animation bridge, ISO C89 without heap or 64-bit/floats.
 * Animated vertex positions use LBS (world-frame matrices * inverse bind).
 * The common bone visitor carries only translations; full IQM local TRS and
 * frame-world matrices are available from modelrt_native_iqm_pose89(). */
#include "../modelrt_internal.h"
#include "../../include/modelrt_native_iqm_pose89.h"
#include <string.h>
#include <limits.h>
#define IQMRT_ID 101u
#define IQMRT_HEADER_SIZE 124u
#define IQMRT_MAX_VERTICES 65536u
#define IQMRT_MAX_TRIANGLES 65536u
#define IQMRT_MAX_MESHES 256u
#define IQMRT_MAX_JOINTS 4096u
#define IQMRT_FLOAT 7u
#define IQMRT_UBYTE 1u

typedef struct iqmrt_matrix_s { modelrt_q16 m[12]; } iqmrt_matrix;
typedef struct iqmrt_state_s {
    const modelrt_u8 *bytes;size_t n;
    modelrt_u32 meshes,mesh_offset,vertices,triangles,tri_offset,joints,joint_offset;
    modelrt_u32 anims,anim_offset,frames,poses,pose_offset,frame_channels,frame_offset;
    modelrt_u32 text_offset,text_count;
    modelrt_u32 arrays[6];modelrt_u32 array_count[6];
    struct iqmrt_matrix_s *inverse_bind,*world,*skin;
    int skin_active;
} iqmrt_state;
static int iqmrt_build_bind(iqmrt_state*s);
static int iqmrt_build_frame(iqmrt_state*s,modelrt_u32 frame);
static modelrt_u32 iqmrt_u32(const modelrt_u8*b)
{return (modelrt_u32)b[0]|((modelrt_u32)b[1]<<8)|((modelrt_u32)b[2]<<16)|((modelrt_u32)b[3]<<24);}
static int iqmrt_span(size_t n,modelrt_u32 off,modelrt_u32 count,size_t each)
{return (size_t)off<=n && each && (size_t)count<=(n-(size_t)off)/each;}
static int iqmrt_float(const modelrt_u8*b,modelrt_q16*out)
{
    modelrt_u32 bits=iqmrt_u32(b),mant,absn;unsigned int exp;int shift,sign;
    exp=(unsigned int)((bits>>23)&255u);sign=(bits>>31)!=0u;
    if(exp==255u)return MODELRT_ERR_FORMAT;
    if(exp==0u){*out=0;return MODELRT_OK;}
    mant=(bits&0x7fffffu)|0x800000u;shift=(int)exp-134;
    if(shift>=8){absn=0x7fffffffu;}
    else if(shift>=0){absn=mant<<shift;if(absn>0x7fffffffu)absn=0x7fffffffu;}
    else if(shift<=-32){absn=0u;}
    else {absn=mant>>(-shift);}
    *out=sign?-(modelrt_q16)absn:(modelrt_q16)absn;
    return MODELRT_OK;
}
static int iqmrt_probe(const char*p,const modelrt_u8*h,size_t n)
{return modelrt_ext_eq(p,".iqm") && n>=16u && !memcmp(h,"INTERQUAKEMODEL",15u)?MODELRT_PROBE_YES:MODELRT_PROBE_NO;}
static int iqmrt_open(modelrt_runtime*rt,modelrt_handle*h,const char*p)
{
    iqmrt_state*s;modelrt_u8*bytes;size_t n;int rc;modelrt_u32 na,nv,offset,i,j,type,format,size,off,mesh_base;
    (void)rt;
    rc=modelrt_read_file_arena(h,p,&bytes,&n);if(rc)return rc;
    if(n<IQMRT_HEADER_SIZE||memcmp(bytes,"INTERQUAKEMODEL",15u)!=0||iqmrt_u32(bytes+16u)!=2u)return MODELRT_ERR_FORMAT;
    if((size_t)iqmrt_u32(bytes+20u)!=n)return MODELRT_ERR_FORMAT;
    s=(iqmrt_state*)modelrt_arena_alloc(h,sizeof(*s),sizeof(void*));if(!s)return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s,sizeof(*s));s->bytes=bytes;s->n=n;
    s->text_count=iqmrt_u32(bytes+28u);s->text_offset=iqmrt_u32(bytes+32u);
    s->meshes=iqmrt_u32(bytes+36u);s->mesh_offset=iqmrt_u32(bytes+40u);
    na=iqmrt_u32(bytes+44u);nv=iqmrt_u32(bytes+48u);offset=iqmrt_u32(bytes+52u);
    s->vertices=nv;s->triangles=iqmrt_u32(bytes+56u);s->tri_offset=iqmrt_u32(bytes+60u);
    s->joints=iqmrt_u32(bytes+68u);s->joint_offset=iqmrt_u32(bytes+72u);
    s->anims=iqmrt_u32(bytes+84u);s->anim_offset=iqmrt_u32(bytes+88u);
    s->frames=iqmrt_u32(bytes+92u);
    s->poses=iqmrt_u32(bytes+76u);s->pose_offset=iqmrt_u32(bytes+80u);
    s->frame_channels=iqmrt_u32(bytes+96u);s->frame_offset=iqmrt_u32(bytes+100u);
    if(nv>IQMRT_MAX_VERTICES||s->triangles>IQMRT_MAX_TRIANGLES||s->meshes>IQMRT_MAX_MESHES||s->joints>IQMRT_MAX_JOINTS||na>64u)return MODELRT_ERR_LIMIT;
    if(!iqmrt_span(n,s->text_offset,s->text_count,1u)||!iqmrt_span(n,s->mesh_offset,s->meshes,24u)||!iqmrt_span(n,s->tri_offset,s->triangles,12u)||!iqmrt_span(n,s->joint_offset,s->joints,48u)||!iqmrt_span(n,offset,na,20u)||!iqmrt_span(n,s->anim_offset,s->anims,20u))return MODELRT_ERR_FORMAT;
    /* Validate the complete animation span before enabling frame visitation. */
    if(s->anims){
        modelrt_u32 total=0u,pmask,k,first,frame_count;
        if(!s->frames || !s->poses || s->poses!=s->joints ||
           !iqmrt_span(n,s->pose_offset,s->poses,88u) ||
           !iqmrt_span(n,s->frame_offset,s->frames,1u) ||
           !iqmrt_span(n,s->frame_offset,s->frame_channels,2u) ||
           s->frame_channels>10240u || s->frames>65536u ||
           (size_t)s->frame_channels>(n-(size_t)s->frame_offset)/((size_t)s->frames*2u))return MODELRT_ERR_FORMAT;
        for(i=0u;i<s->poses;i++){
            modelrt_i32 parent=(modelrt_i32)iqmrt_u32(bytes+s->pose_offset+i*88u);
            pmask=iqmrt_u32(bytes+s->pose_offset+i*88u+4u);
            if(parent < -1 || (parent>=0 && (modelrt_u32)parent>=i))return MODELRT_ERR_FORMAT;
            if((pmask&~1023u)!=0u)return MODELRT_ERR_FORMAT;
            for(k=0u;k<10u;k++)if(pmask&(1u<<k))total++;
        }
        if(total!=s->frame_channels)return MODELRT_ERR_FORMAT;
        for(i=0u;i<s->anims;i++){
            modelrt_u32 ao=s->anim_offset+i*20u;
            first=iqmrt_u32(bytes+ao+4u);frame_count=iqmrt_u32(bytes+ao+8u);
            if(first>s->frames || !frame_count || frame_count>s->frames-first)return MODELRT_ERR_FORMAT;
        }
    }
    for(i=0u;i<na;i++){
        mesh_base=offset+i*20u;type=iqmrt_u32(bytes+mesh_base);
        format=iqmrt_u32(bytes+mesh_base+8u);size=iqmrt_u32(bytes+mesh_base+12u);off=iqmrt_u32(bytes+mesh_base+16u);
        if(type<=5u){
            if(type<=3u){if(format!=IQMRT_FLOAT||size!=(type==1u?2u:type==3u?4u:3u)||!iqmrt_span(n,off,nv,size*4u))return MODELRT_ERR_UNSUPPORTED;}
            else {if(format!=IQMRT_UBYTE||size!=4u||!iqmrt_span(n,off,nv,4u))return MODELRT_ERR_UNSUPPORTED;}
            s->arrays[type]=off;s->array_count[type]=1u;
        }
    }
    if(!s->array_count[0]||!s->meshes||!s->triangles)return MODELRT_ERR_UNSUPPORTED;
    for(i=0;i<s->triangles*3u;i++)if(iqmrt_u32(bytes+s->tri_offset+i*4u)>=nv)return MODELRT_ERR_FORMAT;
    for(i=0;i<s->meshes;i++){
        modelrt_u32 base=s->mesh_offset+i*24u,first=iqmrt_u32(bytes+base+16u),count=iqmrt_u32(bytes+base+20u);
        if(first>s->triangles || count>s->triangles-first)return MODELRT_ERR_FORMAT;
    }
    for(i=0;i<s->joints;i++){
        modelrt_i32 parent=(modelrt_i32)iqmrt_u32(bytes+s->joint_offset+i*48u+4u);
        if(parent < -1 || (parent>=0 && (modelrt_u32)parent>=i))return MODELRT_ERR_FORMAT;
    }
    if(s->array_count[4]&&s->array_count[5]){
        for(i=0;i<nv;i++)for(j=0u;j<4u;j++){
            modelrt_u32 wi=s->arrays[5]+i*4u+j;
            modelrt_u32 bi=s->arrays[4]+i*4u+j;
            if(bytes[wi] && (modelrt_u32)bytes[bi]>=s->joints)return MODELRT_ERR_FORMAT;
        }
    }
    if(s->joints){
        size_t required=(size_t)s->joints*sizeof(struct iqmrt_matrix_s);
        s->inverse_bind=(struct iqmrt_matrix_s*)modelrt_arena_alloc(h,required,sizeof(void*));
        s->world=(struct iqmrt_matrix_s*)modelrt_arena_alloc(h,required,sizeof(void*));
        s->skin=(struct iqmrt_matrix_s*)modelrt_arena_alloc(h,required,sizeof(void*));
        if(!s->inverse_bind||!s->world||!s->skin)return MODELRT_ERR_NO_SPACE;
        rc=iqmrt_build_bind(s);if(rc)return rc;
    }
    h->native_state=s;h->info.mesh_count=s->meshes;h->info.bone_count=s->joints;
    h->info.capabilities=MODELRT_CAP_NATIVE_SCENE|MODELRT_CAP_GEOMETRY;
    if(s->array_count[1])h->info.capabilities|=MODELRT_CAP_UV0;
    if(s->array_count[2])h->info.capabilities|=MODELRT_CAP_NORMALS;
    if(s->joints)h->info.capabilities|=MODELRT_CAP_SKELETON;
    if(s->anims){h->info.animation_count=s->anims;h->info.capabilities|=MODELRT_CAP_ANIMATION;}
    if(s->array_count[4]&&s->array_count[5])h->info.capabilities|=MODELRT_CAP_SKINNING;
    /* Do not expose an unimplemented animation visitor as a playable animation. */
    return MODELRT_OK;
}
/* Multiply a source IEEE754 float by an unsigned channel without 64-bit or
   lossy early Q16 rounding (many IQM scales are below 1/65536). */
static int iqmrt_scaled(modelrt_u32 bits,modelrt_u32 raw,modelrt_q16*out)
{
    modelrt_u32 mant,low,high,carry,expbits,mag,hi,lo;int shift,neg,k;
    expbits=(bits>>23)&255u;neg=(bits>>31)!=0u;
    if(expbits==255u)return MODELRT_ERR_FORMAT;
    if(!expbits||!raw){*out=0;return MODELRT_OK;}
    mant=(bits&0x7fffffu)|0x800000u;
    low=(mant&65535u)*raw;high=(mant>>16)*raw;
    lo=low+((high&65535u)<<16);
    carry=lo<low?1u:0u;hi=(high>>16)+carry;
    shift=(int)expbits-134;
    if(shift>=0){
        if(shift>=31||hi||lo>(0x7fffffffu>>shift))mag=0x7fffffffu;
        else mag=lo<<shift;
    }else{
        k=-shift;
        if(k>=40)mag=0u;
        else if(k>=32)mag=hi>>(k-32);
        else{
            modelrt_u32 part,tail;
            tail=lo>>k;
            if(hi>(0x7fffffffu>>(32-k)))mag=0x7fffffffu;
            else{
                part=hi<<(32-k);
                if(part>0x7fffffffu-tail)mag=0x7fffffffu;
                else mag=part+tail;
            }
        }
    }
    *out=neg?-(modelrt_q16)mag:(modelrt_q16)mag;
    return MODELRT_OK;
}
static modelrt_q16 iqmrt_sat_add(modelrt_q16 a,modelrt_q16 b)
{
    if(b>0 && a>2147483647L-b)return 2147483647L;
    if(b<0 && a<(-2147483647L-1L)-b)return (-2147483647L-1L);
    return a+b;
}
#include "iqm_fixed_skin89.h"

/* IQM frame and rest-pose channels retain their native ordering:
 * translation XYZ, quaternion XYZW, scale XYZ. */
static int iqmrt_joint_trs(const iqmrt_state*s,modelrt_u32 index,iqmrt_transform*tr)
{
    modelrt_u32 base=s->joint_offset+index*48u,k;modelrt_q16*dst;
    for(k=0u;k<10u;k++){
        dst=(k<3u)?&tr->t[k]:(k<7u)?&tr->q[k-3u]:&tr->s[k-7u];
        if(iqmrt_float(s->bytes+base+8u+k*4u,dst))return MODELRT_ERR_FORMAT;
    }
    return MODELRT_OK;
}
static int iqmrt_pose_trs(const iqmrt_state*s,modelrt_u32 frame,modelrt_u32 index,iqmrt_transform*tr)
{
    modelrt_u32 k,i,mask,base,cursor,offset,channel;modelrt_q16 off,change,*dst;
    if(frame>=s->frames||index>=s->poses)return MODELRT_ERR_BAD_ARG;
    cursor=s->frame_offset+frame*s->frame_channels*2u;
    for(i=0u;i<index;i++){
        mask=iqmrt_u32(s->bytes+s->pose_offset+i*88u+4u);
        for(k=0;k<10u;k++)if(mask&(1u<<k))cursor+=2u;
    }
    base=s->pose_offset+index*88u;mask=iqmrt_u32(s->bytes+base+4u);
    for(k=0u;k<10u;k++){
        if(iqmrt_float(s->bytes+base+8u+k*4u,&off))return MODELRT_ERR_FORMAT;
        change=0;
        if(mask&(1u<<k)){
            channel=(modelrt_u32)s->bytes[cursor] | ((modelrt_u32)s->bytes[cursor+1u]<<8);
            cursor+=2u;
            offset=iqmrt_u32(s->bytes+base+48u+k*4u);
            if(iqmrt_scaled(offset,channel,&change))return MODELRT_ERR_FORMAT;
        }
        dst=(k<3u)?&tr->t[k]:(k<7u)?&tr->q[k-3u]:&tr->s[k-7u];
        *dst=iqmrt_sat_add(off,change);
    }
    return iqmrt_normalize_quat(tr->q);
}
static int iqmrt_build_bind(iqmrt_state*s)
{
    modelrt_u32 i;int rc;modelrt_i32 parent;
    iqmrt_transform tr;iqmrt_matrix inv;
    for(i=0u;i<s->joints;i++){
        rc=iqmrt_joint_trs(s,i,&tr);if(rc)return rc;
        rc=iqmrt_matrix_from_trs(&inv,&tr,1);if(rc)return rc;
        parent=(modelrt_i32)iqmrt_u32(s->bytes+s->joint_offset+i*48u+4u);
        if(parent<0)s->inverse_bind[i]=inv;
        else iqmrt_matrix_mul(&s->inverse_bind[i],&inv,&s->inverse_bind[parent]);
    }
    return MODELRT_OK;
}
/* Nlerp takes the shortest quaternion arc and normalizes the result.
 * Time fractions and quaternion arithmetic remain signed Q16.16. */
static modelrt_q16 iqmrt_lerp_q16(modelrt_q16 a,modelrt_q16 b,modelrt_q16 alpha)
{
    return iqmrt_sat_add(iqmrt_fmul(a,IQMRT_ONE-alpha),iqmrt_fmul(b,alpha));
}
static int iqmrt_blend_trs(const iqmrt_transform*a,const iqmrt_transform*b,
                           modelrt_q16 alpha,iqmrt_transform*out)
{
    int i,flip=0;modelrt_q16 dot=0;
    if(alpha<=0){*out=*a;return MODELRT_OK;}
    if(alpha>=IQMRT_ONE){*out=*b;return MODELRT_OK;}
    for(i=0;i<4;i++)dot=iqmrt_sat_add(dot,iqmrt_fmul(a->q[i],b->q[i]));
    if(dot<0)flip=1;
    for(i=0;i<3;i++){
        out->t[i]=iqmrt_lerp_q16(a->t[i],b->t[i],alpha);
        out->s[i]=iqmrt_lerp_q16(a->s[i],b->s[i],alpha);
    }
    for(i=0;i<4;i++)out->q[i]=iqmrt_lerp_q16(a->q[i],
                                                  flip?-b->q[i]:b->q[i],alpha);
    return iqmrt_normalize_quat(out->q);
}
static int iqmrt_sample_trs(const iqmrt_state*s,modelrt_u32 frame0,
                            modelrt_u32 frame1,modelrt_q16 alpha,
                            modelrt_u32 joint,iqmrt_transform*out)
{
    iqmrt_transform a,b;int rc;
    rc=iqmrt_pose_trs(s,frame0,joint,&a);if(rc)return rc;
    if(frame0==frame1||alpha<=0){*out=a;return MODELRT_OK;}
    rc=iqmrt_pose_trs(s,frame1,joint,&b);if(rc)return rc;
    return iqmrt_blend_trs(&a,&b,alpha,out);
}
static int iqmrt_build_sample(iqmrt_state*s,modelrt_u32 frame0,
                              modelrt_u32 frame1,modelrt_q16 alpha)
{
    modelrt_u32 i;int rc;modelrt_i32 parent;
    iqmrt_transform tr;iqmrt_matrix local;
    for(i=0u;i<s->joints;i++){
        rc=iqmrt_sample_trs(s,frame0,frame1,alpha,i,&tr);if(rc)return rc;
        rc=iqmrt_matrix_from_trs(&local,&tr,0);if(rc)return rc;
        parent=(modelrt_i32)iqmrt_u32(s->bytes+s->pose_offset+i*88u);
        if(parent<0)s->world[i]=local;
        else iqmrt_matrix_mul(&s->world[i],&s->world[parent],&local);
        iqmrt_matrix_mul(&s->skin[i],&s->world[i],&s->inverse_bind[i]);
    }
    return MODELRT_OK;
}
static int iqmrt_build_frame(iqmrt_state*s,modelrt_u32 frame)
{
    return iqmrt_build_sample(s,frame,frame,0);
}
/* Cofactor matrix == det(A) * inverse(transpose(A)).
 * The supported IQM transform scale is strictly positive; after normalization
 * the positive determinant factor cancels, and no integer division is needed.
 * Applied per bone before weighted blending (not to the already blended A). */
static void iqmrt_matrix_normal(const iqmrt_matrix*m,
                                const modelrt_vec3*src,modelrt_vec3*dst)
{
    modelrt_q16 a=m->m[0],b=m->m[1],c=m->m[2];
    modelrt_q16 d=m->m[4],e=m->m[5],f=m->m[6];
    modelrt_q16 g=m->m[8],h=m->m[9],i=m->m[10];
    modelrt_q16 v[3],out[3],co[9];int row,col;
    co[0]=iqmrt_sat_add(iqmrt_fmul(e,i),-iqmrt_fmul(f,h));
    co[1]=iqmrt_sat_add(iqmrt_fmul(f,g),-iqmrt_fmul(d,i));
    co[2]=iqmrt_sat_add(iqmrt_fmul(d,h),-iqmrt_fmul(e,g));
    co[3]=iqmrt_sat_add(iqmrt_fmul(c,h),-iqmrt_fmul(b,i));
    co[4]=iqmrt_sat_add(iqmrt_fmul(a,i),-iqmrt_fmul(c,g));
    co[5]=iqmrt_sat_add(iqmrt_fmul(b,g),-iqmrt_fmul(a,h));
    co[6]=iqmrt_sat_add(iqmrt_fmul(b,f),-iqmrt_fmul(c,e));
    co[7]=iqmrt_sat_add(iqmrt_fmul(c,d),-iqmrt_fmul(a,f));
    co[8]=iqmrt_sat_add(iqmrt_fmul(a,e),-iqmrt_fmul(b,d));
    v[0]=src->x;v[1]=src->y;v[2]=src->z;
    for(row=0;row<3;row++){
        out[row]=0;
        for(col=0;col<3;col++)out[row]=iqmrt_sat_add(out[row],
                          iqmrt_fmul(co[row*3+col],v[col]));
    }
    dst->x=out[0];dst->y=out[1];dst->z=out[2];
}
static int iqmrt_skin_vertex(const iqmrt_state*s,modelrt_u32 vi,modelrt_vertex_view*v)
{
    modelrt_u32 k,sum=0u,bone,weight;modelrt_vec3 rest,transformed,acc;
    modelrt_q16 fraction,length2,length;int rc;
    if(!s->skin_active||!s->skin||!s->array_count[4]||!s->array_count[5])return MODELRT_OK;
    for(k=0u;k<4u;k++)sum+=s->bytes[s->arrays[5]+vi*4u+k];
    if(!sum)return MODELRT_OK;
    rest=v->position;acc.x=acc.y=acc.z=0;
    for(k=0u;k<4u;k++){
        weight=s->bytes[s->arrays[5]+vi*4u+k];if(!weight)continue;
        bone=s->bytes[s->arrays[4]+vi*4u+k];if(bone>=s->joints)return MODELRT_ERR_FORMAT;
        rc=iqmrt_fdiv((modelrt_q16)(weight<<16),(modelrt_q16)(sum<<16),&fraction);
        if(rc)return rc;
        iqmrt_matrix_point(&s->skin[bone],&rest,&transformed,0);
        acc.x=iqmrt_sat_add(acc.x,iqmrt_fmul(fraction,transformed.x));
        acc.y=iqmrt_sat_add(acc.y,iqmrt_fmul(fraction,transformed.y));
        acc.z=iqmrt_sat_add(acc.z,iqmrt_fmul(fraction,transformed.z));
    }
    v->position=acc;
    if(v->flags&MODELRT_VTX_NORMAL){
        rest=v->normal;acc.x=acc.y=acc.z=0;
        for(k=0u;k<4u;k++){
            weight=s->bytes[s->arrays[5]+vi*4u+k];if(!weight)continue;
            bone=s->bytes[s->arrays[4]+vi*4u+k];
            rc=iqmrt_fdiv((modelrt_q16)(weight<<16),(modelrt_q16)(sum<<16),&fraction);
            if(rc)return rc;
            iqmrt_matrix_normal(&s->skin[bone],&rest,&transformed);
            acc.x=iqmrt_sat_add(acc.x,iqmrt_fmul(fraction,transformed.x));
            acc.y=iqmrt_sat_add(acc.y,iqmrt_fmul(fraction,transformed.y));
            acc.z=iqmrt_sat_add(acc.z,iqmrt_fmul(fraction,transformed.z));
        }
        length2=iqmrt_sat_add(iqmrt_fmul(acc.x,acc.x),iqmrt_sat_add(iqmrt_fmul(acc.y,acc.y),iqmrt_fmul(acc.z,acc.z)));
        length=iqmrt_fsqr_root(length2);
        if(length>0 && !iqmrt_fdiv(acc.x,length,&v->normal.x)
            && !iqmrt_fdiv(acc.y,length,&v->normal.y)
            && !iqmrt_fdiv(acc.z,length,&v->normal.z)){}
        /* Cofactor-space LBS; normalized once after bone-weight blending. */
    }
    return MODELRT_OK;
}
static modelrt_u32 iqmrt_clip_first(const iqmrt_state*s,int ai)
{return iqmrt_u32(s->bytes+s->anim_offset+(modelrt_u32)ai*20u+4u);}
static modelrt_u32 iqmrt_clip_frames(const iqmrt_state*s,int ai)
{return iqmrt_u32(s->bytes+s->anim_offset+(modelrt_u32)ai*20u+8u);}
static modelrt_u32 iqmrt_clip_rate(const iqmrt_state*s,int ai)
{
    modelrt_q16 q=0;
    if(iqmrt_float(s->bytes+s->anim_offset+(modelrt_u32)ai*20u+12u,&q))return 0u;
    if(q<65536L || (q&65535L)!=0 || q>32767L*65536L)return 0u;
    return (modelrt_u32)(q/65536L);
}
static int iqmrt_animation_count(const modelrt_handle*h)
{return h && h->native_state?(int)((const iqmrt_state*)h->native_state)->anims:0;}
static int iqmrt_animation_info(modelrt_handle*h,int ai,modelrt_animation_info*o)
{
    const iqmrt_state*s;modelrt_u32 ao,ni,frames,rate;
    if(!h||!h->native_state||!o)return MODELRT_ERR_BAD_ARG;
    s=(const iqmrt_state*)h->native_state;
    if(ai<0||(modelrt_u32)ai>=s->anims)return MODELRT_ERR_BAD_ARG;
    modelrt_zero(o,sizeof(*o));ao=s->anim_offset+(modelrt_u32)ai*20u;
    frames=iqmrt_clip_frames(s,ai);rate=iqmrt_clip_rate(s,ai);
    o->index=(modelrt_u32)ai;o->frame_count=frames;
    o->first_tick=0;o->last_tick=(modelrt_i32)(frames-1u);
    o->flags=MODELRT_ANIM_DENSE_FRAMES|MODELRT_ANIM_POSE_SAMPLER;
    if(rate){o->flags|=MODELRT_ANIM_TICK_RATE_KNOWN;o->ticks_per_second=rate;
        if(frames-1u <= (modelrt_u32)(2147483647L/65536L)/rate)
            o->end_time=(modelrt_q16)(((frames-1u)*65536u)/rate);
    }
    ni=iqmrt_u32(s->bytes+ao);
    if(ni<s->text_count && memchr(s->bytes+s->text_offset+ni,0,s->text_count-ni))
        modelrt_copy_string(o->name,sizeof(o->name),(const char*)s->bytes+s->text_offset+ni);
    return MODELRT_OK;
}
static int iqmrt_emit_pose_sample(const iqmrt_state*s,int ai,
                                   modelrt_u32 relative,modelrt_u32 next,
                                   modelrt_q16 alpha,const modelrt_visit_sink*sink,void*u)
{
    modelrt_u32 i,ni,base,first;modelrt_bone_view bone;iqmrt_transform tr;int rc;
    if(ai<0||(modelrt_u32)ai>=s->anims||relative>=iqmrt_clip_frames(s,ai)
        ||next>=iqmrt_clip_frames(s,ai))return MODELRT_ERR_BAD_ARG;
    first=iqmrt_clip_first(s,ai);
    for(i=0u;i<s->poses;i++){
        base=s->pose_offset+i*88u;
        rc=iqmrt_sample_trs(s,first+relative,first+next,alpha,i,&tr);
        if(rc)return rc;
        modelrt_zero(&bone,sizeof(bone));bone.index=i;
        bone.parent=(modelrt_i32)iqmrt_u32(s->bytes+base);
        ni=iqmrt_u32(s->bytes+s->joint_offset+i*48u);
        if(ni<s->text_count && memchr(s->bytes+s->text_offset+ni,0,s->text_count-ni))
            modelrt_copy_string(bone.name,sizeof(bone.name),
                                (const char*)s->bytes+s->text_offset+ni);
        bone.position.x=tr.t[0];bone.position.y=tr.t[1];bone.position.z=tr.t[2];
        if(sink->on_bone&&sink->on_bone(u,&bone))return MODELRT_ERR_CALLBACK;
    }
    return MODELRT_OK;
}
static int iqmrt_visit_sample(modelrt_handle*h,int ai,modelrt_u32 fi,
                               modelrt_u32 next,modelrt_q16 alpha,
                               const modelrt_visit_sink*sink,void*u);
static int iqmrt_visit_frame(modelrt_handle*h,int ai,modelrt_u32 fi,
                             const modelrt_visit_sink*sink,void*u);
static int iqmrt_visit(modelrt_handle*h,int ai,modelrt_q16 t,const modelrt_visit_sink*sink,void*u)
{
    iqmrt_state*s=(iqmrt_state*)h->native_state;modelrt_mesh_info mi;modelrt_triangle_view tr;
    modelrt_bone_view bone;modelrt_u32 m,i,j,k,idx,vid,base,first,count,nameidx;int rc;(void)t;
    if(ai!=MODELRT_ANIM_BIND_POSE){
        modelrt_u32 rate,frame,whole,fraction,clipframes,fracproduct;
        modelrt_q16 alpha;
        if(ai<0||(modelrt_u32)ai>=s->anims||t<0)return MODELRT_ERR_BAD_ARG;
        rate=iqmrt_clip_rate(s,ai);if(!rate)return MODELRT_ERR_UNSUPPORTED;
        whole=(modelrt_u32)t>>16;fraction=(modelrt_u32)t&65535u;
        clipframes=iqmrt_clip_frames(s,ai);
        if(whole>=(clipframes-1u)/rate+1u)return iqmrt_visit_frame(h,ai,clipframes-1u,sink,u);
        fracproduct=fraction*rate; /* 65535 * 32767 fits in 32 bits */
        frame=whole*rate+(fracproduct>>16);
        if(frame>=clipframes-1u)return iqmrt_visit_frame(h,ai,clipframes-1u,sink,u);
        alpha=(modelrt_q16)(fracproduct&65535u);
        return iqmrt_visit_sample(h,ai,frame,frame+1u,alpha,sink,u);
    }
    if(sink->on_model_begin&&sink->on_model_begin(u,&h->info))return MODELRT_ERR_CALLBACK;
    for(m=0;m<s->meshes;m++){
        base=s->mesh_offset+m*24u;first=iqmrt_u32(s->bytes+base+16u);count=iqmrt_u32(s->bytes+base+20u);
        modelrt_zero(&mi,sizeof(mi));mi.index=m;mi.triangle_count=count;
        mi.vertex_count=iqmrt_u32(s->bytes+base+12u);
        nameidx=iqmrt_u32(s->bytes+base);
        if(nameidx<s->text_count && memchr(s->bytes+s->text_offset+nameidx,0,s->text_count-nameidx)!=0)modelrt_copy_string(mi.name,sizeof(mi.name),(const char*)s->bytes+s->text_offset+nameidx);
        if(sink->on_mesh_begin&&sink->on_mesh_begin(u,&mi))return MODELRT_ERR_CALLBACK;
        for(i=first;i<first+count;i++){
            modelrt_zero(&tr,sizeof(tr));tr.mesh_index=m;tr.material_index=(modelrt_i32)m;
            for(j=0;j<3u;j++){
                idx=iqmrt_u32(s->bytes+s->tri_offset+12u*i+4u*j);
                for(k=0;k<3u;k++){
                    vid=s->arrays[0]+idx*12u+k*4u;
                    rc=iqmrt_float(s->bytes+vid,(k==0u?&tr.v[j].position.x:k==1u?&tr.v[j].position.y:&tr.v[j].position.z));if(rc)return rc;
                }
                if(s->array_count[1]){
                    rc=iqmrt_float(s->bytes+s->arrays[1]+idx*8u,&tr.v[j].uv0.x);if(rc)return rc;
                    rc=iqmrt_float(s->bytes+s->arrays[1]+idx*8u+4u,&tr.v[j].uv0.y);if(rc)return rc;
                    tr.v[j].flags|=MODELRT_VTX_UV0;
                }
                if(s->array_count[2]){
                    for(k=0;k<3u;k++){
                        rc=iqmrt_float(s->bytes+s->arrays[2]+idx*12u+k*4u,(k==0u?&tr.v[j].normal.x:k==1u?&tr.v[j].normal.y:&tr.v[j].normal.z));if(rc)return rc;
                    }
                    tr.v[j].flags|=MODELRT_VTX_NORMAL;
                }
                if(s->array_count[4]&&s->array_count[5]){
                    tr.v[j].weight_count=4u;tr.v[j].flags|=MODELRT_VTX_SKIN;
                    for(k=0;k<4u;k++){
                        tr.v[j].bone[k]=(modelrt_i16)s->bytes[s->arrays[4]+idx*4u+k];
                        tr.v[j].weight[k]=(modelrt_q16)((modelrt_u32)s->bytes[s->arrays[5]+idx*4u+k]*257u);
                    }
                }
            }
            if(s->skin_active){
                for(j=0u;j<3u;j++){
                    idx=iqmrt_u32(s->bytes+s->tri_offset+12u*i+4u*j);
                    rc=iqmrt_skin_vertex(s,idx,&tr.v[j]);if(rc)return rc;
                }
            }
            if(sink->on_triangle&&sink->on_triangle(u,&tr))return MODELRT_ERR_CALLBACK;
        }
        if(sink->on_mesh_end&&sink->on_mesh_end(u,&mi))return MODELRT_ERR_CALLBACK;
    }
    for(i=0;i<s->joints;i++){
        base=s->joint_offset+i*48u;modelrt_zero(&bone,sizeof(bone));bone.index=i;
        bone.parent=(modelrt_i32)iqmrt_u32(s->bytes+base+4u);
        nameidx=iqmrt_u32(s->bytes+base);
        if(nameidx<s->text_count && memchr(s->bytes+s->text_offset+nameidx,0,s->text_count-nameidx)!=0)modelrt_copy_string(bone.name,sizeof(bone.name),(const char*)s->bytes+s->text_offset+nameidx);
        if(iqmrt_float(s->bytes+base+8u,&bone.position.x)!=MODELRT_OK||iqmrt_float(s->bytes+base+12u,&bone.position.y)!=MODELRT_OK||iqmrt_float(s->bytes+base+16u,&bone.position.z)!=MODELRT_OK)return MODELRT_ERR_FORMAT;
        if(sink->on_bone&&sink->on_bone(u,&bone))return MODELRT_ERR_CALLBACK;
    }
    return sink->on_model_end&&sink->on_model_end(u,&h->info)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
static int iqmrt_visit_sample(modelrt_handle*h,int ai,modelrt_u32 fi,
                               modelrt_u32 next,modelrt_q16 alpha,
                               const modelrt_visit_sink*sink,void*u)
{
    iqmrt_state*s;modelrt_visit_sink nonbones;int rc;modelrt_u32 first;
    if(!h||!h->native_state||!sink)return MODELRT_ERR_BAD_ARG;
    s=(iqmrt_state*)h->native_state;
    if(ai<0||(modelrt_u32)ai>=s->anims||fi>=iqmrt_clip_frames(s,ai)
       ||next>=iqmrt_clip_frames(s,ai))return MODELRT_ERR_BAD_ARG;
    first=iqmrt_clip_first(s,ai);
    nonbones=*sink;nonbones.on_bone=0;nonbones.on_model_end=0;
    rc=iqmrt_build_sample(s,first+fi,first+next,alpha);if(rc)return rc;
    s->skin_active=1;
    rc=iqmrt_visit(h,MODELRT_ANIM_BIND_POSE,0,&nonbones,u);
    s->skin_active=0;
    if(rc)return rc;
    rc=iqmrt_emit_pose_sample(s,ai,fi,next,alpha,sink,u);
    if(rc)return rc;
    return sink->on_model_end&&sink->on_model_end(u,&h->info)?MODELRT_ERR_CALLBACK:MODELRT_OK;
}
static int iqmrt_visit_frame(modelrt_handle*h,int ai,modelrt_u32 fi,
                             const modelrt_visit_sink*sink,void*u)
{return iqmrt_visit_sample(h,ai,fi,fi,0,sink,u);}
const modelrt_provider modelrt_provider_native_iqm89={IQMRT_ID,"IQM v2 native C89",iqmrt_probe,iqmrt_open,0,iqmrt_visit,iqmrt_animation_count,iqmrt_animation_info,0,0,0,0,0,0,iqmrt_visit_frame};
int modelrt_native_iqm_bytes89(const modelrt_handle *h,const unsigned char **bytes,size_t *size)
{
    const iqmrt_state *s;
    if(!h||!h->is_open||h->info.codec_id!=IQMRT_ID||!bytes||!size)return MODELRT_ERR_BAD_ARG;
    s=(const iqmrt_state*)h->native_state;*bytes=s->bytes;*size=s->n;return MODELRT_OK;
}

int modelrt_native_iqm_pose89(modelrt_handle*h,int animation_index,modelrt_u32 frame,modelrt_u32 joint,modelrt_iqm_pose89*out)
{
    iqmrt_state*s;modelrt_u32 rawframe;int rc;iqmrt_transform tr;modelrt_u32 i;
    if(!h||!h->is_open||h->info.codec_id!=IQMRT_ID||!out)return MODELRT_ERR_BAD_ARG;
    s=(iqmrt_state*)h->native_state;
    if(animation_index<0||(modelrt_u32)animation_index>=s->anims||joint>=s->joints||frame>=iqmrt_clip_frames(s,animation_index))return MODELRT_ERR_BAD_ARG;
    rawframe=iqmrt_clip_first(s,animation_index)+frame;
    rc=iqmrt_pose_trs(s,rawframe,joint,&tr);if(rc)return rc;
    rc=iqmrt_build_frame(s,rawframe);if(rc)return rc;
    modelrt_zero(out,sizeof(*out));out->joint_index=joint;
    out->parent=(modelrt_i32)iqmrt_u32(s->bytes+s->pose_offset+joint*88u);
    for(i=0u;i<3u;i++){
        out->translation[i]=tr.t[i];out->scale[i]=tr.s[i];
    }
    for(i=0u;i<4u;i++)out->rotation_xyzw[i]=tr.q[i];
    for(i=0u;i<12u;i++)out->world_matrix[i]=s->world[joint].m[i];
    return MODELRT_OK;
}

int modelrt_native_iqm_pose_time89(modelrt_handle*h,int animation_index,
                                  modelrt_q16 time,modelrt_u32 joint,
                                  modelrt_iqm_pose89*out)
{
    iqmrt_state*s;modelrt_u32 rate,frame,other,clip,whole,part,fp,raw0,raw1,i;
    modelrt_q16 alpha;iqmrt_transform tr;int rc;
    if(!h||!h->is_open||h->info.codec_id!=IQMRT_ID||!out||time<0)return MODELRT_ERR_BAD_ARG;
    s=(iqmrt_state*)h->native_state;
    if(animation_index<0||(modelrt_u32)animation_index>=s->anims||joint>=s->joints)return MODELRT_ERR_BAD_ARG;
    rate=iqmrt_clip_rate(s,animation_index);if(!rate)return MODELRT_ERR_UNSUPPORTED;
    clip=iqmrt_clip_frames(s,animation_index);whole=(modelrt_u32)time>>16;
    part=(modelrt_u32)time&65535u;
    if(whole>=(clip-1u)/rate+1u){frame=clip-1u;other=frame;alpha=0;}
    else{
        fp=part*rate;frame=whole*rate+(fp>>16);
        if(frame>=clip-1u){frame=clip-1u;other=frame;alpha=0;}
        else{other=frame+1u;alpha=(modelrt_q16)(fp&65535u);}
    }
    raw0=iqmrt_clip_first(s,animation_index)+frame;
    raw1=iqmrt_clip_first(s,animation_index)+other;
    rc=iqmrt_sample_trs(s,raw0,raw1,alpha,joint,&tr);if(rc)return rc;
    rc=iqmrt_build_sample(s,raw0,raw1,alpha);if(rc)return rc;
    modelrt_zero(out,sizeof(*out));out->joint_index=joint;
    out->parent=(modelrt_i32)iqmrt_u32(s->bytes+s->pose_offset+joint*88u);
    for(i=0u;i<3u;i++){out->translation[i]=tr.t[i];out->scale[i]=tr.s[i];}
    for(i=0u;i<4u;i++)out->rotation_xyzw[i]=tr.q[i];
    for(i=0u;i<12u;i++)out->world_matrix[i]=s->world[joint].m[i];
    return MODELRT_OK;
}
