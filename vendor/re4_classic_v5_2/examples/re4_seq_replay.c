/* RE4 SEQ raw event replay: an explicitly selected nested DAT index.
   IMPORTANT: SEQ index is not inferred from FCV index, and this tool emits
   raw numeric event codes without claiming their gameplay meaning. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#define DAT_CAP (48UL*1024UL*1024UL)
#define ARENA_CAP (8UL*1024UL*1024UL)
static re4_u8 data[DAT_CAP],arena_buffer[ARENA_CAP];
static int number(const char *s,re4_u32 *out)
{
    unsigned long n;char *end;
    if(!s || !*s || *s=='-' || *s=='+')return 0;
    errno=0;n=strtoul(s,&end,10);
    if(end==s || *end || errno==ERANGE || n>(unsigned long)RE4_U32_MAX)return 0;
    *out=(re4_u32)n;return 1;
}
static int getfile(const char *name,re4_span *out)
{
    FILE *f;long sz;
    f=fopen(name,"rb");if(!f)return 0;
    if(fseek(f,0L,SEEK_END)!=0 || (sz=ftell(f))<=0L ||
       (unsigned long)sz>DAT_CAP || fseek(f,0L,SEEK_SET)!=0){fclose(f);return 0;}
    if(fread(data,1U,(size_t)sz,f)!=(size_t)sz){fclose(f);return 0;}
    fclose(f);out->data=data;out->size=(re4_size)sz;return 1;
}
static re4_err print_event(void *user,const re4_seq_fire *ev)
{
    FILE *out=(FILE *)user;
    if(fprintf(out,"%lu,%lu,%lu,%u,%u,%u\n",
       (unsigned long)ev->absolute_q6,(unsigned long)ev->loop_index,
       (unsigned long)ev->key_index,(unsigned)ev->key.frame_q6,
       (unsigned)ev->key.se,(unsigned)ev->key.flags)<0)return RE4_ERR_BAD_FORMAT;
    return RE4_OK;
}
int main(int argc,char **argv)
{
    re4_span dat,chunk;re4_arena arena;re4_archive archive;
    re4_seq_timeline seq;re4_seq_observation ob;re4_u32 idx,duration,start,end,count;
    re4_err e;re4_bool all=RE4_FALSE;
    if(argc!=6 && argc!=7){
        fputs("usage: re4_seq_replay NESTED.DAT SEQ_INDEX DURATION_Q6 FROM_Q6 TO_Q6 [--all-keys]\n",stderr);
        return 2;
    }
    if(argc==7){if(strcmp(argv[6],"--all-keys")!=0)return 2;all=RE4_TRUE;}
    if(!number(argv[2],&idx)||!number(argv[3],&duration)||
       !number(argv[4],&start)||!number(argv[5],&end))return 2;
    if(!getfile(argv[1],&dat)){fputs("DAT unreadable\n",stderr);return 2;}
    re4_arena_init(&arena,arena_buffer,sizeof(arena_buffer));
    e=re4_dat_read_indexed(dat,RE4_ENDIAN_LITTLE,RE4_FALSE,&arena,&archive);
    if(e!=RE4_OK || idx>=archive.entry_count){fputs("DAT index invalid\n",stderr);return 2;}
    if(archive.entries[idx].name.len!=4UL ||
       memcmp(archive.entries[idx].name.ptr,"SEQ",3UL)!=0){
        fputs("Selected entry is not SEQ\n",stderr);return 2;
    }
    e=re4_archive_entry_span(dat,&archive.entries[idx],&chunk);
    if(e==RE4_OK)e=re4_seq_timeline_read(chunk,RE4_ENDIAN_LITTLE,&seq);
    if(e==RE4_OK)e=re4_seq_timeline_inspect(&seq,&ob);
    if(e!=RE4_OK){fprintf(stderr,"SEQ invalid: %s\n",re4_err_name(e));return 2;}
    puts("absolute_q6,loop,key_index,local_q6,event_code,auxiliary");
    e=re4_seq_dispatch_range(&seq,duration,start,end,256U,all,
                              print_event,stdout,&count);
    if(e!=RE4_OK){fprintf(stderr,"Dispatch refused: %s (seq_end=%u, duration=%lu)\n",
        re4_err_name(e),(unsigned)ob.last_q6,(unsigned long)duration);return 2;}
    fprintf(stderr,"SEQ_REPLAY: index=%lu raw_keys=%lu emitted=%lu, semantic=UNKNOWN\n",
        (unsigned long)idx,(unsigned long)ob.count,(unsigned long)count);
    return 0;
}
