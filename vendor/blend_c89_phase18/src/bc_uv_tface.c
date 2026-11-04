#include "bc.h"
/* Blender 2.28a historical Mesh.tface -> TFace.uv[4][2] UV exporter.
   Four UV entries per MFace, even for triangles; the fourth is unused.
   This is independent of MLoopUV / CD_PROP_FLOAT2. */
int bc_uv_tface_find(BC_File *b,unsigned int mesh_at,BC_UV *uv)
{
  const BC_Struct *tface=bc_get_struct(b,"TFace");
  const BC_Field *field=bc_get_field(b,"TFace","uv");
  BC_U64 ptr;
  unsigned int count,stride;
  if(!tface||!tface->valid||!field ||
     bc_read_struct_word(b,mesh_at,"Mesh","totface",&count)!=BC_OK ||
     bc_read_struct_ptr(b,mesh_at,"Mesh","tface",&ptr)!=BC_OK)
    return BC_UNSUPPORTED;
  if(count==0U || count>1000000U)return BC_UNSUPPORTED;
  if(ptr.lo==0U && ptr.hi==0U)return BC_UNSUPPORTED;
  stride=(unsigned int)b->type_len[tface->type];
  if(stride<32U||field->offset>stride-32U)return BC_FAILURE;
  if(bc_view_pointer(b,ptr,1U,&uv->view)!=BC_OK)return BC_FAILURE;
  if(count>uv->view.size/stride || count>2147483647U/4U)return BC_FAILURE;
  uv->stride=stride;uv->coord=field->offset;uv->count=count*4U;
  uv->present=1;uv->historical_tface=1;
  return BC_OK;
}
