/* Scan nested RE4 PC 2007 DAT for valid FCV records (no heap). */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define BUF_SIZE (18UL*1024UL*1024UL)
static re4_u8 bin[BUF_SIZE];static re4_u8 memory[4UL*1024UL*1024UL];
int main(int argc,char **argv)
{
 FILE *f;re4_size size;re4_span d,s;re4_arena a;re4_archive arc;re4_fcv clip;
 re4_u32 i,good=0,bad=0,placeholder=0,unknown=0;re4_err er;
 if(argc!=2){fprintf(stderr,"Usage: re4_fcv_scan archive.dat\n");return 2;}
 f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
 if(fseek(f,0,SEEK_END)){fclose(f);return 2;}size=(re4_size)ftell(f);rewind(f);
 if(size>sizeof(bin)){fclose(f);return 2;}
 if(fread(bin,1,(size_t)size,f)!=(size_t)size){fclose(f);return 2;}fclose(f);
 d.data=bin;d.size=size;re4_arena_init(&a,memory,sizeof(memory));
 er=re4_dat_read_indexed(d,RE4_ENDIAN_LITTLE,0,&a,&arc);
 if(er!=RE4_OK){fprintf(stderr,"DAT index failed: %s\n",re4_err_name(er));return 2;}
 for(i=0;i<arc.entry_count;++i){
  if(arc.entries[i].name.len!=4UL || memcmp(arc.entries[i].name.ptr,"FCV",3))continue;
  er=re4_archive_entry_span(d,&arc.entries[i],&s);if(er!=RE4_OK){++bad;continue;}
  if(s.size<16UL || s.data[2]==0){++placeholder;continue;}
  {re4_size mark=a.pos;
   er=re4_fcv_read(s,RE4_ENDIAN_LITTLE,&a,&clip);
   if(er==RE4_OK)er=re4_fcv_validate(s,&clip);
   if(er==RE4_OK)++good;else if(er==RE4_ERR_UNSUPPORTED)++unknown;else{++bad;printf("index=%lu size=%lu error=%s\n",(unsigned long)i,(unsigned long)s.size,re4_err_name(er));}
   a.pos=mark;
  }
 }
 printf("FCV scan: valid=%lu placeholders=%lu unsupported=%lu malformed=%lu\n",(unsigned long)good,(unsigned long)placeholder,(unsigned long)unknown,(unsigned long)bad);
 return bad?1:0;
}
