#include "../gs3d_internal.h"
/* MDL7 fixed-file reader. Header and group sizes based on published MED layout. */
static int gs7_string_end(unsigned long pos,unsigned long limit,unsigned long *end,char *out,unsigned int max){
    unsigned long cur=pos;unsigned char c;unsigned int n=0U;
    if(pos>limit)return GS3D_ERR_RANGE;
    while(cur<limit){
        if(gs_get(cur,&c,1UL))return GS3D_ERR_IO;
        cur++;
        if(c==0U){*end=cur;out[n]='\0';return 0;}
        if(n+1U<max)out[n++]=(char)c;
    }
    return GS3D_ERR_CORRUPT;
}
static int gs7_effect_end(unsigned long pos,unsigned long limit,unsigned long *end){
    /* MED stores effect data as null-terminated text; reject unterminated data. */
    unsigned char c;unsigned long i;
    for(i=pos;i<limit;i++){
        if(gs_get(i,&c,1UL))return GS3D_ERR_IO;
        if(!c){*end=i+1UL;return 0;}
    }
    return GS3D_ERR_CORRUPT;
}
int gs7_material_read(unsigned long offset,unsigned long end,GS3D_Skin *skin);
int gs7_skin(unsigned long *cur,unsigned long group_end,GS3D_Skin *skin){
    unsigned char b[28];unsigned long payload,end,w,h;
    unsigned int typ,flags,kind;
    if(*cur>group_end || 28UL>group_end-*cur)return GS3D_ERR_RANGE;
    if(gs_get(*cur,b,28UL))return GS3D_ERR_RANGE;
    typ=(unsigned int)b[0];flags=typ&0xf0U;kind=typ&15U;
    w=gs_u32(b+4);h=gs_u32(b+8);
    if((kind!=6U && w>65535UL)||h>65535UL)return GS3D_ERR_CAPACITY;
    skin->width=(unsigned short)(kind==6U?0UL:w);skin->height=(unsigned short)h;
    skin->type=(unsigned char)kind;skin->flags=(unsigned char)flags;
    memcpy(skin->external,b+12,16U);skin->external[16]='\0';
    *cur+=28UL;skin->offset=*cur;
    if(kind==7U){
        if(gs7_string_end(*cur,group_end,cur,skin->external,129U))return GS3D_ERR_CORRUPT;
        skin->byte_count=0UL;
    } else {
        if(gs_skin_payload(*cur,w,h,kind,&payload))return GS3D_ERR_UNSUPPORTED;
        if(gs_addmul(*cur,1UL,payload,&end))return GS3D_ERR_RANGE;
        if(end>group_end)return GS3D_ERR_RANGE;
        skin->byte_count=payload;*cur=end;
    }
    if(flags & 0x10U){
        /* Material: 4 RGBA colors (64 bytes) and shininess float (4). */
        if(gs_addmul(*cur,1UL,(unsigned long)gs7_header_stride[3],&end))return GS3D_ERR_RANGE;
        if(gs7_material_read(*cur,end,skin))return GS3D_ERR_CORRUPT;
        if(end>group_end)return GS3D_ERR_RANGE;
        *cur=end;
    }
    if(flags&0x20U){
        if(gs7_effect_end(*cur,group_end,cur))return GS3D_ERR_CORRUPT;
    }
    return 0;
}
