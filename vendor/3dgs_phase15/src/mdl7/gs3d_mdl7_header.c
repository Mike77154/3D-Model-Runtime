#include "../gs3d_internal.h"
int gs7_header(unsigned long *cur){
    unsigned char h[48];unsigned long groups,bones,i;int r;
    if(gs_get(0UL,h,48UL))return GS3D_ERR_RANGE;
    bones=gs_u32(h+8);groups=gs_u32(h+12);
    r=gs7_sections_init(gs_u32(h+16),gs_u32(h+20),gs_u32(h+24));
    if(r)return r;
    if(!groups||groups>GS3D_MAX_GROUPS||bones>GS3D_MAX_BONES)return GS3D_ERR_CAPACITY;
    gs3d_model.groups=(unsigned short)groups;gs3d_model.bones=(unsigned short)bones;
    for(i=0UL;i<10UL;i++)gs7_header_stride[i]=(unsigned short)gs_u16(h+28UL+i*2UL);
    if(gs7_header_stride[0]<16U || gs7_header_stride[0]>256U ||
       gs7_header_stride[1]!=28U || gs7_header_stride[4]<8U ||
       gs7_header_stride[5]<6U || gs7_header_stride[6]<16U ||
       gs7_header_stride[6]>26U || gs7_header_stride[7]<16U ||
       gs7_header_stride[9]<24U)return GS3D_ERR_UNSUPPORTED;
    *cur=48UL;return GS3D_OK;
}
