#include "../gs3d_internal.h"
int gs7_bones(unsigned long *cursor){
    unsigned long i,j,cur=*cursor,end,bone_stride=(unsigned long)gs7_header_stride[0];
    unsigned char tmp[16],nb[32];
    for(i=0UL;i<gs3d_model.bones;i++){
        GS3D_Bone *bone=&gs3d_model.bone[i];
        unsigned long t;
        if(cur>gs_file_sections.mdl_data_end || bone_stride>gs_file_sections.mdl_data_end-cur)return GS3D_ERR_RANGE;
        if(gs_get(cur,tmp,16UL))return GS3D_ERR_RANGE;
        bone->parent=(unsigned short)gs_u16(tmp);
        if(bone->parent!=65535U && bone->parent>=gs3d_model.bones)return GS3D_ERR_CORRUPT;
        for(j=0UL;j<3UL;j++)bone->xyz[j]=gs3d_ieee_q16(gs_u32(tmp+4UL+j*4UL));
        t=bone_stride-16UL;if(t>32UL)t=32UL;
        if(t){if(gs_get(cur+16UL,nb,t))return GS3D_ERR_RANGE;
            memcpy(bone->name,nb,(size_t)t);bone->name[t]='\0';}
        if(gs_addmul(cur,1UL,bone_stride,&end))return GS3D_ERR_RANGE;
        cur=end;
    }
    /* Reject a parent-cycle before building a skeleton. */
    for(i=0UL;i<gs3d_model.bones;i++){
        unsigned long step=0UL;unsigned int p=(unsigned int)i;
        while(p!=65535U){
            if(step++>=gs3d_model.bones)return GS3D_ERR_CORRUPT;
            p=(unsigned int)gs3d_model.bone[p].parent;
        }
    }
    *cursor=cur;return GS3D_OK;
}
