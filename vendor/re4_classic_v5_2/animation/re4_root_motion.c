/* White-room FCV root-position track observer. C89; no floats/no heap. */
#include "re4c3d/re4_root_motion.h"
#include <string.h>
re4_err re4_root_find(const re4_fcv *fcv,re4_root_track *out)
{
    re4_u32 i;re4_u32 found=0;
    if(!fcv||!out)return RE4_ERR_NULL;
    memset(out,0,sizeof(*out));
    if(fcv->block_count && !fcv->blocks)return RE4_ERR_BAD_FORMAT;
    for(i=0;i<fcv->block_count;++i){
        if(fcv->blocks[i].bone_type==1U && fcv->blocks[i].bone_no==0U){
            if(found)return RE4_ERR_BAD_FORMAT;
            out->channel_index=i;out->present=RE4_TRUE;++found;
        }
    }
    return RE4_OK;
}
re4_err re4_root_sample(re4_span s,const re4_fcv *fcv,
    const re4_root_track *track,re4_u16 frame,re4_q16 xyz[3])
{
    const re4_fcv_block *b;
    if(!fcv||!track||!xyz)return RE4_ERR_NULL;
    xyz[0]=xyz[1]=xyz[2]=0;
    if(!track->present)return RE4_OK;
    if(!s.data||!fcv->blocks)return RE4_ERR_NULL;
    if(track->channel_index>=fcv->block_count)return RE4_ERR_BAD_FORMAT;
    b=&fcv->blocks[track->channel_index];
    if(b->bone_type!=1U || b->bone_no!=0U)return RE4_ERR_BAD_FORMAT;
    return re4_fcv_block_sample_hermite_q16(s,b,frame,fcv->endian,xyz);
}
re4_err re4_root_relative(re4_span s,const re4_fcv *fcv,
    const re4_root_track *track,re4_u16 origin,re4_u16 frame,
    re4_q16 off[3])
{
    re4_q16 start[3],end[3];re4_i32 a,b;re4_u32 i;re4_err e;
    if(!off)return RE4_ERR_NULL;
    off[0]=off[1]=off[2]=0;
    e=re4_root_sample(s,fcv,track,origin,start);if(e!=RE4_OK)return e;
    e=re4_root_sample(s,fcv,track,frame,end);if(e!=RE4_OK)return e;
    for(i=0;i<3U;++i){
        a=end[i];b=start[i];
        if((b<0 && a>RE4_I32_MAX+b)||(b>0 && a<RE4_I32_MIN+b))return RE4_ERR_RANGE;
        off[i]=a-b;
    }
    return RE4_OK;
}
re4_err re4_root_translate(re4_skel_pose *pose,const re4_q16 offset[3])
{
    re4_u32 i,j;re4_i32 oldv,d;
    if(!pose||!offset||(!pose->world && pose->bone_count))return RE4_ERR_NULL;
    if(pose->bone_count>128U)return RE4_ERR_NO_SPACE;
    /* Validate ALL matrices before modifying any, so overflow is atomic. */
    for(i=0;i<pose->bone_count;++i)for(j=0;j<3U;++j){
        oldv=pose->world[i].m[j*4U+3U];d=offset[j];
        if((d>0 && oldv>RE4_I32_MAX-d)||(d<0 && oldv<RE4_I32_MIN-d))return RE4_ERR_RANGE;
    }
    for(i=0;i<pose->bone_count;++i)for(j=0;j<3U;++j)
        pose->world[i].m[j*4U+3U]+=offset[j];
    return RE4_OK;
}

/* Mix two relative root displacements, not raw absolute clip origins.
   Require the intermediate difference to fit signed 32-bit; never wrap. */
re4_err re4_root_blend_relative(re4_span sa,const re4_fcv *fa,
    const re4_root_track *ta,re4_u16 oa,re4_u16 at,
    re4_span sb,const re4_fcv *fb,
    const re4_root_track *tb,re4_u16 ob,re4_u16 bt,
    re4_q16 weight_b,re4_q16 xyz[3])
{
    re4_q16 a[3],b[3],result[3];re4_u32 k;re4_err e;
    if(!xyz||!fa||!ta||!fb||!tb)return RE4_ERR_NULL;
    if(weight_b<0||weight_b>65536L)return RE4_ERR_BAD_FORMAT;
    e=re4_root_relative(sa,fa,ta,oa,at,a);if(e!=RE4_OK)return e;
    e=re4_root_relative(sb,fb,tb,ob,bt,b);if(e!=RE4_OK)return e;
    for(k=0;k<3U;++k){
        re4_i32 diff,scaled;re4_u32 magnitude,positive,hi,lo;
        if(weight_b==0){result[k]=a[k];continue;}
        if(weight_b==65536L){result[k]=b[k];continue;}
        if((a[k]>0 && b[k]<RE4_I32_MIN+a[k]) ||
           (a[k]<0 && b[k]>RE4_I32_MAX+a[k]))return RE4_ERR_RANGE;
        diff=b[k]-a[k];
        magnitude=(diff<0)?(re4_u32)(-(diff+1))+1U:(re4_u32)diff;
        hi=magnitude>>16;lo=magnitude&65535U;
        positive=hi*(re4_u32)weight_b+((lo*(re4_u32)weight_b)>>16);
        scaled=(diff<0)?-(re4_i32)positive:(re4_i32)positive;
        if((scaled>0 && a[k]>RE4_I32_MAX-scaled)||
           (scaled<0 && a[k]<RE4_I32_MIN-scaled))return RE4_ERR_RANGE;
        result[k]=a[k]+scaled;
    }
    for(k=0;k<3U;++k)xyz[k]=result[k];
    return RE4_OK;
}
