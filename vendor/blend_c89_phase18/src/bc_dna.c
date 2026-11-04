#include "bc.h"
#include <string.h>
#include <ctype.h>

static unsigned int bc_d_align(unsigned int n,unsigned int a)
{
  return a==0U?n:((n+a-1U)/a)*a;
}
static int bc_d_string(BC_File *b,unsigned int *pos)
{
  unsigned int at=*pos;
  while(*pos<b->dna_bytes && b->dna[*pos]!=0) ++(*pos);
  if(*pos==b->dna_bytes || *pos==at) return BC_FAILURE;
  ++(*pos);return BC_OK;
}
static int bc_d_u32(BC_File *b,unsigned int *pos,unsigned int *v)
{
  if(*pos>b->dna_bytes || b->dna_bytes-*pos<4U)return BC_FAILURE;
  *v=bc_u32(b->dna+*pos,b->endian_le);*pos+=4U;return BC_OK;
}
static int bc_d_u16(BC_File *b,unsigned int *pos,unsigned int *v)
{
  if(*pos>b->dna_bytes || b->dna_bytes-*pos<2U)return BC_FAILURE;
  *v=bc_u16(b->dna+*pos,b->endian_le);*pos+=2U;return BC_OK;
}
static int bc_d_tag(BC_File *b,unsigned int *pos,const char *tag)
{
  if(*pos>b->dna_bytes || b->dna_bytes-*pos<4U || memcmp(b->dna+*pos,tag,4)!=0)return BC_FAILURE;
  *pos+=4U;return BC_OK;
}
const char *bc_type(const BC_File *b,unsigned int i)
{return i<b->ntypes?(const char *)b->dna+b->type_pos[i]:NULL;}
const char *bc_name(const BC_File *b,unsigned int i)
{return i<b->nnames?(const char *)b->dna+b->name_pos[i]:NULL;}
static int bc_d_find_type(const BC_File *b,unsigned int t)
{
  unsigned int i;
  for(i=0;i<b->nstructs;i++)if(b->structs[i].type==t)return (int)i;
  return -1;
}
static unsigned int bc_d_array(const char *str)
{
  unsigned int n=1U,v,got;
  while(*str) {
    if(*str=='['){
      ++str;v=0U;got=0U;
      while(*str>='0' && *str<='9') {
        if(v>1000000U)return 0U;
        v=v*10U+(unsigned int)(*str-'0');++str;++got;
      }
      if(*str!=']' || !got || !v || n>1000000U/v)return 0U;
      n*=v;
    }
    if(*str)++str;
  }
  return n;
}
static unsigned int bc_d_layout(BC_File *b,unsigned int si,unsigned int depth)
{
  BC_Struct *s; BC_Field *f;
  unsigned int i,size,align,sz,fa,arr,cur=0U,sa=1U;
  int child;
  const char *nm;
  if(si>=b->nstructs || depth>64U)return 0U;
  s=&b->structs[si];
  if(s->state==2U)return s->align;
  if(s->state==1U)return 0U;
  s->state=1U;
  for(i=0;i<s->nfield;i++) {
    f=&b->fields[s->first+i];nm=bc_name(b,f->name);
    if(!nm || f->type>=b->ntypes)return 0U;
    arr=bc_d_array(nm);
    if(!arr)return 0U;
    if(strchr(nm,'*')) {sz=(unsigned int)b->ptr_size;fa=sz;}
    else {
      sz=b->type_len[f->type];
      child=bc_d_find_type(b,f->type);
      if(child>=0){fa=bc_d_layout(b,(unsigned int)child,depth+1U);}
      else {fa=sz<=1U?1U:(sz<=2U?2U:(sz<=4U?4U:8U));}
      if(!fa)return 0U;
    }
    if(fa>sa)sa=fa;
    align=bc_d_align(cur,fa);
    if(align>16777216U || sz>16777216U/arr)return 0U;
    size=sz*arr;
    if(align>16777216U-size)return 0U;
    f->offset=align;cur=align+size;
  }
  s->align=sa;
  s->valid=(bc_d_align(cur,sa)==(unsigned int)b->type_len[s->type]);
  s->state=2U;
  return sa;
}
int bc_dna_parse(BC_File *b)
{
  unsigned int pos=0U,n,i,j,ty,nm,offset;
  if(!b->dna_bytes)return BC_FAILURE;
  if(bc_d_tag(b,&pos,"SDNA")<0||bc_d_tag(b,&pos,"NAME")<0||bc_d_u32(b,&pos,&n)<0||n>BC_MAX_NAMES)return BC_FAILURE;
  b->nnames=n;
  for(i=0;i<n;i++) {b->name_pos[i]=pos;if(bc_d_string(b,&pos)<0)return BC_FAILURE;}
  pos=bc_d_align(pos,4U);
  if(bc_d_tag(b,&pos,"TYPE")<0||bc_d_u32(b,&pos,&n)<0||n>BC_MAX_TYPES)return BC_FAILURE;
  b->ntypes=n;
  for(i=0;i<n;i++) {b->type_pos[i]=pos;if(bc_d_string(b,&pos)<0)return BC_FAILURE;}
  pos=bc_d_align(pos,4U);
  if(bc_d_tag(b,&pos,"TLEN")<0 || n>(b->dna_bytes-pos)/2U)return BC_FAILURE;
  for(i=0;i<n;i++) {if(bc_d_u16(b,&pos,&ty)<0)return BC_FAILURE;b->type_len[i]=(unsigned short)ty;}
  pos=bc_d_align(pos,4U);
  if(bc_d_tag(b,&pos,"STRC")<0||bc_d_u32(b,&pos,&n)<0||n>BC_MAX_STRUCTS)return BC_FAILURE;
  b->nstructs=n;
  for(i=0;i<n;i++) {
    if(bc_d_u16(b,&pos,&ty)<0||bc_d_u16(b,&pos,&nm)<0||ty>=b->ntypes || b->nfields+nm>BC_MAX_FIELDS)return BC_FAILURE;
    b->structs[i].type=(unsigned short)ty;
    b->structs[i].nfield=(unsigned short)nm;
    b->structs[i].first=b->nfields;
    for(j=0;j<nm;j++) {
      if(bc_d_u16(b,&pos,&ty)<0||bc_d_u16(b,&pos,&offset)<0||ty>=b->ntypes||offset>=b->nnames)return BC_FAILURE;
      b->fields[b->nfields].type=(unsigned short)ty;
      b->fields[b->nfields].name=(unsigned short)offset;
      ++b->nfields;
    }
  }
  for(i=0;i<b->nstructs;i++)(void)bc_d_layout(b,i,0U);
  return BC_OK;
}
static int bc_d_eqname(const char *s,const char *field)
{
  /* NAME may be '*ptr', 'field[3]', or '(*func)'. Compare base identifiers. */
  const char *p=s;
  while(*p=='*'||*p=='(')++p;
  while(*field && *p==*field){++p;++field;}
  return *field==0 && (*p==0||*p=='['||*p==')');
}
const BC_Struct *bc_get_struct(const BC_File *b,const char *name)
{
  unsigned int i;
  for(i=0;i<b->nstructs;i++)if(strcmp(bc_type(b,b->structs[i].type),name)==0)return &b->structs[i];
  return NULL;
}
const BC_Field *bc_get_field(const BC_File *b,const char *sname,const char *field)
{
  const BC_Struct *s=bc_get_struct(b,sname);
  unsigned int i;
  if(!s||!s->valid)return NULL;
  for(i=0;i<s->nfield;i++)if(bc_d_eqname(bc_name(b,b->fields[s->first+i].name),field))return &b->fields[s->first+i];
  return NULL;
}
int bc_read_struct_word(BC_File *b,unsigned int base,const char *structure,const char *field,unsigned int *out)
{
  const BC_Field *f=bc_get_field(b,structure,field);
  if(!f || base>2147483647U-f->offset)return BC_FAILURE;
  return bc_read_word(b,base+f->offset,out);
}
int bc_read_struct_ptr(BC_File *b,unsigned int base,const char *structure,const char *field,BC_U64 *out)
{
  const BC_Field *f=bc_get_field(b,structure,field);
  if(!f || base>2147483647U-f->offset || !strchr(bc_name(b,f->name),'*'))return BC_FAILURE;
  return bc_read_pointer(b,base+f->offset,out);
}
