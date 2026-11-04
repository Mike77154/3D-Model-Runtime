#include "bc.h"
/* One UV layer per mesh: first supported corner layer, no heap or FP arithmetic.
   CD_MLOOPUV=16, CD_PROP_FLOAT2=49. Blender DNA enum values. */
static int bc_uv_layers(BC_File *b,unsigned int mesh_at,const char *domain,BC_UV *uv)
{
  const BC_Field *fd=bc_get_field(b,"Mesh",domain);
  const BC_Field *ft=bc_get_field(b,"CustomDataLayer","type");
  const BC_Field *fp=bc_get_field(b,"CustomDataLayer","data");
  const BC_Struct *st=bc_get_struct(b,"CustomDataLayer");
  BC_U64 p;BC_View list;
  unsigned int n,i,at,sz,loc,type;
  if(!fd||!ft||!fp||!st||!st->valid)return BC_UNSUPPORTED;
  if(mesh_at>2147483647U-fd->offset)return BC_FAILURE;
  at=mesh_at+fd->offset;
  if(bc_read_struct_word(b,at,"CustomData","totlayer",&n)<0 || n>4096U)return BC_FAILURE;
  if(n==0U)return BC_UNSUPPORTED;
  if(bc_read_struct_ptr(b,at,"CustomData","layers",&p)<0 ||
     bc_view_pointer(b,p,1U,&list)<0)return BC_FAILURE;
  sz=b->type_len[st->type];
  if(!sz || n>list.size/sz || ft->offset>sz-4U ||
     fp->offset>sz-(unsigned int)b->ptr_size)return BC_FAILURE;
  for(i=0U;i<n;i++) {
    loc=list.offset+i*sz;
    if(bc_read_word(b,loc+ft->offset,&type)<0)return BC_FAILURE;
    if(type!=16U && type!=49U)continue;
    if(bc_read_pointer(b,loc+fp->offset,&p)<0 ||
       bc_view_pointer(b,p,1U,&uv->view)<0)return BC_FAILURE;
    uv->stride=8U;
    uv->coord=0U;
    if(type==16U) {
      const BC_Struct *s=bc_get_struct(b,"MLoopUV");
      const BC_Field *f=bc_get_field(b,"MLoopUV","uv");
      if(!s||!s->valid||!f)return BC_UNSUPPORTED;
      uv->stride=b->type_len[s->type];uv->coord=f->offset;
    }
    if(uv->stride<uv->coord || uv->stride-uv->coord<8U)return BC_FAILURE;
    uv->present=1;
    return BC_OK;
  }
  return BC_UNSUPPORTED;
}
int bc_uv_find(BC_File *b,unsigned int mesh_at,BC_UV *uv)
{
  unsigned int n;
  uv->present=0;uv->count=0U;uv->historical_tface=0;
  if(bc_read_struct_word(b,mesh_at,"Mesh","corners_num",&n)<0 &&
     bc_read_struct_word(b,mesh_at,"Mesh","totloop",&n)<0)
    return bc_uv_tface_find(b,mesh_at,uv);
  if(n==0U)return BC_UNSUPPORTED;
  uv->count=n;
  {
    int rc=bc_uv_layers(b,mesh_at,"ldata",uv);
    if(rc==BC_FAILURE)return BC_FAILURE;
    if(rc!=BC_OK) {
      rc=bc_uv_layers(b,mesh_at,"corner_data",uv);
      if(rc!=BC_OK)return rc;
    }
  }
  if(n>uv->view.size/uv->stride)return BC_FAILURE;
  return BC_OK;
}
int bc_uv_emit(BC_File *b,FILE *out,const BC_UV *uv)
{
  unsigned int i,j,at,bits;int q;
  if(!uv->present)return BC_OK;
  if(uv->historical_tface) {
    if(uv->count%4U!=0U || uv->count/4U>uv->view.size/uv->stride)return BC_FAILURE;
  } else if(uv->count>uv->view.size/uv->stride)return BC_FAILURE;
  for(i=0U;i<uv->count;i++) {
    if(uv->historical_tface)
      at=uv->view.offset+(i/4U)*uv->stride+uv->coord+(i%4U)*8U;
    else at=uv->view.offset+i*uv->stride+uv->coord;
    if(out&&fprintf(out,"vt")<0)return BC_FAILURE;
    for(j=0U;j<2U;j++) {
      if(bc_read_word(b,at+j*4U,&bits)<0 || bc_f32_q16(bits,&q)<0)return BC_FAILURE;
      if(out){if(fputc(' ',out)==EOF)return BC_FAILURE;bc_print_q16(out,q);}
    }
    if(out&&fputc('\n',out)==EOF)return BC_FAILURE;
  }
  return BC_OK;
}
