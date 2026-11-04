/* Vertex-group weights, streaming TSV; no runtime allocations or FP. */
#include "bc.h"
#include <string.h>

#define BC_SKIN_MAX_GROUPS 4096U
#define BC_SKIN_NAME_MAX 96U
static char bc_skin_names[BC_SKIN_MAX_GROUPS][BC_SKIN_NAME_MAX];

static int bc_skin_list(BC_File *b,unsigned int at,const char *type,
                        const char *field,BC_U64 *first)
{
  const BC_Field *f=bc_get_field(b,type,field);
  const BC_Field *base=bc_get_field(b,"ListBase","first");
  if(!f || !base || at>2147483647U-f->offset ||
     at+f->offset>2147483647U-base->offset)return BC_UNSUPPORTED;
  return bc_read_pointer(b,at+f->offset+base->offset,first);
}
static int bc_skin_token(BC_U64 start,unsigned int delta,BC_U64 *out)
{
  unsigned int carry;
  out->lo=start.lo+delta;
  carry=(out->lo<start.lo)?1U:0U;
  if(carry && start.hi==4294967295U)return BC_FAILURE;
  out->hi=start.hi+carry;
  return BC_OK;
}
static int bc_skin_mesh_list(BC_File *b,const BC_Block *block,
                             unsigned int offset,BC_U64 *first)
{
  BC_U64 key,p;
  const BC_Block *ob;
  const BC_Struct *st=bc_get_struct(b,"Object");
  unsigned int i,j,at,sz;
  if(bc_skin_list(b,offset,"Mesh","vertex_group_names",first)==BC_OK &&
     (first->lo || first->hi))return BC_OK;
  if(bc_skin_token(block->old,offset-block->offset,&key)<0)return BC_FAILURE;
  if(!st || !st->valid)return BC_UNSUPPORTED;
  sz=b->type_len[st->type];if(!sz)return BC_UNSUPPORTED;
  for(i=0U;i<b->nblocks;i++) {
    ob=&b->blocks[i];
    if(ob->sdna>=b->nstructs || b->structs[ob->sdna].type!=st->type ||
       ob->code[0]!='O' || ob->code[1]!='B')continue;
    if(ob->count>ob->size/sz)return BC_FAILURE;
    for(j=0U;j<ob->count;j++){
      at=ob->offset+j*sz;
      if(bc_read_struct_ptr(b,at,"Object","data",&p)<0)return BC_UNSUPPORTED;
      if(!bc_same64(key,p))continue;
      return bc_skin_list(b,at,"Object","defbase",first);
    }
  }
  return BC_UNSUPPORTED;
}
static int bc_skin_groups(BC_File *b,BC_U64 ptr,unsigned int *num)
{
  const BC_Struct *st=bc_get_struct(b,"bDeformGroup");
  const BC_Field *fname=bc_get_field(b,"bDeformGroup","name");
  unsigned int i,at,avail,n,lim,k;char tmp[BC_SKIN_NAME_MAX];
  BC_U64 next,seen[BC_SKIN_MAX_GROUPS];
  *num=0U;
  if(!st || !st->valid || !fname)return BC_UNSUPPORTED;
  n=b->type_len[st->type];
  if(n==0U || fname->offset>=n)return BC_UNSUPPORTED;
  lim=n-fname->offset;
  if(lim>BC_SKIN_NAME_MAX-1U)lim=BC_SKIN_NAME_MAX-1U;
  while(ptr.lo || ptr.hi){
    if(*num>=BC_SKIN_MAX_GROUPS)return BC_FAILURE;
    for(i=0U;i<*num;i++)if(bc_same64(seen[i],ptr))return BC_FAILURE;
    seen[*num]=ptr;
    if(bc_resolve(b,ptr,n,NULL,&at,&avail)<0)return BC_FAILURE;
    if(bc_at(b,at+fname->offset,lim,(unsigned char *)tmp)<0)return BC_FAILURE;
    tmp[lim]=0;
    for(k=0U;k<lim && tmp[k];k++){
      if(tmp[k]=='\r'||tmp[k]=='\n'||tmp[k]=='\t')tmp[k]='_';
    }
    tmp[k]=0;
    strcpy(bc_skin_names[*num],tmp);
    ++(*num);
    if(bc_read_struct_ptr(b,at,"bDeformGroup","next",&next)<0)return BC_FAILURE;
    ptr=next;
  }
  return BC_OK;
}
/* CD_MDEFORMVERT=2, in older vdata and newer vert_data. */
static int bc_skin_layer(BC_File *b,unsigned int mesh,const char *domain,BC_View *dst)
{
  const BC_Field *fd=bc_get_field(b,"Mesh",domain);
  const BC_Field *ft=bc_get_field(b,"CustomDataLayer","type");
  const BC_Field *fp=bc_get_field(b,"CustomDataLayer","data");
  const BC_Struct *st=bc_get_struct(b,"CustomDataLayer");
  BC_U64 p;BC_View list;
  unsigned int at,n,size,i,loc,kind;
  if(!fd || !ft || !fp || !st || !st->valid)return BC_UNSUPPORTED;
  if(mesh>2147483647U-fd->offset)return BC_FAILURE;
  at=mesh+fd->offset;
  if(bc_read_struct_word(b,at,"CustomData","totlayer",&n)<0 || n>4096U)return BC_FAILURE;
  if(n==0U)return BC_UNSUPPORTED;
  if(bc_read_struct_ptr(b,at,"CustomData","layers",&p)<0 ||
     bc_view_pointer(b,p,1U,&list)<0)return BC_FAILURE;
  size=b->type_len[st->type];
  if(size==0U || n>list.size/size || ft->offset>size-4U ||
     size<(unsigned int)b->ptr_size || fp->offset>size-(unsigned int)b->ptr_size)return BC_FAILURE;
  for(i=0U;i<n;i++) {
    loc=list.offset+i*size;
    if(bc_read_word(b,loc+ft->offset,&kind)<0)return BC_FAILURE;
    if(kind!=2U)continue;
    if(bc_read_pointer(b,loc+fp->offset,&p)<0 ||
       bc_view_pointer(b,p,1U,dst)<0)return BC_FAILURE;
    return BC_OK;
  }
  return BC_UNSUPPORTED;
}
static int bc_skin_source(BC_File *b,unsigned int at,BC_View *v)
{
  BC_U64 p;
  if(bc_read_struct_ptr(b,at,"Mesh","dvert",&p)==BC_OK &&
     (p.lo || p.hi)){
    if(bc_view_pointer(b,p,1U,v)<0)return BC_FAILURE;
    return BC_OK;
  }
  {
    int r=bc_skin_layer(b,at,"vert_data",v);
    if(r!=BC_UNSUPPORTED)return r;
    return bc_skin_layer(b,at,"vdata",v);
  }
}
static int bc_skin_write(BC_File *b,FILE *out,unsigned int mesh_index,
                         unsigned int mesh_at,const BC_Block *block)
{
  const BC_Struct *dv=bc_get_struct(b,"MDeformVert");
  const BC_Struct *dw=bc_get_struct(b,"MDeformWeight");
  const BC_Field *fg=bc_get_field(b,"MDeformWeight","def_nr");
  const BC_Field *fw=bc_get_field(b,"MDeformWeight","weight");
  BC_View source,weights;
  BC_U64 list,p;
  unsigned int ngroups=0U,verts=0U,ds,ws,i,j,n,idx,bits,at,dwat,needed;
  int val,r;
  if(bc_read_struct_word(b,mesh_at,"Mesh","verts_num",&verts)<0 &&
     bc_read_struct_word(b,mesh_at,"Mesh","totvert",&verts)<0)return BC_UNSUPPORTED;
  if(verts>10000000U)return BC_FAILURE;
  r=bc_skin_source(b,mesh_at,&source);
  if(r!=BC_OK)return r;
  if(!dv || !dw || !dv->valid || !dw->valid || !fg || !fw)return BC_UNSUPPORTED;
  ds=b->type_len[dv->type];ws=b->type_len[dw->type];
  if(!ds || !ws || fg->offset>ws-4U || fw->offset>ws-4U || verts>source.size/ds)return BC_FAILURE;
  list.lo=0U;list.hi=0U;
  if(bc_skin_mesh_list(b,block,mesh_at,&list)==BC_OK && (list.lo || list.hi)){
    if(bc_skin_groups(b,list,&ngroups)<0)return BC_FAILURE;
  }
  if(fprintf(out,"# mesh %u vertices=%u groups=%u\n",mesh_index,verts,ngroups)<0)return BC_FAILURE;
  for(i=0U;i<verts;i++) {
    at=source.offset+i*ds;
    if(bc_read_struct_word(b,at,"MDeformVert","totweight",&n)<0 ||
       n>65536U)return BC_FAILURE;
    if(n==0U)continue;
    if(bc_read_struct_ptr(b,at,"MDeformVert","dw",&p)<0)return BC_FAILURE;
    if(ws>2147483647U/n)return BC_FAILURE;
    needed=n*ws;
    if(bc_view_pointer(b,p,needed,&weights)<0)return BC_FAILURE;
    for(j=0U;j<n;j++){
      dwat=weights.offset+j*ws;
      if(bc_read_word(b,dwat+fg->offset,&idx)<0 ||
         bc_read_word(b,dwat+fw->offset,&bits)<0 ||
         bc_f32_q16(bits,&val)<0)return BC_FAILURE;
      if((bits&0x80000000U)!=0U || val<0 || val>65536)return BC_FAILURE;
      if(fprintf(out,"%u\t%u\t%u\t",mesh_index,i,idx)<0)return BC_FAILURE;
      if(idx<ngroups && fputs(bc_skin_names[idx],out)==EOF)return BC_FAILURE;
      if(fputc('\t',out)==EOF)return BC_FAILURE;
      bc_print_q16(out,val);
      if(fputc('\n',out)==EOF)return BC_FAILURE;
    }
  }
  return BC_OK;
}
int bc_export_weights(BC_File *b,FILE *out)
{
  const BC_Struct *ms=bc_get_struct(b,"Mesh");
  const BC_Block *block;
  unsigned int stride,i,j,count=0U,at;
  int r;
  if(!ms||!ms->valid)return BC_UNSUPPORTED;
  stride=b->type_len[ms->type];if(!stride)return BC_UNSUPPORTED;
  if(fputs("# mesh_index\tvertex_index\tgroup_index\tgroup_name\tweight\n",out)==EOF)return BC_FAILURE;
  for(i=0U;i<b->nblocks;i++){
    block=&b->blocks[i];
    if(block->code[0]!='M'||block->code[1]!='E'||block->sdna>=b->nstructs ||
       b->structs[block->sdna].type!=ms->type)continue;
    if(block->count>block->size/stride)return BC_FAILURE;
    for(j=0U;j<block->count;j++){
      at=block->offset+j*stride;
      r=bc_skin_write(b,out,count,at,block);
      if(r!=BC_OK)return r;
      count++;
    }
  }
  return count?BC_OK:BC_UNSUPPORTED;
}
