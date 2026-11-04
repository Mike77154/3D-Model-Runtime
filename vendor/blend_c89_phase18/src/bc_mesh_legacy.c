#include "bc.h"
#include <string.h>

/* Mesh export is streaming: at most one serialized vertex or face is resident.
   Validation runs before emission, so malformed topology never gets silently
   reported as successful OBJ extraction. */
static int bc_read_i32(BC_File *b,unsigned int at,unsigned int *out)
{return bc_read_word(b,at,out);}
static int bc_count(BC_File *b,unsigned int at,const char *first,
                    const char *second,unsigned int *out)
{
  if(bc_read_struct_word(b,at,"Mesh",first,out)==BC_OK)return BC_OK;
  return second?bc_read_struct_word(b,at,"Mesh",second,out):BC_FAILURE;
}
static int bc_ptr(BC_File *b,unsigned int at,const char *field,BC_View *v)
{
  BC_U64 p;
  if(bc_read_struct_ptr(b,at,"Mesh",field,&p)<0)return BC_FAILURE;
  return bc_view_pointer(b,p,1U,v);
}
int bc_custom_layer(BC_File *b,unsigned int at,const char *domain,
                    const char *wanted,BC_View *dst,unsigned int *stride)
{
  const BC_Field *fd=bc_get_field(b,"Mesh",domain);
  const BC_Field *fl=bc_get_field(b,"CustomDataLayer","name");
  const BC_Field *fp=bc_get_field(b,"CustomDataLayer","data");
  const BC_Struct *st=bc_get_struct(b,"CustomDataLayer");
  BC_View list;BC_U64 p;
  unsigned int n,i,loc,sz,available,len;
  char name[128];
  if(!fd||!fl||!fp||!st||!st->valid)return BC_UNSUPPORTED;
  if(at>2147483647U-fd->offset)return BC_FAILURE;
  at+=fd->offset;
  if(bc_read_struct_word(b,at,"CustomData","totlayer",&n)<0||n>4096U)return BC_FAILURE;
  if(bc_read_struct_ptr(b,at,"CustomData","layers",&p)<0)return BC_FAILURE;
  if(bc_view_pointer(b,p,1U,&list)<0)return BC_UNSUPPORTED;
  sz=b->type_len[st->type];
  if(!sz || n>list.size/sz)return BC_FAILURE;
  for(i=0U;i<n;i++) {
    loc=list.offset+i*sz;
    if(fl->offset>=sz)continue;
    available=sz-fl->offset;
    len=available<sizeof(name)?available:(unsigned int)sizeof(name);
    if(bc_at(b,loc+fl->offset,len,(unsigned char *)name)<0)continue;
    if(memchr(name,0,len)==NULL)continue;
    if(strcmp(name,wanted)!=0)continue;
    if(fp->offset>sz || (unsigned int)b->ptr_size>sz-fp->offset)return BC_FAILURE;
    if(bc_read_pointer(b,loc+fp->offset,&p)<0)return BC_FAILURE;
    if(bc_view_pointer(b,p,1U,dst)<0)return BC_FAILURE;
    *stride=(strcmp(wanted,"position")==0)?12U:4U;
    return BC_OK;
  }
  return BC_UNSUPPORTED;
}
static int bc_vertex_source(BC_File *b,unsigned int at,BC_View *view,
                            unsigned int *stride,unsigned int *co)
{
  BC_U64 ptr;
  const BC_Struct *st=bc_get_struct(b,"MVert");
  const BC_Field *f=bc_get_field(b,"MVert","co");
  if(!f)f=bc_get_field(b,"MVert","co_legacy");
  *co=0U;
  if(st&&st->valid&&f&&bc_read_struct_ptr(b,at,"Mesh","mvert",&ptr)==BC_OK &&
     bc_view_pointer(b,ptr,1U,view)==BC_OK) {
    *stride=b->type_len[st->type];*co=f->offset;
    return BC_OK;
  }
  if(bc_custom_layer(b,at,"vdata","position",view,stride)==BC_OK ||
     bc_custom_layer(b,at,"vert_data","position",view,stride)==BC_OK ||
     bc_modern_attr(b,at,"position",view,stride)==BC_OK ||
     bc_modern_attr(b,at,".position",view,stride)==BC_OK)return BC_OK;
  return BC_UNSUPPORTED;
}
static int bc_vertices(BC_File *b,FILE *out,unsigned int at,unsigned int *count)
{
  unsigned int i,j,stride,co,loc,bits;int fix;
  BC_View src;
  if(bc_count(b,at,"verts_num","totvert",count)<0 || *count>10000000U)return BC_FAILURE;
  if(*count==0U)return BC_OK;
  if(bc_vertex_source(b,at,&src,&stride,&co)<0)return BC_UNSUPPORTED;
  if(stride<co || stride-co<12U || *count>src.size/stride)return BC_FAILURE;
  for(i=0U;i<*count;i++) {
    loc=src.offset+i*stride+co;
    if(out && fputc('v',out)==EOF)return BC_FAILURE;
    for(j=0U;j<3U;j++) {
      if(bc_read_i32(b,loc+j*4U,&bits)<0||bc_f32_q16(bits,&fix)<0)return BC_FAILURE;
      if(out){if(fputc(' ',out)==EOF)return BC_FAILURE;bc_print_q16(out,fix);}
    }
    if(out && fputc('\n',out)==EOF)return BC_FAILURE;
  }
  return BC_OK;
}
static int bc_faces_mface(BC_File *b,FILE *out,unsigned int at,
                           unsigned int verts,unsigned int base,
                           const BC_UV *uv,unsigned int uvbase)
{
  const BC_Struct *st=bc_get_struct(b,"MFace");
  const BC_Field *f[4];
  BC_View view;
  unsigned int count,stride,i,j,v[4],pos,width;
  unsigned char raw[4];
  if(!st||!st->valid||bc_ptr(b,at,"mface",&view)<0 ||
     bc_count(b,at,"totface","totface_legacy",&count)<0)return BC_UNSUPPORTED;
  f[0]=bc_get_field(b,"MFace","v1");f[1]=bc_get_field(b,"MFace","v2");
  f[2]=bc_get_field(b,"MFace","v3");f[3]=bc_get_field(b,"MFace","v4");
  for(j=0;j<4U;j++)if(!f[j])return BC_UNSUPPORTED;
  stride=b->type_len[st->type];if(stride<4U||count>view.size/stride)return BC_FAILURE;
  for(i=0U;i<count;i++) {
    pos=view.offset+i*stride;
    for(j=0U;j<4U;j++) {
      /* Blender 2.28a MFace: ushort indices (2 bytes).
         Blender 2.79a MFace: unsigned int indices (4 bytes).
         Use the file's SDNA type length, never a hard-coded width. */
      width=(unsigned int)b->type_len[f[j]->type];
      if((width!=2U && width!=4U) || width>stride ||
         f[j]->offset>stride-width ||
         bc_at(b,pos+f[j]->offset,width,raw)!=BC_OK)return BC_FAILURE;
      v[j]=(width==2U)?bc_u16(raw,b->endian_le):bc_u32(raw,b->endian_le);
    }
    if(v[0]>=verts||v[1]>=verts||v[2]>=verts||
       (v[3]!=0U&&v[3]>=verts))return BC_FAILURE;
    if(out) {
      unsigned int corners=(v[3]==0U)?3U:4U;
      if(fputc('f',out)==EOF)return BC_FAILURE;
      for(j=0U;j<corners;j++) {
        if(uv && uv->present && uv->historical_tface) {
          if(i>2147483647U/4U || uvbase>2147483647U-4U*i-j-1U ||
             fprintf(out," %u/%u",base+v[j]+1U,uvbase+4U*i+j+1U)<0)
            return BC_FAILURE;
        } else if(fprintf(out," %u",base+v[j]+1U)<0)return BC_FAILURE;
      }
      if(fputc('\n',out)==EOF)return BC_FAILURE;
    }
  }
  return BC_OK;
}
static int bc_faces_mpoly(BC_File *b,FILE *out,unsigned int at,
                           unsigned int verts,unsigned int base,
                           const BC_UV *uv,unsigned int uvbase)
{
  const BC_Struct *ps=bc_get_struct(b,"MPoly"),*ls=bc_get_struct(b,"MLoop");
  const BC_Field *fs=bc_get_field(b,"MPoly","loopstart");
  const BC_Field *fc=bc_get_field(b,"MPoly","totloop");
  const BC_Field *fv=bc_get_field(b,"MLoop","v");
  BC_View polys,loops;
  unsigned int n,psz,lsz,i,j,start,cnt,idx,p;
  if(!ps||!ls||!ps->valid||!ls->valid||!fs||!fc||!fv ||
     bc_ptr(b,at,"mpoly",&polys)<0||bc_ptr(b,at,"mloop",&loops)<0 ||
     bc_count(b,at,"totpoly",NULL,&n)<0)return BC_UNSUPPORTED;
  psz=b->type_len[ps->type];lsz=b->type_len[ls->type];
  if(psz<4U||lsz<4U||n>polys.size/psz||fs->offset>psz-4U||
     fc->offset>psz-4U||fv->offset>lsz-4U)return BC_FAILURE;
  for(i=0U;i<n;i++) {
    p=polys.offset+i*psz;
    if(bc_read_word(b,p+fs->offset,&start)<0||
       bc_read_word(b,p+fc->offset,&cnt)<0)return BC_FAILURE;
    if(cnt<3U||cnt>4096U||start>loops.size/lsz||cnt>loops.size/lsz-start)return BC_FAILURE;
    if(out && fputc('f',out)==EOF)return BC_FAILURE;
    for(j=0U;j<cnt;j++) {
      if(bc_read_word(b,loops.offset+(start+j)*lsz+fv->offset,&idx)<0 || idx>=verts)return BC_FAILURE;
      if(out) {
        if(uv && uv->present) {
          if(start+j>=uv->count || fprintf(out," %u/%u",base+idx+1U,uvbase+start+j+1U)<0)return BC_FAILURE;
        } else if(fprintf(out," %u",base+idx+1U)<0)return BC_FAILURE;
      }
    }
    if(out&&fputc('\n',out)==EOF)return BC_FAILURE;
  }
  return BC_OK;
}
static int bc_faces(BC_File *b,FILE *out,unsigned int at,
                     unsigned int verts,unsigned int base,
                     const BC_UV *uv,unsigned int uvbase)
{
  unsigned int count;
  BC_View v;
  if(bc_count(b,at,"faces_num",NULL,&count)==BC_OK && count>0U &&
     bc_ptr(b,at,"face_offset_indices",&v)==BC_OK)
    return bc_modern_faces(b,out,at,verts,base,uv,uvbase);
  /* Blender 3.6.x uses totpoly/poly_offset_indices rather than
     faces_num/face_offset_indices. */
  if(bc_count(b,at,"totpoly",NULL,&count)==BC_OK && count>0U &&
     bc_ptr(b,at,"poly_offset_indices",&v)==BC_OK)
    return bc_modern_faces(b,out,at,verts,base,uv,uvbase);
  if(bc_count(b,at,"totpoly",NULL,&count)==BC_OK && count>0U &&
     bc_ptr(b,at,"mpoly",&v)==BC_OK)return bc_faces_mpoly(b,out,at,verts,base,uv,uvbase);
  if(bc_count(b,at,"totface","totface_legacy",&count)==BC_OK && count>0U &&
     bc_ptr(b,at,"mface",&v)==BC_OK)return bc_faces_mface(b,out,at,verts,base,uv,uvbase);
  if((bc_count(b,at,"faces_num",NULL,&count)==BC_OK ||
      bc_count(b,at,"totpoly",NULL,&count)==BC_OK ||
      bc_count(b,at,"totface",NULL,&count)==BC_OK) && count==0U)return BC_OK;
  return BC_UNSUPPORTED;
}
static void bc_label(BC_File *b,unsigned int at,char *name)
{
  const BC_Field *id=bc_get_field(b,"Mesh","id"),*f=bc_get_field(b,"ID","name");
  unsigned char text[66];unsigned int i,off;
  name[0]=0;
  if(!id||!f || at>2147483647U-id->offset ||
     at+id->offset>2147483647U-f->offset)return;
  off=at+id->offset+f->offset;
  if(bc_at(b,off,66U,text)<0)return;
  for(i=2U;i<65U && text[i];i++) {
    unsigned char c=text[i];
    name[i-2U]=(c>=33U&&c<=126U&&c!='#'&&c!='\\')?(char)c:'_';
  }
  name[i-2U]=0;
}
static int bc_walk(BC_File *b,FILE *out, int listing,int selected)
{
  const BC_Struct *st=bc_get_struct(b,"Mesh");
  const BC_Block *blk;
  unsigned int i,j,stride,n,base=0U,uvbase=0U,seen=0U,at;
  BC_UV uv;
  char label[66];int r;
  if(!st||!st->valid)return BC_UNSUPPORTED;
  stride=b->type_len[st->type];if(!stride)return BC_UNSUPPORTED;
  for(i=0U;i<b->nblocks;i++) {
    blk=&b->blocks[i];
    if(blk->sdna>=b->nstructs ||
       b->structs[blk->sdna].type!=st->type ||
       blk->code[0]!='M'||blk->code[1]!='E')continue;
    if(blk->count>blk->size/stride)return BC_FAILURE;
    for(j=0U;j<blk->count;j++) {
      at=blk->offset+j*stride;
      if(selected>=0 && seen!=(unsigned int)selected){seen++;continue;}
      bc_label(b,at,label);
      r=bc_vertices(b,NULL,at,&n);
      if(r!=BC_OK)return r;
      if(base>2147483647U-n)return BC_FAILURE;
      uv.present=0;uv.count=0U;
      r=bc_uv_find(b,at,&uv);
      if(r==BC_FAILURE)return BC_FAILURE;
      if(uv.present && (bc_uv_emit(b,NULL,&uv)!=BC_OK || uvbase>2147483647U-uv.count))return BC_FAILURE;
      if(bc_faces(b,NULL,at,n,base,&uv,uvbase)!=BC_OK)return BC_FAILURE;
      if(listing) {
        if(fprintf(out,"mesh[%u] name=%s vertices=%u block=%u\n",seen,
                   label[0]?label:"(unnamed)",n,i)<0)return BC_FAILURE;
      } else if(out) {
        if(fprintf(out,"o %s_%u\n",label[0]?label:"Mesh",seen)<0)return BC_FAILURE;
        if(bc_vertices(b,out,at,&n)!=BC_OK ||
           bc_uv_emit(b,out,&uv)!=BC_OK ||
           bc_faces(b,out,at,n,base,&uv,uvbase)!=BC_OK)return BC_FAILURE;
      }
      base+=n;if(uv.present)uvbase+=uv.count;seen++;
    }
  }
  return seen && (selected<0 || seen>(unsigned int)selected)?BC_OK:BC_UNSUPPORTED;
}
int bc_mesh_report(BC_File *b,FILE *out){return bc_walk(b,out,1,-1);}
int bc_extract_obj(BC_File *b,FILE *out){return bc_walk(b,out,0,-1);}
int bc_extract_obj_mesh(BC_File *b,FILE *out,unsigned int mesh_index)
{
  if(mesh_index>32767U)return BC_UNSUPPORTED;
  return bc_walk(b,out,0,(int)mesh_index);
}
