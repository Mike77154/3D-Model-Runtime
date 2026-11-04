/* Shared bounds-checked accessors for serialized Blender animation metadata.
   No allocating or host-pointer reinterpretation. */
#include "bc.h"
#include <string.h>

int bc_anim_array_entry(BC_File *b, BC_U64 array_ptr,
                        unsigned int count, unsigned int index, BC_U64 *result)
{
  unsigned int n, at, available;
  if(!count || count>BC_ANIM_MAX_ITEMS || index>=count ||
     count>2147483647U/(unsigned int)b->ptr_size)return BC_FAILURE;
  n=count*(unsigned int)b->ptr_size;
  if(bc_resolve(b,array_ptr,n,NULL,&at,&available)!=BC_OK)return BC_FAILURE;
  return bc_read_pointer(b,at+index*(unsigned int)b->ptr_size,result);
}
int bc_anim_field_byte(BC_File *b, unsigned int at,
                       const char *structure, const char *field,
                       unsigned int *value)
{
  const BC_Field *f=bc_get_field(b,structure,field);
  unsigned char v;
  if(!f || f->offset>2147483647U-at)return BC_UNSUPPORTED;
  if(bc_at(b,at+f->offset,1U,&v)!=BC_OK)return BC_FAILURE;
  *value=(unsigned int)v;
  return BC_OK;
}
