#include "../gs3d_internal.h"
#define DECL(G) \
 int gs3d_a##G##_deform(unsigned int,unsigned int); \
 int gs3d_a##G##_weights(unsigned long,GS3D_WeightSet *);
DECL(4) DECL(5) DECL(6) DECL(7) DECL(8)
int gs3d_vendor_deform(unsigned int generation,unsigned int group,unsigned int frame){
    switch(generation){
    case 4U:return gs3d_a4_deform(group,frame);
    case 5U:return gs3d_a5_deform(group,frame);
    case 6U:return gs3d_a6_deform(group,frame);
    case 7U:return gs3d_a7_deform(group,frame);
    case 8U:return gs3d_a8_deform(group,frame);
    default:return GS3D_ERR_UNSUPPORTED;
    }
}
int gs3d_vendor_weights(unsigned int generation,unsigned long vertex,GS3D_WeightSet *out){
    switch(generation){
    case 4U:return gs3d_a4_weights(vertex,out);
    case 5U:return gs3d_a5_weights(vertex,out);
    case 6U:return gs3d_a6_weights(vertex,out);
    case 7U:return gs3d_a7_weights(vertex,out);
    case 8U:return gs3d_a8_weights(vertex,out);
    default:return GS3D_ERR_UNSUPPORTED;
    }
}
int gs3d_vendor_deformer_info(unsigned int generation,unsigned int group,GS3D_DeformerInfo *out){
    if(!gs_is_open||!gs3d_family_supports(generation,gs3d_model.format))return GS3D_ERR_UNSUPPORTED;
    return gs3d_get_deformer_info(group,out);
}
