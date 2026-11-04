#include "../gs3d_internal.h"
/* Preserve MDL7 4x4 matrix keys for a downstream skeleton/skin engine.
   Deliberately does not apply unknown bone-space conventions here. */
int gs3d_read_bone_key(unsigned int group,unsigned int frame,
                       unsigned long key,GS3D_BoneKey *out){
    GS3D_Group *g;GS3D_Frame *f;unsigned char b[68];
    unsigned long off;unsigned int i;
    if(!gs_is_open||!out||gs3d_model.format!=7||group>=gs3d_model.groups)
        return GS3D_ERR_RANGE;
    g=&gs3d_model.group[group];
    if(frame>=g->frame_count)return GS3D_ERR_RANGE;
    f=&g->frames[frame];
    if(key>=f->trans_count)return GS3D_ERR_RANGE;
    if(gs7_header_stride[8]<68U)return GS3D_ERR_UNSUPPORTED;
    if(key>(ULONG_MAX-f->trans_offset)/(unsigned long)gs7_header_stride[8])return GS3D_ERR_RANGE;
    off=f->trans_offset+key*(unsigned long)gs7_header_stride[8];
    if(gs_get(off,b,68UL))return GS3D_ERR_RANGE;
    out->bone=(unsigned short)gs_u16(b+64);
    if(out->bone>=gs3d_model.bones)return GS3D_ERR_CORRUPT;
    for(i=0U;i<16U;i++)out->matrix[i]=gs3d_ieee_q16(gs_u32(b+i*4U));
    return GS3D_OK;
}
