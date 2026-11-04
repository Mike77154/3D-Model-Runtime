#include "../gs3d_internal.h"
/* Generation profiles select formats. The binary readers remain shared. */
int gs3d_open_family(const char *path,unsigned int family){
    int r=gs3d_open(path);
    if(r)return r;
    if(gs3d_family_supports(family,gs3d_model.format))return GS3D_OK;
    gs3d_close();return GS3D_ERR_UNSUPPORTED;
}
