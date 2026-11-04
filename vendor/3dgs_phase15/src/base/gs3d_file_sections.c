#include "../gs3d_internal.h"
/* MDL7 header data_size is the end of the model payload, not necessarily
   the end of the file: optional entity and MED editor lumps follow. */
int gs7_sections_init(unsigned long data_size,unsigned long ent_size,unsigned long med_size){
    unsigned long entity_end,med_end;
    if(data_size<48UL||data_size>gs_size)return GS3D_ERR_RANGE;
    if(gs_addmul(data_size,1UL,ent_size,&entity_end))return GS3D_ERR_RANGE;
    if(gs_addmul(entity_end,1UL,med_size,&med_end))return GS3D_ERR_RANGE;
    if(med_end>gs_size)return GS3D_ERR_RANGE;
    gs_file_sections.mdl_data_end=data_size;
    gs_file_sections.entity_lump_start=data_size;
    gs_file_sections.entity_lump_size=ent_size;
    gs_file_sections.med_lump_start=entity_end;
    gs_file_sections.med_lump_size=med_size;
    gs_file_sections.trailing_bytes=gs_size-med_end;
    return GS3D_OK;
}
int gs3d_get_file_sections(GS3D_FileSections *out){
    if(!out||!gs_is_open)return GS3D_ERR_RANGE;
    *out=gs_file_sections;return GS3D_OK;
}
