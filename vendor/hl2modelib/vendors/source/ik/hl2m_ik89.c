/* Simple Source-compatible contact bridge, C89 fixed 16.16.
   For actual foot IK we expose a root correction rather than corrupting knee
   rotations with an unsupported approximation. QC metadata pass is retained. */
#include "hl2m_master89.h"
#include <limits.h>
static int ik89_sub(hl2m_fixed a,hl2m_fixed b,hl2m_fixed *out)
{if(b>0 && a<INT_MIN+b)return HL2M_ERR_RANGE;
 if(b<0 && a>INT_MAX+b)return HL2M_ERR_RANGE;
 *out=a-b;return HL2M_OK;}
static int ik89_add(hl2m_fixed a,hl2m_fixed b,hl2m_fixed *out)
{if(b>0 && a>INT_MAX-b)return HL2M_ERR_RANGE;
 if(b<0 && a<INT_MIN-b)return HL2M_ERR_RANGE;
 *out=a+b;return HL2M_OK;}
static hl2m_fixed ik89_clamp(hl2m_fixed v,hl2m_fixed limit)
{if(v>limit)return limit;if(v< -limit)return -limit;return v;}
int hl2m_ik_root_plant89(const hl2m_pose_matrices *pose,int tip,
    hl2m_vec3 contact_world,hl2m_fixed max_shift,hl2m_vec3 *out)
{
    const hl2m_mat3x4 *m;hl2m_vec3 v;
    if(!pose || !out)return HL2M_ERR_NULL;
    if(tip<0 || tip>=pose->bone_count || max_shift<0 || pose->bone_count>HL2M_MAX_POSE_MATRICES)return HL2M_ERR_RANGE;
    m=&pose->world[tip];
    if(ik89_sub(contact_world.x,m->m[0][3],&v.x) ||
       ik89_sub(contact_world.y,m->m[1][3],&v.y) ||
       ik89_sub(contact_world.z,m->m[2][3],&v.z))return HL2M_ERR_RANGE;
    out->x=ik89_clamp(v.x,max_shift);out->y=ik89_clamp(v.y,max_shift);out->z=ik89_clamp(v.z,max_shift);
    return HL2M_OK;
}
int hl2m_ik_apply_root89(const hl2m_pose_matrices *pose,
    hl2m_vec3 correction,hl2m_pose_matrices *out)
{
    int i;hl2m_pose_matrices p;
    if(!pose || !out)return HL2M_ERR_NULL;
    if(pose->bone_count<0 || pose->bone_count>HL2M_MAX_POSE_MATRICES)return HL2M_ERR_RANGE;
    p=*pose;
    for(i=0;i<p.bone_count;i++){
        if(ik89_add(p.world[i].m[0][3],correction.x,&p.world[i].m[0][3]) ||
           ik89_add(p.world[i].m[1][3],correction.y,&p.world[i].m[1][3]) ||
           ik89_add(p.world[i].m[2][3],correction.z,&p.world[i].m[2][3]))return HL2M_ERR_RANGE;
        if(p.parent[i]==-1){
            if(ik89_add(p.local[i].m[0][3],correction.x,&p.local[i].m[0][3]) ||
               ik89_add(p.local[i].m[1][3],correction.y,&p.local[i].m[1][3]) ||
               ik89_add(p.local[i].m[2][3],correction.z,&p.local[i].m[2][3]))return HL2M_ERR_RANGE;
        }
    }
    *out=p;return HL2M_OK;
}
int hl2m_ik_qc89(const hl2m_qc *qc,const hl2m_ik_pose *pose,
    int frame,hl2m_ik_pose *out,hl2m_ik_report *rep)
{return hl2m_solve_ik_rules(qc,pose,frame,out,rep);}
