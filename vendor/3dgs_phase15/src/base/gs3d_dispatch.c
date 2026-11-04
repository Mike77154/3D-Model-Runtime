#include "../gs3d_internal.h"
static int gs3d_parse(void){
    unsigned char magic[4];int f,r;
    if(gs_get(0UL,magic,4UL)) {gs3d_close();return GS3D_ERR_RANGE;}
    f=gs3d_detect_magic(magic,4UL);
    if(f<0){gs3d_close();return f;}
    gs3d_model.format=f; gs3d_model.file_size=gs_size;
    if(f==7){
#if GS3D_WITH_MDL7
        r=gs_mdl7();
#else
        r=GS3D_ERR_UNSUPPORTED;
#endif
    }else{
#if GS3D_WITH_LEGACY
        r=gs_legacy();
#else
        r=GS3D_ERR_UNSUPPORTED;
#endif
    }
    if(r){gs3d_close();memset(&gs3d_model,0,sizeof(gs3d_model));}
    return r;
}
int gs3d_open_source(const GS3D_Source *src){
    if(!src||!src->read_at||src->size<4UL)return GS3D_ERR_RANGE;
    gs3d_close(); memset(&gs3d_model,0,sizeof(gs3d_model));gs_weights_reset();memset(&gs_file_sections,0,sizeof(gs_file_sections));
    gs_source=*src;gs_size=src->size;gs_is_open=1;
    return gs3d_parse();
}
int gs3d_open_memory(const unsigned char *bytes,unsigned long size){
    if(!bytes||size<4UL)return GS3D_ERR_RANGE;
    gs3d_close();memset(&gs3d_model,0,sizeof(gs3d_model));gs_weights_reset();memset(&gs_file_sections,0,sizeof(gs_file_sections));
    gs_mem=bytes;gs_mem_size=size;gs_size=size;gs_is_open=1;
    return gs3d_parse();
}
int gs3d_open(const char *path){
    long sz;
    if(!path)return GS3D_ERR_RANGE;
    gs3d_close();memset(&gs3d_model,0,sizeof(gs3d_model));gs_weights_reset();memset(&gs_file_sections,0,sizeof(gs_file_sections));
    gs_fp=fopen(path,"rb");if(!gs_fp)return GS3D_ERR_IO;
    gs_owned_file=1;gs_is_open=1;
    if(fseek(gs_fp,0,SEEK_END)!=0){gs3d_close();return GS3D_ERR_IO;}
    sz=ftell(gs_fp);if(sz<0){gs3d_close();return GS3D_ERR_IO;}
    gs_size=(unsigned long)sz;
    return gs3d_parse();
}
int gs3d_apply_frame(unsigned int group,unsigned int frame){
    if(!gs_is_open||group>=gs3d_model.groups||frame>=gs3d_model.group[group].frame_count)
        return GS3D_ERR_RANGE;
    if(gs3d_model.format==7){
#if GS3D_WITH_MDL7
        return gs_mdl7_frame(group,frame);
#else
        return GS3D_ERR_UNSUPPORTED;
#endif
    }
#if GS3D_WITH_LEGACY
    return gs_legacy_frame(group,frame);
#else
    return GS3D_ERR_UNSUPPORTED;
#endif
}
const char *gs3d_error_name(int r){
    switch(r){case 0:return "OK";case -1:return "I/O";case -2:return "magic";
    case -3:return "out-of-file/range";case -4:return "unsupported feature";
    case -5:return "corrupt record";case -6:return "fixed capacity exceeded";}
    return "unknown error";
}
