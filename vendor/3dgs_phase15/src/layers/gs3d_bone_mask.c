#include "../gs3d_internal.h"
/* All bone masks are runtime-owned controls; legacy MDL formats do not
   serialize bone weights. Descendant checks are iterative and bounded. */
int gs3d_mask_validate(const GS3D_BoneMask *m){
    unsigned int i;
    if(!m||!gs_is_open)return GS3D_ERR_RANGE;
    if(gs3d_model.format!=7||!gs3d_model.bones)return GS3D_ERR_UNSUPPORTED;
    if(m->bone_count!=gs3d_model.bones)return GS3D_ERR_RANGE;
    for(i=0U;i<m->bone_count;i++)
        if(m->weight_q16[i]<0L||m->weight_q16[i]>GS3D_Q16_ONE)
            return GS3D_ERR_RANGE;
    return GS3D_OK;
}
int gs3d_mask_init(GS3D_BoneMask *m,long initial){
    unsigned int i;
    if(!m||initial<0L||initial>GS3D_Q16_ONE)return GS3D_ERR_RANGE;
    if(!gs_is_open||gs3d_model.format!=7||!gs3d_model.bones)
        return GS3D_ERR_UNSUPPORTED;
    m->bone_count=gs3d_model.bones;
    for(i=0U;i<GS3D_MAX_BONES;i++)m->weight_q16[i]=0L;
    for(i=0U;i<m->bone_count;i++)m->weight_q16[i]=initial;
    return GS3D_OK;
}
int gs3d_mask_set(GS3D_BoneMask *m,unsigned int bone,long weight){
    int r=gs3d_mask_validate(m);
    if(r)return r;
    if(bone>=m->bone_count||weight<0L||weight>GS3D_Q16_ONE)
        return GS3D_ERR_RANGE;
    m->weight_q16[bone]=weight;
    return GS3D_OK;
}
int gs3d_mask_subtree(GS3D_BoneMask *m,unsigned int root,long weight){
    unsigned int i,idx,hops;int r;
    r=gs3d_mask_validate(m);if(r)return r;
    if(root>=m->bone_count||weight<0L||weight>GS3D_Q16_ONE)
        return GS3D_ERR_RANGE;
    /* Validate the entire parent graph before mutating the mask. */
    for(i=0U;i<m->bone_count;i++){
        idx=i;
        for(hops=0U;hops<m->bone_count;hops++){
            unsigned int parent=gs3d_model.bone[idx].parent;
            if(parent==65535U)break;
            if(parent>=m->bone_count)return GS3D_ERR_CORRUPT;
            idx=parent;
        }
        if(hops>=m->bone_count)return GS3D_ERR_CORRUPT;
    }
    for(i=0U;i<m->bone_count;i++){
        idx=i;
        for(hops=0U;hops<m->bone_count;hops++){
            unsigned int parent;
            if(idx==root){m->weight_q16[i]=weight;break;}
            parent=gs3d_model.bone[idx].parent;
            if(parent==65535U)break;
            idx=parent;
        }
    }
    return GS3D_OK;
}
