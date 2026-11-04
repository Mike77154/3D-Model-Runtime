/* Vendored ANI format dispatcher: C89 linkable unit.
   Parser implementation in ani_codec89.inc is compiled inside legacy core
   until its private helper ABI is lifted into shared internal headers. */
#include "hl2modelib.h"
int hl2m_vendor_ani89(const hl2m_u8 *data,unsigned int size, const hl2m_mdl *mdl,hl2m_ani *out)
{
    return hl2m_decode_ani(data,size,mdl,out);
}
