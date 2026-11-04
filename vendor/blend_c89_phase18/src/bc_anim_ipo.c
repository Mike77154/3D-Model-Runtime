/* Historical Blender IPO editor / pre-2.5 IpoCurve data exporter.
   Decodes stored numeric address codes; does NOT map them onto RNA paths.
   No IPO drivers/modifiers evaluation. Standalone IP datablocks only. */
#include "bc.h"
#define BC_IPO_CURVE_LIMIT 32768U
#define BC_IPO_POINT_LIMIT 500000U
static BC_U64 bc_ipo_seen[BC_IPO_CURVE_LIMIT];

static int bc_ipo_short(BC_File *b,unsigned int base,
                        const char *field,unsigned int *value)
{
  const BC_Field *f=bc_get_field(b,"IpoCurve",field);
  unsigned char buf[2];
  if(!f || f->offset>2147483647U-base ||
     bc_at(b,base+f->offset,2U,buf)!=BC_OK)return BC_UNSUPPORTED;
  *value=bc_u16(buf,b->endian_le);
  return BC_OK;
}
static int bc_ipo_point(BC_File *b,unsigned int at,int *v)
{
  unsigned int bits;
  if(bc_read_word(b,at,&bits)!=BC_OK || bc_f32_q16(bits,v)!=BC_OK)return BC_FAILURE;
  return BC_OK;
}
static int bc_ipo_curve(BC_File *b,FILE *out,unsigned int at,
                        unsigned int ipo_index,unsigned int curve_index)
{
  const BC_Struct *bt,*pt;
  const BC_Field *vec;
  BC_U64 arr;
  unsigned int count,blocktype,adrcode,mode,stride,base,avail,i,off;
  int bezier=0,frame,value,lx,ly,rx,ry;
  if(bc_ipo_short(b,at,"totvert",&count)!=BC_OK ||
     bc_ipo_short(b,at,"blocktype",&blocktype)!=BC_OK ||
     bc_ipo_short(b,at,"adrcode",&adrcode)!=BC_OK ||
     bc_ipo_short(b,at,"ipo",&mode)!=BC_OK)return BC_UNSUPPORTED;
  if(count>BC_IPO_POINT_LIMIT)return BC_FAILURE;
  if(!count)return BC_OK;
  if(bc_read_struct_ptr(b,at,"IpoCurve","bezt",&arr)!=BC_OK)return BC_UNSUPPORTED;
  if(arr.lo!=0U || arr.hi!=0U){
    bezier=1;bt=bc_get_struct(b,"BezTriple");
    if(!bt || !bt->valid || !(vec=bc_get_field(b,"BezTriple","vec")))return BC_UNSUPPORTED;
  }else{
    if(bc_read_struct_ptr(b,at,"IpoCurve","bp",&arr)!=BC_OK)return BC_UNSUPPORTED;
    bt=bc_get_struct(b,"BPoint");
    if(!bt || !bt->valid || !(vec=bc_get_field(b,"BPoint","vec")))return BC_UNSUPPORTED;
  }
  pt=bt;
  stride=b->type_len[pt->type];off=vec->offset;
  if(!stride || count>2147483647U/stride || off>stride ||
     (bezier?36U:8U)>stride-off ||
     bc_resolve(b,arr,count*stride,NULL,&base,&avail)!=BC_OK)return BC_FAILURE;
  for(i=0U;i<count;i++){
    at=base+i*stride+off;
    if(bezier){
      if(bc_ipo_point(b,at+12U,&frame)!=BC_OK ||
         bc_ipo_point(b,at+16U,&value)!=BC_OK ||
         bc_ipo_point(b,at,&lx)!=BC_OK ||
         bc_ipo_point(b,at+4U,&ly)!=BC_OK ||
         bc_ipo_point(b,at+24U,&rx)!=BC_OK ||
         bc_ipo_point(b,at+28U,&ry)!=BC_OK)return BC_FAILURE;
    }else{
      if(bc_ipo_point(b,at,&frame)!=BC_OK ||
         bc_ipo_point(b,at+4U,&value)!=BC_OK)return BC_FAILURE;
      lx=frame;ly=value;rx=frame;ry=value;
    }
    if(fprintf(out,"%u\t%u\t%u\t%u\t%u\t%c\t",
               ipo_index,curve_index,blocktype,adrcode,i,bezier?'B':'P')<0)return BC_FAILURE;
    bc_print_q16(out,frame);fputc('\t',out);
    bc_print_q16(out,value);fputc('\t',out);
    bc_print_q16(out,lx);fputc('\t',out);
    bc_print_q16(out,ly);fputc('\t',out);
    bc_print_q16(out,rx);fputc('\t',out);
    bc_print_q16(out,ry);
    if(fprintf(out,"\t%u\n",mode)<0 || ferror(out))return BC_FAILURE;
  }
  return BC_OK;
}
static int bc_ipo_one(BC_File *b,FILE *out,unsigned int at,unsigned int ipo_index,unsigned int *found)
{
  const BC_Struct *curve=bc_get_struct(b,"IpoCurve");
  const BC_Field *cl=bc_get_field(b,"Ipo","curve"),*first=bc_get_field(b,"ListBase","first");
  BC_U64 ptr,next;
  unsigned int index=0U,pos,avail,k;
  int r;
  if(!curve || !curve->valid || !cl || !first ||
     at>2147483647U-cl->offset || at+cl->offset>2147483647U-first->offset)return BC_UNSUPPORTED;
  if(bc_read_pointer(b,at+cl->offset+first->offset,&ptr)!=BC_OK)return BC_FAILURE;
  while(ptr.lo || ptr.hi){
    if(index>=BC_IPO_CURVE_LIMIT)return BC_FAILURE;
    for(k=0U;k<index;k++)if(bc_same64(bc_ipo_seen[k],ptr))return BC_FAILURE;
    bc_ipo_seen[index]=ptr;
    if(bc_resolve(b,ptr,b->type_len[curve->type],NULL,&pos,&avail)!=BC_OK)return BC_FAILURE;
    r=bc_ipo_curve(b,out,pos,ipo_index,index);
    if(r!=BC_OK)return r;
    if(bc_read_struct_ptr(b,pos,"IpoCurve","next",&next)!=BC_OK)return BC_FAILURE;
    ptr=next;index++;
  }
  *found+=index;
  return BC_OK;
}
int bc_export_ipo(BC_File *b,FILE *out)
{
  const BC_Struct *ipo=bc_get_struct(b,"Ipo");
  const BC_Block *blk;
  unsigned int i,j,at,num=0U,found=0U,stride;
  int r;
  if(!ipo || !ipo->valid)return BC_UNSUPPORTED;
  stride=b->type_len[ipo->type];if(!stride)return BC_UNSUPPORTED;
  if(fputs("# ipo\tcurve\tblocktype\tadrcode\tkey\tsource\tframe\tvalue\tleft_x\tleft_y\tright_x\tright_y\tmode\n",out)==EOF)return BC_FAILURE;
  for(i=0U;i<b->nblocks;i++){
    blk=&b->blocks[i];
    if(blk->sdna>=b->nstructs || b->structs[blk->sdna].type!=ipo->type ||
       blk->code[0]!='I' || blk->code[1]!='P')continue;
    if(blk->count>blk->size/stride)return BC_FAILURE;
    for(j=0U;j<blk->count;j++){
      at=blk->offset+j*stride;
      r=bc_ipo_one(b,out,at,num,&found);
      if(r!=BC_OK)return r;
      num++;
    }
  }
  return num && found && !ferror(out)?BC_OK:BC_UNSUPPORTED;
}
