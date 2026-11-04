#include "../gs3d_internal.h"
int gs7_geometry(GS3D_Group *g,unsigned long *cursor,unsigned long group_end,
                 unsigned long uv_stride,unsigned long tri_stride,unsigned long vert_stride){
    unsigned long cur=*cursor,nuv=g->uv_count,ntri=g->face_count,nv=g->vertex_count;
    unsigned long off,uv0,faces0,verts0,j,k;
    unsigned char tmp[32];GS3D_Face *face;int r;
    off=cur;if(gs_addmul(cur,nuv,uv_stride,&cur))return GS3D_ERR_RANGE;
    if(cur>group_end)return GS3D_ERR_RANGE;
    uv0=g->uv_first;
    for(j=0;j<nuv;j++){
        GS3D_UV *uv=&gs3d_model.uv[uv0+j];
        if(gs_get(off+j*uv_stride,tmp,8UL))return GS3D_ERR_RANGE;
        uv->u=gs3d_ieee_q16(gs_u32(tmp));
        uv->v=65536L-gs3d_ieee_q16(gs_u32(tmp+4));
    }
    g->tri_offset=cur;
    if(gs_addmul(cur,ntri,tri_stride,&cur))return GS3D_ERR_RANGE;
    if(cur>group_end)return GS3D_ERR_RANGE;
    faces0=g->face_first;
    for(j=0;j<ntri;j++){
        face=&gs3d_model.face[faces0+j];
        if(gs_get(g->tri_offset+j*tri_stride,tmp,tri_stride>32UL?32UL:tri_stride))return GS3D_ERR_RANGE;
        for(k=0;k<3UL;k++){
        face->vi[k]=(unsigned short)gs_u16(tmp+k*2UL);
        if(face->vi[k]>=nv)return GS3D_ERR_CORRUPT;
        if(tri_stride>=12UL){
            face->ti[k]=(unsigned short)gs_u16(tmp+6UL+k*2UL);
            if(nuv && face->ti[k]>=nuv)return GS3D_ERR_CORRUPT;
        }
        if(tri_stride>=26UL){
            /* MED writes 0xffff UV indices when the second skinset is unused.
               Only validate second UV indices if second material is present. */
            face->ti2[k]=(unsigned short)gs_u16(tmp+16UL+k*2UL);
            if(gs_u32(tmp+22UL)!=0xffffffffUL && nuv &&
               face->ti2[k]>=nuv)return GS3D_ERR_CORRUPT;
        }
        }
        if(tri_stride>=16UL) face->material=(unsigned short)gs_u32(tmp+12);
        if(tri_stride>=26UL) face->material2=(unsigned short)gs_u32(tmp+22);
    }
    g->base_offset=cur;
    if(gs_addmul(cur,nv,vert_stride,&cur))return GS3D_ERR_RANGE;
    if(cur>group_end)return GS3D_ERR_RANGE;
    verts0=g->vertex_first;
    for(j=0;j<nv;j++){
        r=gs_decode_vertex7(g->base_offset+j*vert_stride,(unsigned int)vert_stride,
                &gs3d_model.v[verts0+j]);
        if(r)return r;
    }
    *cursor=cur;return GS3D_OK;
}
