#include "../../gs3d_internal.h"
/* A8 vendor: morph interpolation is format-routed; skeletal animation stays separate. */
int gs3d_a8_sample_morph(unsigned int group,unsigned int frame_a,
                            unsigned int frame_b,long alpha_q16){
    if(!gs_is_open||!gs3d_family_supports(8U,gs3d_model.format))
        return GS3D_ERR_UNSUPPORTED;
    return gs3d_sample_morph_q16(group,frame_a,frame_b,alpha_q16);
}
