/* RE4 SEQ C89 raw-event replay coverage for every SEQ in a nested DAT.
   Proves event scheduler is deterministic across tick windows and loops;
   does NOT prove how the RE4 EXE interprets codes or pairs FCV/SEQ. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define DAT_CAP (48UL*1024UL*1024UL)
#define ARENA_CAP (8UL*1024UL*1024UL)
static re4_u8 data[DAT_CAP],scratch[ARENA_CAP];
typedef struct sink_s {
    re4_u32 emitted;
    re4_u32 checksum;
    re4_u32 duration;
} sink;
static re4_err accumulate(void *user,const re4_seq_fire *event)
{
    sink *s=(sink *)user;
    re4_u32 local=event->absolute_q6-event->loop_index*s->duration;
    s->checksum=s->checksum*33U + (re4_u32)event->key.se*257U +
                (re4_u32)event->key.flags*7U + local + event->key_index;
    ++s->emitted;
    return RE4_OK;
}
static int getfile(const char *path,re4_span *out)
{
    FILE *f;long n;
    f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0L,SEEK_END)!=0 || (n=ftell(f))<=0L ||
       (unsigned long)n>DAT_CAP || fseek(f,0L,SEEK_SET)!=0){fclose(f);return 0;}
    if(fread(data,1U,(size_t)n,f)!=(size_t)n){fclose(f);return 0;}
    fclose(f);out->data=data;out->size=(re4_size)n;
    return 1;
}
static int is_seq(const re4_archive_entry *e)
{
    return e->name.len==4UL && memcmp(e->name.ptr,"SEQ",3UL)==0;
}
int main(int argc,char **argv)
{
    re4_span dat,chunk;re4_arena arena;re4_archive archive;
    re4_seq_timeline seq;re4_seq_observation ob;re4_err e;
    re4_u32 idx,duration,mid,a,b,c,total,good=0U,bad=0U;
    sink full,first,second,repeat;
    if(argc!=2){fputs("usage: re4_seq_replay_audit NESTED.DAT\n",stderr);return 2;}
    if(!getfile(argv[1],&dat)){fputs("cannot open DAT\n",stderr);return 2;}
    re4_arena_init(&arena,scratch,sizeof(scratch));
    e=re4_dat_read_indexed(dat,RE4_ENDIAN_LITTLE,RE4_FALSE,&arena,&archive);
    if(e!=RE4_OK){fprintf(stderr,"DAT: %s\n",re4_err_name(e));return 2;}
    puts("seq_index,status,keys,nonzero,clip_q6,one_pass,split_pass,repeat_pass,checksum");
    for(idx=0U;idx<archive.entry_count;++idx){
        if(!is_seq(&archive.entries[idx]))continue;
        e=re4_archive_entry_span(dat,&archive.entries[idx],&chunk);
        if(e==RE4_OK)e=re4_seq_timeline_read(chunk,RE4_ENDIAN_LITTLE,&seq);
        if(e==RE4_OK)e=re4_seq_timeline_inspect(&seq,&ob);
        if(e!=RE4_OK){
            printf("%lu,BAD_FORMAT,0,0,0,0,0,0,0\n",(unsigned long)idx);
            ++bad;continue;
        }
        a=0U;b=0U;c=0U;total=0U;
        duration=ob.count ? (re4_u32)ob.last_q6+1U : 64U;
        mid=duration/2U;
        memset(&full,0,sizeof(full));full.duration=duration;
        memset(&first,0,sizeof(first));first.duration=duration;
        memset(&second,0,sizeof(second));second.duration=duration;
        memset(&repeat,0,sizeof(repeat));repeat.duration=duration;
        e=re4_seq_dispatch_range(&seq,duration,0U,duration,1U,RE4_FALSE,
                                  accumulate,&full,&a);
        if(e==RE4_OK)e=re4_seq_dispatch_range(&seq,duration,0U,mid,1U,
                                               RE4_FALSE,accumulate,&first,&b);
        if(e==RE4_OK)e=re4_seq_dispatch_range(&seq,duration,mid,duration,1U,
                                               RE4_FALSE,accumulate,&second,&c);
        if(e==RE4_OK)e=re4_seq_dispatch_range(&seq,duration,duration,
                                               duration*2U,1U,RE4_FALSE,
                                               accumulate,&repeat,&total);
        /* Reconstruct the hash of the two half ranges, in original order.
           Sum of hashes is not equivalent to a whole-sequence rolling hash. */
        if(e==RE4_OK){
            sink combined;re4_u32 n;
            memset(&combined,0,sizeof(combined));combined.duration=duration;
            e=re4_seq_dispatch_range(&seq,duration,0U,mid,1U,RE4_FALSE,
                                      accumulate,&combined,&n);
            if(e==RE4_OK)e=re4_seq_dispatch_range(&seq,duration,mid,duration,
                                                   1U,RE4_FALSE,accumulate,&combined,&n);
            if(e==RE4_OK && (a!=ob.nonzero_events || b+c!=a ||
                             full.checksum!=combined.checksum ||
                             repeat.checksum!=full.checksum || total!=a))
                e=RE4_ERR_BAD_FORMAT;
        }
        if(e==RE4_OK){
            printf("%lu,OK,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
                   (unsigned long)idx,(unsigned long)ob.count,
                   (unsigned long)ob.nonzero_events,(unsigned long)duration,
                   (unsigned long)a,(unsigned long)(b+c),(unsigned long)total,
                   (unsigned long)full.checksum);
            ++good;
        }else{
            printf("%lu,REPLAY_MISMATCH,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
                   (unsigned long)idx,(unsigned long)ob.count,
                   (unsigned long)ob.nonzero_events,(unsigned long)duration,
                   (unsigned long)a,(unsigned long)(b+c),(unsigned long)total,
                   (unsigned long)full.checksum);
            ++bad;
        }
    }
    fprintf(stderr,"SEQ_REPLAY_AUDIT: good=%lu bad=%lu\n",
        (unsigned long)good,(unsigned long)bad);
    return bad?1:0;
}
