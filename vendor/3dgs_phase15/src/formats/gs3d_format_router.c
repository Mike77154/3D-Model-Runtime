#include "../gs3d_internal.h"
#define DECL(N) \
 int gs3d_mdl##N##_format_deform(unsigned int,unsigned int); \
 int gs3d_mdl##N##_format_weights(unsigned long,GS3D_WeightSet *);
DECL(3) DECL(4) DECL(5) DECL(7)
int gs3d_format_deform(unsigned int group,unsigned int frame){
    switch(gs3d_model.format){
        case 3:return gs3d_mdl3_format_deform(group,frame);
        case 4:return gs3d_mdl4_format_deform(group,frame);
        case 5:return gs3d_mdl5_format_deform(group,frame);
        case 7:return gs3d_mdl7_format_deform(group,frame);
        default:return GS3D_ERR_UNSUPPORTED;
    }
}
int gs3d_format_weights(unsigned long vertex,GS3D_WeightSet *out){
    switch(gs3d_model.format){
        case 3:return gs3d_mdl3_format_weights(vertex,out);
        case 4:return gs3d_mdl4_format_weights(vertex,out);
        case 5:return gs3d_mdl5_format_weights(vertex,out);
        case 7:return gs3d_mdl7_format_weights(vertex,out);
        default:return GS3D_ERR_UNSUPPORTED;
    }
}
