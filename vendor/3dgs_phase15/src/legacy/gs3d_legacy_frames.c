#include "../gs3d_internal.h"
int gs_legacy_frames(unsigned long *cursor){
    unsigned long cur=*cursor,nframes=gs3d_model.group[0].frame_count,
                  nverts=gs3d_model.group[0].vertex_count,i,end,vertsize;
    unsigned int type;unsigned char b[24];GS3D_Group *g=&gs3d_model.group[0];
    /* Type = 0 usually packed byte vertices; MDL4/5 may use 16-bit packed vertices. */
    for(i=0;i<nframes;i++){
        GS3D_Frame *f=&g->frames[i];
        if(gs_get(cur,b,4UL))return GS3D_ERR_RANGE;
        type=(unsigned int)gs_u32(b);
        if(type>2U)return GS3D_ERR_UNSUPPORTED;
        cur+=4UL;
        vertsize=(type==0U||gs3d_model.format==3) ? 4UL : 8UL;
        f->packed_stride=(unsigned short)vertsize;
        if(gs_addmul(cur,2UL,vertsize,&end))return GS3D_ERR_RANGE;
        cur=end;
        if(gs_get(cur,(unsigned char *)f->name,16UL))return GS3D_ERR_RANGE;
        f->name[16]='\0';cur+=16UL;f->offset=cur;
        f->count=nverts;
        if(gs_addmul(cur,nverts,vertsize,&end))return GS3D_ERR_RANGE;
        cur=end;
    }
    *cursor=cur;return GS3D_OK;
}
int gs_legacy_frame(unsigned int group,unsigned int frame){
    unsigned long i;GS3D_Group *g=&gs3d_model.group[group];
    GS3D_Frame *f=&g->frames[frame];
    for(i=0;i<g->vertex_count;i++){
        int r=gs_decode_vertex_legacy(f->offset+i*(unsigned long)f->packed_stride,
             f->packed_stride,&gs3d_model.v[g->vertex_first+i]);
        if(r)return r;
    }
    return GS3D_OK;
}
