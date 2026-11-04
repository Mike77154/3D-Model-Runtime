#include "../gs3d_internal.h"
int gs3d_get_vertex_weights(unsigned long vertex,GS3D_WeightSet *out){
    if(!out||!gs_is_open||vertex>=gs3d_model.vertices)return GS3D_ERR_RANGE;
    return gs3d_format_weights(vertex,out);
}
int gs3d_get_deformer_info(unsigned int group,GS3D_DeformerInfo *out){
    if(!out||!gs_is_open||group>=gs3d_model.groups)return GS3D_ERR_RANGE;
    *out=gs_deformer_info[group];return GS3D_OK;
}
