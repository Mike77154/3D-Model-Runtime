#include "../gs3d_internal.h"
int gs_skin_read(unsigned long pos,unsigned long w,unsigned long h,
                 unsigned int type,int write_pixels) {
    unsigned long i,npix,sz;unsigned int kind=type&15U;
    unsigned char b[4],p[4];unsigned long pixel;unsigned int u;
    if(gs_skin_payload(pos,w,h,type,&sz))return GS3D_ERR_UNSUPPORTED;
    if(gs_seek(pos,sz))return GS3D_ERR_RANGE;
    if(!write_pixels)return 0;
    if(kind==6U || kind==7U || kind==1U) return GS3D_ERR_UNSUPPORTED;
    if(w && h>GS3D_MAX_TEXELS/w)return GS3D_ERR_CAPACITY;
    npix=w*h;
    for(i=0UL;i<npix;i++){
        if(kind==0U){if(gs_read(b,1UL))return GS3D_ERR_IO;
            u=(unsigned int)b[0];p[0]=gs3d_palette[u*3U];
            p[1]=gs3d_palette[u*3U+1U];p[2]=gs3d_palette[u*3U+2U];p[3]=255U;
        } else if(kind==2U||kind==10U){
            if(gs_read(b,2UL))return GS3D_ERR_IO;
            pixel=(unsigned long)gs_u16(b);
            p[0]=(unsigned char)((((pixel>>11)&31UL)*255UL)/31UL);
            p[1]=(unsigned char)((((pixel>>5)&63UL)*255UL)/63UL);
            p[2]=(unsigned char)(((pixel&31UL)*255UL)/31UL);p[3]=255U;
        } else if(kind==3U||kind==11U){
            if(gs_read(b,2UL))return GS3D_ERR_IO;
            pixel=(unsigned long)gs_u16(b);
            p[0]=(unsigned char)(((pixel>>8)&15UL)*17UL);
            p[1]=(unsigned char)(((pixel>>4)&15UL)*17UL);
            p[2]=(unsigned char)((pixel&15UL)*17UL);
            p[3]=(unsigned char)(((pixel>>12)&15UL)*17UL);
        } else if(kind==4U||kind==12U){
            if(gs_read(b,3UL))return GS3D_ERR_IO;
            p[0]=b[0];p[1]=b[1];p[2]=b[2];p[3]=255U;
        } else if(kind==5U||kind==13U){
            if(gs_read(b,4UL))return GS3D_ERR_IO;
            p[0]=b[0];p[1]=b[1];p[2]=b[2];p[3]=b[3];
        } else return GS3D_ERR_UNSUPPORTED;
        gs3d_rgba[i*4UL]=p[0];gs3d_rgba[i*4UL+1UL]=p[1];
        gs3d_rgba[i*4UL+2UL]=p[2];gs3d_rgba[i*4UL+3UL]=p[3];
    }
    return 0;
}
GS3D_ExternalTextureDecode gs_external_decoder=(GS3D_ExternalTextureDecode)0;
void *gs_external_userdata=(void *)0;
void gs3d_set_external_texture_decoder(GS3D_ExternalTextureDecode cb,void *userdata){
    gs_external_decoder=cb;gs_external_userdata=userdata;
}
int gs3d_texture_resolve(unsigned int group,unsigned int slot,
                         unsigned int *actual_group,unsigned int *actual_slot){
    unsigned int depth=0U,g=group,s=slot;const GS3D_Skin *sk;
    if(!gs_is_open||!actual_group||!actual_slot||g>=gs3d_model.groups)return GS3D_ERR_RANGE;
    /* Groups without skins may share the common palette of group zero. */
    if(!gs3d_model.group[g].skin_count){g=0U;}
    for(;;){
        if(s>=gs3d_model.group[g].skin_count)return GS3D_ERR_RANGE;
        sk=&gs3d_model.group[g].skins[s];
        if(sk->type!=1U)break;
        if(++depth>GS3D_MAX_SKINS)return GS3D_ERR_CORRUPT;
        if((unsigned int)sk->width==s)return GS3D_ERR_CORRUPT;
        s=(unsigned int)sk->width; /* MDL7 reference index is in width. */
    }
    *actual_group=g;*actual_slot=s;return GS3D_OK;
}
int gs3d_texture_info(unsigned int group,unsigned int slot,GS3D_TextureInfo *out){
    const GS3D_Skin *sk;unsigned int rg,rs;int r;
    if(!out||!gs_is_open||group>=gs3d_model.groups)return GS3D_ERR_RANGE;
    if(slot>=gs3d_model.group[group].skin_count)return GS3D_ERR_RANGE;
    memset(out,0,sizeof(*out));
    sk=&gs3d_model.group[group].skins[slot];
    out->group=group;out->slot=slot;out->image_type=sk->type;
    out->encoded_bytes=sk->byte_count;
    out->has_material=sk->has_material;
    memcpy(out->external,sk->external,129U);
    out->storage=sk->type==1U?GS3D_TEXTURE_REFERENCE:
                 sk->type==6U?GS3D_TEXTURE_DDS:
                 sk->type==7U?GS3D_TEXTURE_EXTERNAL:
                 (!sk->width||!sk->height)?GS3D_TEXTURE_COLOR_ONLY:GS3D_TEXTURE_EMBEDDED;
    r=gs3d_texture_resolve(group,slot,&rg,&rs);if(r)return r;
    out->resolved_group=rg;out->resolved_slot=rs;
    sk=&gs3d_model.group[rg].skins[rs];
    out->width=sk->type==6U?0UL:(unsigned long)sk->width;
    out->height=sk->type==6U?0UL:(unsigned long)sk->height;
    return GS3D_OK;
}
int gs3d_decode_skin(unsigned int group,unsigned int skin,unsigned long *w,unsigned long *h){
    const GS3D_Skin *s;unsigned int rg,rs;unsigned long ww,hh;int r;
    r=gs3d_texture_resolve(group,skin,&rg,&rs);if(r)return r;
    s=&gs3d_model.group[rg].skins[rs];
    if(s->type==6U){
        r=gs_dds_decode(s->offset,s->byte_count,&ww,&hh);
    }else if(s->type==7U){
        if(!gs_external_decoder)return GS3D_ERR_UNSUPPORTED;
        ww=hh=0UL;
        r=gs_external_decoder(gs_external_userdata,s->external,gs3d_rgba,
                              GS3D_MAX_TEXELS,&ww,&hh);
        if(!r&&(!ww||!hh||ww>GS3D_MAX_TEXELS||hh>GS3D_MAX_TEXELS/ww))r=GS3D_ERR_RANGE;
    }else{
        if(!s->width||!s->height)return GS3D_ERR_UNSUPPORTED;
        ww=s->width;hh=s->height;
        r=gs_skin_read(s->offset,ww,hh,(unsigned int)s->type,1);
    }
    if(r)return r;
    if(w)*w=ww;
    if(h)*h=hh;
    return GS3D_OK;
}
