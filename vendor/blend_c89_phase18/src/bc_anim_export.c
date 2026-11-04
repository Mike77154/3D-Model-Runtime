/* Enumerate actual AC ID blocks; SDNA field presence picks legacy/layered. */
#include "bc.h"
#include <string.h>
int bc_export_keys(BC_File *b, FILE *out)
{
  const BC_Struct *action=bc_get_struct(b,"bAction");
  const BC_Block *blk;
  unsigned int i,j,stride,action_id=0U,written=0U,offset,layers;
  int r;
  if(!action || !action->valid)return BC_UNSUPPORTED;
  stride=b->type_len[action->type];
  if(stride==0U)return BC_UNSUPPORTED;
  if(fputs(bc_eval_active()?"# action\tlayer\tstrip\tslot\tcurve\trna_path\tcomponent\tframe\tvalue\n":
           "# action\tlayer\tstrip\tslot\tcurve\trna_path\tcomponent\tkey\tsource\tframe\tvalue\tleft_x\tleft_y\tright_x\tright_y\tinterpolation\n",out)==EOF)
     return BC_FAILURE;
  for(i=0U;i<b->nblocks;i++){
    blk=&b->blocks[i];
    if(blk->sdna>=b->nstructs || b->structs[blk->sdna].type!=action->type ||
       blk->code[0]!='A' || blk->code[1]!='C')continue;
    if(blk->count>blk->size/stride)return BC_FAILURE;
    for(j=0U;j<blk->count;j++){
      offset=blk->offset+j*stride;
      /* Modern action with at least one layer: use layered route.
         Empty modern Actions with legacy curves still use legacy. */
      r=bc_read_struct_word(b,offset,"bAction","layer_array_num",&layers);
      if(r==BC_OK && layers>0U)r=bc_anim_layered(b,out,offset,action_id,&written);
      else r=bc_anim_legacy(b,out,offset,action_id,&written);
      if(r!=BC_OK)return r;
      action_id++;
    }
  }
  if(ferror(out))return BC_FAILURE;
  return (action_id>0U && written>0U)?BC_OK:BC_UNSUPPORTED;
}

int bc_export_sample(BC_File *b,FILE *out,int frame)
{
  int r;
  bc_eval_start(frame);
  r=bc_export_keys(b,out);
  bc_eval_stop();
  return r;
}
