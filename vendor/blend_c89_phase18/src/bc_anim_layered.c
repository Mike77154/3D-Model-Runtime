/* 4.4+ layered Action / Layer / Strip / KeyframeData / Channelbag traversal.
   Every reference is checked against the saved block, not dereferenced as C ptr. */
#include "bc.h"
#define BC_ANIM_LIMIT_LAYERS 256U
#define BC_ANIM_LIMIT_STRIPS 4096U
#define BC_ANIM_LIMIT_BAGS 4096U

static int bc_layer_field(BC_File *b,unsigned int at,const char *type,
                          const char *ptr_field,const char *num_field,
                          BC_U64 *ptr,unsigned int *num,unsigned int limit)
{
  if(bc_read_struct_ptr(b,at,type,ptr_field,ptr)!=BC_OK ||
     bc_read_struct_word(b,at,type,num_field,num)!=BC_OK || *num>limit)return BC_FAILURE;
  if(*num && ptr->lo==0U && ptr->hi==0U)return BC_FAILURE;
  return BC_OK;
}
static int bc_layer_data(BC_File *b,FILE *out,
                         BC_U64 data_array,unsigned int ndata,unsigned int di,
                         unsigned int action_id,unsigned int layer_id,
                         unsigned int strip_id,unsigned int *written)
{
  const BC_Struct *data=bc_get_struct(b,"ActionStripKeyframeData");
  const BC_Struct *bag=bc_get_struct(b,"ActionChannelbag");
  BC_U64 data_ptr,bag_array,bag_ptr,fcurves,fc;
  unsigned int bag_num,bag_idx,fc_num,fc_idx,slot,at,avail;
  int r;
  if(!data || !data->valid || !bag || !bag->valid || di>=ndata)return BC_UNSUPPORTED;
  if(bc_anim_array_entry(b,data_array,ndata,di,&data_ptr)!=BC_OK ||
     bc_resolve(b,data_ptr,b->type_len[data->type],NULL,&at,&avail)!=BC_OK)return BC_FAILURE;
  if(bc_layer_field(b,at,"ActionStripKeyframeData","channelbag_array",
                    "channelbag_array_num",&bag_array,&bag_num,BC_ANIM_LIMIT_BAGS)!=BC_OK)return BC_FAILURE;
  for(bag_idx=0U;bag_idx<bag_num;bag_idx++){
    if(bc_anim_array_entry(b,bag_array,bag_num,bag_idx,&bag_ptr)!=BC_OK ||
       bc_resolve(b,bag_ptr,b->type_len[bag->type],NULL,&at,&avail)!=BC_OK)return BC_FAILURE;
    if(bc_read_struct_word(b,at,"ActionChannelbag","slot_handle",&slot)!=BC_OK ||
       bc_layer_field(b,at,"ActionChannelbag","fcurve_array",
                      "fcurve_array_num",&fcurves,&fc_num,BC_ANIM_MAX_ITEMS)!=BC_OK)return BC_FAILURE;
    for(fc_idx=0U;fc_idx<fc_num;fc_idx++){
      if(bc_anim_array_entry(b,fcurves,fc_num,fc_idx,&fc)!=BC_OK)return BC_FAILURE;
      r=bc_anim_curve(b,out,fc,action_id,layer_id,strip_id,slot,fc_idx);
      if(r!=BC_OK)return r;
      ++(*written);
    }
  }
  return BC_OK;
}
int bc_anim_layered(BC_File *b,FILE *out,unsigned int action_at,
                    unsigned int action_id,unsigned int *written)
{
  const BC_Struct *lay=bc_get_struct(b,"ActionLayer"),*strip=bc_get_struct(b,"ActionStrip");
  BC_U64 layers,data_array,layer_ptr,strips,strip_ptr;
  unsigned int num_layers,num_data,nstrips,i,j,at,avail,typ,idx;
  int r;
  if(!lay || !lay->valid || !strip || !strip->valid)return BC_UNSUPPORTED;
  if(bc_layer_field(b,action_at,"bAction","layer_array","layer_array_num",
                    &layers,&num_layers,BC_ANIM_LIMIT_LAYERS)!=BC_OK ||
     bc_layer_field(b,action_at,"bAction","strip_keyframe_data_array",
                    "strip_keyframe_data_array_num",&data_array,&num_data,BC_ANIM_LIMIT_STRIPS)!=BC_OK)return BC_FAILURE;
  for(i=0U;i<num_layers;i++){
    if(bc_anim_array_entry(b,layers,num_layers,i,&layer_ptr)!=BC_OK ||
       bc_resolve(b,layer_ptr,b->type_len[lay->type],NULL,&at,&avail)!=BC_OK)return BC_FAILURE;
    if(bc_layer_field(b,at,"ActionLayer","strip_array","strip_array_num",
                      &strips,&nstrips,BC_ANIM_LIMIT_STRIPS)!=BC_OK)return BC_FAILURE;
    for(j=0U;j<nstrips;j++){
      if(bc_anim_array_entry(b,strips,nstrips,j,&strip_ptr)!=BC_OK ||
         bc_resolve(b,strip_ptr,b->type_len[strip->type],NULL,&at,&avail)!=BC_OK)return BC_FAILURE;
      r=bc_anim_field_byte(b,at,"ActionStrip","strip_type",&typ);
      if(r!=BC_OK)return r;
      /* Keyframe strip type=0; other strip types are not decoded. */
      if(typ!=0U)continue;
      if(bc_read_struct_word(b,at,"ActionStrip","data_index",&idx)!=BC_OK)return BC_FAILURE;
      if(idx>=num_data)return BC_FAILURE;
      r=bc_layer_data(b,out,data_array,num_data,idx,action_id,i,j,written);
      if(r!=BC_OK)return r;
    }
  }
  return BC_OK;
}
