#include "../../gs3d_internal.h"
/* Generation A4: independent skeletal routing; only MDL7 supports bones. */
int gs3d_a4_sample_skeletal(unsigned int group,unsigned int a,
                               unsigned int b,long alpha){
    if(!gs_is_open||!gs3d_family_supports(4U,gs3d_model.format))
        return GS3D_ERR_UNSUPPORTED;
    return gs3d_sample_skeletal_q16(group,a,b,alpha);
}
