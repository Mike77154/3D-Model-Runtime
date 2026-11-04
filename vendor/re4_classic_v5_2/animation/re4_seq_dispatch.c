/* RE4 PC 2007: bounded, deterministic SEQ event playback.
   Strict C89, zero heap, integer time (10.6 fixed).
   IMPORTANT: does not assign native semantics to event bytes, nor establish
   SEQ-to-FCV ownership. Caller supplies explicitly reviewed association.
*/
#include "re4c3d/re4_animation.h"
#include "re4c3d/re4_base.h"

re4_err re4_seq_dispatch_range(const re4_seq_timeline *seq,
                               re4_u32 clip_duration_q6,
                               re4_u32 from_q6,re4_u32 to_q6,
                               re4_u32 max_loops,re4_bool include_zero,
                               re4_seq_fire_fn fire,void *user,
                               re4_u32 *out_fired)
{
    re4_seq_observation obs;
    re4_u32 first_cycle,last_cycle,cycle,start,stop,first,count,i;
    re4_u32 emitted=0U,offset;
    re4_seq_fire event;
    re4_err e;
    if(out_fired==0 || seq==0 || fire==0)return RE4_ERR_NULL;
    *out_fired=0U;
    if(to_q6<from_q6 || clip_duration_q6==0U || clip_duration_q6>65536U)
        return RE4_ERR_RANGE;
    e=re4_seq_timeline_inspect(seq,&obs);
    if(e!=RE4_OK)return e;
    /* Never wrap a SEQ key into another loop because its duration is wrong. */
    if(obs.count && (re4_u32)obs.last_q6>=clip_duration_q6)
        return RE4_ERR_RANGE;
    if(to_q6==from_q6)return RE4_OK;
    first_cycle=from_q6/clip_duration_q6;
    last_cycle=(to_q6-1U)/clip_duration_q6;
    if(max_loops==0U || last_cycle-first_cycle>=max_loops)
        return RE4_ERR_RANGE;
    for(cycle=first_cycle;cycle<=last_cycle;++cycle){
        /* cycle*duration cannot exceed to_q6-1 for this bounded range. */
        offset=cycle*clip_duration_q6;
        start=(from_q6>offset)?from_q6-offset:0U;
        stop=(to_q6-offset<clip_duration_q6)?to_q6-offset:clip_duration_q6;
        /* The window API is u16: 65536 is represented by last-key+1,
           so take an upper bound of 65535 via explicit last-key inclusion. */
        if(start>=65536U || stop>65536U)return RE4_ERR_RANGE;
        if(start==65535U){
            first=seq->count;count=0U;
            /* Key at time 65535 exists and must be included if stop=65536. */
            if(stop==65536U){
                for(i=0U;i<seq->count;++i){
                    e=re4_seq_timeline_key_at(seq,i,&event.key);
                    if(e!=RE4_OK)return e;
                    if(event.key.frame_q6==65535U){first=i;break;}
                }
                count=seq->count-first;
            }
        }else{
            e=re4_seq_timeline_window(seq,(re4_u16)start,
                                      (re4_u16)(stop==65536U?65535U:stop),
                                      &first,&count);
            if(e!=RE4_OK)return e;
            if(stop==65536U){
                re4_u32 k=first+count;
                while(k<seq->count){
                    e=re4_seq_timeline_key_at(seq,k,&event.key);
                    if(e!=RE4_OK)return e;
                    if(event.key.frame_q6!=65535U)break;
                    ++count;++k;
                }
            }
        }
        for(i=first;i<first+count;++i){
            e=re4_seq_timeline_key_at(seq,i,&event.key);
            if(e!=RE4_OK)return e;
            if(!include_zero && event.key.se==0U)continue;
            event.absolute_q6=offset+(re4_u32)event.key.frame_q6;
            event.loop_index=cycle;
            event.key_index=i;
            e=fire(user,&event);
            if(e!=RE4_OK){*out_fired=emitted;return e;}
            ++emitted;
        }
        /* Avoid unsigned rollover of cycle when last_cycle=UINT_MAX. */
        if(cycle==last_cycle)break;
    }
    *out_fired=emitted;
    return RE4_OK;
}
