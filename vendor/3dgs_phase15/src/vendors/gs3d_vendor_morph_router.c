#include "../gs3d_internal.h"
#define DECL(N) int gs3d_a##N##_sample_morph(unsigned int,unsigned int,unsigned int,long);
DECL(4) DECL(5) DECL(6) DECL(7) DECL(8)
int gs3d_vendor_sample_morph(unsigned int generation,unsigned int group,
                              unsigned int frame_a,unsigned int frame_b,long alpha_q16){
    switch(generation){
    case 4U:return gs3d_a4_sample_morph(group,frame_a,frame_b,alpha_q16);
    case 5U:return gs3d_a5_sample_morph(group,frame_a,frame_b,alpha_q16);
    case 6U:return gs3d_a6_sample_morph(group,frame_a,frame_b,alpha_q16);
    case 7U:return gs3d_a7_sample_morph(group,frame_a,frame_b,alpha_q16);
    case 8U:return gs3d_a8_sample_morph(group,frame_a,frame_b,alpha_q16);
    default:return GS3D_ERR_UNSUPPORTED;
    }
}
