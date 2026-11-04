#include "../gs3d_internal.h"
/* Reject stale/incompatible descriptors before ANY frame data is written. */
int gs3d_blend_validate(const GS3D_Crossfade *f,unsigned int *track){
    const GS3D_Clip *a,*b;const GS3D_Group *g;unsigned int ta,tb;
    if(!f||!track||!gs_is_open)return GS3D_ERR_RANGE;
    a=&f->from;b=&f->to;
    if(a->generation!=b->generation||a->group!=b->group ||
       a->group>=gs3d_model.groups||!f->fade_ticks ||
       !a->ticks_per_frame||!b->ticks_per_frame ||
       !a->count||!b->count ||a->mode>GS3D_CLIP_PINGPONG||
       b->mode>GS3D_CLIP_PINGPONG||
       a->track>GS3D_CLIP_TRACK_SKELETAL||b->track>GS3D_CLIP_TRACK_SKELETAL||
       !gs3d_family_supports(a->generation,gs3d_model.format))
        return GS3D_ERR_RANGE;
    g=&gs3d_model.group[a->group];
    if(a->first>=g->frame_count||b->first>=g->frame_count||
       a->count>g->frame_count-a->first||
       b->count>g->frame_count-b->first)return GS3D_ERR_RANGE;
    ta=a->track;tb=b->track;
    if(ta==GS3D_CLIP_TRACK_AUTO)
        ta=(gs3d_model.format==7&&gs3d_model.bones)?
            GS3D_CLIP_TRACK_SKELETAL:GS3D_CLIP_TRACK_MORPH;
    if(tb==GS3D_CLIP_TRACK_AUTO)
        tb=(gs3d_model.format==7&&gs3d_model.bones)?
            GS3D_CLIP_TRACK_SKELETAL:GS3D_CLIP_TRACK_MORPH;
    if(ta!=tb)return GS3D_ERR_UNSUPPORTED;
    if(ta==GS3D_CLIP_TRACK_SKELETAL){
        if(gs3d_model.format!=7||!gs3d_model.bones||
           !gs3d_model.group[0].frame_count ||
           a->first>=gs3d_model.group[0].frame_count||
           b->first>=gs3d_model.group[0].frame_count||
           a->count>gs3d_model.group[0].frame_count-a->first||
           b->count>gs3d_model.group[0].frame_count-b->first)
            return GS3D_ERR_UNSUPPORTED;
    }
    *track=ta;return GS3D_OK;
}
