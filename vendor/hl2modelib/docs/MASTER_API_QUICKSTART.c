/* C89 example excerpt for applications.  `mdl`, `vvd`, `vtx`, `mesh`, and
   `animated` should be STATIC structs, not allocated on a tiny stack.
   This excerpt intentionally omits application-specific file I/O. */
#include "hl2m_master89.h"
static hl2m_mdl mdl;
static hl2m_vvd vvd;
static hl2m_vtx vtx;
static hl2m_engine_model mesh;
static hl2m_engine_model animated;
static hl2m_pose_matrices inverse_bind, pose;
static hl2m_skin_report report;
static hl2m_vec3 world_pos;
static hl2m_fixed world_yaw;
int example_frame(const hl2m_u8 *mdl_bytes,unsigned int mdlsize,
                  const hl2m_u8 *vvd_bytes,unsigned int vvdsize,
                  const hl2m_u8 *vtx_bytes,unsigned int vtxsize,
                  int animation_index,int frame)
{
    int r;
    r=hl2m_master_decode89(HL2M_FORMAT_MDL,mdl_bytes,mdlsize,0,0,&mdl);if(r)return r;
    r=hl2m_master_decode89(HL2M_FORMAT_VVD,vvd_bytes,vvdsize,0,0,&vvd);if(r)return r;
    r=hl2m_master_decode89(HL2M_FORMAT_VTX,vtx_bytes,vtxsize,0,0,&vtx);if(r)return r;
    r=hl2m_master_mesh89(&mdl,&vvd,&vtx,0,&mesh);if(r)return r;
    r=hl2m_native_inverse_bind89(mdl_bytes,mdlsize,&mdl,&inverse_bind);if(r)return r;
    r=hl2m_master_anim89(mdl_bytes,mdlsize,&mdl,animation_index,frame,&pose,0);if(r)return r;
    r=hl2m_master_skin89(&mesh,&inverse_bind,&pose,&animated,&report);if(r)return r;
    r=hl2m_master_motion89(mdl_bytes,mdlsize,&mdl,animation_index,frame,&world_pos,&world_yaw);
    if(r==HL2M_ERR_UNSUPPORTED){world_pos.x=world_pos.y=world_pos.z=0;world_yaw=0;return HL2M_OK;}
    return r;
}
