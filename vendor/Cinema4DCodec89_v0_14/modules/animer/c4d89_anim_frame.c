#include "c4d89.h"
#include <string.h>
/* Reads all supported scalar tracks at one scene instant while keeping the
   file open. Unsupported irregular-stride tracks are counted, never guessed. */
int c4d89_sample_animation_frame_file(FILE *f,const C4D89_AnimIndex *idx,
         long seconds_q16,unsigned int shortarc,C4D89_FrameTrackFn emit,
         void *user,C4D89_FrameStats *stats)
{
    unsigned int i;int rc;long value;
    C4D89_FrameStats local;
    if(!f||!idx||!emit||!stats||seconds_q16<0L||shortarc>1u||
       idx->count>C4D89_MAX_TRACKS)return C4D89_ERR_CORRUPT;
    memset(&local,0,sizeof(local));
    *stats=local;
    for(i=0u;i<idx->count;i++){
        const C4D89_AnimTrack *t=&idx->tracks[i];
        if(t->key_count==0ul||t->key_stride==0u){local.unsupported++;continue;}
        if(shortarc&&strncmp(t->channel,"Rotation",8u)==0)
            rc=c4d89_sample_track_angle_file(f,t,seconds_q16,&value);
        else rc=c4d89_sample_track_linear_file(f,t,seconds_q16,&value);
        if(rc==C4D89_ERR_UNSUPPORTED){local.unsupported++;continue;}
        if(rc){*stats=local;return rc;}
        if(!emit(user,i,t,value)){*stats=local;return C4D89_ERR_SINK;}
        local.sampled++;
    }
    *stats=local;
    return C4D89_OK;
}
int c4d89_sample_animation_frame(const char *path,const C4D89_AnimIndex *idx,
         long seconds_q16,unsigned int shortarc,C4D89_FrameTrackFn emit,
         void *user,C4D89_FrameStats *stats)
{
    FILE *f;int rc;
    if(!path)return C4D89_ERR_CORRUPT;
    f=fopen(path,"rb");if(!f)return C4D89_ERR_IO;
    rc=c4d89_sample_animation_frame_file(f,idx,seconds_q16,shortarc,emit,user,stats);
    fclose(f);return rc;
}
