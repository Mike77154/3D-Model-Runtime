/* Strict C89, explicit FCV/SEQ pairing, Q6 time normalization.
   No assumptions based on neighboring DAT file offsets or event semantics. */
#include "re4c3d/re4_seq_pair.h"
#include "re4c3d/re4_base.h"
#include <string.h>
re4_u32 re4_seq_pair_fingerprint(re4_span s)
{
    re4_size i;re4_u32 h=2166136261UL;
    if(!s.data)return 0UL;
    for(i=0UL;i<s.size;++i)h=(h^(re4_u32)s.data[i])*16777619UL;
    return h;
}
re4_err re4_seq_pair_probe(re4_span raw_fcv,re4_span raw_seq,
                            re4_arena *arena,re4_seq_pair_report *out)
{
    re4_fcv fcv;re4_seq_timeline seq;re4_seq_observation ob;
    re4_err e;re4_u32 max_q6,period;re4_size previous_pos;
    if(!raw_fcv.data || !raw_seq.data || !arena || !out)return RE4_ERR_NULL;
    memset(out,0,sizeof(*out));
    previous_pos=arena->pos;
    e=re4_fcv_read(raw_fcv,RE4_ENDIAN_LITTLE,arena,&fcv);
    if(e!=RE4_OK){arena->pos=previous_pos;return e;}
    e=re4_fcv_validate(raw_fcv,&fcv);
    arena->pos=previous_pos; /* Release only the temporaries allocated by this probe. */
    if(e!=RE4_OK)return e;
    e=re4_seq_timeline_read(raw_seq,RE4_ENDIAN_LITTLE,&seq);
    if(e!=RE4_OK)return e;
    e=re4_seq_timeline_inspect(&seq,&ob);
    if(e!=RE4_OK)return e;
    max_q6=(re4_u32)fcv.max_time*64UL;
    period=((re4_u32)fcv.max_time+1UL)*64UL;
    out->fcv_last_frame=fcv.max_time;
    out->seq_last_q6=ob.last_q6;
    out->fcv_end_q6=max_q6;
    out->loop_period_q6=period;
    out->seq_keys=ob.count;
    out->nonzero_codes=ob.nonzero_events;
    out->fcv_fingerprint=re4_seq_pair_fingerprint(raw_fcv);
    out->seq_fingerprint=re4_seq_pair_fingerprint(raw_seq);
    if(ob.count && (re4_u32)ob.last_q6>max_q6)
        out->timing=RE4_PAIR_SEQ_OVERRUN;
    else if(period>65536UL)
        out->timing=RE4_PAIR_DISPATCH_TOO_LONG;
    else if(ob.count && (re4_u32)ob.last_q6==max_q6)
        out->timing=RE4_PAIR_EXACT_END;
    else
        out->timing=RE4_PAIR_WITHIN_CLIP;
    return RE4_OK;
}
re4_err re4_seq_pair_dispatch(re4_span raw_fcv,re4_span raw_seq,
                              re4_arena *arena,re4_u32 expected_fcv,
                              re4_u32 expected_seq,re4_bool allow_early,
                              re4_u32 from_q6,re4_u32 to_q6,
                              re4_u32 max_loops,re4_bool include_zero,
                              re4_seq_fire_fn fire,void *user,
                              re4_u32 *out_fired)
{
    re4_seq_pair_report report;re4_seq_timeline seq;re4_err e;
    if(!out_fired || !fire)return RE4_ERR_NULL;
    *out_fired=0UL;
    /* Unspecified/zero checksum cannot accidentally authorize a pairing. */
    if(!expected_fcv || !expected_seq)return RE4_ERR_BAD_FORMAT;
    if(re4_seq_pair_fingerprint(raw_fcv)!=expected_fcv ||
       re4_seq_pair_fingerprint(raw_seq)!=expected_seq)return RE4_ERR_BAD_FORMAT;
    e=re4_seq_pair_probe(raw_fcv,raw_seq,arena,&report);
    if(e!=RE4_OK)return e;
    if(report.timing!=RE4_PAIR_EXACT_END &&
       !(allow_early && report.timing==RE4_PAIR_WITHIN_CLIP))
       return RE4_ERR_RANGE;
    e=re4_seq_timeline_read(raw_seq,RE4_ENDIAN_LITTLE,&seq);
    if(e!=RE4_OK)return e;
    return re4_seq_dispatch_range(&seq,report.loop_period_q6,from_q6,to_q6,
                                   max_loops,include_zero,fire,user,out_fired);
}
