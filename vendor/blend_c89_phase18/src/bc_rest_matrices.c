/* Raw rest matrices from serialized Bone.arm_mat, with true parent linkage.
   Matrix bytes in Blender DNA are stored as columns of four components;
   output transposes to our row-major Q16 matrix convention. This is REST
   data only: not a deformation matrix and not a fully evaluated clip. */
#include "bc.h"
#include <string.h>
#define BC_REST_MAX_BONES 8192U

typedef struct BC_RestToken { BC_U64 ptr; unsigned int parent; } BC_RestToken;
static BC_RestToken bc_rest_queue[BC_REST_MAX_BONES];
static int bc_rest_enqueue(BC_U64 ptr,unsigned int parent,unsigned int *n)
{
  unsigned int j;
  if(ptr.lo==0U && ptr.hi==0U)return BC_OK;
  if(*n>=BC_REST_MAX_BONES)return BC_FAILURE;
  for(j=0U;j<*n;j++)if(bc_same64(ptr,bc_rest_queue[j].ptr))return BC_FAILURE;
  bc_rest_queue[*n].ptr=ptr;
  bc_rest_queue[*n].parent=parent;
  ++(*n);return BC_OK;
}
static int bc_rest_first(BC_File *b,unsigned int at,const char *type,
                         const char *name,BC_U64 *p)
{
  const BC_Field *f=bc_get_field(b,type,name);
  const BC_Field *first=bc_get_field(b,"ListBase","first");
  if(!f||!first||at>2147483647U-f->offset ||
     at+f->offset>2147483647U-first->offset)return BC_UNSUPPORTED;
  return bc_read_pointer(b,at+f->offset+first->offset,p);
}
static int bc_rest_name(BC_File *b,unsigned int at,char *name)
{
  const BC_Field *f=bc_get_field(b,"Bone","name");
  const BC_Struct *s=bc_get_struct(b,"Bone");
  unsigned int n,k;
  if(!f||!s||!s->valid||f->offset>=b->type_len[s->type])return BC_FAILURE;
  n=b->type_len[s->type]-f->offset;
  if(n>95U)n=95U;
  if(bc_at(b,at+f->offset,n,(unsigned char *)name)!=BC_OK)return BC_FAILURE;
  name[n]=0;
  for(k=0U;k<n && name[k];k++)if(name[k]=='\r'||name[k]=='\n'||name[k]=='\t')name[k]='_';
  return BC_OK;
}
static int bc_rest_arm(BC_File *b,FILE *out,unsigned int at,unsigned int id)
{
  const BC_Struct *s=bc_get_struct(b,"Bone");
  const BC_Field *mat=bc_get_field(b,"Bone","arm_mat");
  unsigned int n=0U,i,j,ofs,c,r,base,avail,bits;
  int q;
  BC_U64 p,next,child;
  char name[96];
  if(!s||!s->valid || !mat || mat->offset>b->type_len[s->type] ||
     b->type_len[s->type]-mat->offset<64U)return BC_UNSUPPORTED;
  if(bc_rest_first(b,at,"bArmature","bonebase",&p)!=BC_OK)return BC_UNSUPPORTED;
  if(bc_rest_enqueue(p,0U,&n)!=BC_OK)return BC_FAILURE;
  for(i=0U;i<n;i++){
    if(bc_resolve(b,bc_rest_queue[i].ptr,b->type_len[s->type],NULL,&base,&avail)!=BC_OK ||
       bc_rest_name(b,base,name)!=BC_OK)return BC_FAILURE;
    if(fprintf(out,"%u\t%u\t",id,i)<0)return BC_FAILURE;
    if(bc_rest_queue[i].parent==0U){if(fputs("-1",out)==EOF)return BC_FAILURE;}
    else if(fprintf(out,"%u",bc_rest_queue[i].parent-1U)<0)return BC_FAILURE;
    if(fprintf(out,"\t%s",name)<0)return BC_FAILURE;
    ofs=base+mat->offset;
    for(r=0U;r<4U;r++)for(c=0U;c<4U;c++){
      /* DNA float arm_mat[col][row] -> row-major export. */
      j=c*4U+r;
      if(bc_read_word(b,ofs+j*4U,&bits)!=BC_OK ||
         bc_f32_q16(bits,&q)!=BC_OK)return BC_FAILURE;
      if(fputc('\t',out)==EOF)return BC_FAILURE;
      bc_print_q16(out,q);
    }
    if(fputc('\n',out)==EOF)return BC_FAILURE;
    if(bc_read_struct_ptr(b,base,"Bone","next",&next)!=BC_OK ||
       bc_rest_first(b,base,"Bone","childbase",&child)!=BC_OK)return BC_FAILURE;
    if(bc_rest_enqueue(next,bc_rest_queue[i].parent,&n)!=BC_OK ||
       bc_rest_enqueue(child,i+1U,&n)!=BC_OK)return BC_FAILURE;
  }
  return BC_OK;
}
int bc_export_rest_matrices(BC_File *b,FILE *out)
{
  const BC_Struct *arm=bc_get_struct(b,"bArmature");
  const BC_Block *block;
  unsigned int i,j,stride,count=0U;
  if(!arm||!arm->valid)return BC_UNSUPPORTED;
  stride=b->type_len[arm->type];
  if(stride==0U)return BC_UNSUPPORTED;
  if(fputs("# armature\tbone\tparent\tname\tm00..m33 (row-major Q16.16 decimal)\n",out)==EOF)
    return BC_FAILURE;
  for(i=0U;i<b->nblocks;i++){
    block=&b->blocks[i];
    if(block->code[0]!='A'||block->code[1]!='R'||
       block->sdna>=b->nstructs || b->structs[block->sdna].type!=arm->type)continue;
    if(block->count>block->size/stride)return BC_FAILURE;
    for(j=0U;j<block->count;j++){
      if(bc_rest_arm(b,out,block->offset+j*stride,count)!=BC_OK)return BC_FAILURE;
      count++;
    }
  }
  return count && !ferror(out)?BC_OK:BC_UNSUPPORTED;
}
