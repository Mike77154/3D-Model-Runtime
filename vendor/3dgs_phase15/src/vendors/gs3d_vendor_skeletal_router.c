#include "../gs3d_internal.h"
#define DECL(G) int gs3d_a##G##_sample_skeletal(unsigned int,unsigned int,unsigned int,long);
DECL(4) DECL(5) DECL(6) DECL(7) DECL(8)
int gs3d_vendor_sample_skeletal(unsigned int generation,unsigned int group,
                                unsigned int a,unsigned int b,long alpha){
    switch(generation){
    case 4U:return gs3d_a4_sample_skeletal(group,a,b,alpha);
    case 5U:return gs3d_a5_sample_skeletal(group,a,b,alpha);
    case 6U:return gs3d_a6_sample_skeletal(group,a,b,alpha);
    case 7U:return gs3d_a7_sample_skeletal(group,a,b,alpha);
    case 8U:return gs3d_a8_sample_skeletal(group,a,b,alpha);
    default:return GS3D_ERR_UNSUPPORTED;
    }
}
