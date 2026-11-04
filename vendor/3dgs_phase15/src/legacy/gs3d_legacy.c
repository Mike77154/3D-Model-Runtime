#include "../gs3d_internal.h"
/* Each stage owns its own file layout and returns a checked cursor. */
int gs_legacy(void){
    unsigned long cur=0UL;int r;
    r=gs_legacy_header(&cur);if(r)return r;
    r=gs_legacy_skins(&cur);if(r)return r;
    r=gs_legacy_geometry(&cur);if(r)return r;
    r=gs_legacy_frames(&cur);if(r)return r;
    gs_deformer_info[0].kind=GS3D_DEFORM_VERTEX_FRAME;
    return gs_legacy_frame(0U,0U);
}
