#include "../../gs3d_internal.h"
/* Generation 5: query actual weights or implicit vertex-frame influence. */
int gs3d_a5_weights(unsigned long vertex,GS3D_WeightSet *out){
    if(!gs_is_open||!gs3d_family_supports(5U,gs3d_model.format))return GS3D_ERR_UNSUPPORTED;
    return gs3d_get_vertex_weights(vertex,out);
}
