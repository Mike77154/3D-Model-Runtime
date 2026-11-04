/* RE4 2007 FCV: interleaved (node_type,encoding) byte pairs, joint ID list,
   aligned file-size word and pointer table, per-axis u16 frame IDs and
   (value,in_tangent,out_tangent) records. No heap, no floating point. */
#include "re4c3d/re4_animation.h"
#include "re4c3d/re4_base.h"
#include <string.h>

static re4_u8 fcv_sizes(re4_u8 type, re4_u8 sz[3])
{
    switch (type & 0xF0U) {
    case 0x00U: sz[0]=4; sz[1]=4; sz[2]=4; break;
    case 0x10U: sz[0]=4; sz[1]=2; sz[2]=2; break;
    case 0x20U: sz[0]=4; sz[1]=1; sz[2]=1; break;
    case 0x40U: sz[0]=2; sz[1]=4; sz[2]=4; break;
    case 0x50U: sz[0]=2; sz[1]=2; sz[2]=2; break;
    case 0x60U: sz[0]=2; sz[1]=1; sz[2]=1; break;
    case 0x80U: sz[0]=1; sz[1]=4; sz[2]=4; break;
    case 0x90U: sz[0]=1; sz[1]=2; sz[2]=2; break;
    case 0xA0U: sz[0]=1; sz[1]=1; sz[2]=1; break;
    case 0xF0U: sz[0]=4; sz[1]=0; sz[2]=0; break;
    default: return 0;
    }
    return 1;
}
re4_u8 re4_fcv_value_tuple_size(re4_u8 t) { re4_u8 a[3]; if(!fcv_sizes(t,a))return 0;return a[0]+a[1]+a[2]; }
re4_u8 re4_fcv_axis_value_size(re4_u8 t,re4_u32 axis) {re4_u8 a[3];if(axis>2||!fcv_sizes(t,a))return 0;return a[axis];}
const char *re4_fcv_bone_type_name(re4_u8 t)
{
    switch(t){case 1:return "root_position";case 2:return "rotation_relative";case 4:return "ik_translation";case 8:return "scale";case 16:return "ik_parent";case 32:return "ik_toe";case 64:return "root_rotation";case 128:return "flip";case 160:return "ik_arm";default:return "unknown";}
}
const char *re4_fcv_scalar_name(re4_u8 s){return s==4?"f32":s==2?"i16/10000":s==1?"i8":"unknown";}
re4_err re4_fcv_read(re4_span s,re4_endian en,re4_arena *a,re4_fcv *out)
{
    re4_u32 i,n;re4_size pos;re4_fcv_block *b;
    if(!a||!out||!s.data)return RE4_ERR_NULL;
    memset(out,0,sizeof(*out));if(!re4_span_ok(s,0,3))return RE4_ERR_RANGE;
    n=s.data[2];if(n==0)return RE4_ERR_BAD_FORMAT;
    pos=3UL+3UL*n;pos=(pos+3UL)&~3UL;
    if(!re4_span_ok(s,pos,4UL+4UL*n))return RE4_ERR_RANGE;
    b=(re4_fcv_block*)re4_arena_push(a,n*sizeof(*b),sizeof(void*));if(!b)return RE4_ERR_NO_SPACE;
    memset(b,0,n*sizeof(*b));out->max_time=(re4_u16)(re4_read_u16_e(s.data,en)&0x3FFFU);out->frame_flags=(re4_u8)(re4_read_u16_e(s.data,en)>>14);out->block_count=(re4_u8)n;
    out->declared_file_size=re4_read_u32_e(s.data+pos,en);out->endian=en;
    for(i=0;i<n;++i){
        re4_u8 sizes[3],ty;
        b[i].bone_type=s.data[3UL+2UL*i];ty=s.data[4UL+2UL*i];
        b[i].data_type=ty;
        if(!fcv_sizes(ty,sizes))return RE4_ERR_UNSUPPORTED;
        b[i].value_bytes_per_key=sizes[0]+sizes[1]+sizes[2];
        b[i].axis_value_size[0]=sizes[0];b[i].axis_value_size[1]=sizes[1];b[i].axis_value_size[2]=sizes[2];
        b[i].bone_no=s.data[3UL+2UL*n+i];
        b[i].data_offset=re4_read_u32_e(s.data+pos+4UL+4UL*i,en);
        if(b[i].data_offset>=s.size)return RE4_ERR_RANGE;
    }
    out->blocks=b;return RE4_OK;
}
static re4_err fcv_one(re4_span src,re4_size *p,re4_u8 type,re4_endian en,re4_fcv_axis_view *v)
{
    re4_u8 sz[3];re4_u32 cnt;re4_size bytes;
    if(!fcv_sizes(type,sz))return RE4_ERR_UNSUPPORTED;
    if(!re4_span_ok(src,*p,2))return RE4_ERR_RANGE;
    cnt=re4_read_u16_e(src.data+*p,en);*p+=2;
    if(cnt==0 || cnt>8192U)return RE4_ERR_BAD_FORMAT;
    bytes=2UL*cnt;
    if(!re4_span_ok(src,*p,bytes))return RE4_ERR_RANGE;
    memset(v,0,sizeof(*v));v->tag_count=(re4_u16)cnt;v->value_size=sz[0];v->tangent_in_size=sz[1];v->tangent_out_size=sz[2];v->record_size=sz[0]+sz[1]+sz[2];
    v->time_shifts.data=src.data+*p;v->time_shifts.size=bytes;*p+=bytes;
    bytes=(re4_size)cnt*v->record_size;
    if(!re4_span_ok(src,*p,bytes))return RE4_ERR_RANGE;
    v->values.data=src.data+*p;v->values.size=bytes;*p+=bytes;
    return RE4_OK;
}
re4_err re4_fcv_block_axis_views_e(re4_span s,const re4_fcv_block *b,re4_endian en,re4_fcv_axis_view *x,re4_fcv_axis_view *y,re4_fcv_axis_view *z)
{
    re4_size p;re4_err e;if(!b||!x||!y||!z)return RE4_ERR_NULL;p=b->data_offset;
    e=fcv_one(s,&p,b->data_type,en,x);if(e!=RE4_OK)return e;
    e=fcv_one(s,&p,b->data_type,en,y);if(e!=RE4_OK)return e;
    return fcv_one(s,&p,b->data_type,en,z);
}
re4_err re4_fcv_block_axis_views(re4_span s,const re4_fcv_block *b,re4_fcv_axis_view *x,re4_fcv_axis_view *y,re4_fcv_axis_view *z)
{
    return re4_fcv_block_axis_views_e(s,b,RE4_ENDIAN_LITTLE,x,y,z);
}
/* Signed scalar q16 conversion with integer-only code. Real FCV compressed
   i16 value/tangents are scaled by 10000; f32 bits are converted elsewhere. */
static re4_q16 signed_scalar(const re4_u8 *p,re4_u8 size,re4_endian en)
{
    if(size==4)return re4_f32bits_to_q16(re4_read_u32_e(p,en));
    if(size==2){
        re4_i32 v=(re4_i32)(re4_i16)re4_read_u16_e(p,en);
        return (re4_q16)((v/10000L)*65536L+((v%10000L)*65536L)/10000L);
    }
    return (re4_q16)(((re4_i32)(re4_i8)p[0]*65536L)/10000L);
}
static re4_q16 read_key(const re4_fcv_axis_view *a,re4_u32 k,re4_endian en,re4_u8 field)
{
    re4_size off=(re4_size)k*a->record_size;
    re4_u8 sz=a->value_size;
    if(field==1){off+=a->value_size;sz=a->tangent_in_size;}
    if(field==2){off+=(re4_size)a->value_size+a->tangent_in_size;sz=a->tangent_out_size;}
    if(sz==0)return 0;
    return signed_scalar(a->values.data+off,sz,en);
}
re4_err re4_fcv_axis_value_q16(const re4_fcv_axis_view *a,re4_u32 k,re4_endian en,re4_q16 *o)
{
    if(!a||!o)return RE4_ERR_NULL;
    if(k>=a->tag_count)return RE4_ERR_RANGE;
    if(!re4_span_ok(a->values,(re4_size)k*a->record_size,a->record_size))return RE4_ERR_RANGE;
    *o=read_key(a,k,en,0);return RE4_OK;
}
re4_err re4_fcv_axis_time_at_e(const re4_fcv_axis_view *a,re4_u32 k,re4_endian en,re4_u16 *o)
{
    if(!a||!o)return RE4_ERR_NULL;
    if(k>=a->tag_count)return RE4_ERR_RANGE;
    if(!re4_span_ok(a->time_shifts,(re4_size)k*2UL,2))return RE4_ERR_RANGE;
    *o=re4_read_u16_e(a->time_shifts.data+2UL*k,en);return RE4_OK;
}
re4_err re4_fcv_axis_time_at(const re4_fcv_axis_view *a,re4_u32 k,re4_u16 *o)
{
    return re4_fcv_axis_time_at_e(a,k,RE4_ENDIAN_LITTLE,o);
}
re4_err re4_fcv_axis_sample_nearest_q16(const re4_fcv_axis_view *a,re4_u16 time,re4_endian en,re4_q16 *out,re4_u32 *out_i)
{
    re4_u32 i,best=0,best_dist=0xFFFFFFFFUL;
    if(!a||!out)return RE4_ERR_NULL;
    for(i=0;i<a->tag_count;++i){re4_u16 t;re4_u32 d;re4_err e=re4_fcv_axis_time_at_e(a,i,en,&t);if(e!=RE4_OK)return e;d=t>time?t-time:time-t;if(d<best_dist){best_dist=d;best=i;}}
    if(out_i)*out_i=best;
    return re4_fcv_axis_value_q16(a,best,en,out);
}
/* q16 multiply: unsigned 16x16 partial products, saturate to i32.
   No C99 int64, no floating point. */
static re4_q16 qmul(re4_q16 a,re4_q16 b)
{
    re4_u32 ua,ub,ha,hb,la,lb,r,p;
    re4_bool neg=((a<0)!=(b<0));
    ua=a<0?(re4_u32)(-(a+1))+1UL:(re4_u32)a;
    ub=b<0?(re4_u32)(-(b+1))+1UL:(re4_u32)b;
    ha=ua>>16;hb=ub>>16;la=ua&65535UL;lb=ub&65535UL;
    if(ha && hb>32767UL/ha)return neg?(-2147483647L-1L):2147483647L;
    r=(ha*hb)<<16;
    p=ha*lb;if(p>2147483647UL-r)return neg?(-2147483647L-1L):2147483647L;r+=p;
    p=hb*la;if(p>2147483647UL-r)return neg?(-2147483647L-1L):2147483647L;r+=p;
    p=(la*lb)>>16;if(p>2147483647UL-r)return neg?(-2147483647L-1L):2147483647L;r+=p;
    return neg?-(re4_i32)r:(re4_i32)r;
}
static re4_q16 qadd(re4_q16 a,re4_q16 b)
{
    if(b>0 && a>2147483647L-b)return 2147483647L;
    if(b<0 && a<(-2147483647L-1L)-b)return -2147483647L-1L;
    return a+b;
}
re4_err re4_fcv_axis_sample_hermite_q16(const re4_fcv_axis_view *a,re4_u16 t,re4_endian en,re4_q16 *out)
{
    re4_u32 i;re4_u16 t0,t1;re4_q16 u,u2,u3,h00,h10,h01,h11,p0,p1,m0,m1,dt,r;
    if(!a||!out)return RE4_ERR_NULL;
    if(a->tag_count==0)return RE4_ERR_BAD_FORMAT;
    if(re4_fcv_axis_time_at_e(a,0,en,&t0)!=RE4_OK)return RE4_ERR_RANGE;
    if(t<=t0)return re4_fcv_axis_value_q16(a,0,en,out);
    for(i=1;i<a->tag_count;++i){
        if(re4_fcv_axis_time_at_e(a,i,en,&t1)!=RE4_OK)return RE4_ERR_RANGE;
        if(t<=t1){
            if(t1<=t0)return re4_fcv_axis_value_q16(a,i,en,out);
            dt=((re4_u32)(t1-t0)>32767U)?2147483647L:(re4_q16)((re4_u32)(t1-t0)*65536U);u=(re4_q16)(((re4_u32)(t-t0)*65536U)/(re4_u32)(t1-t0));
            u2=qmul(u,u);u3=qmul(u2,u);
            h00=2L*u3-3L*u2+65536L;h10=u3-2L*u2+u;
            h01=-2L*u3+3L*u2;h11=u3-u2;
            p0=read_key(a,i-1,en,0);p1=read_key(a,i,en,0);
            m0=read_key(a,i-1,en,2);m1=read_key(a,i,en,1);
            r=qadd(qmul(h00,p0),qmul(h01,p1));
            r=qadd(r,qmul(h10,qmul(dt,m0)));r=qadd(r,qmul(h11,qmul(dt,m1)));
            *out=r;return RE4_OK;
        }
        t0=t1;
    }
    return re4_fcv_axis_value_q16(a,a->tag_count-1,en,out);
}
re4_err re4_fcv_block_sample_nearest_q16(re4_span s,const re4_fcv_block *b,re4_u16 t,re4_endian en,re4_q16 o[3])
{
    re4_fcv_axis_view a[3];re4_u32 i;re4_err e;
    if(!b||!o)return RE4_ERR_NULL;
    e=re4_fcv_block_axis_views_e(s,b,en,&a[0],&a[1],&a[2]);if(e!=RE4_OK)return e;
    for(i=0;i<3;++i){e=re4_fcv_axis_sample_nearest_q16(&a[i],t,en,&o[i],0);if(e!=RE4_OK)return e;}
    return RE4_OK;
}
re4_err re4_fcv_validate(re4_span s,const re4_fcv *f)
{
    re4_u32 i;if(!f||!f->blocks)return RE4_ERR_NULL;
    for(i=0;i<f->block_count;++i){re4_fcv_axis_view x,y,z;re4_err e=re4_fcv_block_axis_views_e(s,&f->blocks[i],f->endian,&x,&y,&z);if(e!=RE4_OK)return e;}
    return RE4_OK;
}

re4_err re4_fcv_axis_key_q16(const re4_fcv_axis_view *a,re4_u32 i,re4_endian en,re4_q16 xyz[3])
{
    if(!a||!xyz)return RE4_ERR_NULL;
    if(i>=a->tag_count||!re4_span_ok(a->values,(re4_size)i*a->record_size,a->record_size))return RE4_ERR_RANGE;
    xyz[0]=read_key(a,i,en,0);
    xyz[1]=read_key(a,i,en,1);
    xyz[2]=a->tangent_out_size?read_key(a,i,en,2):xyz[1];
    return RE4_OK;
}
re4_err re4_fcv_keyframe_union(re4_span src,const re4_fcv *fcv,
                                re4_u8 *bits,re4_size capacity,re4_u32 *count)
{
    re4_u32 i,j,k,n;re4_u16 t;re4_err e;re4_fcv_axis_view axes[3];
    if(!fcv||!bits||!count)return RE4_ERR_NULL;
    if(capacity<8192UL)return RE4_ERR_NO_SPACE;
    memset(bits,0,8192UL);n=0;
    for(i=0;i<fcv->block_count;++i){
        e=re4_fcv_block_axis_views_e(src,&fcv->blocks[i],fcv->endian,&axes[0],&axes[1],&axes[2]);
        if(e!=RE4_OK)return e;
        for(j=0;j<3U;++j)for(k=0;k<axes[j].tag_count;++k){
            e=re4_fcv_axis_time_at_e(&axes[j],k,fcv->endian,&t);if(e!=RE4_OK)return e;
            if((bits[t>>3]&(re4_u8)(1U<<(t&7U)))==0){bits[t>>3]|=(re4_u8)(1U<<(t&7U));++n;}
        }
    }
    *count=n;return RE4_OK;
}
re4_err re4_fcv_block_sample_hermite_q16(re4_span src,const re4_fcv_block *b,
                                          re4_u16 time,re4_endian en,re4_q16 xyz[3])
{
    re4_fcv_axis_view a[3];re4_err e;re4_u32 i;
    if(!b||!xyz)return RE4_ERR_NULL;
    e=re4_fcv_block_axis_views_e(src,b,en,&a[0],&a[1],&a[2]);if(e!=RE4_OK)return e;
    for(i=0;i<3U;++i){e=re4_fcv_axis_sample_hermite_q16(&a[i],time,en,&xyz[i]);if(e!=RE4_OK)return e;}
    return RE4_OK;
}
