/* Command-line corpus validator; no input from standard input. */
#include "dts89_shaderer.h"
#include <stdio.h>
#define AUDIT_DTS_MAX (32u*1024u*1024u)
#define AUDIT_CS_MAX (512u*1024u)
#define AUDIT_ARENA_MAX (64u*1024u*1024u)
static dts89_u8 dts_data[AUDIT_DTS_MAX];
static dts89_u8 cs_data[AUDIT_CS_MAX];
static dts89_u8 arena_buffer[AUDIT_ARENA_MAX];
static int file_bytes(const char *path,dts89_u8 *buf,dts89_u32 cap,dts89_u32 *size)
{
 FILE *f;long n;
 f=fopen(path,"rb");if(!f)return 1;
 if(fseek(f,0,SEEK_END)){fclose(f);return 1;}
 n=ftell(f);if(n<0 || (unsigned long)n>cap){fclose(f);return 1;}
 rewind(f);
 if(fread(buf,1,(size_t)n,f)!=(size_t)n){fclose(f);return 1;}
 fclose(f);*size=(dts89_u32)n;return 0;
}
static void print_span(dts89_string s)
{
 dts89_u32 i;
 if(!s.ptr){fputs("(none)",stdout);return;}
 for(i=0;i<s.len && i<200u;++i){
  unsigned char c=(unsigned char)s.ptr[i];
  fputc(c>=32u && c<127u ? (int)c : '?',stdout);
 }
}
int main(int argc,char **argv)
{
 dts89_u32 size,script_size=0u,matched=0u,maps=0u,unsupported=0u;
 dts89_shape shape;dts89_ts_file script;
 dts89_arena a;dts89_shaderer_material m;
 dts89_s32 i;dts89_u32 j;int rc;
 if(argc!=3){fputs("usage: dts89_shaderer_audit model.dts materials.cs\n",stderr);return 2;}
 if(file_bytes(argv[1],dts_data,AUDIT_DTS_MAX,&size)){
  fputs("model read failed\n",stderr);return 2;
 }
 if(file_bytes(argv[2],cs_data,AUDIT_CS_MAX,&script_size)){
  fputs("script read failed\n",stderr);return 2;
 }
 dts89_arena_init(&a,arena_buffer,AUDIT_ARENA_MAX);
 rc=dts89_decode_shape(dts_data,size,&a,&shape);
 if(rc){fprintf(stderr,"DTS decode error=%d\n",rc);return 1;}
 rc=dts89_tscript_parse_singletons(cs_data,script_size,&a,&script);
 if(rc){fprintf(stderr,"script decode error=%d\n",rc);return 1;}
 printf("SHADERER AUDIT: %s\nDTS materials=%d script objects=%d\n",argv[1],
    shape.num_materials,script.num_objects);
 for(i=0;i<shape.num_materials;++i){
  rc=dts89_shaderer_build(&shape,i,&script,&m);
  if(rc){fprintf(stderr,"material %d failed %d\n",i,rc);return 1;}
  matched+=m.script_matched;
  if(m.unsupported_features)unsupported++;
  printf("[%d] ",i);print_span(m.name);
  printf(" script=%lu wrapS=%lu wrapT=%lu mip=%lu blend=%lu alphaTest=%lu twoSided=%lu unsupported=0x%08lx\n",
    (unsigned long)m.script_matched,(unsigned long)m.sampler_s,
    (unsigned long)m.sampler_t,(unsigned long)m.mipmap,(unsigned long)m.blend,
    (unsigned long)m.alpha_test,(unsigned long)m.double_sided,(unsigned long)m.unsupported_features);
  for(j=0;j<DTS89_SHADERER_STAGES;++j){
   if(!m.stage[j].maps)continue;
   printf("  stage %lu flags=0x%08lx diffuse=",(unsigned long)j,(unsigned long)m.stage[j].maps);
   print_span(m.stage[j].diffuse);fputs(" normal=",stdout);print_span(m.stage[j].normal);
   fputs(" specular=",stdout);print_span(m.stage[j].specular);
   fputc('\n',stdout);maps++;
  }
 }
 printf("SUMMARY linked=%lu/%d mappedStages=%lu flaggedAdvanced=%lu\n",
 (unsigned long)matched,shape.num_materials,(unsigned long)maps,(unsigned long)unsupported);
 return matched==0u?3:0;
}
