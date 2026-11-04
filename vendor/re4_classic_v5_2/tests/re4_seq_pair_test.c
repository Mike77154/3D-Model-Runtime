/* Synthetic FCV/SEQ pair regressions; strict C89; no game bytes shipped. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CK(x) do{if(!(x)){fprintf(stderr,"PAIR_TEST_FAIL:%d\n",__LINE__);return 1;}}while(0)
typedef struct pair_sink_s {re4_u32 count;re4_u32 last;} pair_sink;
static re4_err fire(void *user,const re4_seq_fire *event)
{
    pair_sink *s=(pair_sink *)user;
    s->last=event->absolute_q6;++s->count;return RE4_OK;
}
static void key(re4_u8 *p,re4_u32 i,re4_u8 code,re4_u16 time)
{
    p[4U+4U*i]=code;p[6U+4U*i]=(re4_u8)time;
    p[7U+4U*i]=(re4_u8)(time>>8);
}
int main(void)
{
    re4_u8 fcv[64],seq[16],memory[4096];
    re4_span f,s;re4_arena a;re4_seq_pair_report r;
    pair_sink sink;re4_u32 n,fh,sh;
    memset(fcv,0,sizeof(fcv));fcv[0]=6U;fcv[2]=1U;
    fcv[3]=4U;fcv[5]=2U;fcv[8]=64U;fcv[12]=16U;
    fcv[16]=1U;fcv[32]=1U;fcv[48]=1U;
    memset(seq,0,sizeof(seq));seq[0]=2U;
    key(seq,0,17U,0U);key(seq,1,23U,384U);
    f.data=fcv;f.size=sizeof(fcv);s.data=seq;s.size=12U;
    re4_arena_init(&a,memory,sizeof(memory));
    CK(re4_seq_pair_probe(f,s,&a,&r)==RE4_OK && a.pos==0UL);
    CK(r.timing==RE4_PAIR_EXACT_END && r.fcv_last_frame==6U);
    CK(r.seq_last_q6==384U && r.loop_period_q6==448U);
    fh=r.fcv_fingerprint;sh=r.seq_fingerprint;
    memset(&sink,0,sizeof(sink));
    CK(re4_seq_pair_dispatch(f,s,&a,fh,sh,RE4_FALSE,0U,896U,2U,
                              RE4_FALSE,fire,&sink,&n)==RE4_OK);
    CK(n==4U && sink.count==4U && sink.last==832U);
    CK(re4_seq_pair_dispatch(f,s,&a,fh+1U,sh,RE4_FALSE,0U,448U,1U,
                              RE4_FALSE,fire,&sink,&n)==RE4_ERR_BAD_FORMAT && n==0U);
    /* Early termination must be explicitly allowed, even when time fits. */
    key(seq,1,23U,320U);sh=re4_seq_pair_fingerprint(s);
    CK(re4_seq_pair_probe(f,s,&a,&r)==RE4_OK && r.timing==RE4_PAIR_WITHIN_CLIP);
    CK(re4_seq_pair_dispatch(f,s,&a,fh,sh,RE4_FALSE,0U,448U,1U,
                              RE4_FALSE,fire,&sink,&n)==RE4_ERR_RANGE);
    CK(re4_seq_pair_dispatch(f,s,&a,fh,sh,RE4_TRUE,0U,448U,1U,
                              RE4_FALSE,fire,&sink,&n)==RE4_OK && n==2U);
    /* SEQ beyond clip end is rejected even with early-end permission. */
    key(seq,1,23U,448U);sh=re4_seq_pair_fingerprint(s);
    CK(re4_seq_pair_probe(f,s,&a,&r)==RE4_OK && r.timing==RE4_PAIR_SEQ_OVERRUN);
    CK(re4_seq_pair_dispatch(f,s,&a,fh,sh,RE4_TRUE,0U,448U,1U,
                              RE4_FALSE,fire,&sink,&n)==RE4_ERR_RANGE);
    /* A long FCV cannot be scheduled using the old 16-bit period interface. */
    fcv[0]=76U;fcv[1]=4U; /* 1100 frames */
    key(seq,1,23U,320U);
    CK(re4_seq_pair_probe(f,s,&a,&r)==RE4_OK &&
       r.timing==RE4_PAIR_DISPATCH_TOO_LONG);
    /* Malformed SEQ and checksum change both fail closed. */
    key(seq,0,17U,400U);
    CK(re4_seq_pair_probe(f,s,&a,&r)==RE4_ERR_BAD_FORMAT);
    key(seq,0,17U,0U);
    CK(re4_seq_pair_dispatch(f,s,&a,0U,sh,RE4_TRUE,0U,1U,1U,
                              RE4_FALSE,fire,&sink,&n)==RE4_ERR_BAD_FORMAT);
    puts("SEQ/FCV pair: exact, early, overrun, too-long, checksum and replay OK");
    return 0;
}
