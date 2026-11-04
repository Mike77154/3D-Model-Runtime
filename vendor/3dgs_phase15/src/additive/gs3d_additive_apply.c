#include "../gs3d_internal.h"
static int apply(const GS3D_AdditiveRequest *r,const GS3D_BoneMask *m,int all){
    GS3D_ClipSample b,a,n;unsigned int g;int s;
    if(gs_is_open && gs3d_model.format>=3 && gs3d_model.format<=5){
        if(m)return GS3D_ERR_UNSUPPORTED; /* No bone mask in legacy. */
        if(!r)return GS3D_ERR_RANGE;
        if(!all)return gs3d_additive_legacy(r->base.group,r);
        for(g=0U;g<gs3d_model.groups;g++){
            s=gs3d_additive_legacy(g,r);if(s)return s;
        }
        return GS3D_OK;
    }
    s=gs3d_additive_validate(r,m);if(s)return s;
    s=gs3d_clip_resolve(&r->base,r->base_ticks,&b);if(s)return s;
    s=gs3d_clip_resolve(&r->additive,r->additive_ticks,&a);if(s)return s;
    s=gs3d_clip_resolve(&r->reference,r->reference_ticks,&n);if(s)return s;
    if(all){
        for(g=0U;g<gs3d_model.groups;g++){
            const GS3D_Group *group=&gs3d_model.group[g];
            if(!group->frame_count)continue;
            if(b.frame_a>=group->frame_count||b.frame_b>=group->frame_count||
               a.frame_a>=group->frame_count||a.frame_b>=group->frame_count||
               n.frame_a>=group->frame_count||n.frame_b>=group->frame_count)
                return GS3D_ERR_RANGE;
        }
        for(g=0U;g<gs3d_model.groups;g++){
            if(!gs3d_model.group[g].frame_count)continue;
            s=gs3d_additive_skeletal(g,r,m,&b,&a,&n);if(s)return s;
        }
        return GS3D_OK;
    }
    return gs3d_additive_skeletal(r->base.group,r,m,&b,&a,&n);
}
int gs3d_additive_apply(const GS3D_AdditiveRequest *r,
                        const GS3D_BoneMask *m){return apply(r,m,0);}
int gs3d_additive_apply_all(const GS3D_AdditiveRequest *r,
                            const GS3D_BoneMask *m){return apply(r,m,1);}
