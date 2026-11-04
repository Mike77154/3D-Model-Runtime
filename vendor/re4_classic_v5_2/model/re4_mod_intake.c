/* RE4 external mod asset declaration parser. Strict C89; no IO or heap. */
#include "re4c3d/re4_mod_intake.h"
#include <string.h>
static int fold(int c){return c>='A'&&c<='Z'?c+('a'-'A'):c;}
static int eq(const char *a,const char *b){int x,y;do{x=fold((unsigned char)*a++);y=fold((unsigned char)*b++);if(x!=y)return 0;}while(x);return 1;}
static int ext(const char *s,const char *e){size_t n=strlen(s),m=strlen(e);return n>=m&&eq(s+n-m,e);}
int re4_mod_path_safe(const char *s)
{
 const char *start,*p;size_t n=0U,seg;
 if(!s||!s[0]||s[0]=='/'||s[0]=='\\')return 0;
 start=s;
 for(p=s;;++p){
  unsigned char c=(unsigned char)*p;
  if(c==0 || c=='/' || c=='\\'){
   seg=(size_t)(p-start);
   if(seg==0U || (seg==1U&&start[0]=='.') ||
      (seg==2U&&start[0]=='.'&&start[1]=='.'))return 0;
   if(c==0)break;
   start=p+1;
  }else{
   if(c<33U || c>126U || c==':' || c=='*'|| c=='?' || c=='"' ||
      c=='<' || c=='>' || c=='|' || c=='=' || c=='#')return 0;
  }
  if(++n>=RE4_MOD_PATH_CAP)return 0;
 }
 return 1;
}
static int copy_path(char *out,const char *s)
{
 size_t i=0U;
 if(!re4_mod_path_safe(s))return 0;
 for(;s[i];++i)out[i]=s[i]=='\\'?'/':s[i];
 out[i]=0;return 1;
}
static int already(const re4_mod_package *p,const char *s)
{
 re4_u32 i;
 if(eq(p->master.path,s))return 1;
 for(i=0U;i<p->part_count;++i)if(eq(p->parts[i].path,s))return 1;
 for(i=0U;i<p->clip_count;++i)if(eq(p->clips[i].path,s))return 1;
 for(i=0U;i<p->texture_count;++i)if(eq(p->textures[i].path,s))return 1;
 return 0;
}
static int valid_extensions(const re4_mod_package *p)
{
 re4_u32 i;
 if(p->format==RE4_MOD_PMD2007){
  if(!ext(p->master.path,".pmd"))return 0;
  for(i=0U;i<p->part_count;++i)if(!ext(p->parts[i].path,".pmd"))return 0;
  for(i=0U;i<p->texture_count;++i)if(!ext(p->textures[i].path,".tga"))return 0;
 }else if(p->format==RE4_MOD_BIN_UHD){
  if(!ext(p->master.path,".bin"))return 0;
  for(i=0U;i<p->part_count;++i)if(!ext(p->parts[i].path,".bin"))return 0;
  for(i=0U;i<p->texture_count;++i)
   if(!ext(p->textures[i].path,".tpl") && !ext(p->textures[i].path,".dds") &&
      !ext(p->textures[i].path,".tga"))return 0;
 }else return 0;
 for(i=0U;i<p->clip_count;++i)if(!ext(p->clips[i].path,".fcv"))return 0;
 return 1;
}
re4_err re4_mod_parse(re4_span text,re4_mod_package *out)
{
 re4_size pos=0U,start;re4_u32 line_no=0U,header_seen=0U;char row[256],*equal,*val;
 size_t n,i;
 if(!out||!text.data)return RE4_ERR_NULL;
 memset(out,0,sizeof(*out));
 if(text.size>16384UL)return RE4_ERR_RANGE;
 while(pos<text.size){
  start=pos;
  while(pos<text.size && text.data[pos]!='\n' && text.data[pos]!='\r'){
   if(text.data[pos]==0U)return RE4_ERR_BAD_FORMAT;
   ++pos;
  }
  n=(size_t)(pos-start);
  if(n>=sizeof(row))return RE4_ERR_RANGE;
  memcpy(row,text.data+start,n);row[n]=0;
  if(pos<text.size && text.data[pos]=='\r')++pos;
  if(pos<text.size && text.data[pos]=='\n')++pos;
  ++line_no;
  if(n==0U || row[0]=='#')continue;
  if(!header_seen){if(!eq(row,RE4_MOD_HEADER))return RE4_ERR_BAD_FORMAT;header_seen=1U;continue;}
  equal=strchr(row,'=');
  if(!equal || equal==row)return RE4_ERR_BAD_FORMAT;
  *equal=0;val=equal+1;
  if(!val[0]||strchr(val,'='))return RE4_ERR_BAD_FORMAT;
  if(eq(row,"format")){
   if(out->format)return RE4_ERR_BAD_FORMAT;
   if(eq(val,"pmd2007"))out->format=RE4_MOD_PMD2007;
   else if(eq(val,"bin-uhd"))out->format=RE4_MOD_BIN_UHD;
   else return RE4_ERR_UNSUPPORTED;
  }else if(eq(row,"master")){
   if(out->master.path[0] || already(out,val) || !copy_path(out->master.path,val))return RE4_ERR_BAD_FORMAT;
  }else if(eq(row,"part")){
   if(out->part_count>=RE4_MOD_MAX_PARTS||already(out,val))return RE4_ERR_BAD_FORMAT;
   if(!copy_path(out->parts[out->part_count].path,val))return RE4_ERR_BAD_FORMAT;
   ++out->part_count;
  }else if(eq(row,"clip")){
   if(out->clip_count>=RE4_MOD_MAX_CLIPS||already(out,val))return RE4_ERR_BAD_FORMAT;
   if(!copy_path(out->clips[out->clip_count].path,val))return RE4_ERR_BAD_FORMAT;
   ++out->clip_count;
  }else if(eq(row,"texture")){
   if(out->texture_count>=RE4_MOD_MAX_TEXTURES||already(out,val))return RE4_ERR_BAD_FORMAT;
   if(!copy_path(out->textures[out->texture_count].path,val))return RE4_ERR_BAD_FORMAT;
   ++out->texture_count;
  }else return RE4_ERR_BAD_FORMAT;
 }
 if(!header_seen || !out->master.path[0] || !out->format)return RE4_ERR_BAD_FORMAT;
 if(!valid_extensions(out))return RE4_ERR_BAD_FORMAT;
 /* Disallow mixed-case aliases and path separator aliases to same file. */
 for(i=0U;i<out->part_count;++i)
  if(eq(out->parts[i].path,out->master.path))return RE4_ERR_BAD_FORMAT;
 return RE4_OK;
}
