#include "../gs3d_internal.h"
int gs_legacy_geometry(unsigned long *cursor){
    unsigned long off,cur=*cursor,uvcount,ntris,nverts,nskins,skinw,skinh,i,j;
    unsigned char b[12],h[84];GS3D_Group *g=&gs3d_model.group[0];
    nskins=g->skin_count;uvcount=g->uv_count;ntris=g->face_count;nverts=g->vertex_count;
    if(gs_get(0UL,h,84UL))return GS3D_ERR_RANGE;
    off=cur;
    if(gs_addmul(cur,uvcount,4UL,&cur))return GS3D_ERR_RANGE;
    g->uv_first=0UL;
    for(i=0;i<uvcount;i++){
        if(gs_get(off+i*4UL,b,4UL))return GS3D_ERR_RANGE;
        /* MDL5 UVs use pixel coordinates of the embedded skin (not header dims). */
        skinw=nskins ? g->skins[0].width : gs_u32(h+52);
        skinh=nskins ? g->skins[0].height : gs_u32(h+56);
        if(!skinw||!skinh)skinw=skinh=1UL;
        gs3d_model.uv[i].u=((long)gs_s16(b)*65536L+32768L)/(long)skinw;
        gs3d_model.uv[i].v=65536L-((long)gs_s16(b+2)*65536L+32768L)/(long)skinh;
    }
    off=cur;
    if(gs_addmul(cur,ntris,12UL,&cur))return GS3D_ERR_RANGE;
    for(i=0;i<ntris;i++){
        GS3D_Face *f=&gs3d_model.face[i];
        if(gs_get(off+i*12UL,b,12UL))return GS3D_ERR_RANGE;
        for(j=0;j<3UL;j++){
            f->vi[j]=(unsigned short)gs_u16(b+j*2UL);
            f->ti[j]=(unsigned short)gs_u16(b+6UL+j*2UL);
            if(f->vi[j]>=nverts || (uvcount && f->ti[j]>=uvcount))return GS3D_ERR_CORRUPT;
        }
    }
    *cursor=cur;return 0;
}
