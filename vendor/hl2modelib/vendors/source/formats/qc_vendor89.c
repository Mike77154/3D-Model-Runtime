/* Vendored QC format dispatcher: C89 linkable unit.
   Parser implementation in qc_codec89.inc is compiled inside legacy core
   until its private helper ABI is lifted into shared internal headers. */
#include "hl2modelib.h"
int hl2m_vendor_qc89(const hl2m_u8 *data,unsigned int size,hl2m_qc *out)
{
    return hl2m_decode_qc(data,size,out);
}
