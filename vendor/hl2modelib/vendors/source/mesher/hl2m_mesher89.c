/* Separate linkable mesher adapter. Implementation (strip groups + VVD remap)
   is maintained in hl2m_mesher_*.inc compiled into legacy core because it
   uses private C89 parsers. The master links only to this vendor frontend. */
#include "hl2modelib.h"
int hl2m_vendor_mesh89(const hl2m_mdl *mdl,const hl2m_vvd *vvd,
                       const hl2m_vtx *vtx,int lod,hl2m_engine_model *out)
{
    if(!mdl || !vvd || !vtx || !out)return HL2M_ERR_NULL;
    return hl2m_build_engine_model(mdl,vvd,vtx,lod,out);
}
