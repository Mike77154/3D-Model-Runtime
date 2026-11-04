#include "../gs3d_internal.h"
int gs_mdl7_weight_info(unsigned long v,GS3D_WeightSet *out){
    if(!out||v>=gs3d_model.vertices)return GS3D_ERR_RANGE;
    *out=gs_vertex_weights[v];
    if(out->count)return GS3D_OK;
    if(gs3d_model.v[v].bone<gs3d_model.bones){
        out->kind=GS3D_DEFORM_RIGID_BONE;out->count=1U;
        out->item[0].bone=gs3d_model.v[v].bone;
        out->item[0].weight_q16=65536L;
    }
    return GS3D_OK;
}
