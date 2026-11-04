#include "../gs3d_internal.h"
int gs_legacy_skins(unsigned long *cursor){
    unsigned long i,sz,end,cur=*cursor,w,h;unsigned char b[8];unsigned int type;
    GS3D_Group *g=&gs3d_model.group[0];unsigned char head[84];
    if(gs_get(0UL,head,84UL))return GS3D_ERR_RANGE;
    w=gs_u32(head+52);h=gs_u32(head+56);
    for(i=0UL;i<g->skin_count;i++){
        GS3D_Skin *s=&g->skins[i];
        if(gs_get(cur,b,4UL))return GS3D_ERR_RANGE;
        type=(unsigned int)gs_u32(b);cur+=4UL;
        if(type>255U || type==1U)return GS3D_ERR_UNSUPPORTED;
        s->type=(unsigned char)type;
        s->width=(unsigned short)w;s->height=(unsigned short)h;
        if(gs3d_model.format==5){
            if(gs_get(cur,b,8UL))return GS3D_ERR_RANGE;
            if(gs_u32(b)>65535UL || gs_u32(b+4)>65535UL)return GS3D_ERR_CAPACITY;
            s->width=(unsigned short)gs_u32(b);s->height=(unsigned short)gs_u32(b+4);
            cur+=8UL;
        }
        s->offset=cur;sz=0UL;
        if(gs_skin_payload(cur,(unsigned long)s->width,(unsigned long)s->height,type,&sz))return GS3D_ERR_UNSUPPORTED;
        if(gs_addmul(cur,1UL,sz,&end))return GS3D_ERR_RANGE;
        s->byte_count=sz;cur=end;
    }
    *cursor=cur;return GS3D_OK;
}
