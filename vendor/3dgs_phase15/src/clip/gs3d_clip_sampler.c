#include "../gs3d_internal.h"
static int gs3d_clip_validate_current(const GS3D_Clip *clip){
    GS3D_Group *g;
    if(!clip || !gs_is_open || clip->group>=gs3d_model.groups ||
       !gs3d_family_supports((unsigned int)clip->generation,gs3d_model.format))
       return GS3D_ERR_UNSUPPORTED;
    g=&gs3d_model.group[clip->group];
    if(clip->first>=g->frame_count ||
       (unsigned int)clip->count>(unsigned int)g->frame_count-clip->first)
       return GS3D_ERR_RANGE;
    return GS3D_OK;
}
static int gs3d_clip_dispatch(const GS3D_Clip *c,unsigned int group,
                             const GS3D_ClipSample *s){
    unsigned int track=c->track;
    if(track==GS3D_CLIP_TRACK_AUTO)
        track=(gs3d_model.format==7 && gs3d_model.bones>0U) ?
            GS3D_CLIP_TRACK_SKELETAL : GS3D_CLIP_TRACK_MORPH;
    if(track==GS3D_CLIP_TRACK_SKELETAL)
        return gs3d_vendor_sample_skeletal(c->generation,group,s->frame_a,
                                          s->frame_b,s->alpha_q16);
    return gs3d_vendor_sample_morph(c->generation,group,s->frame_a,
                                   s->frame_b,s->alpha_q16);
}
int gs3d_clip_apply(const GS3D_Clip *clip,unsigned long ticks,
                    GS3D_ClipSample *sample){
    GS3D_ClipSample s;int r;
    r=gs3d_clip_validate_current(clip);if(r)return r;
    r=gs3d_clip_resolve(clip,ticks,&s);if(r)return r;
    r=gs3d_clip_dispatch(clip,(unsigned int)clip->group,&s);
    if(!r&&sample)*sample=s;
    return r;
}
/* Shared-frame index playback across mesh groups is OPT-IN. All groups with
   frames must cover the requested clip; validate before mutating any. */
int gs3d_clip_apply_all(const GS3D_Clip *clip,unsigned long ticks,
                        GS3D_ClipSample *sample){
    GS3D_ClipSample s;unsigned int g;int r;
    r=gs3d_clip_validate_current(clip);if(r)return r;
    r=gs3d_clip_resolve(clip,ticks,&s);if(r)return r;
    for(g=0U;g<gs3d_model.groups;g++){
        if(!gs3d_model.group[g].frame_count)continue;
        if(s.frame_a>=gs3d_model.group[g].frame_count ||
           s.frame_b>=gs3d_model.group[g].frame_count)return GS3D_ERR_RANGE;
    }
    for(g=0U;g<gs3d_model.groups;g++){
        if(!gs3d_model.group[g].frame_count)continue;
        r=gs3d_clip_dispatch(clip,g,&s);if(r)return r;
    }
    if(sample)*sample=s;
    return GS3D_OK;
}
