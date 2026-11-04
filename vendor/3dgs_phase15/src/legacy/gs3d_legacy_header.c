#include "../gs3d_internal.h"
int gs_legacy_header(unsigned long *cursor){
    unsigned char h[84];unsigned long i,nskins,uvcount,nverts,ntris,nframes,skinw,skinh;
    GS3D_Group *g=&gs3d_model.group[0];
    if(gs_get(0UL,h,84UL))return GS3D_ERR_RANGE;
    for(i=0UL;i<3UL;i++){
        gs_legacy_scale[i]=gs_u32(h+8UL+i*4UL);
        gs_legacy_offset[i]=gs3d_ieee_q16(gs_u32(h+20UL+i*4UL));
    }
    nskins=gs_u32(h+48);skinw=gs_u32(h+52);skinh=gs_u32(h+56);
    nverts=gs_u32(h+60);ntris=gs_u32(h+64);nframes=gs_u32(h+68);uvcount=gs_u32(h+72);
    if(!nverts||!ntris||!nframes||nverts>GS3D_MAX_VERTS||ntris>GS3D_MAX_FACES||
       uvcount>GS3D_MAX_UVS||nframes>GS3D_MAX_FRAMES||nskins>GS3D_MAX_SKINS)
        return GS3D_ERR_CAPACITY;
    if(skinw>65535UL||skinh>65535UL)return GS3D_ERR_CAPACITY;
    gs3d_model.groups=1U;strcpy(g->name,"LegacyMesh");
    g->vertex_count=nverts;g->face_count=ntris;g->uv_count=uvcount;
    g->skin_count=(unsigned short)nskins;g->frame_count=(unsigned short)nframes;
    g->vertex_stride=gs3d_model.format==3 ? 4U:8U;
    gs3d_model.vertices=nverts;gs3d_model.uvs=uvcount;gs3d_model.faces=ntris;
    *cursor=84UL;return GS3D_OK;
}
