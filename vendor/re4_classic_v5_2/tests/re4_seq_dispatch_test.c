/* C89 / no heap: deterministic raw SEQ event replay regressions. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CK(x) do{if(!(x)){fprintf(stderr,"DISPATCH_FAIL:%d\n",__LINE__);return 1;}}while(0)
typedef struct test_sink_s {re4_seq_fire e[32];re4_u32 used;re4_u32 stop_at;} test_sink;
static re4_err collect(void *user,const re4_seq_fire *event)
{
    test_sink *s=(test_sink *)user;
    if(s->used==s->stop_at)return RE4_ERR_UNSUPPORTED;
    if(s->used>=32U)return RE4_ERR_NO_SPACE;
    s->e[s->used++]=*event;return RE4_OK;
}
static void put(re4_u8 *b,re4_u32 i,re4_u8 code,re4_u8 aux,re4_u16 q6)
{
    b[4U+i*4U]=code;b[5U+i*4U]=aux;
    b[6U+i*4U]=(re4_u8)(q6&255U);b[7U+i*4U]=(re4_u8)(q6>>8);
}
int main(void)
{
    re4_u8 raw[36];re4_span sp;re4_seq_timeline seq;
    re4_err e;re4_u32 count;test_sink sink;
    memset(raw,0,sizeof(raw));raw[0]=5;
    put(raw,0,0,0,0);put(raw,1,10,1,64);put(raw,2,11,2,64);
    put(raw,3,12,0,128);put(raw,4,0,3,160);
    sp.data=raw;sp.size=24U;
    CK(re4_seq_timeline_read(sp,RE4_ENDIAN_LITTLE,&seq)==RE4_OK);
    memset(&sink,0,sizeof(sink));sink.stop_at=32U;
    e=re4_seq_dispatch_range(&seq,192U,0U,500U,4U,RE4_FALSE,
                              collect,&sink,&count);
    CK(e==RE4_OK && count==8U && sink.used==8U);
    CK(sink.e[0].absolute_q6==64U && sink.e[0].key_index==1U);
    CK(sink.e[1].absolute_q6==64U && sink.e[1].key_index==2U);
    CK(sink.e[2].absolute_q6==128U && sink.e[2].key.se==12U);
    CK(sink.e[3].absolute_q6==256U && sink.e[3].loop_index==1U);
    CK(sink.e[6].absolute_q6==448U && sink.e[7].absolute_q6==448U);
    memset(&sink,0,sizeof(sink));sink.stop_at=32U;
    CK(re4_seq_dispatch_range(&seq,192U,64U,128U,1U,RE4_FALSE,
                              collect,&sink,&count)==RE4_OK);
    CK(count==2U && sink.e[0].key.se==10U && sink.e[1].key.se==11U);
    memset(&sink,0,sizeof(sink));sink.stop_at=32U;
    CK(re4_seq_dispatch_range(&seq,192U,128U,256U,2U,RE4_FALSE,
                              collect,&sink,&count)==RE4_OK);
    CK(count==1U && sink.e[0].key.se==12U);
    memset(&sink,0,sizeof(sink));sink.stop_at=32U;
    CK(re4_seq_dispatch_range(&seq,192U,0U,192U,1U,RE4_TRUE,
                              collect,&sink,&count)==RE4_OK);
    CK(count==5U && sink.e[0].key.se==0U && sink.e[4].key.flags==3U);
    memset(&sink,0,sizeof(sink));sink.stop_at=2U;
    CK(re4_seq_dispatch_range(&seq,192U,0U,190U,1U,RE4_FALSE,
                              collect,&sink,&count)==RE4_ERR_UNSUPPORTED);
    CK(count==2U && sink.used==2U);/* callback errors preserve partial count */
    CK(re4_seq_dispatch_range(&seq,192U,0U,500U,2U,RE4_FALSE,
                              collect,&sink,&count)==RE4_ERR_RANGE);
    CK(re4_seq_dispatch_range(&seq,160U,0U,1U,1U,RE4_FALSE,
                              collect,&sink,&count)==RE4_ERR_RANGE);
    CK(re4_seq_dispatch_range(&seq,192U,30U,20U,1U,RE4_FALSE,
                              collect,&sink,&count)==RE4_ERR_RANGE);
    CK(re4_seq_dispatch_range(&seq,192U,30U,30U,1U,RE4_FALSE,
                              collect,&sink,&count)==RE4_OK && count==0U);
    /* 16-bit fixed-point endpoint 65535 must survive a 65536 duration. */
    memset(raw,0,sizeof(raw));raw[0]=2;put(raw,0,5,0,65534U);
    put(raw,1,6,0,65535U);
    CK(re4_seq_timeline_read(sp,RE4_ENDIAN_LITTLE,&seq)==RE4_OK);
    memset(&sink,0,sizeof(sink));sink.stop_at=32U;
    CK(re4_seq_dispatch_range(&seq,65536U,65534U,65536U,1U,
                              RE4_FALSE,collect,&sink,&count)==RE4_OK);
    CK(count==2U && sink.e[0].absolute_q6==65534U &&
       sink.e[1].absolute_q6==65535U);
    memset(&sink,0,sizeof(sink));sink.stop_at=32U;
    CK(re4_seq_dispatch_range(&seq,65536U,65535U,65536U,1U,
                              RE4_FALSE,collect,&sink,&count)==RE4_OK);
    CK(count==1U && sink.e[0].key.se==6U);
    /* Time-zero event fires once per loop, not at the previous loop's end. */
    memset(raw,0,sizeof(raw));raw[0]=2;
    put(raw,0,19,0,0U);put(raw,1,20,0,64U);
    CK(re4_seq_timeline_read(sp,RE4_ENDIAN_LITTLE,&seq)==RE4_OK);
    memset(&sink,0,sizeof(sink));sink.stop_at=32U;
    CK(re4_seq_dispatch_range(&seq,65U,0U,130U,2U,
                              RE4_FALSE,collect,&sink,&count)==RE4_OK);
    CK(count==4U && sink.e[0].absolute_q6==0U &&
       sink.e[1].absolute_q6==64U && sink.e[2].absolute_q6==65U &&
       sink.e[3].absolute_q6==129U);
    puts("SEQ raw event dispatch: loops, same-time order, bounds, zero, callback and q6 edge OK");
    return 0;
}
