/* Regression for the actual byte layout of RE4 PC2007 SEQ timeline keys.
   100% C89. Synthetic bytes based on the observed DAT layout. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CK(x) do {if(!(x)){fprintf(stderr,"SEQ_TEST_FAIL at %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    re4_u8 data[32];re4_span sp;re4_seq_timeline seq;
    re4_seq_key k;re4_seq_observation ob;re4_u32 first,count;
    memset(data,0,sizeof(data));
    data[0]=4;/* Four events, one per frame. */
    /* First key: frame 0, event 0, aux 0. */
    /* Second key: event 17, aux 2, frame 64 (=1 frame). */
    data[8]=17;data[9]=2;data[10]=64;
    /* Third: event 65, aux 3, frame 128. */
    data[12]=65;data[13]=3;data[14]=128;
    /* Fourth: event 9, aux 0, frame 192. */
    data[16]=9;data[18]=192;
    sp.data=data;sp.size=sizeof(data);
    CK(re4_seq_timeline_read(sp,RE4_ENDIAN_LITTLE,&seq)==RE4_OK);
    CK(re4_seq_timeline_key_at(&seq,1,&k)==RE4_OK);
    CK(k.frame_q6==64U && k.se==17U && k.flags==2U);
    CK(re4_seq_timeline_key_at(&seq,2,&k)==RE4_OK);
    CK(k.frame_q6==128U && k.se==65U && k.flags==3U);
    CK(re4_seq_timeline_inspect(&seq,&ob)==RE4_OK);
    CK(ob.count==4U && ob.last_q6==192U && ob.nonzero_events==3U);
    CK(ob.nonzero_auxiliary==2U && ob.same_time_pairs==0U);
    CK(re4_seq_timeline_window(&seq,64U,192U,&first,&count)==RE4_OK);
    CK(first==1U && count==2U);
    CK(re4_seq_timeline_window(&seq,192U,192U,&first,&count)==RE4_OK);
    CK(count==0U);
    CK(re4_seq_timeline_window(&seq,192U,128U,&first,&count)==RE4_ERR_RANGE);
    data[14]=32;/* Non-monotonic event time: report broken order. */
    CK(re4_seq_timeline_inspect(&seq,&ob)==RE4_ERR_BAD_FORMAT);
    data[14]=128;
    sp.size=19U;
    CK(re4_seq_timeline_read(sp,RE4_ENDIAN_LITTLE,&seq)==RE4_ERR_RANGE);
    /* Big-endian timestamp; ensure event bytes do not swap with time. */
    memset(data,0,sizeof(data));data[1]=1;data[4]=42;data[6]=0x01;data[7]=0x40;
    sp.size=sizeof(data);
    CK(re4_seq_timeline_read(sp,RE4_ENDIAN_BIG,&seq)==RE4_OK);
    CK(re4_seq_timeline_key_at(&seq,0,&k)==RE4_OK);
    CK(k.frame_q6==320U && k.se==42U && k.flags==0U);
    puts("SEQ timeline layout, order, q6 window and bounds OK");
    return 0;
}
