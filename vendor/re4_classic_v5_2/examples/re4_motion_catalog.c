/* Per-FCV motion role catalog for RE4 PC 2007 nested DAT containers.
   Only reads original binary, never modifies it. C89, fixed memory. */
#include "re4c3d/re4c3d.h"
#include "re4c3d/re4_motion_audit.h"
#include <stdio.h>
#include <string.h>
#define DAT_MAX (48UL*1024UL*1024UL)
static re4_u8 payload[DAT_MAX];
static re4_u8 scratch_data[8UL*1024UL*1024UL];
int main(int argc,char **argv)
{
 FILE *fp;long n;re4_span dat,chunk;re4_arena scratch;re4_archive archive;
 re4_err er;re4_u32 i,good=0,empty=0,broken=0;
 if(argc!=2){fputs("Usage: re4_motion_catalog package.dat\n",stderr);return 2;}
 fp=fopen(argv[1],"rb");if(!fp){perror(argv[1]);return 2;}
 if(fseek(fp,0,SEEK_END)!=0){fclose(fp);return 2;}
 n=ftell(fp);if(n<=0||((unsigned long)n)>DAT_MAX){fclose(fp);fputs("DAT too large or empty\n",stderr);return 2;}
 rewind(fp);if(fread(payload,1,(size_t)n,fp)!=(size_t)n){fclose(fp);return 2;}fclose(fp);
 dat.data=payload;dat.size=(re4_size)n;
 re4_arena_init(&scratch,scratch_data,sizeof(scratch_data));
 er=re4_dat_read_indexed(dat,RE4_ENDIAN_LITTLE,0,&scratch,&archive);
 if(er!=RE4_OK){fprintf(stderr,"DAT index: %s\n",re4_err_name(er));return 2;}
 puts("index,size,status,max_frame,blocks,keys,rotation,root,ik,other_special,unknown");
 for(i=0;i<archive.entry_count;++i){
  re4_motion_audit report;
  re4_archive_entry *entry=&archive.entries[i];
  if(entry->name.len!=4UL||memcmp(entry->name.ptr,"FCV",3)!=0)continue;
  er=re4_archive_entry_span(dat,entry,&chunk);
  if(er!=RE4_OK){printf("%lu,0,ENTRY_ERROR,0,0,0,0,0,0,0,0\n",(unsigned long)i);++broken;continue;}
  if(chunk.size<16UL||chunk.data[2]==0){printf("%lu,%lu,EMPTY,0,0,0,0,0,0,0,0\n",(unsigned long)i,(unsigned long)chunk.size);++empty;continue;}
  er=re4_motion_audit_fcv(chunk,RE4_ENDIAN_LITTLE,&scratch,&report);
  if(er!=RE4_OK){printf("%lu,%lu,%s,0,0,0,0,0,0,0,0\n",(unsigned long)i,(unsigned long)chunk.size,re4_err_name(er));++broken;continue;}
  printf("%lu,%lu,OK,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
   (unsigned long)i,(unsigned long)chunk.size,(unsigned long)report.max_frame,
   (unsigned long)report.blocks,(unsigned long)report.keys,(unsigned long)report.rotation_blocks,
   (unsigned long)report.root_blocks,(unsigned long)report.ik_blocks,
   (unsigned long)report.other_special_blocks,(unsigned long)report.unknown_blocks);
  ++good;
 }
 fprintf(stderr,"FCV roles: valid=%lu empty=%lu malformed=%lu\n",
  (unsigned long)good,(unsigned long)empty,(unsigned long)broken);
 return broken?1:0;
}
