/* Compile this excerpt after including hl2m_master89.h. Store large model
   types in static memory; no heap required. The calling engine owns bytes. */
#include "hl2m_master89.h"
static hl2m_smd smd;
static hl2m_vta vta;
static hl2m_qc qc;
static hl2m_ani ani;
int codec_formats( const hl2m_u8 *smd_bytes,unsigned int smd_n,
                   const hl2m_u8 *vta_bytes,unsigned int vta_n,
                   const hl2m_u8 *qc_bytes,unsigned int qc_n,
                   const hl2m_u8 *ani_bytes,unsigned int ani_n,
                   const hl2m_mdl *mdl)
{
    if(hl2m_master_decode89(HL2M_FORMAT_SMD,smd_bytes,smd_n,0,0,&smd))return 1;
    if(hl2m_master_decode89(HL2M_FORMAT_VTA,vta_bytes,vta_n,0,0,&vta))return 2;
    if(hl2m_master_decode89(HL2M_FORMAT_QC,qc_bytes,qc_n,0,0,&qc))return 3;
    if(hl2m_master_decode89(HL2M_FORMAT_ANI,ani_bytes,ani_n,mdl,0,&ani))return 4;
    return 0;
}
