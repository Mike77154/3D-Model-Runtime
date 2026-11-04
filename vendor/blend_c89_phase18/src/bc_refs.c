#include "bc.h"
/* Resolve a serialized address without converting it to a host pointer.
   Supports interior addresses and old 64-bit addresses on 32-bit hosts.
   The address arithmetic is performed as two 32-bit limbs. */
int bc_resolve(BC_File *b, BC_U64 ptr, unsigned int needed,
               const BC_Block **block, unsigned int *at,
               unsigned int *available)
{
  unsigned int i, borrow, dhi, dlo, file_off;
  const BC_Block *p;
  if(ptr.lo==0U && ptr.hi==0U)return BC_FAILURE;
  for(i=0U;i<b->nblocks;i++) {
    p=&b->blocks[i];
    if(p->size==0U)continue;
    if(ptr.hi<p->old.hi || (ptr.hi==p->old.hi && ptr.lo<p->old.lo))continue;
    borrow=(ptr.lo<p->old.lo)?1U:0U;
    dhi=ptr.hi-p->old.hi-borrow;
    dlo=ptr.lo-p->old.lo;
    if(dhi!=0U || dlo>=p->size || needed>p->size-dlo)continue;
    if(p->offset>b->bytes || dlo>b->bytes-p->offset)continue;
    file_off=p->offset+dlo;
    if(block)*block=p;
    if(at)*at=file_off;
    if(available)*available=p->size-dlo;
    return BC_OK;
  }
  return BC_FAILURE;
}
/* A serialized C string, bounded both by an owning block and the output array.
   It is not valid unless the terminator lies within the permitted range. */
int bc_read_cstr(BC_File *b,BC_U64 ptr,char *out,unsigned int cap)
{
  unsigned int pos,avail,i,lim;
  if(cap<2U || bc_resolve(b,ptr,1U,NULL,&pos,&avail)<0)return BC_FAILURE;
  lim=avail<cap?avail:cap;
  if(bc_at(b,pos,lim,(unsigned char *)out)<0)return BC_FAILURE;
  for(i=0U;i<lim;i++)if(out[i]==0)return BC_OK;
  out[0]=0;
  return BC_FAILURE;
}

int bc_view_pointer(BC_File *b,BC_U64 ptr,unsigned int min_bytes,BC_View *v)
{
  if(bc_resolve(b,ptr,min_bytes,NULL,&v->offset,&v->size)<0)return BC_FAILURE;
  return BC_OK;
}
