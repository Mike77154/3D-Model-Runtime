/* Offline diagnostic for user-authored RE4 mod models. C89/no heap.
   Strictly validates paths, file presence, PMD/BIN parsing and FCV rotation
   coverage. No repacking, arbitrary file execution or game modifications. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define MANIFEST_CAP 16384UL
#define PMD_CAP (16UL*1024UL*1024UL)
#define FCV_CAP (2UL*1024UL*1024UL)
#define ARENA_CAP (20UL*1024UL*1024UL)
static re4_u8 manifest_bytes[MANIFEST_CAP], model_bytes[PMD_CAP],
 part_bytes[PMD_CAP],clip_bytes[FCV_CAP], arena_bytes[ARENA_CAP],
 part_arena_bytes[ARENA_CAP];
static re4_pmd_model model;
static re4_arena arena;
static int load(const char *path,re4_u8 *mem,unsigned long cap,re4_span *out)
{
 FILE *f;long n;
 f=fopen(path,"rb");if(!f)return 0;
 if(fseek(f,0L,SEEK_END)!=0 || (n=ftell(f))<0 ||
    (unsigned long)n>cap || fseek(f,0L,SEEK_SET)!=0){fclose(f);return 0;}
 if(fread(mem,1U,(size_t)n,f)!=(size_t)n){fclose(f);return 0;}
 fclose(f);out->data=mem;out->size=(re4_size)n;return 1;
}
static int join(char *out,size_t cap,const char *dir,const char *rel)
{
 size_t a=strlen(dir),b=strlen(rel);
 if(!re4_mod_path_safe(rel) || a+b+2U>cap)return 0;
 memcpy(out,dir,a);out[a]='/';memcpy(out+a+1U,rel,b+1U);return 1;
}
static int present(const char *base,const char *path)
{
 char full[1024];FILE *f;
 if(!join(full,sizeof(full),base,path))return 0;
 f=fopen(full,"rb");if(!f)return 0;fclose(f);return 1;
}
int main(int argc,char **argv)
{
 re4_span src,fs;re4_mod_package pkg;re4_err er;re4_u32 i;
 char file[1024];int issues=0;re4_clip_coverage cv;re4_fcv fcv;
 if(argc!=3){fputs("usage: re4_mod_probe MANIFEST.txt MOD_DIRECTORY\n",stderr);return 2;}
 if(!load(argv[1],manifest_bytes,MANIFEST_CAP,&src)){
  fputs("MANIFEST_IO_OR_TOO_LARGE\n",stderr);return 2;
 }
 er=re4_mod_parse(src,&pkg);
 if(er!=RE4_OK){printf("MANIFEST_REJECT,%s\n",re4_err_name(er));return 2;}
 printf("MOD_FORMAT,%s\n",pkg.format==RE4_MOD_PMD2007?"PMD2007":"BIN_UHD");
 printf("MODEL_PARTS,%lu\n",(unsigned long)pkg.part_count);
 printf("MODEL_CLIPS,%lu\n",(unsigned long)pkg.clip_count);
 if(!join(file,sizeof(file),argv[2],pkg.master.path)||
    !load(file,model_bytes,PMD_CAP,&fs)){
    fputs("MASTER_MISSING_OR_TOO_LARGE\n",stderr);return 3;
 }
 re4_arena_init(&arena,arena_bytes,ARENA_CAP);
 if(pkg.format==RE4_MOD_PMD2007){
   er=re4_pmd_read_survey(fs,&arena,&model);
   printf("MASTER,%s,%s",pkg.master.path,re4_err_name(er));
   if(er==RE4_OK)printf(",bones=%lu,vertices=%lu",(unsigned long)model.skeleton_count,(unsigned long)model.total_vertex_count_hint);
   putchar('\n');if(er!=RE4_OK)return 4;
 }else{
   re4_bin_model bm;
   er=re4_bin_model_read(fs,RE4_ENDIAN_LITTLE,&arena,&bm);
   printf("MASTER,%s,%s\n",pkg.master.path,re4_err_name(er));
   if(er!=RE4_OK)return 4;
 }
 for(i=0U;i<pkg.part_count;++i){
  const char *p=pkg.parts[i].path;re4_span ps;
  if(!join(file,sizeof(file),argv[2],p)||!load(file,part_bytes,PMD_CAP,&ps)){
   printf("PART,%s,FILE_MISSING_OR_TOO_LARGE\n",p);++issues;continue;
  }
  {re4_arena part_arena;
   re4_arena_init(&part_arena,part_arena_bytes,ARENA_CAP);
   if(pkg.format==RE4_MOD_PMD2007){
    re4_pmd_model pm;re4_character_links links;re4_character_link_audit audit;
    er=re4_pmd_read_survey(ps,&part_arena,&pm);
    if(er==RE4_OK)er=re4_character_link(&model,&pm,&links);
    if(er==RE4_OK)er=re4_character_audit(&model,&pm,&links,&audit);
    if(er!=RE4_OK){printf("PART,%s,PARSE_OR_LINK_ERROR_%s\n",p,re4_err_name(er));++issues;}
    else{
      printf("PART,%s,PARSED,direct=%lu,stationary=%lu,ancestry_conflicts=%lu\n",
        p,(unsigned long)audit.direct,(unsigned long)audit.stationary,
        (unsigned long)audit.ancestry_conflicts);
      if(re4_character_check_policy(&audit,RE4_PART_POLICY_STRICT)!=RE4_OK)++issues;
    }
   }else{
    re4_bin_model bm;
    er=re4_bin_model_read(ps,RE4_ENDIAN_LITTLE,&part_arena,&bm);
    printf("PART,%s,%s,ANIMATION_BINDING_UNVERIFIED\n",p,re4_err_name(er));
    if(er!=RE4_OK)++issues;
   }
  }
 }
 for(i=0U;i<pkg.clip_count;++i){
  re4_arena aa;static re4_u8 clip_arena_buf[1048576UL];
  const char *p=pkg.clips[i].path;
  if(!join(file,sizeof(file),argv[2],p)||!load(file,clip_bytes,FCV_CAP,&fs)){
   printf("CLIP,%s,FILE_MISSING_OR_TOO_LARGE\n",p);++issues;continue;
  }
  re4_arena_init(&aa,clip_arena_buf,sizeof(clip_arena_buf));
  if(re4_fcv_read(fs,RE4_ENDIAN_LITTLE,&aa,&fcv)!=RE4_OK ||
     re4_fcv_validate(fs,&fcv)!=RE4_OK){
   printf("CLIP,%s,FCV_INVALID\n",p);++issues;continue;
  }
  if(pkg.format==RE4_MOD_PMD2007 && er==RE4_OK){
   er=re4_character_clip_coverage(&model,&fcv,&cv);
   if(er!=RE4_OK){printf("CLIP,%s,COVERAGE_ERROR_%s\n",p,re4_err_name(er));++issues;continue;}
   printf("CLIP,%s,PARSED,rotation_linked=%lu,rotation_unmatched=%lu,special=%lu\n",
     p,(unsigned long)cv.linked_rotation_channels,
     (unsigned long)cv.unmatched_rotation_channels,
     (unsigned long)cv.special_channels);
   if(cv.rotation_channels && !cv.linked_rotation_channels)++issues;
  }else printf("CLIP,%s,PARSED,FAMILY_BINDING_NOT_VERIFIED\n",p);
 }
 for(i=0U;i<pkg.texture_count;++i){
  const char *p=pkg.textures[i].path;
  printf("TEXTURE,%s,%s\n",p,present(argv[2],p)?"EXISTS_UNDECODED":"MISSING");
  if(!present(argv[2],p))++issues;
 }
 printf("SUMMARY,%s,issues=%d\n",issues?"REVIEW_REQUIRED":"STRUCTURALLY_READABLE_NOT_CERTIFIED",issues);
 return issues?1:0;
}
