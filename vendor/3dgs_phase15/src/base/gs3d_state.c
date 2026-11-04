#include "../gs3d_internal.h"
GS3D_Model gs3d_model;
int gs3d_pose_ready=0;
unsigned char gs3d_rgba[GS3D_MAX_TEXELS*4UL];
unsigned char gs3d_palette[768];
unsigned long gs_legacy_scale[3];
long gs_legacy_offset[3];
unsigned short gs7_header_stride[10];
GS3D_FileSections gs_file_sections;
void gs3d_close(void) {
    if(gs_owned_file && gs_fp) fclose(gs_fp);
    gs3d_pose_ready=0;
    gs_fp=NULL; gs_owned_file=0; gs_is_open=0; gs_size=0UL;
    gs_mem=NULL; gs_mem_size=0UL; gs_cursor=0UL;
    memset(&gs_source,0,sizeof(gs_source));
}
