#include "../gs3d_internal.h"
/* Only the first contiguous run of a prefix is a candidate.
   A second disconnected run is rejected: never join unrelated clips. */
int gs3d_clip_from_names(GS3D_Clip *out,unsigned int generation,
    unsigned int group,const char *start_name,const char *end_name,
    unsigned long ticks_per_frame,unsigned int mode,unsigned int track){
    int first,last;
    if(!start_name||!end_name)return GS3D_ERR_RANGE;
    first=gs3d_find_frame(group,start_name);
    last=gs3d_find_frame(group,end_name);
    if(first<0||last<first)return GS3D_ERR_RANGE;
    return gs3d_clip_create(out,generation,group,(unsigned int)first,
         (unsigned int)(last-first+1),ticks_per_frame,mode,track);
}
int gs3d_clip_from_prefix(GS3D_Clip *out,unsigned int generation,
    unsigned int group,const char *prefix,unsigned long ticks_per_frame,
    unsigned int mode,unsigned int track){
    GS3D_Group *g;unsigned int i,first=0U,count=0U,stage=0U;
    unsigned long len;
    if(!gs_is_open||!prefix||!prefix[0]||group>=gs3d_model.groups)
        return GS3D_ERR_RANGE;
    len=(unsigned long)strlen(prefix);
    if(len>16UL)return GS3D_ERR_RANGE;
    g=&gs3d_model.group[group];
    for(i=0U;i<g->frame_count;i++){
        if(strncmp(g->frames[i].name,prefix,(size_t)len)==0){
            if(stage==2U)return GS3D_ERR_CORRUPT;
            if(stage==0U){first=i;stage=1U;}
            count++;
        }else if(stage==1U)stage=2U;
    }
    if(!count)return GS3D_ERR_RANGE;
    return gs3d_clip_create(out,generation,group,first,count,
                             ticks_per_frame,mode,track);
}
