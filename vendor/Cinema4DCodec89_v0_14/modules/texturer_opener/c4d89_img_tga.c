#include "c4d89_img_internal.h"
int c4d89_img_tga(const unsigned char *s,unsigned long n,unsigned char *out,
                  unsigned long cap,C4D89_ImgInfo *info)
{
    unsigned int type,depth,desc,paldepth,kind,pxsize,palnum;
    unsigned long pos,palpos,w,h,idx,run,py,px,p,base,count;
    unsigned int rr=0u,gg=0u,bb=0u,aa=255u,rep=0u;
    int rle,grey,cm;
    if(n<18ul)return C4D89_IMG_INVALID;
    type=s[2];depth=s[16];desc=s[17];
    w=i16le(s+12);h=i16le(s+14);
    if(desc&0xc0u)return C4D89_IMG_INVALID;
    if(!size_ok(w,h,cap))return C4D89_IMG_LIMIT;
    rle=(type==9u||type==10u||type==11u);
    grey=(type==3u||type==11u);cm=(type==1u||type==9u);
    if(!cm && !grey && type!=2u && type!=10u)return C4D89_IMG_UNSUPPORTED;
    if(cm && s[1]!=1u)return C4D89_IMG_CORRUPT;
    if(!cm && s[1]!=0u)return C4D89_IMG_CORRUPT;
    if(!cm && !grey && depth!=16u&&depth!=24u&&depth!=32u)return C4D89_IMG_UNSUPPORTED;
    if(grey && depth!=8u && depth!=16u)return C4D89_IMG_UNSUPPORTED;
    if(cm && depth!=8u && depth!=16u)return C4D89_IMG_UNSUPPORTED;
    pos=18ul+(unsigned long)s[0];if(pos>n)return C4D89_IMG_TRUNCATED;
    palpos=pos;palnum=i16le(s+5);paldepth=s[7];
    if(cm){ if(paldepth!=16u&&paldepth!=24u&&paldepth!=32u)return C4D89_IMG_UNSUPPORTED;
        base=(unsigned long)(paldepth+7u)/8ul;
        if(palnum==0u||(unsigned long)palnum>(n-pos)/base)return C4D89_IMG_TRUNCATED;
        pos+=(unsigned long)palnum*base;
    }
    pxsize=(depth+7u)/8u;run=0ul;count=w*h;
    for(idx=0ul;idx<count;idx++){
        if(!run){
            if(rle){if(pos>=n)return C4D89_IMG_TRUNCATED;
                kind=s[pos++];run=(unsigned long)(kind&127u)+1ul;rep=(kind&128u)?1u:0u;
                /* Reject packets that spill past the declared image. */
                if(run>count-idx)return C4D89_IMG_CORRUPT;
            } else {run=1ul;rep=0u;}
        }
        if(!rep || run==((unsigned long)(kind&127u)+1ul)){
            unsigned long q,value;
            if(pos>n||(unsigned long)pxsize>n-pos)return C4D89_IMG_TRUNCATED;
            q=pos;
            if(cm){value=pxsize==1u?s[q]:(unsigned long)i16le(s+q);
                if(value<(unsigned long)i16le(s+3)||value>=(unsigned long)i16le(s+3)+(unsigned long)palnum)return C4D89_IMG_CORRUPT;
                q=palpos+(value-(unsigned long)i16le(s+3))*(unsigned long)((paldepth+7u)/8u);
                depth=paldepth;
            }
            if(grey){rr=gg=bb=s[q];aa=pxsize==2u?s[q+1]:255u;}
            else if(depth==16u){value=i16le(s+q);bb=(unsigned int)((value&31ul)*255ul/31ul);
                gg=(unsigned int)(((value>>5)&31ul)*255ul/31ul);
                rr=(unsigned int)(((value>>10)&31ul)*255ul/31ul);
                aa=(desc&15u)?((value&0x8000ul)?255u:0u):255u;
            } else {bb=s[q];gg=s[q+1];rr=s[q+2];aa=depth==32u?s[q+3]:255u;}
            if(cm)depth=s[16];
            pos+=pxsize;
        }
        run--;
        px=idx%w;py=idx/w;
        if(!(desc&0x20u))py=h-1ul-py;
        if(desc&0x10u)px=w-1ul-px;
        p=(py*w+px)*4ul;out[p]=(unsigned char)rr;out[p+1]=(unsigned char)gg;
        out[p+2]=(unsigned char)bb;out[p+3]=(unsigned char)aa;
    }
    info->width=(unsigned int)w;info->height=(unsigned int)h;info->format=C4D89_IMG_FORMAT_TGA;
    info->source_channels=grey?1u:(s[16]==32u?4u:3u);return C4D89_IMG_OK;
}
