/* RE4 FCV kind-1 root motion telemetry from original nested LE DAT.
   C89; caller-owned/static buffers, no heap, writes only to caller's CSV.
   Samples original frame 0, midpoint, endpoint. This does not interpret
   native gameplay locomotion or scale. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define DAT_CAP (24UL*1024UL*1024UL)
#define ARENA_CAP (4UL*1024UL*1024UL)
static re4_u8 datmem[DAT_CAP],arenamem[ARENA_CAP];
static void emit_xyz(FILE *f,const re4_q16 *x)
{
    fprintf(f,",%ld,%ld,%ld",(long)x[0],(long)x[1],(long)x[2]);
}
int main(int argc,char **argv)
{
    FILE *f,*csv;long filesize;re4_span src,entry;re4_archive dat;
    re4_fcv clip;re4_arena arena;re4_root_track root;
    re4_q16 start[3],mid[3],end[3],dmid[3],dend[3];
    re4_u32 i,ok=0,empty=0,invalid=0,nonzero=0,hasroot=0;
    re4_err er;re4_size mark;
    if(argc!=3){fputs("Usage: re4_root_catalog NESTED.dat out.csv\n",stderr);return 2;}
    f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    if(fseek(f,0,SEEK_END)!=0 || (filesize=ftell(f))<0 ||
       (unsigned long)filesize>DAT_CAP || fseek(f,0,SEEK_SET)!=0 ||
       fread(datmem,1,(size_t)filesize,f)!=(size_t)filesize){fclose(f);return 2;}
    fclose(f);src.data=datmem;src.size=(re4_size)filesize;
    re4_arena_init(&arena,arenamem,ARENA_CAP);
    er=re4_dat_read_indexed(src,RE4_ENDIAN_LITTLE,RE4_FALSE,&arena,&dat);
    if(er!=RE4_OK){fprintf(stderr,"DAT index: %s\n",re4_err_name(er));return 2;}
    csv=fopen(argv[2],"wb");if(!csv){perror(argv[2]);return 2;}
    fputs("dat_index,fcv_frames,channels,root_present,start_x_q16,start_y_q16,start_z_q16,mid_x_q16,mid_y_q16,mid_z_q16,end_x_q16,end_y_q16,end_z_q16,mid_delta_x_q16,mid_delta_y_q16,mid_delta_z_q16,end_delta_x_q16,end_delta_y_q16,end_delta_z_q16,nonzero_probe,status\n",csv);
    mark=arena.pos;
    for(i=0;i<dat.entry_count;++i){
        if(dat.entries[i].name.len!=4UL || memcmp(dat.entries[i].name.ptr,"FCV",3UL)!=0)continue;
        er=re4_archive_entry_span(src,&dat.entries[i],&entry);
        if(er!=RE4_OK || entry.size<16UL || entry.data[2]==0U){++empty;continue;}
        arena.pos=mark;
        er=re4_fcv_read(entry,RE4_ENDIAN_LITTLE,&arena,&clip);
        if(er==RE4_OK)er=re4_fcv_validate(entry,&clip);
        if(er==RE4_OK)er=re4_root_find(&clip,&root);
        if(er!=RE4_OK){++invalid;fprintf(csv,"%lu,0,0,0",(unsigned long)i);
           fputs(",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,error\n",csv);continue;}
        er=re4_root_sample(entry,&clip,&root,0U,start);
        if(er==RE4_OK)er=re4_root_sample(entry,&clip,&root,(re4_u16)(clip.max_time/2U),mid);
        if(er==RE4_OK)er=re4_root_sample(entry,&clip,&root,clip.max_time,end);
        if(er==RE4_OK)er=re4_root_relative(entry,&clip,&root,0U,(re4_u16)(clip.max_time/2U),dmid);
        if(er==RE4_OK)er=re4_root_relative(entry,&clip,&root,0U,clip.max_time,dend);
        if(er!=RE4_OK){++invalid;continue;}
        ++ok;if(root.present)++hasroot;
        if(dmid[0]||dmid[1]||dmid[2]||dend[0]||dend[1]||dend[2])++nonzero;
        fprintf(csv,"%lu,%u,%u,%u",(unsigned long)i,(unsigned)clip.max_time,
                (unsigned)clip.block_count,(unsigned)root.present);
        emit_xyz(csv,start);emit_xyz(csv,mid);emit_xyz(csv,end);
        emit_xyz(csv,dmid);emit_xyz(csv,dend);
        fprintf(csv,",%u,ok\n",(unsigned)(dmid[0]||dmid[1]||dmid[2]||dend[0]||dend[1]||dend[2]));
    }
    if(fclose(csv)!=0)return 3;
    printf("ROOT CATALOG ok=%lu empty=%lu bad=%lu with_root=%lu nonzero_probes=%lu\n",
        (unsigned long)ok,(unsigned long)empty,(unsigned long)invalid,
        (unsigned long)hasroot,(unsigned long)nonzero);
    return invalid?1:0;
}
