#include "../gs3d_internal.h"
/* DDS surface decoder: DXT1/3/5 (BC1/2/3) and 32-bit RGB(A) bit masks.
   Decodes mip level zero directly into static RGBA buffer. No heap / float.
   DX10, BC4+, volume maps and cubemaps intentionally unsupported. */
static unsigned char gs_dds_expand5(unsigned int x){return (unsigned char)((x*255U)/31U);}
static unsigned char gs_dds_expand6(unsigned int x){return (unsigned char)((x*255U)/63U);}
static void gs_dds_color(unsigned int c,unsigned char rgba[4]){
    rgba[0]=gs_dds_expand5((c>>11)&31U);
    rgba[1]=gs_dds_expand6((c>>5)&63U);
    rgba[2]=gs_dds_expand5(c&31U);rgba[3]=255U;
}
static unsigned char gs_dds_mask(unsigned long raw,unsigned long mask,unsigned char fallback){
    unsigned int bits=0U,shift=0U;
    unsigned long maxv,v,t=mask;
    if(!mask)return fallback;
    while(!(t&1UL)){shift++;t>>=1;}
    while(t&1UL){bits++;t>>=1;}
    if(t||bits>24U)return fallback;
    maxv=(1UL<<bits)-1UL;
    v=(raw&mask)>>shift;
    return (unsigned char)((v*255UL+maxv/2UL)/maxv);
}
static void gs_dds_rgb(unsigned char out[4],const unsigned char rgba[4]){
    out[0]=rgba[0];out[1]=rgba[1];out[2]=rgba[2];out[3]=rgba[3];
}
static void gs_dds_block(unsigned char *b,unsigned int kind,
                         unsigned long x,unsigned long y,
                         unsigned long w,unsigned long h){
    unsigned char colors[4][4],alpha[8];
    unsigned int a,b2,c,pixel,col,row,sel,ci,ai;
    unsigned long dst,bit,word;
    unsigned int off=kind==1U?0U:8U;
    a=gs_u16(b+off);b2=gs_u16(b+off+2U);
    gs_dds_color(a,colors[0]);gs_dds_color(b2,colors[1]);
    for(c=0U;c<3U;c++){
        if(a>b2||kind!=1U){
            colors[2][c]=(unsigned char)((2U*(unsigned int)colors[0][c]+(unsigned int)colors[1][c])/3U);
            colors[3][c]=(unsigned char)(((unsigned int)colors[0][c]+2U*(unsigned int)colors[1][c])/3U);
        }else{
            colors[2][c]=(unsigned char)(((unsigned int)colors[0][c]+(unsigned int)colors[1][c])/2U);
            colors[3][c]=0U;
        }
    }
    colors[2][3]=255U;colors[3][3]=(unsigned char)((a<=b2&&kind==1U)?0U:255U);
    if(kind==3U){
        alpha[0]=b[0];alpha[1]=b[1];
        for(c=2U;c<8U;c++){
            if(alpha[0]>alpha[1])alpha[c]=(unsigned char)(((8U-c)*alpha[0]+(c-1U)*alpha[1])/7U);
            else if(c<6U)alpha[c]=(unsigned char)(((6U-c)*alpha[0]+(c-1U)*alpha[1])/5U);
            else alpha[c]=(unsigned char)(c==6U?0U:255U);
        }
    }
    for(row=0U;row<4U;row++)for(col=0U;col<4U;col++){
        if(x+col>=w||y+row>=h)continue;
        pixel=row*4U+col;
        ci=(unsigned int)((b[off+4U+pixel/4U]>>(2U*(pixel%4U)))&3U);
        dst=((y+row)*w+x+col)*4UL;
        gs_dds_rgb(gs3d_rgba+dst,colors[ci]);
        if(kind==2U){
            sel=b[pixel/2U];
            gs3d_rgba[dst+3UL]=(unsigned char)((((sel>>(4U*(pixel%2U)))&15U)*17U));
        }else if(kind==3U){
            bit=3UL*(unsigned long)pixel;
            word=(unsigned long)b[2U+(unsigned int)(bit/8UL)];
            if((bit%8UL)>5UL && 3U+(unsigned int)(bit/8UL)<8U)
                word|=((unsigned long)b[3U+(unsigned int)(bit/8UL)])<<8;
            ai=(unsigned int)((word>>(bit%8UL))&7UL);
            gs3d_rgba[dst+3UL]=alpha[ai];
        }
    }
}
int gs_dds_decode(unsigned long off,unsigned long n,unsigned long *width,unsigned long *height){
    unsigned char h[128],px[4],blk[16];unsigned long w,ht,bw,bh,needed,dataoff,pos,x,y,k;
    unsigned long rgbmask,gmask,bmask,amask,v;
    unsigned int kind=0U,bytespp=0U,channels;int rc;
    if(!width||!height||n<128UL)return GS3D_ERR_CORRUPT;
    if(gs_get(off,h,128UL))return GS3D_ERR_RANGE;
    if(memcmp(h,"DDS ",4U)!=0||gs_u32(h+4)!=124UL||gs_u32(h+76)!=32UL)return GS3D_ERR_CORRUPT;
    ht=gs_u32(h+12);w=gs_u32(h+16);
    if(!w||!ht||w>GS3D_MAX_TEXELS||ht>GS3D_MAX_TEXELS/w)return GS3D_ERR_CAPACITY;
    /* Exclude unsupported layered/cubemap/volume without silently mixing slices. */
    if(gs_u32(h+112)&0x0000FE00UL)return GS3D_ERR_UNSUPPORTED;
    if((gs_u32(h+80)&4UL)!=0UL){
        if(memcmp(h+84,"DXT1",4U)==0)kind=1U;
        else if(memcmp(h+84,"DXT3",4U)==0)kind=2U;
        else if(memcmp(h+84,"DXT5",4U)==0)kind=3U;
        else return GS3D_ERR_UNSUPPORTED;
        bw=(w+3UL)/4UL;bh=(ht+3UL)/4UL;bytespp=kind==1U?8U:16U;
        if(bw>ULONG_MAX/bh)return GS3D_ERR_RANGE;
        k=bw*bh;if(k>ULONG_MAX/(unsigned long)bytespp)return GS3D_ERR_RANGE;
        needed=k*(unsigned long)bytespp;
        if(needed>n-128UL)return GS3D_ERR_RANGE;
        dataoff=off+128UL;
        for(y=0UL;y<bh;y++)for(x=0UL;x<bw;x++){
            pos=dataoff+(y*bw+x)*(unsigned long)bytespp;
            rc=gs_get(pos,blk,(unsigned long)bytespp);if(rc)return GS3D_ERR_IO;
            gs_dds_block(blk,kind,x*4UL,y*4UL,w,ht);
        }
    }else{
        channels=gs_u32(h+88);
        if(channels!=32U)return GS3D_ERR_UNSUPPORTED;
        rgbmask=gs_u32(h+92);gmask=gs_u32(h+96);bmask=gs_u32(h+100);amask=gs_u32(h+104);
        if(!rgbmask||!gmask||!bmask)return GS3D_ERR_UNSUPPORTED;
        k=w*ht;if(k>ULONG_MAX/4UL)return GS3D_ERR_RANGE;
        if(k*4UL>n-128UL)return GS3D_ERR_RANGE;
        dataoff=off+128UL;
        for(pos=0UL;pos<k;pos++){
            rc=gs_get(dataoff+pos*4UL,px,4UL);if(rc)return GS3D_ERR_IO;
            v=gs_u32(px);
            gs3d_rgba[pos*4UL]=gs_dds_mask(v,rgbmask,0U);
            gs3d_rgba[pos*4UL+1UL]=gs_dds_mask(v,gmask,0U);
            gs3d_rgba[pos*4UL+2UL]=gs_dds_mask(v,bmask,0U);
            gs3d_rgba[pos*4UL+3UL]=gs_dds_mask(v,amask,255U);
        }
    }
    *width=w;*height=ht;return GS3D_OK;
}
