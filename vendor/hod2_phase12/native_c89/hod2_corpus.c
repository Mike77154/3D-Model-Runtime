/* HOD2 corpus audit and bytewise version comparison. Strict C89; no heap, no floats.
 * The POL/MOT/TEX banks are user-supplied; no EXE tables are assumed. */
#include "hod2_native.h"
#include <string.h>
#include <limits.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#define HC_CAT_COUNT 6U
#define HC_MAX_NAME 256U
#define HC_COPY 16384U
static unsigned char hc_bufa[HC_COPY],hc_bufb[HC_COPY];
static const char *hc_cats[HC_CAT_COUNT]={"pol","tex","mot","cam","evt","coli"};
static unsigned long hc_seen,hc_error,hc_identical,hc_different,hc_only_a,hc_only_b;

typedef int (*HCVisit)(const char *,const char *,const char *,void *);

static int hc_join(char *out,size_t cap,const char *a,const char *b){
 size_t l,r;if(!a||!b)return 0;l=strlen(a);r=strlen(b);
 if(l+r+2U>cap)return 0;
 memcpy(out,a,l);
 if(l && a[l-1U]!='/' && a[l-1U]!='\\')out[l++]='/';
 memcpy(out+l,b,r+1U);return 1;
}
static int hc_bin(const char *name){size_t n=strlen(name);return n>4U &&
 name[n-4U]=='.' && (name[n-3U]=='b'||name[n-3U]=='B') &&
 (name[n-2U]=='i'||name[n-2U]=='I') && (name[n-1U]=='n'||name[n-1U]=='N');}
static int hc_each(const char *root,const char *cat,HCVisit fn,void *ctx){
 char dir[HOD2_MAX_PATH];int good=1;
 if(!hc_join(dir,sizeof(dir),root,cat))return 0;
#ifdef _WIN32
 {WIN32_FIND_DATAA data;HANDLE h;char pattern[HOD2_MAX_PATH];
  if(!hc_join(pattern,sizeof(pattern),dir,"*.bin"))return 0;
  h=FindFirstFileA(pattern,&data);
  if(h==INVALID_HANDLE_VALUE)return 0;
  do{if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && hc_bin(data.cFileName)){
      if(!fn(root,cat,data.cFileName,ctx))good=0;
   }}while(FindNextFileA(h,&data));
  FindClose(h);
 }
#else
 {DIR *d;struct dirent *e;d=opendir(dir);if(!d)return 0;
  while((e=readdir(d))!=0){
   if(hc_bin(e->d_name) && !fn(root,cat,e->d_name,ctx))good=0;
  }
  closedir(d);
 }
#endif
 return good;
}
static int hc_path(char *out,const char *root,const char *cat,const char *name){
 char dir[HOD2_MAX_PATH];return hc_join(dir,sizeof(dir),root,cat) && hc_join(out,HOD2_MAX_PATH,dir,name);
}
static int hc_compare_file(const char *one,const char *two){
 FILE *a,*b;size_t n,m;int same=1;
 a=fopen(one,"rb");b=fopen(two,"rb");
 if(!a||!b){if(a)fclose(a);if(b)fclose(b);return -1;}
 do{n=fread(hc_bufa,1,HC_COPY,a);m=fread(hc_bufb,1,HC_COPY,b);
  if(n!=m || memcmp(hc_bufa,hc_bufb,n)!=0){same=0;break;}
 }while(n==HC_COPY);
 if(ferror(a)||ferror(b))same=-1;
 fclose(a);fclose(b);return same;
}
typedef struct HCCompare {FILE *f;const char *other;int reverse;} HCCompare;
static int hc_cmp_visit(const char *root,const char *cat,const char *name,void *user){
 HCCompare *c=(HCCompare*)user;char p[HOD2_MAX_PATH],q[HOD2_MAX_PATH];int same;
 if(!hc_path(p,root,cat,name)||!hc_path(q,c->other,cat,name))return 0;
 same=hc_compare_file(p,q);
 if(same<0){
  if(c->reverse){hc_only_b++;fprintf(c->f,"%s,%s,ONLY_B\n",cat,name);} 
  else{hc_only_a++;fprintf(c->f,"%s,%s,ONLY_A\n",cat,name);}
 }else if(!c->reverse){
  if(same){hc_identical++;fprintf(c->f,"%s,%s,IDENTICAL\n",cat,name);}
  else{hc_different++;fprintf(c->f,"%s,%s,DIFFERENT\n",cat,name);}
 }
 return 1;
}
int hod2_corpus_diff(const char *root_a,const char *root_b,const char *csv){
 FILE *f;HCCompare c;unsigned int i;int ok=1;
 if(!root_a||!root_b||!csv)return 0;
 f=fopen(csv,"wb");if(!f)return 0;
 hc_identical=hc_different=hc_only_a=hc_only_b=0UL;
 fputs("category,bank,status\n",f);
 c.f=f;c.other=root_b;c.reverse=0;
 for(i=0U;i<HC_CAT_COUNT;i++)if(!hc_each(root_a,hc_cats[i],hc_cmp_visit,&c))ok=0;
 c.other=root_a;c.reverse=1;
 for(i=0U;i<HC_CAT_COUNT;i++)if(!hc_each(root_b,hc_cats[i],hc_cmp_visit,&c))ok=0;
 if(fclose(f))ok=0;
 printf("CORPUS DIFF identical=%lu different=%lu only_A=%lu only_B=%lu\n",hc_identical,hc_different,hc_only_a,hc_only_b);
 return ok;
}
static int hc_mot_check(const unsigned char *p,size_t n,unsigned long *blocks){
 unsigned long base,off,next;unsigned int i,count;
 if(n<8U)return 0;
 base=hod2_u32(p);
 if(base<4UL || (base&3UL)!=0UL || base>(unsigned long)n)return 0;
 count=(unsigned int)(base/4UL);if(count>4096U)return 0;
 off=base;
 for(i=0U;i<count;i++){
  next=(i+1U<count)?hod2_u32(p+(unsigned long)(i+1U)*4UL):(unsigned long)n;
  if(hod2_u32(p+(unsigned long)i*4UL)!=off || next<off || next>(unsigned long)n)return 0;
  off=next;
 }
 *blocks=count;return 1;
}
typedef struct HCAudit {FILE *f;} HCAudit;
static int hc_audit_visit(const char *root,const char *cat,const char *name,void *user){
 HCAudit *c=(HCAudit*)user;char p[HOD2_MAX_PATH];size_t n,dn;unsigned long total=0UL,mat=0UL,tri=0UL,uv=0UL,blocks=0UL,r,e;
 unsigned int cnt=0U,i;int ok=1,isprs=0;Hod2PolyStat stat;
 if(!hc_path(p,root,cat,name))return 0;
 if(!hod2_read_file(p,hod2_workspace_raw,HOD2_FILE_CAP,&n)){
  fprintf(c->f,"%s,%s,0,0,0,0,0,0,0,0,READ_ERROR\n",cat,name);hc_error++;return 1;
 }
 if(!hod2_file_decode(hod2_workspace_raw,n,hod2_workspace_decoded,HOD2_FILE_CAP,&dn)){
  fprintf(c->f,"%s,%s,%lu,0,0,0,0,0,0,0,DECODE_ERROR\n",cat,name,(unsigned long)n);hc_error++;return 1;
 }
 isprs=(dn!=n);
 if(strcmp(cat,"pol")==0){
  if(!hod2_pol_model(hod2_workspace_decoded,dn,0U,&r,&e,&cnt))ok=0;
  else for(i=0U;i<cnt;i++){
   if(!hod2_pol_model(hod2_workspace_decoded,dn,i,&r,&e,(unsigned int*)0)||
      !hod2_pol_scan(hod2_workspace_decoded,dn,r,e,&stat,(Hod2VertexCb)0,(void*)0)){
    ok=0;break;
   }
   total++;mat+=stat.materials;tri+=stat.triangles;uv+=stat.nonfinite_uv;
  }
 }else if(strcmp(cat,"mot")==0)ok=hc_mot_check(hod2_workspace_decoded,dn,&blocks);
 if(!ok)hc_error++;
 hc_seen++;
 fprintf(c->f,"%s,%s,%lu,%lu,%d,%lu,%lu,%lu,%lu,%lu,%s\n",cat,name,
   (unsigned long)n,(unsigned long)dn,isprs,total,mat,tri,uv,blocks,ok?"OK":"UNSUPPORTED_OR_MALFORMED");
 return 1;
}
int hod2_corpus_audit(const char *root,const char *csv){
 FILE *f;HCAudit c;unsigned int i;int ok=1;
 if(!root||!csv)return 0;
 f=fopen(csv,"wb");if(!f)return 0;
 c.f=f;hc_seen=hc_error=0UL;
 fputs("category,bank,raw_bytes,decoded_bytes,prs_inferred,models,materials,triangles,bad_uv,mot_blocks,status\n",f);
 for(i=0U;i<HC_CAT_COUNT;i++)if(!hc_each(root,hc_cats[i],hc_audit_visit,&c))ok=0;
 if(fclose(f))ok=0;
 printf("CORPUS AUDIT inspected=%lu unsupported_or_malformed=%lu\n",hc_seen,hc_error);
 return ok;
}
