#include "../gs3d_internal.h"
int gs7_frames(GS3D_Group *g,unsigned long *cursor,unsigned long group_end){
    unsigned long cur=*cursor,nf=g->frame_count,j,end;
    unsigned long fr_stride=(unsigned long)gs7_header_stride[9];
    unsigned long fv_stride=(unsigned long)gs7_header_stride[7];
    unsigned long bt_stride=(unsigned long)gs7_header_stride[8];
    GS3D_Frame *f;unsigned char tmp[24];
    for(j=0;j<nf;j++){
        f=&g->frames[j];
        if(cur>group_end || 24UL>group_end-cur)return GS3D_ERR_RANGE;
        if(gs_get(cur,tmp,24UL))return GS3D_ERR_RANGE;
        memcpy(f->name,tmp,16UL);f->name[16]='\0';
        f->count=gs_u32(tmp+16);f->trans_count=gs_u32(tmp+20);
        if(f->count>GS3D_MAX_VERTS || f->trans_count>GS3D_MAX_BONES)return GS3D_ERR_CAPACITY;
        f->offset=cur+fr_stride;
        if(gs_addmul(cur,1UL,fr_stride,&end))return GS3D_ERR_RANGE;
        cur=end;
        if(gs_addmul(cur,f->count,fv_stride,&end))return GS3D_ERR_RANGE;
        cur=end;
        f->trans_offset=cur;
        if(gs_addmul(cur,f->trans_count,bt_stride,&end))return GS3D_ERR_RANGE;
        cur=end;
        if(cur>group_end)return GS3D_ERR_RANGE;
    }
    *cursor=cur;return GS3D_OK;
}
int gs_mdl7_frame(unsigned int group,unsigned int frame){
    unsigned long i,idx,p;GS3D_Group *g=&gs3d_model.group[group];
    GS3D_Frame *f=&g->frames[frame];GS3D_Vec v;unsigned char b[2];int r;
    /* Restore bind geometry: every frame's overrides are independent. */
    for(i=0;i<g->vertex_count;i++){
        r=gs_decode_vertex7(g->base_offset+i*(unsigned long)g->vertex_stride,
                            g->vertex_stride,&gs3d_model.v[g->vertex_first+i]);
        if(r)return r;
    }
    for(i=0;i<f->count;i++){
        p=f->offset+i*(unsigned long)g->frame_stride;
        if(gs_get(p+12UL,b,2UL))return GS3D_ERR_RANGE;
        idx=(unsigned long)gs_u16(b);
        if(idx>=g->vertex_count)return GS3D_ERR_CORRUPT;
        r=gs_decode_vertex7(p,g->frame_stride,&v);if(r)return r;
        /* Frame vertindex is a *vertex* selector, NOT a skin bone.
           Keep the original bone attachment when replacing positions. */
        v.bone=gs3d_model.v[g->vertex_first+idx].bone;
        gs3d_model.v[g->vertex_first+idx]=v;
    }
    /* Bone transformation matrices are not applied in this version. */
    return 0;
}

