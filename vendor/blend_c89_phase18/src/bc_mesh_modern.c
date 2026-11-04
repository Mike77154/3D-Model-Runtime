#include "bc.h"
#include <string.h>
/* 5.x AttributeStorage->Attribute->AttributeArray->data.
   Resolve serialized token ranges; do not reinterpret as native C pointers. */
int bc_modern_attr(BC_File *b,unsigned int mesh_at,const char *wanted,
                   BC_View *outview,unsigned int *stride)
{
  const BC_Field *fstore=bc_get_field(b,"Mesh","attribute_storage");
  const BC_Struct *sattr=bc_get_struct(b,"Attribute");
  BC_View attrs,itemdata;
  BC_U64 p;
  unsigned int n,size,i,item,at;
  char text[128];
  if(!fstore||!sattr||!sattr->valid)return BC_UNSUPPORTED;
  if(mesh_at>2147483647U-fstore->offset)return BC_FAILURE;
  at=mesh_at+fstore->offset;
  if(bc_read_struct_word(b,at,"AttributeStorage","dna_attributes_num",&n)<0 || n>65536U)
    return BC_FAILURE;
  if(n==0U)return BC_UNSUPPORTED;
  if(bc_read_struct_ptr(b,at,"AttributeStorage","dna_attributes",&p)<0 ||
     bc_view_pointer(b,p,1U,&attrs)<0)return BC_FAILURE;
  size=b->type_len[sattr->type];
  if(!size||n>attrs.size/size)return BC_FAILURE;
  for(i=0U;i<n;i++) {
    item=attrs.offset+i*size;
    if(bc_read_struct_ptr(b,item,"Attribute","name",&p)<0 ||
       bc_read_cstr(b,p,text,(unsigned int)sizeof(text))<0)continue;
    if(strcmp(text,wanted)!=0)continue;
    /* Field-dependent enum validation (5.x). Use the file's SDNA offsets. */
    {
      const BC_Field *fst=bc_get_field(b,"Attribute","storage_type");
      const BC_Field *ft=bc_get_field(b,"Attribute","data_type");
      const BC_Field *fd=bc_get_field(b,"Attribute","domain");
      unsigned char raw[2];
      unsigned int expect_position=(strcmp(wanted,"position")==0 || strcmp(wanted,".position")==0);
      if(fst) {
        if(bc_at(b,item+fst->offset,1U,raw)<0)return BC_FAILURE;
        if(raw[0]!=0U)return BC_UNSUPPORTED; /* AttrStorageType::Array */
      }
      if(ft) {
        if(bc_at(b,item+ft->offset,2U,raw)<0)return BC_FAILURE;
        if(bc_u16(raw,b->endian_le)!=(expect_position?7U:3U))return BC_UNSUPPORTED;
      }
      if(fd) {
        if(bc_at(b,item+fd->offset,1U,raw)<0)return BC_FAILURE;
        if(raw[0]!=(expect_position?0U:3U))return BC_UNSUPPORTED;
      }
    }
    /* Dense array storage only. Single-valued attributes require another vendor. */
    if(bc_read_struct_ptr(b,item,"Attribute","data",&p)<0 ||
       bc_view_pointer(b,p,1U,&itemdata)<0)return BC_FAILURE;
    if(bc_read_struct_ptr(b,itemdata.offset,"AttributeArray","data",&p)<0 ||
       bc_view_pointer(b,p,1U,outview)<0)return BC_FAILURE;
    *stride=(strcmp(wanted,"position")==0||strcmp(wanted,".position")==0)?12U:4U;
    /* Do not mistakenly infer array length from remaining enclosing storage. */
    if(bc_get_field(b,"AttributeArray","is_single")!=NULL) {
      const BC_Field *si=bc_get_field(b,"AttributeArray","is_single");
      unsigned char v;
      if(bc_at(b,itemdata.offset+si->offset,1U,&v)<0)return BC_FAILURE;
      if(v!=0U)return BC_UNSUPPORTED;
    }
    if(bc_get_field(b,"AttributeArray","size")!=NULL) {
      const BC_Field *sz=bc_get_field(b,"AttributeArray","size");
      unsigned char raw[8];BC_U64 v;
      if(bc_at(b,itemdata.offset+sz->offset,8U,raw)<0)return BC_FAILURE;
      v=bc_u64(raw,b->endian_le);
      if(v.hi!=0U||v.lo>outview->size/(*stride))return BC_FAILURE;
    }
    return BC_OK;
  }
  return BC_UNSUPPORTED;
}
int bc_modern_faces(BC_File *b,FILE *out,unsigned int mesh_at,
                    unsigned int verts,unsigned int base,
                    const BC_UV *uv,unsigned int uvbase)
{
  BC_View offsets,corners;
  BC_U64 p;
  unsigned int nf,ncorner,i,j,start,end,idx,stride=4U;
  if(bc_read_struct_word(b,mesh_at,"Mesh","faces_num",&nf)<0 &&
     bc_read_struct_word(b,mesh_at,"Mesh","totpoly",&nf)<0)
    return BC_UNSUPPORTED;
  if(nf>10000000U)return BC_FAILURE;
  if(bc_read_struct_ptr(b,mesh_at,"Mesh","face_offset_indices",&p)<0 &&
     bc_read_struct_ptr(b,mesh_at,"Mesh","poly_offset_indices",&p)<0)
    return BC_UNSUPPORTED;
  if(bc_view_pointer(b,p,4U,&offsets)<0)return BC_FAILURE;
  if(nf==0U)return BC_OK;
  if(nf>=offsets.size/4U)return BC_FAILURE;
  if(bc_read_struct_word(b,mesh_at,"Mesh","corners_num",&ncorner)<0 &&
     bc_read_struct_word(b,mesh_at,"Mesh","totloop",&ncorner)<0)
    return BC_FAILURE;
  if(bc_modern_attr(b,mesh_at,".corner_vert",&corners,&stride)<0 &&
     bc_custom_layer(b,mesh_at,"corner_data",".corner_vert",&corners,&stride)<0 &&
     bc_custom_layer(b,mesh_at,"ldata",".corner_vert",&corners,&stride)<0)
    return BC_UNSUPPORTED;
  if(stride<4U || ncorner>corners.size/stride)return BC_FAILURE;
  for(i=0U;i<nf;i++) {
    if(bc_read_word(b,offsets.offset+i*4U,&start)<0 ||
       bc_read_word(b,offsets.offset+(i+1U)*4U,&end)<0)return BC_FAILURE;
    if(end<start||end-start<3U||end-start>4096U||end>ncorner)return BC_FAILURE;
    if(out&&fputc('f',out)==EOF)return BC_FAILURE;
    for(j=start;j<end;j++) {
      if(bc_read_word(b,corners.offset+j*stride,&idx)<0 || idx>=verts)return BC_FAILURE;
      if(out){
        if(uv && uv->present){
          if(j>=uv->count || fprintf(out," %u/%u",base+idx+1U,uvbase+j+1U)<0)return BC_FAILURE;
        } else if(fprintf(out," %u",base+idx+1U)<0)return BC_FAILURE;
      }
    }
    if(out&&fputc('\n',out)==EOF)return BC_FAILURE;
  }
  return BC_OK;
}
