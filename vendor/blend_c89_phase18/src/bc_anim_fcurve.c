/* Decode FCurve samples from SDNA, exporting raw keyframe values as Q16.16.
   Bezier control handles are PRESERVED but interpolation is NOT evaluated. */
#include "bc.h"
#include <string.h>

#define BC_KEY_MAX_POINTS 500000U
#define BC_ANIM_PATH 512U

static int bc_q32(BC_File *b,unsigned int at,int *out)
{
  unsigned int bits;
  if(bc_read_word(b,at,&bits)!=BC_OK)return BC_FAILURE;
  return bc_f32_q16(bits,out);
}
static int bc_field_addr(BC_File *b,unsigned int base,
                         const char *structname,const char *field,
                         unsigned int *at)
{
  const BC_Field *f=bc_get_field(b,structname,field);
  if(!f || f->offset>2147483647U-base)return BC_UNSUPPORTED;
  *at=base+f->offset;
  return BC_OK;
}
static int bc_fcurve_path(BC_File *b,unsigned int at,char *out)
{
  BC_U64 ptr;
  unsigned int i;
  int r=bc_read_struct_ptr(b,at,"FCurve","rna_path_ptr",&ptr);
  if(r!=BC_OK)r=bc_read_struct_ptr(b,at,"FCurve","rna_path",&ptr);
  if(r!=BC_OK)return BC_UNSUPPORTED;
  if(ptr.hi==0U && ptr.lo==0U){out[0]=0;return BC_OK;}
  if(bc_read_cstr(b,ptr,out,BC_ANIM_PATH)!=BC_OK)return BC_FAILURE;
  for(i=0U;out[i];i++)if(out[i]=='\t'||out[i]=='\n'||out[i]=='\r')out[i]='_';
  return BC_OK;
}
static int bc_emit_sample(FILE *out,unsigned int action_id,
                          unsigned int layer_id,unsigned int strip_id,
                          unsigned int slot,unsigned int curve_id,
                          unsigned int component,unsigned int sample,
                          const char *path,char source,
                          int frame,int value,int lx,int ly,int rx,int ry,
                          unsigned int ipo)
{
  if(fprintf(out,"%u\t%u\t%u\t%u\t%u\t%s\t%u\t%u\t%c\t",
             action_id,layer_id,strip_id,slot,curve_id,path,component,sample,source)<0)
    return BC_FAILURE;
  bc_print_q16(out,frame);fputc('\t',out);
  bc_print_q16(out,value);fputc('\t',out);
  bc_print_q16(out,lx);fputc('\t',out);
  bc_print_q16(out,ly);fputc('\t',out);
  bc_print_q16(out,rx);fputc('\t',out);
  bc_print_q16(out,ry);
  if(fprintf(out,"\t%u\n",ipo)<0)return BC_FAILURE;
  return ferror(out)?BC_FAILURE:BC_OK;
}
int bc_anim_curve(BC_File *b, FILE *out, BC_U64 ptr,
                  unsigned int action_id,unsigned int layer_id,
                  unsigned int strip_id,unsigned int slot,
                  unsigned int curve_id)
{
  const BC_Struct *s=bc_get_struct(b,"FCurve"),*point;
  BC_U64 samples_ptr;
  unsigned int at,available,count,component,i,base,off,stride,bytes,ipo=255U;
  int frame,value,lx,ly,rx,ry,bezier=0,r;
  char path[BC_ANIM_PATH];
  if(!s || !s->valid || !b->type_len[s->type])return BC_UNSUPPORTED;
  if(bc_resolve(b,ptr,b->type_len[s->type],NULL,&at,&available)!=BC_OK)return BC_FAILURE;
  if(bc_read_struct_word(b,at,"FCurve","totvert",&count)!=BC_OK || count>BC_KEY_MAX_POINTS ||
     bc_read_struct_word(b,at,"FCurve","array_index",&component)!=BC_OK ||
     bc_fcurve_path(b,at,path)!=BC_OK)return BC_FAILURE;
  if(count==0U)return BC_OK;
  if(bc_eval_active())return bc_eval_curve(b,out,at,count,action_id,layer_id,
                                           strip_id,slot,curve_id,component,path);
  r=bc_read_struct_ptr(b,at,"FCurve","bezt",&samples_ptr);
  if(r!=BC_OK)return BC_UNSUPPORTED;
  if(samples_ptr.lo!=0U || samples_ptr.hi!=0U){
    bezier=1;point=bc_get_struct(b,"BezTriple");
  } else {
    if(bc_read_struct_ptr(b,at,"FCurve","fpt",&samples_ptr)!=BC_OK)return BC_UNSUPPORTED;
    point=bc_get_struct(b,"FPoint");
  }
  if(!point || !point->valid)return BC_UNSUPPORTED;
  stride=b->type_len[point->type];
  if(stride==0U || count>2147483647U/stride)return BC_FAILURE;
  bytes=count*stride;
  if(bc_resolve(b,samples_ptr,bytes,NULL,&base,&available)!=BC_OK)return BC_FAILURE;
  if(bezier){
    if(bc_field_addr(b,0U,"BezTriple","vec",&off)!=BC_OK ||
       off>stride || 36U>stride-off)return BC_UNSUPPORTED;
  } else {
    if(bc_field_addr(b,0U,"FPoint","vec",&off)!=BC_OK ||
       off>stride || 8U>stride-off)return BC_UNSUPPORTED;
  }
  for(i=0U;i<count;i++){
    if(base>2147483647U-i*stride)return BC_FAILURE;
    at=base+i*stride+off;
    if(bezier){
      /* vec[0]=left, vec[1]=control, vec[2]=right; [x,y,z] each */
      if(bc_q32(b,at+12U,&frame)!=BC_OK || bc_q32(b,at+16U,&value)!=BC_OK ||
         bc_q32(b,at,&lx)!=BC_OK || bc_q32(b,at+4U,&ly)!=BC_OK ||
         bc_q32(b,at+24U,&rx)!=BC_OK || bc_q32(b,at+28U,&ry)!=BC_OK)
        return BC_FAILURE;
      r=bc_anim_field_byte(b,base+i*stride,"BezTriple","ipo",&ipo);
      if(r!=BC_OK)return r;
    }else{
      if(bc_q32(b,at,&frame)!=BC_OK || bc_q32(b,at+4U,&value)!=BC_OK)
        return BC_FAILURE;
      lx=frame;ly=value;rx=frame;ry=value;ipo=255U;
    }
    if(bc_emit_sample(out,action_id,layer_id,strip_id,slot,curve_id,component,
                      i,path,bezier?'B':'P',frame,value,lx,ly,rx,ry,ipo)!=BC_OK)
      return BC_FAILURE;
  }
  return BC_OK;
}
