#include "../gs3d_internal.h"
/* Explicit ranges, no fabricated FPS or automatic assumptions about clips. */
int gs3d_clip_create(GS3D_Clip *out,unsigned int generation,
    unsigned int group,unsigned int first,unsigned int count,
    unsigned long ticks_per_frame,unsigned int mode,unsigned int track){
    GS3D_Group *g;
    if(!out)return GS3D_ERR_RANGE;
    if(!gs_is_open)return GS3D_ERR_UNSUPPORTED;
    if(group>=gs3d_model.groups || count==0U || ticks_per_frame==0UL ||
       mode>GS3D_CLIP_PINGPONG || track>GS3D_CLIP_TRACK_SKELETAL ||
       !gs3d_family_supports(generation,gs3d_model.format))return GS3D_ERR_RANGE;
    g=&gs3d_model.group[group];
    if(first>=g->frame_count || count>(unsigned int)g->frame_count-first)
        return GS3D_ERR_RANGE;
    if(track==GS3D_CLIP_TRACK_SKELETAL &&
       (gs3d_model.format!=7 || !gs3d_model.bones ||
        !gs3d_model.group[0].frame_count ||
        first+count>gs3d_model.group[0].frame_count))
        return GS3D_ERR_UNSUPPORTED;
    out->generation=(unsigned short)generation;
    out->group=(unsigned short)group;
    out->first=(unsigned short)first;
    out->count=(unsigned short)count;
    out->ticks_per_frame=ticks_per_frame;
    out->mode=(unsigned char)mode;
    out->track=(unsigned char)track;
    return GS3D_OK;
}
