/* Vendored VTA format dispatcher: C89 linkable unit.
   Parser implementation in vta_codec89.inc is compiled inside legacy core
   until its private helper ABI is lifted into shared internal headers. */
#include "hl2modelib.h"
int hl2m_vendor_vta89(const hl2m_u8 *data,unsigned int size,hl2m_vta *out)
{
    return hl2m_decode_vta(data,size,out);
}
