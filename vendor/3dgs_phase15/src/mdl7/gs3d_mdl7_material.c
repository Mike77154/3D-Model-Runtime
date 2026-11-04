#include "../gs3d_internal.h"
/* MDL7 MATERIAL: Diffuse RGBA at 0, Ambient RGBA at 16.
   Ambient alpha stores opacity in MED exports. Q16.16 only. */
int gs7_material_read(unsigned long off,unsigned long end,GS3D_Skin *skin){
    unsigned char b[32];unsigned int i;unsigned long bits;
    if(end<off||end-off<68UL||gs_get(off,b,32UL))return GS3D_ERR_RANGE;
    for(i=0U;i<3U;i++){
        bits=gs_u32(b+4U*i);
        if((bits&0x7f800000UL)==0x7f800000UL || (bits&0x80000000UL))return GS3D_ERR_CORRUPT;
        skin->diffuse_q16[i]=gs3d_ieee_q16(bits);
    }
    bits=gs_u32(b+28);
    if((bits&0x7f800000UL)==0x7f800000UL || (bits&0x80000000UL))return GS3D_ERR_CORRUPT;
    skin->opacity_q16=gs3d_ieee_q16(bits);
    skin->has_material=1U;
    return GS3D_OK;
}
