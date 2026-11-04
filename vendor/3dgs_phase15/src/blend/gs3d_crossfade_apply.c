#include "../gs3d_internal.h"
static int blend_group(unsigned int group,unsigned int track,
    const GS3D_Crossfade *f,const GS3D_BlendSample *s){
    if(track==GS3D_CLIP_TRACK_SKELETAL)
        return gs3d_blend_skeletal(group,&s->from,&s->to,s->weight_q16);
    return gs3d_blend_morph(group,&f->from,&s->from,
        &f->to,&s->to,s->weight_q16);
}
int gs3d_crossfade_apply(const GS3D_Crossfade *f,GS3D_BlendSample *out){
    GS3D_BlendSample sample;unsigned int track;int r;
    r=gs3d_blend_validate(f,&track);if(r)return r;
    r=gs3d_crossfade_sample(f,&sample);if(r)return r;
    r=blend_group(f->from.group,track,f,&sample);
    if(!r && out)*out=sample;
    return r;
}
int gs3d_crossfade_apply_all(const GS3D_Crossfade *f,GS3D_BlendSample *out){
    GS3D_BlendSample sample;unsigned int track,g;int r;
    r=gs3d_blend_validate(f,&track);if(r)return r;
    r=gs3d_crossfade_sample(f,&sample);if(r)return r;
    /* Validate ALL groups before mutating any group's vertices. */
    for(g=0U;g<gs3d_model.groups;g++){
        const GS3D_Group *q=&gs3d_model.group[g];
        if(!q->frame_count)continue;
        if(sample.from.frame_a>=q->frame_count||sample.from.frame_b>=q->frame_count||
           sample.to.frame_a>=q->frame_count||sample.to.frame_b>=q->frame_count)
            return GS3D_ERR_RANGE;
    }
    for(g=0U;g<gs3d_model.groups;g++){
        if(!gs3d_model.group[g].frame_count)continue;
        r=blend_group(g,track,f,&sample);if(r)return r;
    }
    if(out)*out=sample;
    return GS3D_OK;
}
