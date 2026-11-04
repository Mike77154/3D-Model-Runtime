/* RE4 FCV IK flags: safe candidate identification and optional contact locks.
   White-room C89 implementation. No native IK equivalence is asserted. */
#include "re4c3d/re4_ik.h"
#include <string.h>
static re4_i32 name_index(const re4_pmd_model *m, re4_u8 id)
{
    re4_u32 k;re4_strref n;
    if(!m->skeleton_names)return -1;
    for(k=0;k<m->skeleton_count && k<m->skeleton_name_count;++k){
        n=m->skeleton_names[k].name;
        if(n.ptr && n.len>=8UL && !memcmp(n.ptr,"born__",6UL) &&
           n.ptr[6]>='0'&&n.ptr[6]<='9'&&n.ptr[7]>='0'&&n.ptr[7]<='9'&&
           (re4_u32)(10*(n.ptr[6]-'0')+(n.ptr[7]-'0'))==(re4_u32)id)
            return (re4_i32)k;
    }
    return -1;
}
static re4_i32 child(const re4_pmd_model *m,re4_i32 parent)
{
    re4_u32 k;if(parent<0)return -1;
    for(k=0;k<m->skeleton_count;++k)if(m->skeletons[k].parent==parent)return (re4_i32)k;
    return -1;
}
re4_err re4_ik_discover(const re4_pmd_model *m,const re4_fcv *fcv,
    re4_ik_chain *out,re4_u32 cap,re4_u32 *out_count,re4_u32 *out_valid)
{
    re4_u32 i,c=0,v=0;re4_ik_chain x;
    if(!m||!fcv||!out_count||!out_valid)return RE4_ERR_NULL;
    *out_count=0;*out_valid=0;
    if(m->skeleton_count>RE4_SKEL_MAX_BONES || (m->skeleton_count && !m->skeletons))return RE4_ERR_BAD_FORMAT;
    if(fcv->block_count && !fcv->blocks)return RE4_ERR_BAD_FORMAT;
    for(i=0;i<fcv->block_count;++i){
        const re4_fcv_block *b=&fcv->blocks[i];
        if(!(b->bone_type&0x30U))continue;
        if(c>=cap||!out)return RE4_ERR_NO_SPACE;
        memset(&x,0,sizeof(x));
        x.channel_index=i;x.root_parts_id=b->bone_no;x.kind_flags=b->bone_type;
        x.bend_axis_code=(re4_u8)(b->data_type&0x0FU);
        x.toe_flag=(re4_u8)((b->bone_type&0x20U)!=0U);
        x.four_joint_flag=(re4_u8)((b->bone_type&0x80U)!=0U);
        x.root=name_index(m,b->bone_no);
        x.joint=child(m,x.root);
        x.effector=child(m,x.joint);
        x.toe= (x.toe_flag||x.four_joint_flag)?child(m,x.effector):-1;
        x.topology_ok=(x.root>=0 && x.joint>=0 && x.effector>=0 &&
                       x.bend_axis_code<=5U) ?RE4_TRUE:RE4_FALSE;
        if((x.toe_flag||x.four_joint_flag) && x.toe<0)x.topology_ok=RE4_FALSE;
        if(x.topology_ok)++v;
        out[c++]=x;
    }
    *out_count=c;*out_valid=v;return RE4_OK;
}
re4_err re4_ik_lock_contacts(const re4_pmd_model *m,
    const re4_ik_chain *chains,re4_u32 count,
    const re4_skel_pose *reference,re4_skel_pose *current,
    re4_u32 iterations,re4_q16 step,
    re4_u32 *out_applied,re4_u32 *out_skipped)
{
    re4_u32 i,j,applied=0,skipped=0;re4_q16 tgt[3],error;re4_err e;
    if(!m||!reference||!current||!reference->world||!current->world ||
       (count && !chains))return RE4_ERR_NULL;
    if(reference->bone_count<m->skeleton_count||current->bone_count<m->skeleton_count)
        return RE4_ERR_BAD_FORMAT;
    if(iterations>256U || step<=0 || step>65536L)return RE4_ERR_BAD_FORMAT;
    for(i=0;i<count;++i){
        const re4_ik_chain *c=&chains[i];
        if(!c->topology_ok||c->root<0||c->joint<0||c->effector<0 ||
           (re4_u32)c->effector>=m->skeleton_count){++skipped;continue;}
        for(j=0;j<3U;++j)tgt[j]=reference->world[c->effector].m[4U*j+3U];
        e=re4_skel_ik_ccd_axis(m,current,(re4_u32)c->root,(re4_u32)c->joint,
            (re4_u32)c->effector,tgt,iterations,step,
            (re4_u8)((c->bend_axis_code<2U)?1U:((c->bend_axis_code<4U)?0U:2U)),&error);
        if(e!=RE4_OK)return e;
        ++applied;
    }
    if(out_applied)*out_applied=applied;
    if(out_skipped)*out_skipped=skipped;
    return RE4_OK;
}
