#include "../gs3d_internal.h"
static int sample_layer(const GS3D_LayerRequest *req,
    GS3D_ClipSample *a,GS3D_ClipSample *b){
    int r=gs3d_clip_resolve(&req->base,req->base_ticks,a);
    if(r)return r;
    return gs3d_clip_resolve(&req->overlay,req->overlay_ticks,b);
}
static int apply_layer(const GS3D_LayerRequest *req,
    const GS3D_BoneMask *mask,GS3D_RootDelta *root,int all){
    GS3D_ClipSample a,b;GS3D_RootDelta motion;unsigned int g;int r;
    r=gs3d_layer_validate(req,mask);if(r)return r;
    if(req->consume_root_motion && !root)return GS3D_ERR_RANGE;
    r=sample_layer(req,&a,&b);if(r)return r;
    /* Check ALL groups before modifying vertex buffers. */
    if(all){
        for(g=0U;g<gs3d_model.groups;g++){
            const GS3D_Group *p=&gs3d_model.group[g];
            if(!p->frame_count)continue;
            if(a.frame_a>=p->frame_count||a.frame_b>=p->frame_count||
               b.frame_a>=p->frame_count||b.frame_b>=p->frame_count)
                return GS3D_ERR_RANGE;
        }
    }
    if(root){
        memset(&motion,0,sizeof(motion));
        if(req->base.mode==GS3D_CLIP_CLAMP &&
           req->previous_base_ticks<=req->base_ticks){
            r=gs3d_root_motion_delta(&req->base,req->previous_base_ticks,
                                    req->base_ticks,req->root_bone,&motion);
            if(r)return r;
        }else return GS3D_ERR_UNSUPPORTED;
    }
    if(all){
        for(g=0U;g<gs3d_model.groups;g++){
            if(!gs3d_model.group[g].frame_count)continue;
            r=gs3d_layer_skeletal(g,req,mask,&a,&b);if(r)return r;
        }
    }else{
        r=gs3d_layer_skeletal(req->base.group,req,mask,&a,&b);if(r)return r;
    }
    if(root)*root=motion;
    return GS3D_OK;
}
int gs3d_layer_apply(const GS3D_LayerRequest *req,
    const GS3D_BoneMask *mask,GS3D_RootDelta *root){
    return apply_layer(req,mask,root,0);
}
int gs3d_layer_apply_all(const GS3D_LayerRequest *req,
    const GS3D_BoneMask *mask,GS3D_RootDelta *root){
    return apply_layer(req,mask,root,1);
}
