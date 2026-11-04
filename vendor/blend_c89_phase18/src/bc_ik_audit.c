/* Phase15 serial IK constraint auditor. Reads SDNA only, does not evaluate
   Blender's IK tree, pole-angle, joint limits, stretching, or solver options. */
#include "bc.h"
#include <limits.h>
#include <string.h>
#define BC_AUDIT_IK 3U
static int at_field(BC_File *b,unsigned int base,const char *type,
                    const char *field,unsigned int len,unsigned int *at)
{
  const BC_Field *f=bc_get_field(b,type,field);
  const BC_Struct *s=bc_get_struct(b,type);
  unsigned int n;
  if(!f||!s||!s->valid)return BC_UNSUPPORTED;
  n=b->type_len[s->type];
  if(f->offset>n||len>n-f->offset||base>UINT_MAX-f->offset)return BC_FAILURE;
  *at=base+f->offset;return BC_OK;
}
static int read_short(BC_File *b,unsigned int base,const char *type,
                     const char *field,unsigned int *out)
{
  unsigned int at;unsigned char data[2];
  if(at_field(b,base,type,field,2U,&at)!=BC_OK||
     bc_at(b,at,2U,data)!=BC_OK)return BC_FAILURE;
  *out=bc_u16(data,b->endian_le);return BC_OK;
}
static int read_name(BC_File *b,unsigned int base,const char *type,
                     const char *field,char out[65])
{
  unsigned int at,i;
  if(at_field(b,base,type,field,64U,&at)!=BC_OK||
     bc_at(b,at,64U,(unsigned char *)out)!=BC_OK)return BC_FAILURE;
  out[64]=0;
  for(i=0U;i<64U;i++){
    if(out[i]=='\0')return BC_OK;
    if(out[i]=='\n'||out[i]=='\r'||out[i]=='\t')return BC_FAILURE;
  }
  return BC_FAILURE;
}
static int null_ptr(BC_U64 p){return !p.lo&&!p.hi;}
int bc_export_ik_audit(BC_File *b,FILE *out)
{
  const BC_Struct *cs=bc_get_struct(b,"bConstraint");
  const BC_Struct *ks=bc_get_struct(b,"bKinematicConstraint");
  const BC_Block *blk,*owner;
  unsigned int i,j,stride,at,kind,flags,chain,ikflags,subtype,k_at,avail,rows=0U;
  BC_U64 ptr,target,pole;
  char name[65],subtarget[65],polesubtarget[65];
  if(!cs ||!cs->valid ||!ks ||!ks->valid)return BC_UNSUPPORTED;
  stride=b->type_len[cs->type];if(stride==0U)return BC_UNSUPPORTED;
  if(fputs("# ordinal name constraint_flags chain_length ik_flags ik_type target_hi target_lo subtarget pole_hi pole_lo pole_subtarget state\n",out)==EOF)return BC_FAILURE;
  for(i=0U;i<b->nblocks;i++){
    blk=&b->blocks[i];
    if(blk->sdna>=b->nstructs ||b->structs[blk->sdna].type!=cs->type)continue;
    if(blk->count>blk->size/stride)return BC_FAILURE;
    for(j=0U;j<blk->count;j++){
      if(j>(UINT_MAX-blk->offset)/stride)return BC_FAILURE;
      at=blk->offset+j*stride;
      if(read_short(b,at,"bConstraint","type",&kind)!=BC_OK)return BC_FAILURE;
      if(kind!=BC_AUDIT_IK)continue;
      if(read_short(b,at,"bConstraint","flag",&flags)!=BC_OK ||
         read_name(b,at,"bConstraint","name",name)!=BC_OK ||
         bc_read_struct_ptr(b,at,"bConstraint","data",&ptr)!=BC_OK ||
         null_ptr(ptr) ||
         bc_resolve(b,ptr,b->type_len[ks->type],&owner,&k_at,&avail)!=BC_OK ||
         owner->sdna>=b->nstructs || b->structs[owner->sdna].type!=ks->type ||
         read_short(b,k_at,"bKinematicConstraint","rootbone",&chain)!=BC_OK ||
         read_short(b,k_at,"bKinematicConstraint","flag",&ikflags)!=BC_OK ||
         read_short(b,k_at,"bKinematicConstraint","type",&subtype)!=BC_OK ||
         bc_read_struct_ptr(b,k_at,"bKinematicConstraint","tar",&target)!=BC_OK ||
         bc_read_struct_ptr(b,k_at,"bKinematicConstraint","poletar",&pole)!=BC_OK ||
         read_name(b,k_at,"bKinematicConstraint","subtarget",subtarget)!=BC_OK ||
         read_name(b,k_at,"bKinematicConstraint","polesubtarget",polesubtarget)!=BC_OK)
        return BC_FAILURE;
      if(fprintf(out,"%u\t%s\t%u\t%u\t%u\t%u\t%08x\t%08x\t%s\t%08x\t%08x\t%s\t%s\n",
                 rows,name,flags,chain,ikflags,subtype,
                 target.hi,target.lo,subtarget,pole.hi,pole.lo,polesubtarget,
                 flags&(4U|512U)?"disabled":"active")<0)return BC_FAILURE;
      ++rows;
    }
  }
  return ferror(out)?BC_FAILURE:BC_OK;
}
