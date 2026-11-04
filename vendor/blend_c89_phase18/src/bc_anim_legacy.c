/* Action.curves linked FCurve list - Blender 2.5 through legacy 4.x.
   Does not interpret pre-2.5 IPO curves. */
#include "bc.h"
static BC_U64 bc_anim_seen[BC_ANIM_MAX_ITEMS];
int bc_anim_legacy(BC_File *b,FILE *out,unsigned int action_at,
                   unsigned int action_id,unsigned int *written)
{
  const BC_Field *curves=bc_get_field(b,"bAction","curves");
  const BC_Field *first=bc_get_field(b,"ListBase","first");
  const BC_Struct *curve=bc_get_struct(b,"FCurve");
  BC_U64 ptr,next;
  unsigned int count=0U,at,available,j;
  int r;
  if(!curves || !first || !curve || !curve->valid)return BC_UNSUPPORTED;
  if(action_at>2147483647U-curves->offset ||
     action_at+curves->offset>2147483647U-first->offset)return BC_FAILURE;
  if(bc_read_pointer(b,action_at+curves->offset+first->offset,&ptr)!=BC_OK)return BC_FAILURE;
  while(ptr.lo!=0U || ptr.hi!=0U){
    if(count>=BC_ANIM_MAX_ITEMS)return BC_FAILURE;
    for(j=0U;j<count;j++)if(bc_same64(ptr,bc_anim_seen[j]))return BC_FAILURE;
    bc_anim_seen[count]=ptr;
    if(bc_resolve(b,ptr,b->type_len[curve->type],NULL,&at,&available)!=BC_OK)return BC_FAILURE;
    r=bc_anim_curve(b,out,ptr,action_id,0U,0U,0U,count);
    if(r!=BC_OK)return r;
    if(bc_read_struct_ptr(b,at,"FCurve","next",&next)!=BC_OK)return BC_FAILURE;
    ptr=next;
    ++count;
  }
  *written+=count;
  return BC_OK;
}
