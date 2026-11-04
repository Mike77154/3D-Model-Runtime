/* Offline inspect of explicitly supplied FCV/SEQ (does not infer ownership).
   Strict C89; buffers are fixed; no heap or third-party dependencies. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#define CAP (4UL*1024UL*1024UL)
static re4_u8 fcv_bytes[CAP],seq_bytes[CAP],arena_bytes[65536U];
static int getfile(const char *path,re4_u8 *buf,re4_span *out)
{
    FILE *f;long size;
    f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0 || (size=ftell(f))<0L ||
       (unsigned long)size>CAP || fseek(f,0,SEEK_SET)!=0){fclose(f);return 0;}
    if(fread(buf,1U,(size_t)size,f)!=(size_t)size){fclose(f);return 0;}
    fclose(f);out->data=buf;out->size=(re4_size)size;return 1;
}
int main(int argc,char **argv)
{
    re4_arena arena;re4_span f,s;re4_seq_pair_report r;
    re4_err err;const char *status;
    if(argc!=3){fputs("Usage: re4_seq_pair_probe CLIP.fcv EVENTS.seq\n",stderr);return 2;}
    if(!getfile(argv[1],fcv_bytes,&f)||!getfile(argv[2],seq_bytes,&s)){
        fputs("Unable to read input or file exceeds 4 MiB\n",stderr);return 2;
    }
    re4_arena_init(&arena,arena_bytes,sizeof(arena_bytes));
    err=re4_seq_pair_probe(f,s,&arena,&r);
    if(err!=RE4_OK){fprintf(stderr,"Invalid pair inputs: %s\n",re4_err_name(err));return 2;}
    status=r.timing==RE4_PAIR_EXACT_END?"EXACT_END_TIMING_ONLY":
           r.timing==RE4_PAIR_WITHIN_CLIP?"SEQ_ENDS_EARLY":
           r.timing==RE4_PAIR_SEQ_OVERRUN?"SEQ_OVERRUN":"DISPATCH_TOO_LONG";
    puts("timing_status,fcv_last_frame,seq_last_q6,fcv_end_q6,period_q6,seq_keys,nonzero_codes,fcv_fnv32,seq_fnv32");
    printf("%s,%u,%u,%lu,%lu,%lu,%lu,%08lx,%08lx\n",status,
           (unsigned int)r.fcv_last_frame,(unsigned int)r.seq_last_q6,
           (unsigned long)r.fcv_end_q6,(unsigned long)r.loop_period_q6,
           (unsigned long)r.seq_keys,(unsigned long)r.nonzero_codes,
           (unsigned long)r.fcv_fingerprint,(unsigned long)r.seq_fingerprint);
    return r.timing==RE4_PAIR_SEQ_OVERRUN?1:0;
}
