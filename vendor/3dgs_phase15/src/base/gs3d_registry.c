#include "../../include/gs3d_registry.h"
static const GS3D_FormatInfo gs3d_formats[] = {
    {3,"3DGS MDL3",GS3D_FEATURE_VERTEX_FRAMES|GS3D_FEATURE_SKINS},
    {4,"3DGS MDL4",GS3D_FEATURE_VERTEX_FRAMES|GS3D_FEATURE_SKINS},
    {5,"3DGS MDL5",GS3D_FEATURE_VERTEX_FRAMES|GS3D_FEATURE_SKINS},
    {7,"3DGS MDL7",GS3D_FEATURE_VERTEX_FRAMES|GS3D_FEATURE_SKINS|GS3D_FEATURE_BONES|GS3D_FEATURE_UV2}
};
unsigned int gs3d_format_count(void){return (unsigned int)(sizeof(gs3d_formats)/sizeof(gs3d_formats[0]));}
const GS3D_FormatInfo *gs3d_format_info(unsigned int i){
    if(i>=gs3d_format_count())return 0;
    return &gs3d_formats[i];
}
const GS3D_FormatInfo *gs3d_format_by_signature(int magic){
    unsigned int i;for(i=0;i<gs3d_format_count();i++)if(gs3d_formats[i].signature==magic)return &gs3d_formats[i];
    return 0;
}
int gs3d_detect_magic(const unsigned char *bytes,unsigned long n){
    if(!bytes||n<4UL)return GS3D_ERR_RANGE;
    if(bytes[0]!='M'||bytes[1]!='D'||bytes[2]!='L')return GS3D_ERR_MAGIC;
    if(bytes[3]=='3'||bytes[3]=='4'||bytes[3]=='5'||bytes[3]=='7')return (int)(bytes[3]-'0');
    return GS3D_ERR_UNSUPPORTED;
}
int gs3d_family_supports(unsigned int family,int fmt){
    if(family==4U)return fmt==3;
    if(family==5U)return fmt==4||fmt==5;
    if(family==6U)return fmt==4||fmt==5||fmt==7;
    if(family==7U||family==8U)return fmt==4||fmt==5||fmt==7;
    return 0;
}
