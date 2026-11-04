/* RE4 MotionSeqKey: uint16 frame in 10.6 fixed, u8 SE, u8 free.
   Strict C89, caller-span only; separate from unrelated OP conversation SEQ. */
#include "re4c3d/re4_animation.h"
#include "re4c3d/re4_base.h"
#include <string.h>
re4_err re4_seq_timeline_read(re4_span src,re4_endian endian,re4_seq_timeline *out)
{
    re4_u32 count;re4_size used;
    if(!src.data||!out)return RE4_ERR_NULL;
    memset(out,0,sizeof(*out));
    if(!re4_span_ok(src,0UL,4UL))return RE4_ERR_RANGE;
    count=re4_read_u16_e(src.data,endian);
    used=4UL+4UL*(re4_size)count;
    if(!re4_span_ok(src,0UL,used))return RE4_ERR_RANGE;
    if(src.data[3]!=0U)return RE4_ERR_BAD_FORMAT;
    out->count=(re4_u16)count;out->flags=src.data[2];out->data=src;
    out->endian=endian;
    return RE4_OK;
}
re4_err re4_seq_timeline_key_at(const re4_seq_timeline *seq,re4_u32 i,re4_seq_key *out)
{
    re4_size off;
    if(!seq||!out)return RE4_ERR_NULL;
    if(i>=seq->count)return RE4_ERR_RANGE;
    off=4UL+(re4_size)i*4UL;
    if(!re4_span_ok(seq->data,off,4UL))return RE4_ERR_RANGE;
    /* RE4 PC 2007: {event_code:u8, auxiliary:u8, frame_q6:u16}.
       Previous revisions accidentally read the time from bytes 0..1,
       causing 11,524/12,289 original SEQ timelines to look unordered. */
    out->se=seq->data.data[off];out->flags=seq->data.data[off+1UL];
    out->frame_q6=re4_read_u16_e(seq->data.data+off+2UL,seq->endian);
    return RE4_OK;
}

/* Validate a bounded sequence without interpreting event semantics. */
re4_err re4_seq_timeline_inspect(const re4_seq_timeline *seq,
                                re4_seq_observation *out)
{
    re4_u32 i;
    re4_seq_key k;
    re4_u16 previous=0U;
    re4_err err;
    if(seq==0 || out==0)return RE4_ERR_NULL;
    memset(out,0,sizeof(*out));
    out->count=seq->count;
    for(i=0U;i<seq->count;++i){
        err=re4_seq_timeline_key_at(seq,i,&k);
        if(err!=RE4_OK)return err;
        if(i>0U && k.frame_q6<previous)return RE4_ERR_BAD_FORMAT;
        if(i==0U)out->first_q6=k.frame_q6;
        out->last_q6=k.frame_q6;
        if(k.se!=0U)++out->nonzero_events;
        if(k.flags!=0U)++out->nonzero_auxiliary;
        if(i>0U && k.frame_q6==previous)++out->same_time_pairs;
        previous=k.frame_q6;
    }
    return RE4_OK;
}
/* Half-open event window [start_q6,end_q6): suitable for deterministic
   frame advancement without firing events twice at window boundaries. */
re4_err re4_seq_timeline_window(const re4_seq_timeline *seq,
                                re4_u16 start_q6,re4_u16 end_q6,
                                re4_u32 *out_first,re4_u32 *out_count)
{
    re4_u32 lo,hi,mid,first;
    re4_seq_key k;
    re4_err e;
    if(seq==0 || out_first==0 || out_count==0)return RE4_ERR_NULL;
    *out_first=0U;*out_count=0U;
    if(start_q6>end_q6)return RE4_ERR_RANGE;
    lo=0U;hi=seq->count;
    while(lo<hi){
        mid=lo+(hi-lo)/2U;
        e=re4_seq_timeline_key_at(seq,mid,&k);if(e!=RE4_OK)return e;
        if(k.frame_q6<start_q6)lo=mid+1U;else hi=mid;
    }
    first=lo;hi=seq->count;
    while(lo<hi){
        mid=lo+(hi-lo)/2U;
        e=re4_seq_timeline_key_at(seq,mid,&k);if(e!=RE4_OK)return e;
        if(k.frame_q6<end_q6)lo=mid+1U;else hi=mid;
    }
    *out_first=first;*out_count=lo-first;
    return RE4_OK;
}
