/* Extract FCV archives by numbered DAT index. C89, bounded caller-owned memory.
   Original files remain on the user's machine, never bundled in code ZIP. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p,0775)
#endif
#define DAT_CAP (24UL*1024UL*1024UL)
#define ARENA_CAP (4UL*1024UL*1024UL)
static re4_u8 dat_mem[DAT_CAP],arena_mem[ARENA_CAP];
int main(int argc,char **argv)
{
    FILE *f,*out,*manifest;long n;
    re4_span file,inner;re4_archive ar;re4_arena arena;re4_err e;
    re4_u32 i,ok=0,bad=0,empty=0;re4_fcv clip;re4_size mark;
    char name[1024];
    if(argc!=3||strlen(argv[2])+50U>=sizeof(name)){
        fputs("Usage: re4_fcv_extract NESTED.dat OUTDIR\n",stderr);return 2;
    }
    f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    if(fseek(f,0,SEEK_END)!=0||(n=ftell(f))<0||(unsigned long)n>DAT_CAP||fseek(f,0,SEEK_SET)!=0 ||
       fread(dat_mem,1,(size_t)n,f)!=(size_t)n){fclose(f);return 2;}
    fclose(f);file.data=dat_mem;file.size=(re4_size)n;
    re4_arena_init(&arena,arena_mem,ARENA_CAP);
    e=re4_dat_read_indexed(file,RE4_ENDIAN_LITTLE,RE4_FALSE,&arena,&ar);
    if(e!=RE4_OK){fprintf(stderr,"DAT: %s\n",re4_err_name(e));return 2;}
    MKDIR(argv[2]);
    sprintf(name,"%s/catalog.csv",argv[2]);
    manifest=fopen(name,"wb");if(!manifest){perror(name);return 2;}
    fputs("dat_index,size,status,max_frame,channels\n",manifest);
    mark=arena.pos;
    for(i=0U;i<ar.entry_count;++i){
        if(ar.entries[i].name.len!=4UL||memcmp(ar.entries[i].name.ptr,"FCV",3UL)!=0)continue;
        e=re4_archive_entry_span(file,&ar.entries[i],&inner);
        if(e!=RE4_OK){++bad;fprintf(manifest,"%lu,0,entry_error,0,0\n",(unsigned long)i);continue;}
        if(inner.size<16UL||inner.data[2]==0U){
            ++empty;fprintf(manifest,"%lu,%lu,empty,0,0\n",(unsigned long)i,(unsigned long)inner.size);continue;
        }
        arena.pos=mark;
        e=re4_fcv_read(inner,RE4_ENDIAN_LITTLE,&arena,&clip);
        if(e==RE4_OK)e=re4_fcv_validate(inner,&clip);
        if(e!=RE4_OK){
            ++bad;fprintf(manifest,"%lu,%lu,%s,0,0\n",(unsigned long)i,(unsigned long)inner.size,re4_err_name(e));continue;
        }
        sprintf(name,"%s/clip_%04lu.fcv",argv[2],(unsigned long)i);
        out=fopen(name,"wb");if(!out){fclose(manifest);return 3;}
        if(fwrite(inner.data,1,(size_t)inner.size,out)!=(size_t)inner.size||fclose(out)!=0){
            fclose(manifest);return 3;
        }
        ++ok;fprintf(manifest,"%lu,%lu,ok,%u,%u\n",(unsigned long)i,(unsigned long)inner.size,
            (unsigned)clip.max_time,(unsigned)clip.block_count);
    }
    fclose(manifest);
    printf("FCV EXTRACT: valid=%lu empty=%lu failed=%lu\n",(unsigned long)ok,(unsigned long)empty,(unsigned long)bad);
    return bad?1:0;
}
