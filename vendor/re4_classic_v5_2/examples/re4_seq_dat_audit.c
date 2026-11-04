/* RE4 2007 nested DAT SEQ / adjacent FCV relation auditor.
   Strict C89, no heap, no float, no original game asset redistribution.
   Does NOT assume raw SEQ codes mean animation phase changes. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define DAT_CAP (48UL*1024UL*1024UL)
#define ARENA_CAP (8UL*1024UL*1024UL)
static re4_u8 dat_buf[DAT_CAP],scratch[ARENA_CAP];
static int readfile(const char *path,re4_span *dst)
{
    FILE *f;long len;f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0||(len=ftell(f))<=0L ||
       (unsigned long)len>DAT_CAP||fseek(f,0,SEEK_SET)!=0){fclose(f);return 0;}
    if(fread(dat_buf,1U,(size_t)len,f)!=(size_t)len){fclose(f);return 0;}
    fclose(f);dst->data=dat_buf;dst->size=(re4_size)len;return 1;
}
static int is_ext(const re4_archive_entry *ent,const char *ext)
{
    return ent->name.len==4UL && memcmp(ent->name.ptr,ext,3UL)==0;
}
int main(int argc,char **argv)
{
    re4_span dat,chunk,preceding;re4_archive archive;re4_arena ar;
    re4_err e;re4_u32 i,good=0U,bad=0U,unpaired=0U,near=0U,events=0U;
    int details=0;
    if(argc!=2 && argc!=3){fputs("usage: re4_seq_dat_audit NESTED.DAT [--events]\n",stderr);return 2;}
    if(argc==3){if(strcmp(argv[2],"--events")!=0)return 2;details=1;}
    if(!readfile(argv[1],&dat)){fputs("Cannot read nested DAT\n",stderr);return 2;}
    re4_arena_init(&ar,scratch,sizeof(scratch));
    e=re4_dat_read_indexed(dat,RE4_ENDIAN_LITTLE,RE4_FALSE,&ar,&archive);
    if(e!=RE4_OK){fprintf(stderr,"DAT index: %s\n",re4_err_name(e));return 2;}
    puts("seq_index,seq_bytes,status,keys,first_q6,last_q6,event_codes,aux_values,equal_times,adjacent_fcv_index,fcv_max_frame,seq_end_frame,fraction_64,relation");
    for(i=0U;i<archive.entry_count;++i){
        re4_seq_timeline seq;re4_seq_observation obs;
        re4_u32 fcv_idx=65535U,fcv_max=65535U;
        const char *rel="UNPAIRED";
        re4_archive_entry *entry=&archive.entries[i];
        if(!is_ext(entry,"SEQ"))continue;
        e=re4_archive_entry_span(dat,entry,&chunk);
        if(e==RE4_OK)e=re4_seq_timeline_read(chunk,RE4_ENDIAN_LITTLE,&seq);
        if(e==RE4_OK)e=re4_seq_timeline_inspect(&seq,&obs);
        if(e!=RE4_OK){
            printf("%lu,%lu,%s,0,0,0,0,0,0,-1,-1,-1,-1,BAD\n",(unsigned long)i,
                (unsigned long)entry->packed_size,re4_err_name(e));++bad;continue;
        }
        if(i>0U && is_ext(&archive.entries[i-1U],"FCV") &&
           re4_archive_entry_span(dat,&archive.entries[i-1U],&preceding)==RE4_OK &&
           preceding.size>=2UL){
            fcv_idx=i-1U;fcv_max=re4_read_u16_e(preceding.data,RE4_ENDIAN_LITTLE)&0x3FFFU;
            if(obs.count && (obs.last_q6>>6)<=fcv_max+1U){rel="TIME_WITHIN_ADJACENT_FCV";++near;}
            else rel="TIME_MISMATCH_NOT_PROOF";
        }else ++unpaired;
        printf("%lu,%lu,OK,%lu,%u,%u,%lu,%lu,%lu,",(unsigned long)i,
            (unsigned long)chunk.size,(unsigned long)obs.count,
            (unsigned)obs.first_q6,(unsigned)obs.last_q6,
            (unsigned long)obs.nonzero_events,(unsigned long)obs.nonzero_auxiliary,
            (unsigned long)obs.same_time_pairs);
        if(fcv_idx==65535U)printf("-1,-1,");
        else printf("%lu,%lu,",(unsigned long)fcv_idx,(unsigned long)fcv_max);
        printf("%lu,%u,%s\n",(unsigned long)(obs.last_q6>>6),
            (unsigned)(obs.last_q6&63U),rel);
        ++good;events+=obs.count;
        if(details){
            re4_u32 k;re4_seq_key key;
            for(k=0U;k<seq.count;++k){
                if(re4_seq_timeline_key_at(&seq,k,&key)!=RE4_OK){++bad;break;}
                fprintf(stderr,"SEQ_EVENT,seq=%lu,key=%lu,frame=%lu,fraction=%u,code=%u,aux=%u\n",
                    (unsigned long)i,(unsigned long)k,(unsigned long)(key.frame_q6>>6),
                    (unsigned)(key.frame_q6&63U),(unsigned)key.se,(unsigned)key.flags);
            }
        }
    }
    fprintf(stderr,"SEQ_AUDIT: valid=%lu malformed=%lu events=%lu adjacent_timed=%lu no_adjacent_fcv=%lu\n",
        (unsigned long)good,(unsigned long)bad,(unsigned long)events,
        (unsigned long)near,(unsigned long)unpaired);
    return bad?1:0;
}
