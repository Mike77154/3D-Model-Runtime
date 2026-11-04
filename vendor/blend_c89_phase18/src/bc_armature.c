/* Hierarchy of serialized Bone linked lists. Static bounded BFS. */
#include "bc.h"
#include <string.h>
#define BC_ARM_MAX_BONES 8192U
#define BC_ARM_NAME 96U

typedef struct BC_BoneQueue { BC_U64 token; unsigned int parent; } BC_BoneQueue;
static BC_BoneQueue bc_bone_queue[BC_ARM_MAX_BONES];

static int bc_arm_first(BC_File *b,unsigned int at,const char *type,
                        const char *field,BC_U64 *ptr)
{
  const BC_Field *f=bc_get_field(b,type,field);
  const BC_Field *first=bc_get_field(b,"ListBase","first");
  if(!f || !first || at>2147483647U-f->offset ||
     at+f->offset>2147483647U-first->offset)return BC_UNSUPPORTED;
  return bc_read_pointer(b,at+f->offset+first->offset,ptr);
}
static int bc_arm_queue(BC_U64 ptr,unsigned int parent,unsigned int *total)
{
  unsigned int i;
  if(ptr.lo==0U && ptr.hi==0U)return BC_OK;
  for(i=0U;i<*total;i++)if(bc_same64(ptr,bc_bone_queue[i].token))return BC_FAILURE;
  if(*total>=BC_ARM_MAX_BONES)return BC_FAILURE;
  bc_bone_queue[*total].token=ptr;
  bc_bone_queue[*total].parent=parent;
  ++(*total);
  return BC_OK;
}
static int bc_arm_name(BC_File *b,unsigned int at,char *dst)
{
  const BC_Field *f=bc_get_field(b,"Bone","name");
  const BC_Struct *s=bc_get_struct(b,"Bone");
  unsigned int n,k,sz;
  if(!f||!s||!s->valid)return BC_FAILURE;
  sz=b->type_len[s->type];
  if(f->offset>=sz)return BC_FAILURE;
  n=sz-f->offset;if(n>=BC_ARM_NAME)n=BC_ARM_NAME-1U;
  if(bc_at(b,at+f->offset,n,(unsigned char *)dst)<0)return BC_FAILURE;
  dst[n]=0;
  for(k=0U;k<n && dst[k];k++){
    if(dst[k]=='\r' || dst[k]=='\n' || dst[k]=='\t')dst[k]='_';
  }
  dst[k]=0;
  return BC_OK;
}
static int bc_arm_vec(BC_File *b,unsigned int at,const char *field,int *out)
{
  const BC_Field *f=bc_get_field(b,"Bone",field);
  unsigned int bits,k;
  if(!f || at>2147483647U-f->offset || f->offset>2147483647U-12U)return BC_UNSUPPORTED;
  for(k=0U;k<3U;k++){
    if(bc_read_word(b,at+f->offset+k*4U,&bits)<0 ||
       bc_f32_q16(bits,&out[k])<0)return BC_FAILURE;
  }
  return BC_OK;
}
static int bc_arm_one(BC_File *b,FILE *out,unsigned int at,unsigned int arm_index)
{
  const BC_Struct *bone=bc_get_struct(b,"Bone");
  BC_U64 ptr,child,next;
  unsigned int count=0U,i,found,available;
  int a[3],z[3],k,r;
  char name[BC_ARM_NAME];
  if(!bone || !bone->valid)return BC_UNSUPPORTED;
  if(bc_arm_first(b,at,"bArmature","bonebase",&ptr)<0)return BC_UNSUPPORTED;
  if(bc_arm_queue(ptr,0U,&count)<0)return BC_FAILURE;
  for(i=0U;i<count;i++){
    if(bc_resolve(b,bc_bone_queue[i].token,b->type_len[bone->type],NULL,&found,&available)<0)
      return BC_FAILURE;
    if(bc_arm_name(b,found,name)<0)return BC_FAILURE;
    r=bc_arm_vec(b,found,"arm_head",a);
    if(r==BC_UNSUPPORTED)r=bc_arm_vec(b,found,"head",a);
    if(r!=BC_OK)return r;
    r=bc_arm_vec(b,found,"arm_tail",z);
    if(r==BC_UNSUPPORTED)r=bc_arm_vec(b,found,"tail",z);
    if(r!=BC_OK)return r;
    if(fprintf(out,"%u\t%u\t",arm_index,i)<0)return BC_FAILURE;
    if(bc_bone_queue[i].parent==0U){if(fputs("-1",out)==EOF)return BC_FAILURE;}
    else if(fprintf(out,"%u",bc_bone_queue[i].parent-1U)<0)return BC_FAILURE;
    if(fprintf(out,"\t%s",name)<0)return BC_FAILURE;
    for(k=0;k<3;k++) {if(fputc('\t',out)==EOF)return BC_FAILURE;bc_print_q16(out,a[k]);}
    for(k=0;k<3;k++) {if(fputc('\t',out)==EOF)return BC_FAILURE;bc_print_q16(out,z[k]);}
    if(fputc('\n',out)==EOF)return BC_FAILURE;
    if(bc_read_struct_ptr(b,found,"Bone","next",&next)<0 ||
       bc_arm_first(b,found,"Bone","childbase",&child)<0)return BC_FAILURE;
    /* A sibling has the same parent; children have the current bone as parent. */
    if(bc_arm_queue(next,bc_bone_queue[i].parent,&count)<0 ||
       bc_arm_queue(child,i+1U,&count)<0)return BC_FAILURE;
  }
  return BC_OK;
}
int bc_export_bones(BC_File *b,FILE *out)
{
  const BC_Struct *st=bc_get_struct(b,"bArmature");
  const BC_Block *blk;
  unsigned int stride,i,j,at,num=0U;
  int r;
  if(!st || !st->valid)return BC_UNSUPPORTED;
  stride=b->type_len[st->type];if(!stride)return BC_UNSUPPORTED;
  if(fputs("# armature_index\tbone_index\tparent_index\tname\thead_x\thead_y\thead_z\ttail_x\ttail_y\ttail_z\n",out)==EOF)
    return BC_FAILURE;
  for(i=0U;i<b->nblocks;i++) {
    blk=&b->blocks[i];
    if(blk->sdna>=b->nstructs || b->structs[blk->sdna].type!=st->type ||
       blk->code[0]!='A' || blk->code[1]!='R')continue;
    if(blk->count>blk->size/stride)return BC_FAILURE;
    for(j=0U;j<blk->count;j++) {
      at=blk->offset+j*stride;
      r=bc_arm_one(b,out,at,num);
      if(r!=BC_OK)return r;
      num++;
    }
  }
  return num?BC_OK:BC_UNSUPPORTED;
}
